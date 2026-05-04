"""orchestrator.py — point d'entree unique du harness de benchmarks.

Lit ``config/instances.yaml`` et ``config/benchmark.yaml``, execute les passes
A (parallele structurelle) et B (timings sur 4 coeurs CCDs distincts), declenche
les invariants, plots, summary, notifications. Reprenable, robuste, instrumente.

Pipeline :
  1. Validation des configs (echec rapide si YAML invalide).
  2. Setup output : results/<UTC>/, env.txt, git_info.txt, CSV vides.
  3. Resume : skip les taches deja faites (lecture des CSV existants).
  4. Passe A (parallele 64 jobs) : structure + invariants.
  5. Verification des invariants theoriques.
  6. Passe B (4 coeurs CCDs distincts) : timings purs avec medianes.
  7. Plots PDF/PNG + SUMMARY.md.

CLI : voir ``orchestrator.py --help``.
"""

from __future__ import annotations

import argparse
import csv
import dataclasses
import datetime as dt
import json
import logging
import os
import platform
import shutil
import signal
import socket
import subprocess
import sys
import threading
import time
import traceback
from concurrent.futures import (
    ProcessPoolExecutor,
    as_completed,
)
from pathlib import Path
from statistics import median, pstdev
from typing import Any

import yaml

# Imports locaux : on prefere des chemins explicites pour fonctionner aussi bien
# en `python3 orchestrator.py` (cwd = benchmarks/) qu'en `python3 -m orchestrator`.
_HERE = Path(__file__).resolve().parent
if str(_HERE) not in sys.path:
    sys.path.insert(0, str(_HERE))

from runners.run_dp import run_dp  # noqa: E402
from runners.run_z3 import run_z3_maxsat, run_z3_sat  # noqa: E402
from runners import invariants as invariants_mod  # noqa: E402
from notifier import Notifier  # noqa: E402

ORCHESTRATOR_VERSION = "0.1.0"

logger = logging.getLogger("orchestrator")


# ===========================================================================
# Dataclasses de configuration
# ===========================================================================

@dataclasses.dataclass
class InstanceConfig:
    id: str
    path: str
    family: str
    n_vars: int | None = None
    n_clauses: int | None = None
    params: dict[str, Any] = dataclasses.field(default_factory=dict)
    expected_psw_max: int | None = None
    expected_psw_severity: str = "none"
    timeout_s: int | None = None
    enabled: bool = True
    notes: str = ""


@dataclasses.dataclass
class BenchmarkConfig:
    machine: dict[str, Any]
    dp: dict[str, Any]
    z3: dict[str, Any]
    passe_a: dict[str, Any]
    passe_b: dict[str, Any]
    random_sample: dict[str, Any]
    plots: dict[str, Any]
    logging_cfg: dict[str, Any]
    notifications: dict[str, Any]
    # Bench des requetes sur DAG compile (in-process). Optionnelle :
    # absente du YAML => dict vide (passe C inactive).
    passe_c: dict[str, Any] = dataclasses.field(default_factory=dict)


_VALID_FAMILIES = {"type1", "type2", "type3", "random", "tseytin", "factorization", "misc"}
_VALID_SEVERITIES = {"hard", "soft", "none"}


def _load_instances(path: Path) -> list[InstanceConfig]:
    raw = yaml.safe_load(path.read_text())
    if not raw or "instances" not in raw:
        raise ValueError(f"{path} : cle 'instances' manquante")
    out: list[InstanceConfig] = []
    seen_ids: set[str] = set()
    for entry in raw["instances"]:
        inst = InstanceConfig(
            id=entry["id"],
            path=entry["path"],
            family=entry["family"],
            n_vars=entry.get("n_vars"),
            n_clauses=entry.get("n_clauses"),
            params=entry.get("params", {}) or {},
            expected_psw_max=entry.get("expected_psw_max"),
            expected_psw_severity=entry.get("expected_psw_severity", "none"),
            timeout_s=entry.get("timeout_s"),
            enabled=entry.get("enabled", True),
            notes=entry.get("notes", ""),
        )
        if inst.id in seen_ids:
            raise ValueError(f"id dupplique dans {path}: {inst.id}")
        seen_ids.add(inst.id)
        if inst.family not in _VALID_FAMILIES:
            raise ValueError(f"famille inconnue pour {inst.id}: {inst.family}")
        if inst.expected_psw_severity not in _VALID_SEVERITIES:
            raise ValueError(f"severity inconnue pour {inst.id}: {inst.expected_psw_severity}")
        if inst.timeout_s is not None and inst.timeout_s <= 0:
            raise ValueError(f"timeout_s invalide pour {inst.id}: {inst.timeout_s}")
        out.append(inst)
    return out


def _load_benchmark_config(path: Path) -> BenchmarkConfig:
    raw = yaml.safe_load(path.read_text())
    if not raw:
        raise ValueError(f"{path} vide")
    required = ["machine", "dp", "z3", "passe_a", "passe_b", "random_sample",
                "plots", "logging", "notifications"]
    for key in required:
        if key not in raw:
            raise ValueError(f"{path} : cle '{key}' manquante")
    return BenchmarkConfig(
        machine=raw["machine"],
        dp=raw["dp"],
        z3=raw["z3"],
        passe_a=raw["passe_a"],
        passe_b=raw["passe_b"],
        random_sample=raw["random_sample"],
        plots=raw["plots"],
        logging_cfg=raw["logging"],
        notifications=raw["notifications"],
        passe_c=raw.get("passe_c", {}) or {},
    )


def _validate_paths(instances: list[InstanceConfig], repo_root: Path) -> list[str]:
    """Retourne la liste des paths manquants (relatifs a src/)."""
    missing = []
    src = repo_root / "src"
    for inst in instances:
        if not (src / inst.path).exists():
            missing.append(f"{inst.id} -> {inst.path}")
    return missing


# ===========================================================================
# Setup output : dossier results/<timestamp>/, env.txt, git_info.txt, CSV vides
# ===========================================================================

# Schemas CSV : ordre et noms de colonnes. Source de verite cote orchestrateur.
STRUCTURE_CSV_COLS = [
    "timestamp_utc", "instance_id", "family", "mode", "seed", "runner", "status",
    "n_vars", "n_clauses",
    "ps_width", "ps_width_pv", "ps_width_pvb", "trie_num_ps_sets",
    "time_parsing_ms", "time_phase0_ms", "time_phase1_ms", "time_phase2_ms",
    "time_phase3_ms", "time_total_ms", "elapsed_wallclock_ms",
    "maxsat", "sharpsat", "dnnf_count_recomputed", "dnnf_count_match",
    "dnnf_nodes", "dnnf_edges", "dnnf_bound_7k3nm",
    "exit_code", "command", "error_message",
    # B6 : queue de stderr (1 KB max, newlines remplaces par " | ") pour
    # diagnostic post-mortem des crashes (cf. bug du trie.c silencieux dans
    # le run 1).
    "stderr_tail",
    # Passe C : timings et resultats des requetes sur DAG compile (in-process).
    # Champs vides pour les lignes de passe A et B (rétro-compat). Renseignes
    # uniquement quand runner == "dp_query".
    "query_co_ms", "query_va_ms", "query_ct_ms", "query_me_ms",
    "query_ce_ms", "query_im_ms", "query_enum_first_ms", "query_enum_all_ms",
    "query_enum_count", "query_co_result", "query_va_result",
    "query_ce_result", "query_im_result", "query_ct_count",
    "query_repetitions", "query_enum_all_skipped",
]

