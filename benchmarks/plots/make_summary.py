"""make_summary.py — generation de SUMMARY.md a partir des CSV d'un run.

Lit structure.csv / timings.csv / z3.csv / invariants.csv / env.txt /
git_info.txt et remplit le template Jinja2 ``summary_template.md.j2``.
"""

from __future__ import annotations

import csv
import datetime as dt
import logging
import platform
import re
import socket
from pathlib import Path
from typing import Any

import jinja2
import pandas as pd

logger = logging.getLogger(__name__)

_FAMILIES = ["type1", "type2", "type3", "random", "tseytin", "factorization", "misc"]


def _read_text(path: Path) -> str:
    if path.exists():
        try:
            return path.read_text()
        except Exception:
            return ""
    return ""


def _git_meta(repo_root: Path) -> dict[str, str]:
    """Extrait git_hash, git_branch, git_remote depuis git_info.txt si dispo."""
    info = _read_text(repo_root / "git_info.txt")  # fallback
    out = {"git_hash": "(inconnu)", "git_branch": "(inconnu)", "git_remote": "origin"}
    return out


def _parse_git_info(input_dir: Path) -> dict[str, str]:
    info = _read_text(input_dir / "git_info.txt")
    out = {"git_hash": "(inconnu)", "git_branch": "(inconnu)", "git_remote": "origin"}
    # Recherche des sections.
    sections = re.split(r"^## ", info, flags=re.MULTILINE)
    for sec in sections:
        if sec.startswith("git rev-parse HEAD"):
            lines = [l for l in sec.split("\n", 1)[1].strip().split("\n") if l.strip()]
            if lines:
                out["git_hash"] = lines[0].strip()[:12]
        elif sec.startswith("git rev-parse --abbrev-ref HEAD"):
            lines = [l for l in sec.split("\n", 1)[1].strip().split("\n") if l.strip()]
            if lines:
                out["git_branch"] = lines[0].strip()
    return out


def _machine_meta() -> dict[str, Any]:
    cpu_model = platform.processor() or platform.machine()
    n_cpus = 0
    try:
        import os
        n_cpus = os.cpu_count() or 0
    except Exception:
        pass
    ram_gib = 0
    try:
        import psutil  # type: ignore
        ram_gib = round(psutil.virtual_memory().total / (1024 ** 3), 1)
    except Exception:
        pass
    return {
        "hostname": socket.gethostname(),
        "cpu_model": cpu_model,
        "n_cpus": n_cpus,
        "ram_gib": ram_gib,
    }


def _families_overview(structure: pd.DataFrame, instances_meta: dict[str, Any]
                       ) -> tuple[list[dict[str, Any]], dict[str, int]]:
    """Calcule la table 'Vue globale' par famille."""
    rows: list[dict[str, Any]] = []
    totals = {"total": 0, "ok": 0, "timeout": 0, "crash": 0, "disabled": 0}

    # Disabled : depuis instances.yaml.
    disabled_by_family: dict[str, int] = {}
    for fam in _FAMILIES:
        disabled_by_family[fam] = 0
    for inst in instances_meta.get("instances_list", []):
        if not inst.get("enabled", True):
            disabled_by_family[inst["family"]] = disabled_by_family.get(inst["family"], 0) + 1

    if structure.empty:
        for fam in _FAMILIES:
            rows.append({"name": fam, "total": 0, "ok": 0, "timeout": 0, "crash": 0,
                          "disabled": disabled_by_family.get(fam, 0)})
            totals["disabled"] += disabled_by_family.get(fam, 0)
        return rows, totals

    df = structure[structure["runner"] == "dp"]
    grouped = df.groupby(["family", "status"]).size().unstack(fill_value=0)

    for fam in _FAMILIES:
        if fam in grouped.index:
            row = grouped.loc[fam]
            ok = int(row.get("ok", 0))
            timeout = int(row.get("timeout", 0))
            crash = int(row.get("crash", 0))
            total = int(row.sum())
        else:
            ok = timeout = crash = total = 0
        disabled = disabled_by_family.get(fam, 0)
        rows.append({"name": fam, "total": total, "ok": ok,
                      "timeout": timeout, "crash": crash, "disabled": disabled})
        totals["total"] += total
        totals["ok"] += ok
        totals["timeout"] += timeout
        totals["crash"] += crash
        totals["disabled"] += disabled
    return rows, totals


