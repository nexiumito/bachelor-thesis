"""run_dp.py — wrapper sat_solver --json.

Lance le binaire C ``./sat_solver <instance> <mode> --json`` dans un sous-process,
applique des limites memoire (RLIMIT_AS) et un timeout, parse la ligne JSON et
retourne un dict normalise.

Aucune ecriture CSV : c'est l'orchestrateur qui consolide.
"""

from __future__ import annotations

import json
import os
import resource
import shlex
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any


# ----------------------------------------------------------------------------
# Mini-dataclass pour decouper run_dp() du config_loader. Le orchestrator peut
# passer une instance plus riche -- on accede uniquement a id et path.
# ----------------------------------------------------------------------------
@dataclass
class _MinimalInstance:
    id: str
    path: str


def _classify_crash(exit_code: int | None, stderr: str | None) -> str:
    """Classifie un exit code anormal en message lisible.

    sat_solver n'a pas de cleanup particulier ; un exit code != 0 sans ligne
    JSON signifie crash avant la fin (segfault, OOM kernel, signal, etc.).
    """
    if exit_code in (139, -11):
        return "segfault"
    if exit_code in (137, -9):
        return "killed (probably OOM or signal)"
    if stderr and "MemoryError" in stderr:
        return "out_of_memory"
    if exit_code is None:
        return "unknown_crash"
    return f"exit_code={exit_code}"


