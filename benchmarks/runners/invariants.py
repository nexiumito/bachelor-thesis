"""invariants.py — verification des invariants theoriques apres la passe A.

Parcourt structure.csv et z3.csv et verifie pour chaque (instance, mode) les
invariants suivants :

    I1 — dnnf_count_match           (hard, Lemme 4 BCMS)
    I2 — dag_within_bcms_bound      (hard, Lemme 7 BCMS, |D| <= 7*k^3*(n+m))
    I3 — maxsat_match_z3            (hard, equivalence semantique MaxSAT)
    I4 — psw_within_family_bound    (severite selon instances.yaml)
    I5 — consistency_match_dp_z3    (hard, sharpsat>0 == Z3 sat)
    I6 — phase_times_nonneg         (soft, sanity)

Ecrit une ligne par (instance, mode, seed, invariant) dans invariants.csv.
Append failures.log pour chaque FAIL hard. Continue meme si certains FAIL.
"""

from __future__ import annotations

import csv
import datetime as dt
import logging
from dataclasses import dataclass
from pathlib import Path
from typing import Any

logger = logging.getLogger(__name__)


INVARIANTS_CSV_COLS = [
    "instance_id", "mode", "seed", "invariant", "severity",
    "status", "expected", "observed", "message",
]


@dataclass
class _Check:
    instance_id: str
    mode: str
    seed: int
    invariant: str
    severity: str
    status: str   # OK | FAIL | WARN | SKIPPED
    expected: str
    observed: str
    message: str


def _parse_int(s: Any) -> int | None:
    if s is None or s == "":
        return None
    try:
        return int(float(s))
    except (TypeError, ValueError):
        return None


def _parse_float(s: Any) -> float | None:
    if s is None or s == "":
        return None
    try:
        return float(s)
    except (TypeError, ValueError):
        return None


def _parse_bool(s: Any) -> bool | None:
    if s is None or s == "":
        return None
    if isinstance(s, bool):
        return s
    s_low = str(s).strip().lower()
    if s_low in ("true", "1", "yes"):
        return True
    if s_low in ("false", "0", "no"):
        return False
    return None


def _is_overflow(v: Any) -> bool:
    return isinstance(v, str) and v.strip().lower() == "overflow"


def _check_dnnf_count_match(row: dict[str, Any]) -> _Check:
    base = dict(
        instance_id=row["instance_id"],
        mode=row["mode"],
        seed=int(row.get("seed") or 0),
        invariant="dnnf_count_match",
        severity="hard",
    )
    if row.get("status") != "ok":
        return _Check(**base, status="SKIPPED", expected="true", observed="-",
                       message="run non OK")
    match = _parse_bool(row.get("dnnf_count_match"))
    sharpsat_raw = row.get("sharpsat")
    dnnf_recomp_raw = row.get("dnnf_count_recomputed")
    if _is_overflow(sharpsat_raw) or _is_overflow(dnnf_recomp_raw):
        # Si l'un des deux a overflow, on compare seulement le drapeau match
        # tel que rapporte par le binaire C.
        if match is True:
            return _Check(**base, status="OK", expected="true", observed="true",
                           message="overflow JSON, drapeau match=true conserve")
        return _Check(**base, status="FAIL", expected="true",
                       observed=str(match), message="overflow detecte et drapeau != true")
    if match is None:
        return _Check(**base, status="SKIPPED", expected="true", observed="-",
                       message="dnnf_count_match absent")
    if match is True:
        return _Check(**base, status="OK", expected="true", observed="true", message="")
    return _Check(**base, status="FAIL", expected="true",
                   observed=f"sharpsat={sharpsat_raw} dnnf={dnnf_recomp_raw}",
                   message=f"dnnf_count={dnnf_recomp_raw} != sharpsat={sharpsat_raw}")