Z3_CSV_COLS = [
    "timestamp_utc", "instance_id", "family", "z3_kind", "z3_status",
    "z3_maxsat", "z3_sat_status", "z3_solve_ms", "z3_total_ms",
    "z3_conflicts", "z3_decisions", "z3_restarts", "z3_propagations",
    "z3_time_internal_s", "z3_n_vars", "z3_n_clauses", "z3_stats_keys_available",
    "z3_error",
    # B2 : drapeau leve quand le runner Z3 a court-circuite en UNSAT a cause
    # d'une clause vide DIMACS (sans appeler Z3).
    "z3_short_circuit_empty_clause",
]

TIMINGS_CSV_COLS = [
    "timestamp_utc", "instance_id", "family", "mode", "seed", "runner",
    "repetitions", "time_total_ms_median", "time_total_ms_min",
    "time_total_ms_max", "time_total_ms_stddev",
    "time_phase0_ms_median", "time_phase1_ms_median", "time_phase2_ms_median",
    "time_phase3_ms_median",
    "n_failures", "status",
]


def _setup_output_dir(output_dir: Path, repo_root: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "figures").mkdir(exist_ok=True)
    # Cree les CSV avec headers s'ils n'existent pas (idempotent pour --resume).
    for name, cols in [
        ("structure.csv", STRUCTURE_CSV_COLS),
        ("z3.csv", Z3_CSV_COLS),
        ("timings.csv", TIMINGS_CSV_COLS),
    ]:
        path = output_dir / name
        if not path.exists():
            with path.open("w", newline="", encoding="utf-8") as fh:
                csv.writer(fh).writerow(cols)
    # progress.log et failures.log : touch.
    (output_dir / "progress.log").touch(exist_ok=True)
    (output_dir / "failures.log").touch(exist_ok=True)
    # env.txt + git_info.txt : ecrits une seule fois (premiere execution).
    if not (output_dir / "env.txt").exists():
        _write_env_snapshot(output_dir / "env.txt")
    if not (output_dir / "git_info.txt").exists():
        _write_git_info(output_dir / "git_info.txt", repo_root)


def _safe_run(cmd: list[str], cwd: Path | None = None) -> str:
    """Lance une commande shell et retourne stdout (ou message d'erreur)."""
    try:
        proc = subprocess.run(
            cmd, capture_output=True, text=True, timeout=15,
            cwd=str(cwd) if cwd else None, check=False,
        )
        return proc.stdout + (proc.stderr if proc.returncode != 0 else "")
    except FileNotFoundError:
        return f"<commande introuvable: {cmd[0]}>\n"
    except Exception as e:
        return f"<erreur {cmd}: {e}>\n"


def _write_env_snapshot(path: Path) -> None:
    parts: list[str] = []
    parts.append(f"# env.txt - genere le {dt.datetime.now(dt.timezone.utc).isoformat()}\n")
    parts.append(f"hostname: {socket.gethostname()}\n")
    parts.append(f"python: {platform.python_version()}\n")
    parts.append(f"system: {platform.system()} {platform.release()}\n")
    parts.append("\n## uname -a\n")
    parts.append(_safe_run(["uname", "-a"]))
    parts.append("\n## lscpu\n")
    parts.append(_safe_run(["lscpu"]))
    parts.append("\n## free -h\n")
    parts.append(_safe_run(["free", "-h"]))
    parts.append("\n## /etc/os-release\n")
    if Path("/etc/os-release").exists():
        parts.append(Path("/etc/os-release").read_text())
    parts.append("\n## gcc --version\n")
    parts.append(_safe_run(["gcc", "--version"]))
    parts.append("\n## python3 --version\n")
    parts.append(_safe_run(["python3", "--version"]))
    parts.append("\n## pip freeze (extrait)\n")
    pip_out = _safe_run(["python3", "-m", "pip", "freeze"])
    for line in pip_out.splitlines():
        low = line.lower()
        if any(pkg in low for pkg in ["z3", "matplotlib", "pandas", "numpy",
                                       "pyyaml", "psutil", "jinja2", "requests"]):
            parts.append(line + "\n")
    parts.append("\n## env (relevant only)\n")
    for k in sorted(os.environ):
        if k.startswith(("RANDOM_SEED", "OMP_", "TASKSET", "MKL_", "OPENBLAS_")):
            parts.append(f"{k}={os.environ[k]}\n")
    path.write_text("".join(parts))


def _write_git_info(path: Path, repo_root: Path) -> None:
    parts = [f"# git_info.txt - genere le {dt.datetime.now(dt.timezone.utc).isoformat()}\n"]
    parts.append("\n## git rev-parse HEAD\n")
    parts.append(_safe_run(["git", "rev-parse", "HEAD"], cwd=repo_root))
    parts.append("\n## git rev-parse --abbrev-ref HEAD\n")
    parts.append(_safe_run(["git", "rev-parse", "--abbrev-ref", "HEAD"], cwd=repo_root))
    parts.append("\n## git status -s\n")
    parts.append(_safe_run(["git", "status", "-s"], cwd=repo_root))
    parts.append("\n## git log -3 --oneline\n")
    parts.append(_safe_run(["git", "log", "-3", "--oneline"], cwd=repo_root))
    path.write_text("".join(parts))


# ===========================================================================
# Resume : detection des taches deja faites
# ===========================================================================

def _existing_keys_structure(csv_path: Path) -> set[tuple[str, str, int, str]]:
    """Lit structure.csv et retourne les cles deja faites."""
    if not csv_path.exists():
        return set()
    keys: set[tuple[str, str, int, str]] = set()
    try:
        with csv_path.open("r", newline="", encoding="utf-8") as fh:
            for row in csv.DictReader(fh):
                try:
                    seed = int(row.get("seed") or 0)
                except ValueError:
                    seed = 0
                keys.add((row["instance_id"], row["mode"], seed, row["runner"]))
    except Exception as e:
        logger.warning("Lecture %s impossible: %s (on repart from scratch)", csv_path, e)
    return keys


def _existing_keys_z3(csv_path: Path) -> set[tuple[str, str]]:
    if not csv_path.exists():
        return set()
    keys: set[tuple[str, str]] = set()
    try:
        with csv_path.open("r", newline="", encoding="utf-8") as fh:
            for row in csv.DictReader(fh):
                keys.add((row["instance_id"], row["z3_kind"]))
    except Exception:
        pass
    return keys


# ===========================================================================
# Helpers timeouts par instance
# ===========================================================================

def _timeout_for(inst: InstanceConfig, passe_cfg: dict[str, Any]) -> int:
    if inst.timeout_s is not None:
        return int(inst.timeout_s)
    fam_to = passe_cfg.get("family_timeouts", {}).get(inst.family)
    if fam_to is not None:
        return int(fam_to)
    return int(passe_cfg.get("default_timeout_s", 1800))


# ===========================================================================
# Heartbeat thread
# ===========================================================================