def _invariant_summary(invariants_csv: Path) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    """Resume invariants.csv : counts + liste des FAIL hard."""
    if not invariants_csv.exists():
        return [], []
    invs: dict[str, dict[str, int]] = {}
    fails: list[dict[str, Any]] = []
    try:
        with invariants_csv.open("r", newline="", encoding="utf-8") as fh:
            for row in csv.DictReader(fh):
                name = row["invariant"]
                invs.setdefault(name, {"ok": 0, "fail_hard": 0, "fail_soft": 0, "skipped": 0})
                status = row["status"]
                severity = row["severity"]
                if status == "OK":
                    invs[name]["ok"] += 1
                elif status == "FAIL":
                    if severity == "hard":
                        invs[name]["fail_hard"] += 1
                        fails.append({
                            "invariant": name,
                            "instance_id": row["instance_id"],
                            "mode": row["mode"],
                            "message": row["message"],
                        })
                    else:
                        invs[name]["fail_soft"] += 1
                elif status == "WARN":
                    invs[name]["fail_soft"] += 1
                else:
                    invs[name]["skipped"] += 1
    except Exception as e:
        logger.warning("Lecture invariants.csv: %s", e)
    table = [{"name": k, **v} for k, v in invs.items()]
    return table, fails


def _top_instances(structure: pd.DataFrame, timings: pd.DataFrame,
                   z3: pd.DataFrame, n: int = 10) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    """Top n plus lentes / plus rapides en DP greedy."""
    if timings.empty or structure.empty:
        return [], []
    tim = timings[(timings["runner"] == "dp") &
                  (timings["mode"] == "greedy") &
                  (timings["status"].isin(["ok", "partial"]))].copy()
    if tim.empty:
        return [], []
    struct = structure[(structure["runner"] == "dp") &
                       (structure["mode"] == "greedy") &
                       (structure["status"] == "ok")]
    # `family` est deja dans `tim` (orchestrateur) ; le re-inclure cote droit
    # creerait `family_x` / `family_y` et viderait la colonne dans le SUMMARY.
    merged = tim.merge(
        struct[["instance_id", "n_vars", "n_clauses", "ps_width"]],
        on="instance_id", how="left",
    )
    merged["time_total_ms_median"] = pd.to_numeric(
        merged["time_total_ms_median"], errors="coerce")
    merged = merged.dropna(subset=["time_total_ms_median"])

    # Z3 maxsat
    z3_sat = z3[(z3["z3_kind"] == "maxsat") & (z3["z3_status"] == "sat")] if not z3.empty else pd.DataFrame()
    if not z3_sat.empty:
        z3_sat = z3_sat.copy()
        z3_sat["z3_solve_ms"] = pd.to_numeric(z3_sat["z3_solve_ms"], errors="coerce")
        merged = merged.merge(z3_sat[["instance_id", "z3_solve_ms"]],
                               on="instance_id", how="left")
    else:
        merged["z3_solve_ms"] = float("nan")

    def _row(r: pd.Series) -> dict[str, Any]:
        dp_ms = float(r["time_total_ms_median"])
        z3_ms = r["z3_solve_ms"]
        z3_str = f"{float(z3_ms):.1f}" if pd.notna(z3_ms) else "-"
        speedup_str = "-"
        if pd.notna(z3_ms) and z3_ms > 0:
            speedup_str = f"{(float(z3_ms) / dp_ms):.2f}x"
        return {
            "instance_id": r["instance_id"],
            "family": r.get("family", ""),
            "n": int(r["n_vars"]) if pd.notna(r.get("n_vars")) else "-",
            "m": int(r["n_clauses"]) if pd.notna(r.get("n_clauses")) else "-",
            "psw": int(r["ps_width"]) if pd.notna(r.get("ps_width")) else "-",
            "dp_ms": f"{dp_ms:.1f}",
            "z3_ms": z3_str,
            "speedup": speedup_str,
        }

    sorted_desc = merged.sort_values("time_total_ms_median", ascending=False).head(n)
    sorted_asc = merged.sort_values("time_total_ms_median", ascending=True).head(n)
    return [_row(r) for _, r in sorted_desc.iterrows()], [_row(r) for _, r in sorted_asc.iterrows()]