def _check_dag_bound(row: dict[str, Any]) -> _Check:
    base = dict(
        instance_id=row["instance_id"],
        mode=row["mode"],
        seed=int(row.get("seed") or 0),
        invariant="dag_within_bcms_bound",
        severity="hard",
    )
    if row.get("status") != "ok":
        return _Check(**base, status="SKIPPED", expected="-", observed="-",
                       message="run non OK")
    nodes = _parse_int(row.get("dnnf_nodes"))
    bound_raw = row.get("dnnf_bound_7k3nm")
    bound = _parse_int(bound_raw) if not _is_overflow(bound_raw) else None
    if nodes is None or bound is None:
        return _Check(**base, status="SKIPPED", expected="-",
                       observed=f"nodes={nodes} bound={bound_raw}",
                       message="valeurs manquantes ou overflow")
    if nodes <= bound:
        return _Check(**base, status="OK",
                       expected=f"<={bound}",
                       observed=str(nodes), message="")
    return _Check(**base, status="FAIL",
                   expected=f"<={bound}",
                   observed=str(nodes),
                   message=f"|D|={nodes} > borne={bound} "
                           f"(psw={row.get('ps_width')}, n={row.get('n_vars')}, "
                           f"m={row.get('n_clauses')})")


def _check_maxsat_match_z3(row: dict[str, Any], z3_row: dict[str, Any] | None) -> _Check:
    base = dict(
        instance_id=row["instance_id"],
        mode=row["mode"],
        seed=int(row.get("seed") or 0),
        invariant="maxsat_match_z3",
        severity="hard",
    )
    if row.get("status") != "ok":
        return _Check(**base, status="SKIPPED", expected="-", observed="-",
                       message="run DP non OK")
    if z3_row is None:
        return _Check(**base, status="SKIPPED", expected="-", observed="-",
                       message="pas de run Z3 maxsat disponible")
    z3_status = z3_row.get("z3_status")
    if z3_status != "sat":
        return _Check(**base, status="SKIPPED", expected="-",
                       observed=f"z3_status={z3_status}",
                       message="Z3 non concluant")
    dp_max = _parse_int(row.get("maxsat"))
    z3_max = _parse_int(z3_row.get("z3_maxsat"))
    if dp_max is None or z3_max is None:
        return _Check(**base, status="SKIPPED", expected="-",
                       observed=f"dp={dp_max} z3={z3_max}",
                       message="valeurs absentes")
    if dp_max == z3_max:
        return _Check(**base, status="OK",
                       expected=str(z3_max), observed=str(dp_max), message="")
    return _Check(**base, status="FAIL",
                   expected=str(z3_max), observed=str(dp_max),
                   message=f"DP={dp_max} vs Z3={z3_max}")


def _check_psw_bound(row: dict[str, Any], inst: Any) -> _Check:
    mode = row["mode"]
    configured_severity = (getattr(inst, "expected_psw_severity", "none")
                           if inst else "none")
    # La borne psw du papier (Saether/Telle/Vatshelle Sect. 6.1) porte sur la
    # decomposition OPTIMALE. Seul greedy est l'heuristique conçue pour s'en
    # approcher. linear et random ne pretendent pas la respecter : un
    # depassement y est un constat attendu (visualise dans greedy_vs_linear),
    # pas une violation theorique. On degrade donc hard -> soft pour ces modes
    # pour eviter de spammer les notifs FAIL hard pendant le bench long.
    if mode != "greedy" and configured_severity == "hard":
        effective_severity = "soft"
    else:
        effective_severity = configured_severity
    base = dict(
        instance_id=row["instance_id"],
        mode=mode,
        seed=int(row.get("seed") or 0),
        invariant="psw_within_family_bound",
        severity=effective_severity,
    )
    if row.get("status") != "ok":
        return _Check(**base, status="SKIPPED", expected="-", observed="-",
                       message="run non OK")
    expected = getattr(inst, "expected_psw_max", None) if inst else None
    severity = base["severity"]
    if expected is None or severity == "none":
        return _Check(**base, status="SKIPPED", expected="-",
                       observed=str(row.get("ps_width")), message="pas de borne")
    psw = _parse_int(row.get("ps_width"))
    if psw is None:
        return _Check(**base, status="SKIPPED", expected=f"<={expected}",
                       observed="-", message="ps_width absent")
    if psw <= expected:
        return _Check(**base, status="OK",
                       expected=f"<={expected}", observed=str(psw), message="")
    # FAIL : hard ou soft selon la severite.
    status = "FAIL" if severity == "hard" else "WARN"
    return _Check(**base, status=status,
                   expected=f"<={expected}", observed=str(psw),
                   message=f"psw={psw} > borne attendue={expected} "
                           f"(famille {getattr(inst, 'family', '?')}, mode {row['mode']})")