class HeartbeatState:
    """Etat partage entre le thread principal et le thread heartbeat."""

    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.start_time = time.time()
        self.done = 0
        self.failed = 0
        self.total = 0
        self.phase = "init"  # init | passe_a | invariants | passe_b | plots | summary

    def update(self, *, done: int | None = None, failed: int | None = None,
               total: int | None = None, phase: str | None = None) -> None:
        with self.lock:
            if done is not None:
                self.done = done
            if failed is not None:
                self.failed = failed
            if total is not None:
                self.total = total
            if phase is not None:
                self.phase = phase

    def snapshot(self) -> tuple[int, int, int, str]:
        with self.lock:
            return self.done, self.failed, self.total, self.phase


def _eta_str(state: HeartbeatState) -> str:
    done, _failed, total, _phase = state.snapshot()
    if done <= 0 or total <= 0:
        return "inconnue"
    elapsed = time.time() - state.start_time
    rate = elapsed / done
    remaining = max(0, total - done) * rate
    return _human_duration(remaining)


def _human_duration(secs: float) -> str:
    secs = int(secs)
    h, rem = divmod(secs, 3600)
    m, s = divmod(rem, 60)
    if h > 0:
        return f"{h}h{m:02d}m"
    if m > 0:
        return f"{m}m{s:02d}s"
    return f"{s}s"


def _heartbeat_loop(progress_log: Path, state: HeartbeatState,
                    interval_s: int, stop_event: threading.Event,
                    notifier: Notifier, notify_interval_s: int) -> None:
    """Thread daemon : ecrit progress.log et envoie une notif toutes les
    notify_interval_s secondes (typiquement 30 min)."""
    last_notify = 0.0
    while not stop_event.is_set():
        done, failed, total, phase = state.snapshot()
        ts = dt.datetime.now(dt.timezone.utc).isoformat()
        pct = 100.0 * done / total if total else 0.0
        line = (f"[{ts}] {phase}: {done}/{total} taches done ({pct:.1f}%), "
                f"{failed} failures, ETA {_eta_str(state)}\n")
        try:
            with progress_log.open("a", encoding="utf-8") as fh:
                fh.write(line)
        except Exception as e:
            logger.warning("progress.log: %s", e)
        # Notifier (heartbeat espace)
        now = time.time()
        if now - last_notify >= notify_interval_s:
            notifier.heartbeat(done, total, failed, _eta_str(state))
            last_notify = now
        # Wait avec sortie precoce si stop demande
        stop_event.wait(timeout=interval_s)


# ===========================================================================
# CSV writers thread-safe
# ===========================================================================

class CsvAppender:
    """Append CSV thread-safe, flush systematique pour la reprise."""

    def __init__(self, path: Path, columns: list[str], flush_each_row: bool = True) -> None:
        self.path = path
        self.columns = columns
        self.flush_each_row = flush_each_row
        self._lock = threading.Lock()

    def append(self, row: dict[str, Any]) -> None:
        normalized = []
        for col in self.columns:
            val = row.get(col)
            if val is None:
                normalized.append("")
            elif isinstance(val, bool):
                normalized.append("true" if val else "false")
            elif isinstance(val, float):
                # Pas de scientifique sauf valeur enorme.
                if abs(val) >= 1e10:
                    normalized.append(f"{val:.6e}")
                else:
                    normalized.append(f"{val:.6f}")
            else:
                # str(val) ; csv module gere le quoting RFC 4180.
                normalized.append(val)
        with self._lock:
            with self.path.open("a", newline="", encoding="utf-8") as fh:
                writer = csv.writer(fh, quoting=csv.QUOTE_MINIMAL)
                writer.writerow(normalized)
                if self.flush_each_row:
                    fh.flush()


# ===========================================================================
# Workers : fonctions appelees dans les sous-process. Doivent etre top-level
# pour etre picklables par ProcessPoolExecutor.
# ===========================================================================

@dataclasses.dataclass
class DpTask:
    instance_id: str
    instance_path: str
    mode: str
    seed: int
    timeout_s: int
    memory_cap_bytes: int
    binary_path: str
    cwd: str
    nice_level: int
    taskset_cpu: int | None
    repo_root: str
    with_queries: bool = False


def _worker_run_dp(task: DpTask) -> dict[str, Any]:
    # Reconstruit un objet "instance" minimal pour run_dp.
    class _I:  # noqa: N801
        pass
    inst = _I()
    inst.id = task.instance_id
    inst.path = task.instance_path
    return run_dp(
        inst,
        mode=task.mode,
        seed=task.seed,
        timeout_s=task.timeout_s,
        memory_cap_bytes=task.memory_cap_bytes,
        binary_path=task.binary_path,
        cwd=task.cwd,
        nice_level=task.nice_level,
        taskset_cpu=task.taskset_cpu,
        repo_root=task.repo_root,
        with_queries=task.with_queries,
    )


@dataclasses.dataclass
class Z3Task:
    instance_id: str
    instance_path: str
    kind: str  # "maxsat" | "sat"
    timeout_s: int
    repo_root: str


def _worker_run_z3(task: Z3Task) -> dict[str, Any]:
    class _I:
        pass
    inst = _I()
    inst.id = task.instance_id
    inst.path = task.instance_path
    if task.kind == "maxsat":
        return run_z3_maxsat(inst, task.timeout_s, repo_root=task.repo_root)
    return run_z3_sat(inst, task.timeout_s, repo_root=task.repo_root)


# ===========================================================================
# Construction des taches
# ===========================================================================

def _build_passe_a_dp_tasks(
    instances: list[InstanceConfig],
    cfg: BenchmarkConfig,
    repo_root: Path,
) -> list[DpTask]:
    out: list[DpTask] = []
    mem_cap = int(cfg.machine.get("memory_cap_gib_per_proc", 50)) * (1024 ** 3)
    nice = int(cfg.machine.get("nice_level", 19))
    binary = cfg.dp.get("binary", "src/sat_solver")
    cwd = cfg.dp.get("cwd", "src")
    modes = list(cfg.dp.get("modes_arbre", ["greedy", "linear"]))

    for inst in instances:
        if not inst.enabled:
            continue
        timeout = _timeout_for(inst, cfg.passe_a)
        for mode in modes:
            out.append(DpTask(
                instance_id=inst.id,
                instance_path=inst.path,
                mode=mode,
                seed=0,
                timeout_s=timeout,
                memory_cap_bytes=mem_cap,
                binary_path=binary,
                cwd=cwd,
                nice_level=nice,
                taskset_cpu=None,
                repo_root=str(repo_root),
            ))

    # random_sample : 5 instances * 3 seeds.
    if cfg.random_sample.get("enabled", True):
        sample_ids = set(cfg.random_sample.get("instances", []))
        seeds = list(cfg.random_sample.get("seeds", [0, 1, 2]))
        for inst in instances:
            if inst.id not in sample_ids or not inst.enabled:
                continue
            timeout = _timeout_for(inst, cfg.passe_a)
            for seed in seeds:
                out.append(DpTask(
                    instance_id=inst.id,
                    instance_path=inst.path,
                    mode="random",
                    seed=int(seed),
                    timeout_s=timeout,
                    memory_cap_bytes=mem_cap,
                    binary_path=binary,
                    cwd=cwd,
                    nice_level=nice,
                    taskset_cpu=None,
                    repo_root=str(repo_root),
                ))
    return out


