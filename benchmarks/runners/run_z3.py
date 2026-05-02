"""run_z3.py — runs Z3 (MaxSAT et SAT decision) sur une instance.

Deux objectifs :
  1. MaxSAT : nombre de clauses simultanement satisfaisables, comparable a
     ``dp_result.maxsat_value``.
  2. SAT decision : la formule est-elle satisfaisable ? Comparable a
     ``dnnf_consistency``.

Recupere aussi les stats internes Z3 (conflicts, decisions, restarts,
propagations) pour la correlation backtracks Z3 vs ps-width DP.
"""

from __future__ import annotations

import time
from pathlib import Path
from typing import Any

# z3 est importe au niveau module : si Z3 manque, le runner Python crash a
# l'import, l'orchestrateur le detecte et logue un warning (l'utilisateur peut
# relancer avec --skip-z3).
try:
    from z3 import Bool, Not, Optimize, Or, Solver, sat, set_param, unsat
    _Z3_AVAILABLE = True
    _Z3_IMPORT_ERROR: str | None = None
except Exception as exc:  # pragma: no cover -- chemin defensif
    _Z3_AVAILABLE = False
    _Z3_IMPORT_ERROR = str(exc)


# ----------------------------------------------------------------------------
# Configuration globale Z3 : on essaie de forcer un comportement deterministe
# et mono-thread. Idempotent.
# ----------------------------------------------------------------------------
def _configure_z3() -> None:
    if not _Z3_AVAILABLE:
        return
    try:
        set_param("verbose", 0)
        # Force Z3 en mono-thread pour des mesures reproductibles, surtout
        # en passe B ou chaque job est cense occuper exactement 1 coeur.
        set_param("smt.threads", 1)
    except Exception:
        # set_param n'est pas crucial -- on n'echoue pas si Z3 ne le supporte pas.
        pass


_configure_z3()


# ----------------------------------------------------------------------------
# Parser DIMACS minimaliste (independant du parseur C, on n'a pas besoin de la
# masque optimisation -- juste la liste des clauses comme listes d'entiers).
# ----------------------------------------------------------------------------
def parse_dimacs(path: str | Path) -> tuple[int, int, list[list[int]]]:
    """Lit un fichier DIMACS CNF et retourne (n_vars, n_clauses, clauses)."""
    n_vars, n_clauses = 0, 0
    clauses: list[list[int]] = []
    with open(path) as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("c") or line.startswith("%"):
                continue
            if line.startswith("p"):
                # 'p cnf N M'
                parts = line.split()
                if len(parts) >= 4:
                    n_vars = int(parts[2])
                    n_clauses = int(parts[3])
                continue
            # Clause : termine par 0.
            try:
                lits = [int(x) for x in line.split() if x and x != "0"]
            except ValueError:
                continue
            if lits:
                clauses.append(lits)
    return n_vars, n_clauses, clauses


# ----------------------------------------------------------------------------
# Extraction souple des stats Z3.
# ----------------------------------------------------------------------------
def _extract_stats(stats_obj: Any) -> dict[str, Any]:
    """Lit les stats Z3 avec tolerance aux changements de cles entre versions."""
    out: dict[str, Any] = {
        "z3_conflicts": None,
        "z3_decisions": None,
        "z3_restarts": None,
        "z3_propagations": None,
        "z3_time_internal_s": None,
    }
    try:
        keys_seen = list(stats_obj.keys())
    except Exception:
        return out

    aliases = {
        "z3_conflicts":      ["sat conflicts", "conflicts"],
        "z3_decisions":      ["sat decisions", "decisions"],
        "z3_restarts":       ["sat restarts", "restarts"],
        "z3_propagations":   ["sat propagations", "propagations", "binary propagations"],
        "z3_time_internal_s": ["time", "total time"],
    }
    for out_key, candidates in aliases.items():
        for cand in candidates:
            if cand in keys_seen:
                try:
                    out[out_key] = stats_obj.get_key_value(cand) \
                        if hasattr(stats_obj, "get_key_value") else stats_obj[cand]
                except Exception:
                    pass
                break
    out["z3_stats_keys_available"] = ",".join(keys_seen)
    return out