def _dp_vs_z3_by_family(structure: pd.DataFrame, timings: pd.DataFrame,
                        z3: pd.DataFrame) -> list[dict[str, Any]]:
    if timings.empty or structure.empty or z3.empty:
        return []
    tim = timings[(timings["runner"] == "dp") &
                  (timings["mode"] == "greedy") &
                  (timings["status"].isin(["ok", "partial"]))].copy()
    z3m = z3[(z3["z3_kind"] == "maxsat") & (z3["z3_status"] == "sat")].copy()
    if tim.empty or z3m.empty:
        return []
    tim["time_total_ms_median"] = pd.to_numeric(tim["time_total_ms_median"], errors="coerce")
    z3m["z3_solve_ms"] = pd.to_numeric(z3m["z3_solve_ms"], errors="coerce")
    struct = structure[(structure["runner"] == "dp") & (structure["mode"] == "greedy")]
    fam_by_id = struct.set_index("instance_id")["family"].to_dict()
    tim["family"] = tim["instance_id"].map(fam_by_id)
    z3m["family"] = z3m["instance_id"].map(fam_by_id)

    out: list[dict[str, Any]] = []
    for fam in _FAMILIES:
        sub_dp = tim[tim["family"] == fam].dropna(subset=["time_total_ms_median"])
        sub_z3 = z3m[z3m["family"] == fam].dropna(subset=["z3_solve_ms"])
        if sub_dp.empty or sub_z3.empty:
            continue
        merged = sub_dp.merge(sub_z3[["instance_id", "z3_solve_ms"]],
                               on="instance_id", how="inner")
        if merged.empty:
            continue
        merged["speedup"] = merged["z3_solve_ms"] / merged["time_total_ms_median"]
        dp_med = merged["time_total_ms_median"].median()
        z3_med = merged["z3_solve_ms"].median()
        sp_med = merged["speedup"].median()
        slower = (merged["time_total_ms_median"] > merged["z3_solve_ms"]).sum()
        faster = (merged["time_total_ms_median"] < merged["z3_solve_ms"]).sum()
        out.append({
            "name": fam,
            "dp_med": f"{dp_med:.1f}",
            "z3_med": f"{z3_med:.1f}",
            "speedup_med": f"{sp_med:.2f}x",
            "dp_slower": int(slower),
            "dp_faster": int(faster),
        })
    return out


_QUERY_LABELS = [
    ("query_co_ms",         "CO"),
    ("query_va_ms",         "VA"),
    ("query_ct_ms",         "CT"),
    ("query_me_ms",         "ME-1"),
    ("query_ce_ms",         "CE"),
    ("query_im_ms",         "IM"),
    ("query_enum_first_ms", "ME-multi (1er)"),
]