def _check_consistency_match(row: dict[str, Any], z3_sat_row: dict[str, Any] | None) -> _Check:
    base = dict(
        instance_id=row["instance_id"],
        mode=row["mode"],
        seed=int(row.get("seed") or 0),
        invariant="consistency_match_dp_z3",
        severity="hard",
    )
    if row.get("status") != "ok":
        return _Check(**base, status="SKIPPED", expected="-", observed="-",
                       message="run DP non OK")
    if z3_sat_row is None:
        return _Check(**base, status="SKIPPED", expected="-", observed="-",
                       message="pas de run Z3 sat disponible")
    z3_status = z3_sat_row.get("z3_sat_status") or z3_sat_row.get("z3_status")
    if z3_status not in ("sat", "unsat"):
        return _Check(**base, status="SKIPPED", expected="-",
                       observed=f"z3={z3_status}", message="Z3 non concluant")
    sharpsat_raw = row.get("sharpsat")
    if _is_overflow(sharpsat_raw):
        # Overflow signifie SAT (forcement > 0).
        dp_sat = True
    else:
        sharpsat = _parse_int(sharpsat_raw)
        if sharpsat is None:
            return _Check(**base, status="SKIPPED", expected="-",
                           observed="-", message="sharpsat absent")
        dp_sat = sharpsat > 0
    z3_sat = (z3_status == "sat")
    if dp_sat == z3_sat:
        return _Check(**base, status="OK",
                       expected=str(z3_sat), observed=str(dp_sat), message="")
    return _Check(**base, status="FAIL",
                   expected=str(z3_sat), observed=str(dp_sat),
                   message=f"DP sat={dp_sat} vs Z3 sat={z3_sat}")


def _check_phase_times_nonneg(row: dict[str, Any]) -> _Check:
    base = dict(
        instance_id=row["instance_id"],
        mode=row["mode"],
        seed=int(row.get("seed") or 0),
        invariant="phase_times_nonneg",
        severity="soft",
    )
    if row.get("status") != "ok":
        return _Check(**base, status="SKIPPED", expected="-", observed="-",
                       message="run non OK")
    phases = ["time_phase0_ms", "time_phase1_ms", "time_phase2_ms", "time_phase3_ms"]
    vals = [_parse_float(row.get(k)) for k in phases]
    total = _parse_float(row.get("time_total_ms"))
    if any(v is None for v in vals) or total is None:
        return _Check(**base, status="SKIPPED", expected="-",
                       observed="-", message="valeurs incompletes")
    if any(v < 0 for v in vals):
        return _Check(**base, status="WARN", expected=">=0",
                       observed=str(vals), message="phase negative detectee")
    sum_phases = sum(vals)
    # On tolere une erreur d'arrondi de 1 ms.
    if total + 1.0 < sum_phases:
        return _Check(**base, status="WARN",
                       expected=f"total~{sum_phases}",
                       observed=str(total),
                       message=f"time_total_ms={total} < sum(phases)={sum_phases}")
    return _Check(**base, status="OK", expected=">=0",
                   observed=f"sum={sum_phases:.3f}", message="")


# ===========================================================================
# Entree publique
# ===========================================================================