def _build_passe_a_z3_tasks(
    instances: list[InstanceConfig],
    cfg: BenchmarkConfig,
    repo_root: Path,
) -> list[Z3Task]:
    if not cfg.z3.get("enabled", True):
        return []
    out: list[Z3Task] = []
    timeout = int(cfg.z3.get("timeout_s", 600))
    for inst in instances:
        if not inst.enabled:
            continue
        out.append(Z3Task(
            instance_id=inst.id, instance_path=inst.path,
            kind="maxsat", timeout_s=timeout, repo_root=str(repo_root),
        ))
        out.append(Z3Task(
            instance_id=inst.id, instance_path=inst.path,
            kind="sat", timeout_s=timeout, repo_root=str(repo_root),
        ))
    return out


# ===========================================================================
# Runtime : passe A (parallele), passe B (4 coeurs CCDs distincts)
# ===========================================================================

def _row_for_dp(result: dict[str, Any], inst_by_id: dict[str, InstanceConfig],
                runner_label: str = "dp") -> dict[str, Any]:
    inst = inst_by_id.get(result.get("instance_id", ""))
    family = inst.family if inst else ""
    # Tronque + neutralise les newlines pour le CSV (sinon un stderr multiligne
    # casse la coherence visuelle des outils CSV simples).
    stderr_raw = (result.get("stderr") or "")
    stderr_tail = stderr_raw[-1024:].replace("\r\n", " | ").replace("\n", " | ")
    row = {
        "timestamp_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "instance_id": result.get("instance_id"),
        "family": family,
        "mode": result.get("mode"),
        "seed": result.get("seed"),
        "runner": runner_label,
        "status": result.get("status"),
        "n_vars": result.get("n_vars"),
        "n_clauses": result.get("n_clauses"),
        "ps_width": result.get("ps_width"),
        "ps_width_pv": result.get("ps_width_pv"),
        "ps_width_pvb": result.get("ps_width_pvb"),
        "trie_num_ps_sets": result.get("trie_num_ps_sets"),
        "time_parsing_ms": result.get("time_parsing_ms"),
        "time_phase0_ms": result.get("time_phase0_ms"),
        "time_phase1_ms": result.get("time_phase1_ms"),
        "time_phase2_ms": result.get("time_phase2_ms"),
        "time_phase3_ms": result.get("time_phase3_ms"),
        "time_total_ms": result.get("time_total_ms"),
        "elapsed_wallclock_ms": result.get("elapsed_wallclock_ms"),
        "maxsat": result.get("maxsat"),
        "sharpsat": result.get("sharpsat"),
        "dnnf_count_recomputed": result.get("dnnf_count_recomputed"),
        "dnnf_count_match": result.get("dnnf_count_match"),
        "dnnf_nodes": result.get("dnnf_nodes"),
        "dnnf_edges": result.get("dnnf_edges"),
        "dnnf_bound_7k3nm": result.get("dnnf_bound_7k3nm"),
        "exit_code": result.get("exit_code"),
        "command": result.get("command"),
        "error_message": result.get("error_message"),
        "stderr_tail": stderr_tail,
    }
    # Champs query_* uniquement renseignes quand runner == "dp_query"
    # (passe C). Sur les passes A et B, les colonnes restent vides.
    for key in (
        "query_co_ms", "query_va_ms", "query_ct_ms", "query_me_ms",
        "query_ce_ms", "query_im_ms", "query_enum_first_ms", "query_enum_all_ms",
        "query_enum_count", "query_co_result", "query_va_result",
        "query_ce_result", "query_im_result", "query_ct_count",
        "query_repetitions", "query_enum_all_skipped",
    ):
        row[key] = result.get(key)
    return row


def _row_for_z3(result: dict[str, Any], inst_by_id: dict[str, InstanceConfig]) -> dict[str, Any]:
    inst = inst_by_id.get(result.get("instance_id", ""))
    family = inst.family if inst else ""
    return {
        "timestamp_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "instance_id": result.get("instance_id"),
        "family": family,
        "z3_kind": result.get("z3_kind"),
        "z3_status": result.get("z3_status"),
        "z3_maxsat": result.get("z3_maxsat"),
        "z3_sat_status": result.get("z3_sat_status"),
        "z3_solve_ms": result.get("z3_solve_ms"),
        "z3_total_ms": result.get("z3_total_ms"),
        "z3_conflicts": result.get("z3_conflicts"),
        "z3_decisions": result.get("z3_decisions"),
        "z3_restarts": result.get("z3_restarts"),
        "z3_propagations": result.get("z3_propagations"),
        "z3_time_internal_s": result.get("z3_time_internal_s"),
        "z3_n_vars": result.get("z3_n_vars"),
        "z3_n_clauses": result.get("z3_n_clauses"),
        "z3_stats_keys_available": result.get("z3_stats_keys_available"),
        "z3_error": result.get("z3_error"),
        "z3_short_circuit_empty_clause": result.get("z3_short_circuit_empty_clause", False),
    }


def _append_failure_log(failures_log: Path, result: dict[str, Any]) -> None:
    ts = dt.datetime.now(dt.timezone.utc).isoformat()
    msg = (f"[{ts}] FAIL {result.get('instance_id')} mode={result.get('mode')} "
           f"seed={result.get('seed')} status={result.get('status')} "
           f"err={result.get('error_message')}\n"
           f"  cmd: {result.get('command')}\n")
    try:
        with failures_log.open("a", encoding="utf-8") as fh:
            fh.write(msg)
    except Exception as e:
        logger.warning("failures.log: %s", e)