def _query_stats(structure: pd.DataFrame, z3: pd.DataFrame, timings: pd.DataFrame
                 ) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    """Stats par requete (mediane, us/arete, speedup vs Z3) + break-even N.

    Retourne ([stats par requete], {breakeven_median, finite, total_instances_query}).
    Listes vides si la passe C n'a pas encore tourne.
    """
    if structure.empty:
        return [], {}
    dp_q = structure[(structure["runner"] == "dp_query") &
                     (structure["status"] == "ok")]
    if dp_q.empty:
        return [], {}

    # ---- Stats par requete (mediane sur toutes les instances OK passe C). ----
    z3_sat = (z3[z3["z3_kind"] == "sat"][["instance_id", "z3_solve_ms"]]
              if not z3.empty else pd.DataFrame())
    if not z3_sat.empty:
        z3_sat = z3_sat.copy()
        z3_sat["z3_solve_ms"] = pd.to_numeric(z3_sat["z3_solve_ms"], errors="coerce")
    df = dp_q.merge(z3_sat, on="instance_id", how="left") if not z3_sat.empty else dp_q.copy()
    df["dnnf_edges"] = pd.to_numeric(df.get("dnnf_edges"), errors="coerce")
    # Filtre instances UNSAT (DAG reduit a FALSE, dnnf_edges == 0) :
    # sur ces instances les requetes retournent immediatement (~30 ns)
    # car elles testent if(!root) en debut de fonction, ce qui pollue les
    # medianes et les ratios us/arete (division par 0).
    df = df[df["dnnf_edges"] > 0]
    if df.empty:
        return [], {}

    stats = []
    for col, label in _QUERY_LABELS:
        if col not in df.columns:
            continue
        sub = df.copy()
        sub["t_q"] = pd.to_numeric(sub[col], errors="coerce")
        sub = sub.dropna(subset=["t_q"])
        sub = sub[sub["t_q"] > 0]
        if sub.empty:
            continue
        median_ms = float(sub["t_q"].median())
        edges = sub["dnnf_edges"].dropna()
        med_us_per_edge = (
            float((sub["t_q"] * 1000.0 / sub["dnnf_edges"]).median())
            if not edges.empty and (edges > 0).any() else None
        )
        if "z3_solve_ms" in sub.columns:
            sp = sub["z3_solve_ms"] / sub["t_q"]
            sp = sp.dropna()
            speedup_med = f"{float(sp.median()):.2f}x" if not sp.empty else "-"
        else:
            speedup_med = "-"
        stats.append({
            "name": label,
            "median_us": f"{median_ms * 1000.0:.2f}",
            "median_us_per_edge": (f"{med_us_per_edge:.4f}"
                                   if med_us_per_edge is not None else "-"),
            "speedup_vs_z3": speedup_med,
        })

    # ---- Break-even N : utilise time_total_ms_median (passe B greedy) si dispo. ----
    breakeven: dict[str, Any] = {}
    if not timings.empty and not z3_sat.empty:
        dp_b = timings[(timings["runner"] == "dp") &
                       (timings["mode"] == "greedy")][
                       ["instance_id", "time_total_ms_median"]].copy()
        merged = dp_q.merge(dp_b, on="instance_id", how="left") \
                     .merge(z3_sat, on="instance_id", how="left")
        # Meme filtre UNSAT (dnnf_edges > 0) que ci-dessus pour eviter des
        # N* artificiellement petits sur des formules UNSAT trivialement
        # resolues par le DP (t_query et dp_compile tres petits).
        merged["dnnf_edges"] = pd.to_numeric(merged.get("dnnf_edges"), errors="coerce")
        merged = merged[merged["dnnf_edges"] > 0]
        merged["t_query"]    = pd.to_numeric(merged["query_co_ms"], errors="coerce")
        merged["dp_compile"] = pd.to_numeric(merged["time_total_ms_median"], errors="coerce")
        merged["z3_solve"]   = pd.to_numeric(merged["z3_solve_ms"], errors="coerce")
        merged = merged.dropna(subset=["t_query", "dp_compile", "z3_solve"])
        if not merged.empty:
            import numpy as np
            diff = merged["z3_solve"] - merged["t_query"]
            n_star = np.where(diff > 0,
                              np.ceil(merged["dp_compile"] / np.maximum(diff, 1e-12)),
                              np.inf)
            finite = n_star[n_star != np.inf]
            breakeven = {
                "breakeven_median": (f"{float(np.median(finite)):.0f}"
                                     if len(finite) else "infini"),
                "breakeven_finite_count": int(len(finite)),
                "total_instances_query": int(len(merged)),
            }
    return stats, breakeven


def _instances_meta(repo_root: Path) -> dict[str, Any]:
    inst_yaml = repo_root / "benchmarks" / "config" / "instances.yaml"
    if not inst_yaml.exists():
        return {"instances_list": []}
    try:
        import yaml
        return {"instances_list": yaml.safe_load(inst_yaml.read_text()).get("instances", [])}
    except Exception:
        return {"instances_list": []}