def check_all_invariants(
    structure_csv: Path,
    z3_csv: Path | None,
    instances: dict[str, Any],
    output_csv: Path,
    failures_log: Path,
    notifier: Any | None = None,
) -> dict[str, Any]:
    """Parcourt structure.csv, applique tous les invariants, ecrit invariants.csv.

    Args:
        structure_csv: chemin du CSV de la passe A.
        z3_csv: chemin du CSV Z3 (peut etre None si Z3 desactive).
        instances: dict id -> InstanceConfig (pour expected_psw_max).
        output_csv: chemin de sortie (invariants.csv).
        failures_log: append des FAIL hard.
        notifier: Notifier optionnel (pour fail_hard).

    Returns:
        dict avec totaux et breakdown par invariant.
    """
    if not structure_csv.exists():
        logger.warning("structure.csv inexistant, invariants skip")
        return {"total": 0, "ok": 0, "fail_hard": 0, "fail_soft": 0,
                "skipped": 0, "by_invariant": {}}

    # Index Z3 : (instance_id, kind) -> row.
    z3_index: dict[tuple[str, str], dict[str, Any]] = {}
    if z3_csv and z3_csv.exists():
        with z3_csv.open("r", newline="", encoding="utf-8") as fh:
            for row in csv.DictReader(fh):
                z3_index[(row["instance_id"], row["z3_kind"])] = row

    total = 0
    counts = {"OK": 0, "FAIL": 0, "WARN": 0, "SKIPPED": 0}
    fail_hard = 0
    fail_soft = 0
    by_invariant: dict[str, dict[str, int]] = {}

    with output_csv.open("w", newline="", encoding="utf-8") as fh_out:
        writer = csv.DictWriter(fh_out, fieldnames=INVARIANTS_CSV_COLS)
        writer.writeheader()

        with structure_csv.open("r", newline="", encoding="utf-8") as fh_in:
            for row in csv.DictReader(fh_in):
                if row.get("runner") != "dp":
                    continue
                inst = instances.get(row["instance_id"])
                z3_max_row = z3_index.get((row["instance_id"], "maxsat"))
                z3_sat_row = z3_index.get((row["instance_id"], "sat"))

                checks = [
                    _check_dnnf_count_match(row),
                    _check_dag_bound(row),
                    _check_maxsat_match_z3(row, z3_max_row),
                    _check_psw_bound(row, inst),
                    _check_consistency_match(row, z3_sat_row),
                    _check_phase_times_nonneg(row),
                ]
                for c in checks:
                    total += 1
                    counts[c.status] = counts.get(c.status, 0) + 1
                    by_invariant.setdefault(c.invariant, {"ok": 0, "fail": 0,
                                                           "warn": 0, "skipped": 0})
                    if c.status == "OK":
                        by_invariant[c.invariant]["ok"] += 1
                    elif c.status == "FAIL":
                        by_invariant[c.invariant]["fail"] += 1
                        if c.severity == "hard":
                            fail_hard += 1
                            _append_failure(failures_log, c)
                            if notifier is not None:
                                try:
                                    notifier.fail_hard(c.instance_id, c.mode,
                                                       c.invariant, c.message)
                                except Exception:
                                    pass
                        else:
                            fail_soft += 1
                    elif c.status == "WARN":
                        by_invariant[c.invariant]["warn"] += 1
                        fail_soft += 1
                    else:
                        by_invariant[c.invariant]["skipped"] += 1
                    writer.writerow({
                        "instance_id": c.instance_id,
                        "mode": c.mode,
                        "seed": c.seed,
                        "invariant": c.invariant,
                        "severity": c.severity,
                        "status": c.status,
                        "expected": c.expected,
                        "observed": c.observed,
                        "message": c.message,
                    })

    return {
        "total": total,
        "ok": counts.get("OK", 0),
        "fail_hard": fail_hard,
        "fail_soft": fail_soft,
        "skipped": counts.get("SKIPPED", 0),
        "by_invariant": by_invariant,
    }


def _append_failure(failures_log: Path, c: _Check) -> None:
    ts = dt.datetime.now(dt.timezone.utc).isoformat()
    msg = (f"[{ts}] INVARIANT FAIL {c.invariant} ({c.severity}) "
           f"{c.instance_id} mode={c.mode} seed={c.seed} : {c.message}\n")
    try:
        with failures_log.open("a", encoding="utf-8") as fh:
            fh.write(msg)
    except Exception as e:
        logger.warning("failures.log: %s", e)


__all__ = ["check_all_invariants", "INVARIANTS_CSV_COLS"]