def run_passe_a(
    instances: list[InstanceConfig],
    cfg: BenchmarkConfig,
    repo_root: Path,
    output_dir: Path,
    state: HeartbeatState,
    shutdown_event: threading.Event,
    skip_z3: bool,
    inst_by_id: dict[str, InstanceConfig],
) -> tuple[int, int]:
    """Lance la passe A (parallele 64 jobs).

    Returns: (n_done, n_failed).
    """
    structure_csv = CsvAppender(
        output_dir / "structure.csv", STRUCTURE_CSV_COLS,
        flush_each_row=cfg.logging_cfg.get("csv_flush_each_row", True),
    )
    z3_csv = CsvAppender(
        output_dir / "z3.csv", Z3_CSV_COLS,
        flush_each_row=cfg.logging_cfg.get("csv_flush_each_row", True),
    )
    failures_log = output_dir / "failures.log"

    # Construction des taches.
    dp_tasks = _build_passe_a_dp_tasks(instances, cfg, repo_root)
    z3_tasks = (_build_passe_a_z3_tasks(instances, cfg, repo_root)
                if (cfg.passe_a.get("z3_in_passe_a", True) and not skip_z3) else [])

    # Resume : skip taches deja faites.
    done_dp = _existing_keys_structure(output_dir / "structure.csv")
    done_z3 = _existing_keys_z3(output_dir / "z3.csv")
    dp_to_run = [t for t in dp_tasks
                 if (t.instance_id, t.mode, t.seed, "dp") not in done_dp]
    z3_to_run = [t for t in z3_tasks if (t.instance_id, t.kind) not in done_z3]
    skipped = (len(dp_tasks) - len(dp_to_run)) + (len(z3_tasks) - len(z3_to_run))
    total = len(dp_to_run) + len(z3_to_run)

    logger.info("Passe A: %d taches DP + %d taches Z3 (skip %d deja faites)",
                len(dp_to_run), len(z3_to_run), skipped)

    state.update(phase="passe_a", done=0, failed=0, total=total)

    if total == 0:
        return 0, 0

    n_failed = 0
    n_done = 0
    max_workers = int(cfg.machine.get("passe_a_jobs", 64))

    with ProcessPoolExecutor(max_workers=max_workers) as exe:
        futures: dict[Any, str] = {}
        for t in dp_to_run:
            futures[exe.submit(_worker_run_dp, t)] = "dp"
        for t in z3_to_run:
            futures[exe.submit(_worker_run_z3, t)] = "z3"

        try:
            for fut in as_completed(futures):
                if shutdown_event.is_set():
                    logger.warning("Shutdown demande, on annule les futures non lancees")
                    for f in list(futures.keys()):
                        if not f.running() and not f.done():
                            f.cancel()
                    break
                kind = futures[fut]
                try:
                    result = fut.result()
                except Exception as exc:
                    logger.exception("Worker exception: %s", exc)
                    n_failed += 1
                    n_done += 1
                    state.update(done=n_done, failed=n_failed)
                    continue

                if kind == "dp":
                    structure_csv.append(_row_for_dp(result, inst_by_id))
                    if result.get("status") not in ("ok",):
                        n_failed += 1
                        _append_failure_log(failures_log, result)
                else:
                    z3_csv.append(_row_for_z3(result, inst_by_id))
                    z3_status = result.get("z3_status")
                    if z3_status not in ("sat", "unsat"):
                        n_failed += 1

                n_done += 1
                state.update(done=n_done, failed=n_failed)
        finally:
            # ProcessPoolExecutor s'arrete proprement avec wait=True via with.
            pass

    return n_done, n_failed


# ---------------------------------------------------------------------------
# Passe B : timings purs sur 4 coeurs CCDs distincts, repetitions
# ---------------------------------------------------------------------------

def _build_passe_b_dp_tasks(
    instances: list[InstanceConfig],
    cfg: BenchmarkConfig,
    repo_root: Path,
    structure_csv_path: Path,
) -> list[DpTask]:
    """Construit les taches Passe B en filtrant celles qui ont echoue en Passe A."""
    out: list[DpTask] = []
    mem_cap = int(cfg.machine.get("memory_cap_gib_per_proc", 50)) * (1024 ** 3)
    nice = int(cfg.machine.get("nice_level", 19))
    binary = cfg.dp.get("binary", "src/sat_solver")
    cwd = cfg.dp.get("cwd", "src")
    modes = list(cfg.dp.get("modes_arbre", ["greedy", "linear"]))
    cpus = list(cfg.machine.get("passe_b_taskset_cpus", [0, 8, 16, 24]))
    repetitions = int(cfg.passe_b.get("repetitions", 3))
    skip_failed = bool(cfg.passe_b.get("skip_if_passe_a_failed", True))

    # Lecture du status passe A pour filtrage.
    passe_a_ok: set[tuple[str, str, int]] = set()
    if structure_csv_path.exists():
        try:
            with structure_csv_path.open("r", newline="", encoding="utf-8") as fh:
                for row in csv.DictReader(fh):
                    if row.get("runner") == "dp" and row.get("status") == "ok":
                        try:
                            seed = int(row.get("seed") or 0)
                        except ValueError:
                            seed = 0
                        passe_a_ok.add((row["instance_id"], row["mode"], seed))
        except Exception as e:
            logger.warning("Lecture structure.csv impossible: %s", e)

    cpu_idx = 0
    for inst in instances:
        if not inst.enabled:
            continue
        timeout = _timeout_for(inst, cfg.passe_b)
        for mode in modes:
            if skip_failed and (inst.id, mode, 0) not in passe_a_ok:
                continue
            for _rep in range(repetitions):
                out.append(DpTask(
                    instance_id=inst.id, instance_path=inst.path, mode=mode, seed=0,
                    timeout_s=timeout, memory_cap_bytes=mem_cap,
                    binary_path=binary, cwd=cwd, nice_level=nice,
                    taskset_cpu=int(cpus[cpu_idx % len(cpus)]) if cpus else None,
                    repo_root=str(repo_root),
                ))
                cpu_idx += 1

    # random_sample en passe B aussi (modes random pour les 5 instances).
    if cfg.random_sample.get("enabled", True):
        sample_ids = set(cfg.random_sample.get("instances", []))
        seeds = list(cfg.random_sample.get("seeds", [0, 1, 2]))
        for inst in instances:
            if inst.id not in sample_ids or not inst.enabled:
                continue
            timeout = _timeout_for(inst, cfg.passe_b)
            for seed in seeds:
                if skip_failed and (inst.id, "random", int(seed)) not in passe_a_ok:
                    continue
                for _rep in range(repetitions):
                    out.append(DpTask(
                        instance_id=inst.id, instance_path=inst.path,
                        mode="random", seed=int(seed),
                        timeout_s=timeout, memory_cap_bytes=mem_cap,
                        binary_path=binary, cwd=cwd, nice_level=nice,
                        taskset_cpu=int(cpus[cpu_idx % len(cpus)]) if cpus else None,
                        repo_root=str(repo_root),
                    ))
                    cpu_idx += 1
    return out


def _aggregate_repetitions(
    rows: list[dict[str, Any]],
    instance_id: str,
    mode: str,
    seed: int,
    runner: str,
    family: str,
) -> dict[str, Any]:
    """Calcule la mediane / min / max / stddev pour un (instance, mode, seed)."""
    ok_rows = [r for r in rows if r.get("status") == "ok"]
    n_failures = sum(1 for r in rows if r.get("status") != "ok")

    def _ms(r: dict[str, Any], k: str) -> float | None:
        try:
            v = r.get(k)
            return float(v) if v is not None else None
        except (TypeError, ValueError):
            return None

    times_total = [_ms(r, "time_total_ms") for r in ok_rows]
    times_total = [v for v in times_total if v is not None]

    def _med(rows_: list[dict[str, Any]], key: str) -> float | None:
        vs = [_ms(r, key) for r in rows_]
        vs = [v for v in vs if v is not None]
        return median(vs) if vs else None

    base = {
        "timestamp_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "instance_id": instance_id,
        "family": family,
        "mode": mode,
        "seed": seed,
        "runner": runner,
        "repetitions": len(rows),
        "n_failures": n_failures,
    }

    if not times_total:
        base.update({
            "time_total_ms_median": None,
            "time_total_ms_min": None,
            "time_total_ms_max": None,
            "time_total_ms_stddev": None,
            "time_phase0_ms_median": None,
            "time_phase1_ms_median": None,
            "time_phase2_ms_median": None,
            "time_phase3_ms_median": None,
            "status": "all_failed",
        })
        return base

    base.update({
        "time_total_ms_median": median(times_total),
        "time_total_ms_min": min(times_total),
        "time_total_ms_max": max(times_total),
        "time_total_ms_stddev": pstdev(times_total) if len(times_total) > 1 else 0.0,
        "time_phase0_ms_median": _med(ok_rows, "time_phase0_ms"),
        "time_phase1_ms_median": _med(ok_rows, "time_phase1_ms"),
        "time_phase2_ms_median": _med(ok_rows, "time_phase2_ms"),
        "time_phase3_ms_median": _med(ok_rows, "time_phase3_ms"),
        "status": "ok" if n_failures == 0 else "partial",
    })
    return base