def run_dp(
    instance: Any,
    mode: str,
    seed: int,
    timeout_s: int,
    memory_cap_bytes: int,
    binary_path: str = "src/sat_solver",
    cwd: str = "src",
    nice_level: int = 19,
    taskset_cpu: int | None = None,
    repo_root: str = ".",
) -> dict[str, Any]:
    """Lance sat_solver --json et parse la sortie.

    Args:
        instance: objet avec attributs ``id`` et ``path`` (relatif au cwd ``src/``).
        mode: ``greedy`` | ``linear`` | ``random``.
        seed: passe via env ``RANDOM_SEED`` ; 0 sauf mode random.
        timeout_s: timeout total du sous-process en secondes.
        memory_cap_bytes: borne RLIMIT_AS appliquee dans preexec_fn.
        binary_path: chemin du binaire relatif au repo root.
        cwd: dossier de travail du sous-process (relatif au repo root).
        nice_level: priorite via ``nice -n <level>``.
        taskset_cpu: si non None, pinning CPU via ``taskset -c <cpu>``.
        repo_root: racine du repo, utilise pour construire ``cwd_abs``.

    Returns:
        Un dict contenant les cles du JSON natif + champs ajoutes :
            instance_id, mode, seed, status, exit_code, elapsed_wallclock_ms,
            stderr (4 derniers KB), command, error_message (si applicable).

        ``status`` est dans :
            ``ok`` | ``timeout`` | ``crash`` | ``alloc_fail`` |
            ``parse_error`` | ``json_parse_error`` | ``schema_unknown``.
    """
    # Construction de la commande shell : taskset -c X nice -n N ./sat_solver ...
    cmd_parts: list[str] = []
    if taskset_cpu is not None:
        cmd_parts += ["taskset", "-c", str(taskset_cpu)]
    cmd_parts += ["nice", "-n", str(nice_level)]
    # On lance via le nom du binaire relatif a cwd (./sat_solver).
    cmd_parts += [f"./{Path(binary_path).name}", instance.path, mode, "--json"]
    cmd_str = " ".join(shlex.quote(p) for p in cmd_parts)

    # Environnement : seed pour le mode random, hereditairement transmis.
    env = os.environ.copy()
    env["RANDOM_SEED"] = str(seed)

    # preexec_fn : memory cap via RLIMIT_AS. POSIX uniquement (OK Linux/macOS).
    def _set_limits() -> None:
        try:
            resource.setrlimit(
                resource.RLIMIT_AS,
                (memory_cap_bytes, memory_cap_bytes),
            )
        except (ValueError, resource.error):
            # Cap non applicable (deja plus restrictif, ou non supporte) : on
            # continue sans bloquer. Le timeout reste la barriere principale.
            pass

    cwd_abs = (Path(repo_root) / cwd).resolve()

    base_result: dict[str, Any] = {
        "instance_id": instance.id,
        "mode": mode,
        "seed": seed,
        "command": cmd_str,
    }

    t0 = time.perf_counter()
    try:
        proc = subprocess.run(
            cmd_parts,
            cwd=str(cwd_abs),
            env=env,
            capture_output=True,
            text=True,
            timeout=timeout_s,
            preexec_fn=_set_limits,
            check=False,
        )
    except subprocess.TimeoutExpired as e:
        elapsed = (time.perf_counter() - t0) * 1000
        stderr_bytes = e.stderr or b""
        if isinstance(stderr_bytes, str):
            stderr_str = stderr_bytes
        else:
            stderr_str = stderr_bytes.decode("utf-8", errors="replace")
        return {
            **base_result,
            "status": "timeout",
            "exit_code": None,
            "elapsed_wallclock_ms": elapsed,
            "stderr": stderr_str[-4096:],
            "error_message": f"timeout after {timeout_s}s",
        }
    except FileNotFoundError as e:
        # taskset / nice / sat_solver introuvable.
        elapsed = (time.perf_counter() - t0) * 1000
        return {
            **base_result,
            "status": "crash",
            "exit_code": None,
            "elapsed_wallclock_ms": elapsed,
            "stderr": "",
            "error_message": f"binaire introuvable: {e}",
        }

    elapsed = (time.perf_counter() - t0) * 1000
    stderr_text = (proc.stderr or "")[-4096:]

    # Parsing de la sortie : on tolere une eventuelle ligne ASCII parasite
    # avant la ligne JSON, en prenant la derniere ligne qui commence par '{'.
    stdout = (proc.stdout or "").strip()
    if not stdout:
        return {
            **base_result,
            "status": "crash",
            "exit_code": proc.returncode,
            "elapsed_wallclock_ms": elapsed,
            "stderr": stderr_text,
            "error_message": _classify_crash(proc.returncode, stderr_text),
        }

    json_lines = [line for line in stdout.split("\n") if line.startswith("{")]
    if not json_lines:
        return {
            **base_result,
            "status": "json_parse_error",
            "exit_code": proc.returncode,
            "elapsed_wallclock_ms": elapsed,
            "stderr": stderr_text,
            "stdout_excerpt": stdout[:500],
            "error_message": "aucune ligne JSON dans stdout",
        }

    try:
        data = json.loads(json_lines[-1])
    except json.JSONDecodeError as e:
        return {
            **base_result,
            "status": "json_parse_error",
            "exit_code": proc.returncode,
            "elapsed_wallclock_ms": elapsed,
            "stderr": stderr_text,
            "stdout_excerpt": stdout[:500],
            "error_message": f"json.loads: {e}",
        }

    # Refus des schemas inconnus : si le binaire C evolue et change le format
    # JSON, on echoue explicitement plutot que d'inventer des valeurs.
    schema_version = data.get("schema_version")
    if schema_version != 1:
        return {
            **base_result,
            "status": "schema_unknown",
            "exit_code": proc.returncode,
            "elapsed_wallclock_ms": elapsed,
            "stderr": stderr_text,
            "error_message": f"schema_version inconnu: {schema_version}",
        }

    # Le JSON natif contient deja toutes les cles techniques (status, file, etc.).
    # On le reprend tel quel et on enrichit avec les champs cote Python.
    enriched: dict[str, Any] = dict(data)
    enriched["instance_id"] = instance.id
    enriched["mode"] = mode
    enriched["seed"] = seed
    enriched["elapsed_wallclock_ms"] = elapsed
    enriched["exit_code"] = proc.returncode
    enriched["stderr"] = stderr_text
    enriched["command"] = cmd_str

    # Le binaire C a peut-etre renvoye un status d'erreur (parse_error,
    # alloc_fail, ...) -- on le respecte. Sinon defaut "ok".
    if "status" not in enriched or not enriched["status"]:
        enriched["status"] = "ok"

    return enriched


__all__ = ["run_dp", "_MinimalInstance"]