# ----------------------------------------------------------------------------
# Z3 MaxSAT.
# ----------------------------------------------------------------------------
def run_z3_maxsat(instance: Any, timeout_s: int, repo_root: str = ".") -> dict[str, Any]:
    """Lance Z3 Optimize sur l'instance avec toutes les clauses en soft.

    Resolution: ``opt.check()`` ; en cas de ``sat``, on parcourt le modele et
    on compte combien de clauses sont satisfaites (= MaxSAT).

    Returns:
        dict avec ``z3_kind="maxsat"``, ``z3_status``, ``z3_maxsat`` (entier ou
        None si pas sat), ``z3_solve_ms``, ``z3_total_ms``, plus les stats.
    """
    base: dict[str, Any] = {
        "instance_id": instance.id,
        "z3_kind": "maxsat",
    }
    t_total = time.perf_counter()

    if not _Z3_AVAILABLE:
        return {
            **base,
            "z3_status": "error",
            "z3_error": f"z3-solver indisponible: {_Z3_IMPORT_ERROR}",
            "z3_total_ms": (time.perf_counter() - t_total) * 1000,
        }

    # Resolution du chemin : ``instance.path`` est relatif au cwd ``src/``,
    # ``repo_root`` permet de reconstruire un chemin absolu.
    abs_path = Path(repo_root) / "src" / instance.path

    try:
        n, m, clauses = parse_dimacs(abs_path)
    except (OSError, ValueError) as e:
        return {
            **base,
            "z3_status": "parse_error",
            "z3_error": str(e),
            "z3_total_ms": (time.perf_counter() - t_total) * 1000,
        }

    V = [Bool(f"x{i+1}") for i in range(n)]
    opt = Optimize()
    opt.set("timeout", int(timeout_s * 1000))
    for c in clauses:
        lits = [V[abs(l) - 1] if l > 0 else Not(V[abs(l) - 1]) for l in c]
        opt.add_soft(Or(lits) if len(lits) > 1 else lits[0])

    t_solve = time.perf_counter()
    try:
        result = opt.check()
    except Exception as e:
        return {
            **base,
            "z3_status": "z3_exception",
            "z3_error": str(e),
            "z3_solve_ms": (time.perf_counter() - t_solve) * 1000,
            "z3_total_ms": (time.perf_counter() - t_total) * 1000,
            "z3_n_vars": n,
            "z3_n_clauses": m,
        }
    t_solve_ms = (time.perf_counter() - t_solve) * 1000

    satisfied: int | None = None
    status_str = str(result)
    if status_str == "sat":
        try:
            model = opt.model()
            satisfied = 0
            for c in clauses:
                for l in c:
                    var = V[abs(l) - 1]
                    val = bool(model.evaluate(var, model_completion=True))
                    if (l > 0 and val) or (l < 0 and not val):
                        satisfied += 1
                        break
        except Exception:
            satisfied = None

    if status_str == "sat":
        z3_status = "sat"
    elif status_str == "unsat":
        z3_status = "unsat"
    else:
        # 'unknown' = timeout ou non-conclusif.
        z3_status = "timeout" if t_solve_ms >= timeout_s * 1000 - 100 else "unknown"

    stats = {}
    try:
        stats = _extract_stats(opt.statistics())
    except Exception:
        stats = {}

    return {
        **base,
        "z3_status": z3_status,
        "z3_maxsat": satisfied,
        "z3_solve_ms": t_solve_ms,
        "z3_total_ms": (time.perf_counter() - t_total) * 1000,
        "z3_n_vars": n,
        "z3_n_clauses": m,
        **stats,
    }


# ----------------------------------------------------------------------------
# Z3 SAT decision (Solver classique, hard constraints).
# ----------------------------------------------------------------------------
def run_z3_sat(instance: Any, timeout_s: int, repo_root: str = ".") -> dict[str, Any]:
    """Decide la satisfiabilite via Solver Z3 classique."""
    base: dict[str, Any] = {
        "instance_id": instance.id,
        "z3_kind": "sat",
    }
    t_total = time.perf_counter()

    if not _Z3_AVAILABLE:
        return {
            **base,
            "z3_status": "error",
            "z3_error": f"z3-solver indisponible: {_Z3_IMPORT_ERROR}",
            "z3_total_ms": (time.perf_counter() - t_total) * 1000,
        }

    abs_path = Path(repo_root) / "src" / instance.path
    try:
        n, m, clauses = parse_dimacs(abs_path)
    except (OSError, ValueError) as e:
        return {
            **base,
            "z3_status": "parse_error",
            "z3_error": str(e),
            "z3_total_ms": (time.perf_counter() - t_total) * 1000,
        }

    V = [Bool(f"x{i+1}") for i in range(n)]
    solver = Solver()
    solver.set("timeout", int(timeout_s * 1000))
    for c in clauses:
        lits = [V[abs(l) - 1] if l > 0 else Not(V[abs(l) - 1]) for l in c]
        solver.add(Or(lits) if len(lits) > 1 else lits[0])

    t_solve = time.perf_counter()
    try:
        result = solver.check()
    except Exception as e:
        return {
            **base,
            "z3_status": "z3_exception",
            "z3_error": str(e),
            "z3_solve_ms": (time.perf_counter() - t_solve) * 1000,
            "z3_total_ms": (time.perf_counter() - t_total) * 1000,
            "z3_n_vars": n,
            "z3_n_clauses": m,
        }
    t_solve_ms = (time.perf_counter() - t_solve) * 1000

    if result == sat:
        z3_sat_status = "sat"
    elif result == unsat:
        z3_sat_status = "unsat"
    else:
        z3_sat_status = "timeout" if t_solve_ms >= timeout_s * 1000 - 100 else "unknown"

    stats = {}
    try:
        stats = _extract_stats(solver.statistics())
    except Exception:
        stats = {}

    return {
        **base,
        "z3_status": z3_sat_status,
        "z3_sat_status": z3_sat_status,
        "z3_solve_ms": t_solve_ms,
        "z3_total_ms": (time.perf_counter() - t_total) * 1000,
        "z3_n_vars": n,
        "z3_n_clauses": m,
        **stats,
    }


__all__ = ["run_z3_maxsat", "run_z3_sat", "parse_dimacs"]