def run_passe_b(
    instances: list[InstanceConfig],
    cfg: BenchmarkConfig,
    repo_root: Path,
    output_dir: Path,
    state: HeartbeatState,
    shutdown_event: threading.Event,
    skip_z3: bool,
    inst_by_id: dict[str, InstanceConfig],
) -> tuple[int, int]:
    timings_csv = CsvAppender(
        output_dir / "timings.csv", TIMINGS_CSV_COLS,
        flush_each_row=cfg.logging_cfg.get("csv_flush_each_row", True),
    )
    failures_log = output_dir / "failures.log"
    cpus = list(cfg.machine.get("passe_b_taskset_cpus", [0, 8, 16, 24]))
    n_workers = max(1, len(cpus))

    dp_tasks = _build_passe_b_dp_tasks(instances, cfg, repo_root, output_dir / "structure.csv")
    z3_tasks: list[Z3Task] = []
    if cfg.passe_b.get("z3_in_passe_b", True) and not skip_z3 and cfg.z3.get("enabled", True):
        timeout = int(cfg.z3.get("timeout_s", 600))
        for inst in instances:
            if inst.enabled:
                z3_tasks.append(Z3Task(
                    instance_id=inst.id, instance_path=inst.path, kind="maxsat",
                    timeout_s=timeout, repo_root=str(repo_root),
                ))
                z3_tasks.append(Z3Task(
                    instance_id=inst.id, instance_path=inst.path, kind="sat",
                    timeout_s=timeout, repo_root=str(repo_root),
                ))

    total = len(dp_tasks) + len(z3_tasks)
    state.update(phase="passe_b", done=0, failed=0, total=total)
    logger.info("Passe B: %d taches DP + %d taches Z3 (4 workers, taskset CPUs %s)",
                len(dp_tasks), len(z3_tasks), cpus)

    if total == 0:
        return 0, 0

    # Buffer des resultats DP par cle (instance, mode, seed) -> liste de runs.
    # Aggregat une fois toutes les repetitions recues.
    dp_buffer: dict[tuple[str, str, int], list[dict[str, Any]]] = {}
    expected_reps_dp: dict[tuple[str, str, int], int] = {}
    for t in dp_tasks:
        key = (t.instance_id, t.mode, t.seed)
        expected_reps_dp[key] = expected_reps_dp.get(key, 0) + 1

    # Buffer Z3.
    z3_buffer: dict[tuple[str, str], dict[str, Any]] = {}

    n_done = 0
    n_failed = 0

    with ProcessPoolExecutor(max_workers=n_workers) as exe:
        futures: dict[Any, tuple[str, Any]] = {}
        for t in dp_tasks:
            futures[exe.submit(_worker_run_dp, t)] = ("dp", t)
        for t in z3_tasks:
            futures[exe.submit(_worker_run_z3, t)] = ("z3", t)

        for fut in as_completed(futures):
            if shutdown_event.is_set():
                logger.warning("Shutdown demande, annule futures non commencees")
                for f in list(futures.keys()):
                    if not f.running() and not f.done():
                        f.cancel()
                break
            kind, task = futures[fut]
            try:
                result = fut.result()
            except Exception as exc:
                logger.exception("Worker passe B: %s", exc)
                n_failed += 1
                n_done += 1
                state.update(done=n_done, failed=n_failed)
                continue

            if kind == "dp":
                key = (task.instance_id, task.mode, task.seed)
                dp_buffer.setdefault(key, []).append(result)
                if result.get("status") != "ok":
                    n_failed += 1
                    _append_failure_log(failures_log, result)
                # Si toutes les repetitions sont rentrees, on aggrege.
                if len(dp_buffer[key]) >= expected_reps_dp[key]:
                    inst = inst_by_id.get(task.instance_id)
                    fam = inst.family if inst else ""
                    agg = _aggregate_repetitions(
                        dp_buffer[key], task.instance_id, task.mode,
                        task.seed, "dp", fam,
                    )
                    timings_csv.append(agg)
                    del dp_buffer[key]
            else:
                # Z3 : 1 seule run par (instance, kind) en passe B.
                key2 = (task.instance_id, task.kind)
                z3_buffer[key2] = result
                # Convertir en ligne timings (single rep).
                inst = inst_by_id.get(task.instance_id)
                fam = inst.family if inst else ""
                t_solve = result.get("z3_solve_ms")
                row: dict[str, Any] = {
                    "timestamp_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
                    "instance_id": task.instance_id, "family": fam,
                    "mode": "n/a", "seed": 0,
                    "runner": f"z3_{task.kind}",
                    "repetitions": 1,
                    "time_total_ms_median": t_solve,
                    "time_total_ms_min": t_solve,
                    "time_total_ms_max": t_solve,
                    "time_total_ms_stddev": 0.0,
                    "time_phase0_ms_median": None,
                    "time_phase1_ms_median": None,
                    "time_phase2_ms_median": None,
                    "time_phase3_ms_median": None,
                    "n_failures": 0 if result.get("z3_status") in ("sat", "unsat") else 1,
                    "status": "ok" if result.get("z3_status") in ("sat", "unsat") else "all_failed",
                }
                timings_csv.append(row)
                if result.get("z3_status") not in ("sat", "unsat"):
                    n_failed += 1

            n_done += 1
            state.update(done=n_done, failed=n_failed)

    # On flush les buffers DP partiels (taches qui n'ont pas atteint le quota).
    for key, runs in dp_buffer.items():
        instance_id, mode, seed = key
        inst = inst_by_id.get(instance_id)
        fam = inst.family if inst else ""
        agg = _aggregate_repetitions(runs, instance_id, mode, seed, "dp", fam)
        agg["status"] = "partial" if any(r.get("status") == "ok" for r in runs) else "all_failed"
        timings_csv.append(agg)

    return n_done, n_failed


# ===========================================================================
# Passe C : bench des requetes sur DAG compile (in-process)
# ===========================================================================