def _human_duration(secs: float) -> str:
    secs = int(secs)
    h, rem = divmod(secs, 3600)
    m, s = divmod(rem, 60)
    if h > 0:
        return f"{h}h{m:02d}m"
    if m > 0:
        return f"{m}m{s:02d}s"
    return f"{s}s"


def _run_duration(input_dir: Path) -> str:
    """Estime la duree du run a partir de la 1ere et derniere ligne de progress.log."""
    p = input_dir / "progress.log"
    if not p.exists():
        return "(inconnue)"
    try:
        lines = [l for l in p.read_text().splitlines() if l.strip()]
        if len(lines) < 2:
            return "(inconnue)"
        ts_pat = re.compile(r"\[([^\]]+)\]")
        first = ts_pat.search(lines[0])
        last = ts_pat.search(lines[-1])
        if not first or not last:
            return "(inconnue)"
        t0 = dt.datetime.fromisoformat(first.group(1).replace("Z", "+00:00"))
        t1 = dt.datetime.fromisoformat(last.group(1).replace("Z", "+00:00"))
        return _human_duration((t1 - t0).total_seconds())
    except Exception:
        return "(inconnue)"


def run(input_dir: Path, repo_root: Path, cfg: Any,
        orchestrator_version: str = "0.1.0") -> Path:
    """Genere SUMMARY.md dans input_dir. Retourne le chemin."""
    input_dir = Path(input_dir).resolve()
    structure = pd.DataFrame()
    timings = pd.DataFrame()
    z3 = pd.DataFrame()
    if (input_dir / "structure.csv").exists():
        structure = pd.read_csv(input_dir / "structure.csv")
    if (input_dir / "timings.csv").exists():
        timings = pd.read_csv(input_dir / "timings.csv")
    if (input_dir / "z3.csv").exists():
        z3 = pd.read_csv(input_dir / "z3.csv")

    instances_meta = _instances_meta(repo_root)

    families, totals = _families_overview(structure, instances_meta)
    invariants_table, invariant_failures = _invariant_summary(input_dir / "invariants.csv")
    top_slowest, top_fastest = _top_instances(structure, timings, z3, n=10)
    dp_vs_z3 = _dp_vs_z3_by_family(structure, timings, z3)
    query_stats_list, breakeven_meta = _query_stats(structure, z3, timings)

    git_meta = _parse_git_info(input_dir)
    machine = _machine_meta()

    template_path = Path(__file__).parent / "summary_template.md.j2"
    template = jinja2.Template(template_path.read_text())

    # Configuration : on tente de lire les valeurs cles.
    try:
        a_jobs = cfg.machine.get("passe_a_jobs", "?")
        b_cpu = ",".join(str(x) for x in cfg.machine.get("passe_b_taskset_cpus", []))
        b_repetitions = cfg.passe_b.get("repetitions", 3)
    except Exception:
        a_jobs, b_cpu, b_repetitions = "?", "?", "?"

    context = {
        "run_timestamp": input_dir.name,
        "orchestrator_version": orchestrator_version,
        "total_duration_human": _run_duration(input_dir),
        "a_jobs": a_jobs,
        "b_cpu": b_cpu,
        "b_repetitions": b_repetitions,
        "families": families,
        "total": totals["total"],
        "total_ok": totals["ok"],
        "total_timeout": totals["timeout"],
        "total_crash": totals["crash"],
        "total_disabled": totals["disabled"],
        "invariants": invariants_table,
        "invariant_failures": invariant_failures,
        "top_slowest": top_slowest,
        "top_fastest": top_fastest,
        "dp_vs_z3": dp_vs_z3,
        "query_stats": query_stats_list,
        "breakeven_median": breakeven_meta.get("breakeven_median"),
        "breakeven_finite_count": breakeven_meta.get("breakeven_finite_count"),
        "total_instances_query": breakeven_meta.get("total_instances_query"),
        **git_meta,
        **machine,
    }

    output_path = input_dir / "SUMMARY.md"
    output_path.write_text(template.render(**context))
    return output_path


__all__ = ["run"]