def _build_passe_c_dp_tasks(
    instances: list[InstanceConfig],
    cfg: BenchmarkConfig,
    repo_root: Path,
    structure_csv_path: Path,
) -> list[DpTask]:
    """Construit les taches de la passe C.

    Filtre les instances : seules celles dont la passe A est OK avec mode
    greedy ET dont dnnf_nodes < passe_c.dnnf_nodes_max (au-dela, les requetes
    prennent trop de temps et le bench derive). 1 appel par instance (pas de
    repetitions multiples cote orchestrateur : le solveur fait deja ses 5
    repetitions internes via --json-with-queries).
    """
    out: list[DpTask] = []
    passe_c_cfg = getattr(cfg, "passe_c", {}) or {}
    if not passe_c_cfg.get("enabled", True):
        return out

    dnnf_nodes_max = int(passe_c_cfg.get("dnnf_nodes_max", 1_000_000))
    timeout = int(passe_c_cfg.get("default_timeout_s", 600))
    mem_cap = int(cfg.machine.get("memory_cap_gib_per_proc", 50)) * (1024 ** 3)
    nice = int(cfg.machine.get("nice_level", 19))
    binary = cfg.dp.get("binary", "src/sat_solver")
    cwd = cfg.dp.get("cwd", "src")
    cpus = list(cfg.machine.get("passe_b_taskset_cpus", [0, 8, 16, 24]))

    # Lecture de structure.csv pour identifier les instances OK + < seuil.
    eligible: dict[str, int] = {}  # instance_id -> dnnf_nodes
    if structure_csv_path.exists():
        try:
            with structure_csv_path.open("r", newline="", encoding="utf-8") as fh:
                for row in csv.DictReader(fh):
                    if row.get("runner") != "dp":
                        continue
                    if row.get("status") != "ok":
                        continue
                    if row.get("mode") != "greedy":
                        continue
                    try:
                        nn = int(float(row.get("dnnf_nodes") or 0))
                    except ValueError:
                        continue
                    if 0 < nn < dnnf_nodes_max:
                        eligible[row["instance_id"]] = nn
        except Exception as e:
            logger.warning("Lecture structure.csv impossible (passe C) : %s", e)

    cpu_idx = 0
    for inst in instances:
        if not inst.enabled:
            continue
        if inst.id not in eligible:
            continue
        out.append(DpTask(
            instance_id=inst.id, instance_path=inst.path, mode="greedy", seed=0,
            timeout_s=timeout, memory_cap_bytes=mem_cap,
            binary_path=binary, cwd=cwd, nice_level=nice,
            taskset_cpu=int(cpus[cpu_idx % len(cpus)]) if cpus else None,
            repo_root=str(repo_root),
            with_queries=True,
        ))
        cpu_idx += 1
    return out


def run_passe_c(
    instances: list[InstanceConfig],
    cfg: BenchmarkConfig,
    repo_root: Path,
    output_dir: Path,
    state: HeartbeatState,
    shutdown_event: threading.Event,
    inst_by_id: dict[str, InstanceConfig],
) -> tuple[int, int]:
    """Lance la passe C (4 workers sur 4 CCDs distincts, in-process).

    Pour chaque instance eligible (OK passe A en mode greedy + dnnf_nodes
    sous le seuil), un seul appel solveur en --json-with-queries qui
    chronometre les 7 requetes en interne. Les lignes resultantes sont
    appendues a structure.csv avec runner='dp_query' (rétro-compat : les
    consommateurs filtrent sur ce label pour distinguer des passes A/B).
    """
    structure_csv = CsvAppender(
        output_dir / "structure.csv", STRUCTURE_CSV_COLS,
        flush_each_row=cfg.logging_cfg.get("csv_flush_each_row", True),
    )
    failures_log = output_dir / "failures.log"
    cpus = list(cfg.machine.get("passe_b_taskset_cpus", [0, 8, 16, 24]))
    n_workers = max(1, len(cpus))

    dp_tasks = _build_passe_c_dp_tasks(
        instances, cfg, repo_root, output_dir / "structure.csv",
    )

    # Skip taches deja faites (resume).
    done = _existing_keys_structure(output_dir / "structure.csv")
    dp_to_run = [t for t in dp_tasks
                 if (t.instance_id, t.mode, t.seed, "dp_query") not in done]
    skipped = len(dp_tasks) - len(dp_to_run)
    total = len(dp_to_run)

    state.update(phase="passe_c", done=0, failed=0, total=total)
    logger.info("Passe C: %d taches DP-query (skip %d deja faites, %d workers)",
                total, skipped, n_workers)

    if total == 0:
        return 0, 0

    n_done = 0
    n_failed = 0

    with ProcessPoolExecutor(max_workers=n_workers) as exe:
        futures: dict[Any, str] = {}
        for t in dp_to_run:
            futures[exe.submit(_worker_run_dp, t)] = "dp_query"

        for fut in as_completed(futures):
            if shutdown_event.is_set():
                logger.warning("Shutdown demande, annule futures non commencees")
                for f in list(futures.keys()):
                    if not f.running() and not f.done():
                        f.cancel()
                break
            try:
                result = fut.result()
            except Exception as exc:
                logger.exception("Worker passe C exception: %s", exc)
                n_failed += 1
                n_done += 1
                state.update(done=n_done, failed=n_failed)
                continue

            structure_csv.append(_row_for_dp(result, inst_by_id, runner_label="dp_query"))
            if result.get("status") not in ("ok",):
                n_failed += 1
                _append_failure_log(failures_log, result)
            n_done += 1
            state.update(done=n_done, failed=n_failed)

    return n_done, n_failed


# ===========================================================================
# Plots et SUMMARY
# ===========================================================================

def run_plots(output_dir: Path, cfg: BenchmarkConfig) -> None:
    try:
        from plots import make_all_plots  # noqa: WPS433
    except Exception as e:
        logger.warning("Impossible d'importer plots.make_all_plots: %s", e)
        return
    try:
        make_all_plots.run(input_dir=output_dir, plots_config=cfg.plots)
    except Exception as e:
        logger.exception("make_all_plots a echoue: %s", e)


def run_summary(output_dir: Path, cfg: BenchmarkConfig, repo_root: Path) -> None:
    try:
        from plots import make_summary  # noqa: WPS433
    except Exception as e:
        logger.warning("Impossible d'importer plots.make_summary: %s", e)
        return
    try:
        make_summary.run(
            input_dir=output_dir, repo_root=repo_root,
            cfg=cfg, orchestrator_version=ORCHESTRATOR_VERSION,
        )
    except Exception as e:
        logger.exception("make_summary a echoue: %s", e)


# ===========================================================================
# CLI principale
# ===========================================================================

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Orchestrateur du harness benchmarks")
    p.add_argument("--config", default="config", help="dossier des configs")
    p.add_argument("--output", default=None, help="dossier de sortie (defaut: results/<UTC>)")
    p.add_argument("--resume", default=None, help="reprend un run existant")
    p.add_argument("--only-passe-a", action="store_true")
    p.add_argument("--only-passe-b", action="store_true")
    p.add_argument("--only-passe-c", action="store_true")
    p.add_argument("--only-plots", action="store_true")
    p.add_argument("--only-summary", action="store_true")
    p.add_argument("--only-invariants", action="store_true")
    p.add_argument("--families", default=None, help="filtre familles (CSV)")
    p.add_argument("--instances", default=None, help="filtre ids instances (CSV)")
    p.add_argument("--modes", default=None, help="override modes (CSV)")
    p.add_argument("--skip-z3", action="store_true")
    p.add_argument("--dry-run", action="store_true")
    p.add_argument("-v", "--verbose", action="store_true")
    p.add_argument("--version", action="store_true")
    return p.parse_args()


def _filter_instances(
    instances: list[InstanceConfig],
    families: str | None,
    ids: str | None,
) -> list[InstanceConfig]:
    out = list(instances)
    if families:
        wanted = {x.strip() for x in families.split(",") if x.strip()}
        out = [i for i in out if i.family in wanted]
    if ids:
        wanted = {x.strip() for x in ids.split(",") if x.strip()}
        out = [i for i in out if i.id in wanted]
    return out


def _setup_logging(verbose: bool) -> None:
    level = logging.DEBUG if verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )


def _ensure_disk_space(output_dir: Path, min_gib: float = 5.0) -> None:
    try:
        usage = shutil.disk_usage(output_dir)
        free_gib = usage.free / (1024 ** 3)
        if free_gib < min_gib:
            raise SystemExit(
                f"Disque trop plein : {free_gib:.1f} GiB libre < {min_gib:.1f} GiB requis"
            )
    except FileNotFoundError:
        # Le dossier n'existe pas encore ; on cree, on verra plus tard.
        pass


def main() -> int:
    args = parse_args()
    _setup_logging(args.verbose)

    if args.version:
        print(f"orchestrator {ORCHESTRATOR_VERSION}")
        return 0

    repo_root = (_HERE.parent).resolve()
    config_dir = (_HERE / args.config).resolve() if not Path(args.config).is_absolute() \
        else Path(args.config)

    try:
        instances = _load_instances(config_dir / "instances.yaml")
        cfg = _load_benchmark_config(config_dir / "benchmark.yaml")
    except Exception as e:
        logger.error("Erreur de chargement des configs : %s", e)
        return 1

    # Validation des paths : avertir mais ne pas exit (un path peut manquer
    # temporairement si on travaille sur un sous-set).
    missing = _validate_paths(instances, repo_root)
    if missing:
        logger.warning("Paths manquants (%d):\n  %s", len(missing), "\n  ".join(missing[:10]))

    # Filtres CLI.
    instances = _filter_instances(instances, args.families, args.instances)
    if args.modes:
        cfg.dp = dict(cfg.dp)
        cfg.dp["modes_arbre"] = [m.strip() for m in args.modes.split(",") if m.strip()]

    # Output dir.
    if args.resume:
        output_dir = Path(args.resume).resolve()
    elif args.output:
        output_dir = Path(args.output).resolve()
    else:
        ts = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%d_%H%M%S")
        output_dir = (_HERE / "results" / ts).resolve()

    _ensure_disk_space(output_dir.parent if not output_dir.exists() else output_dir)

    if args.dry_run:
        # Construction des taches sans execution.
        dp_a = _build_passe_a_dp_tasks(instances, cfg, repo_root)
        z3_a = _build_passe_a_z3_tasks(instances, cfg, repo_root)
        print(f"DRY RUN - output_dir={output_dir}")
        print(f"  Instances : {len(instances)} (enabled: "
              f"{sum(1 for i in instances if i.enabled)})")
        print(f"  Passe A DP tasks : {len(dp_a)}")
        print(f"  Passe A Z3 tasks : {len(z3_a)}")
        return 0

    _setup_output_dir(output_dir, repo_root)
    inst_by_id = {i.id: i for i in instances}

    notifier = Notifier(cfg.notifications)

    state = HeartbeatState()
    shutdown_event = threading.Event()
    stop_heartbeat = threading.Event()

    def _signal_handler(signum: int, _frame: Any) -> None:
        logger.warning("Signal %d recu, arret propre", signum)
        shutdown_event.set()

    signal.signal(signal.SIGINT, _signal_handler)
    signal.signal(signal.SIGTERM, _signal_handler)

    progress_log = output_dir / "progress.log"
    interval_s = int(cfg.logging_cfg.get("heartbeat_interval_s", 300))
    notify_interval_s = int(cfg.notifications.get("heartbeat_interval_min", 30)) * 60
    hb_thread = threading.Thread(
        target=_heartbeat_loop,
        args=(progress_log, state, interval_s, stop_heartbeat, notifier, notify_interval_s),
        daemon=True,
    )
    hb_thread.start()

    # Estimation grossiere de l'ETA initiale.
    enabled_count = sum(1 for i in instances if i.enabled)
    eta_initial = _human_duration(enabled_count * cfg.passe_a.get("default_timeout_s", 1800) / 64)
    notifier.started(enabled_count, eta_initial)

    n_total_done = 0
    n_total_failed = 0

    try:
        # ---------------- PASSE A ----------------
        if not (args.only_passe_b or args.only_passe_c or args.only_plots
                or args.only_summary or args.only_invariants):
            if cfg.passe_a.get("enabled", True):
                d, f = run_passe_a(
                    instances, cfg, repo_root, output_dir, state,
                    shutdown_event, args.skip_z3, inst_by_id,
                )
                n_total_done += d
                n_total_failed += f

        # ---------------- INVARIANTS ----------------
        if not (args.only_passe_a or args.only_passe_b or args.only_passe_c
                or args.only_plots or args.only_summary):
            try:
                state.update(phase="invariants")
                inv_summary = invariants_mod.check_all_invariants(
                    structure_csv=output_dir / "structure.csv",
                    z3_csv=output_dir / "z3.csv" if cfg.z3.get("enabled", True) else None,
                    instances={i.id: i for i in instances},
                    output_csv=output_dir / "invariants.csv",
                    failures_log=output_dir / "failures.log",
                    notifier=notifier,
                )
                logger.info("Invariants : %s", inv_summary)
            except Exception as e:
                logger.exception("Invariants ont echoue : %s", e)

        # ---------------- PASSE B ----------------
        if not (args.only_passe_a or args.only_passe_c or args.only_plots
                or args.only_summary or args.only_invariants):
            if cfg.passe_b.get("enabled", True) and not shutdown_event.is_set():
                d, f = run_passe_b(
                    instances, cfg, repo_root, output_dir, state,
                    shutdown_event, args.skip_z3, inst_by_id,
                )
                n_total_done += d
                n_total_failed += f

        # ---------------- PASSE C (bench des requetes sur DAG compile) ----------------
        if not (args.only_passe_a or args.only_passe_b or args.only_plots
                or args.only_summary or args.only_invariants):
            passe_c_cfg = getattr(cfg, "passe_c", {}) or {}
            if passe_c_cfg.get("enabled", True) and not shutdown_event.is_set():
                d, f = run_passe_c(
                    instances, cfg, repo_root, output_dir, state,
                    shutdown_event, inst_by_id,
                )
                n_total_done += d
                n_total_failed += f

        # ---------------- PLOTS ----------------
        if not (args.only_passe_a or args.only_passe_b or args.only_passe_c
                or args.only_summary or args.only_invariants):
            if cfg.plots.get("enabled", True) and not shutdown_event.is_set():
                state.update(phase="plots")
                run_plots(output_dir, cfg)

        # ---------------- SUMMARY ----------------
        if not (args.only_passe_a or args.only_passe_b or args.only_passe_c
                or args.only_plots or args.only_invariants):
            if not shutdown_event.is_set():
                state.update(phase="summary")
                run_summary(output_dir, cfg, repo_root)

    except KeyboardInterrupt:
        logger.warning("KeyboardInterrupt")
        shutdown_event.set()
    except Exception as e:
        logger.exception("Erreur fatale : %s", e)
        notifier.error(str(e)[:500] + "\n" + traceback.format_exc()[-500:])
        return 2
    finally:
        stop_heartbeat.set()
        hb_thread.join(timeout=2)

    # Notification finale (flush implicite des FAIL hard bufferises).
    elapsed = time.time() - state.start_time
    notifier.finished(
        summary_path=str(output_dir / "SUMMARY.md"),
        n_ok=n_total_done - n_total_failed,
        n_fail=n_total_failed,
        duration_str=_human_duration(elapsed),
    )
    # Arret propre du thread de flush du notifier.
    notifier.stop()

    if shutdown_event.is_set():
        return 130
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
