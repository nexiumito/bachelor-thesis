"""invariants.py — verification des invariants theoriques apres la passe A.

Parcourt structure.csv et z3.csv et verifie pour chaque (instance, mode) les
invariants suivants :

    I1 — dnnf_count_match           (hard, Lemme 4 BCMS)
    I2 — dag_within_bcms_bound      (hard, Lemme 7 BCMS, |D| <= 7*k^3*(n+m))
    I3 — maxsat_match_z3            (hard, equivalence semantique MaxSAT)
    I4 — psw_within_family_bound    (severite selon instances.yaml)
    I5 — consistency_match_dp_z3    (hard, sharpsat>0 == Z3 sat)
    I6 — phase_times_nonneg         (soft, sanity)

Pour les lignes runner='dp_query' (passe C, bench des requetes sur DAG
compile), deux invariants supplementaires sont verifies :

    I7 — entails_match_z3           (hard, F entraine sa propre 1ere clause)
    I8 — enumerate_count_match      (hard, ME multi == CT, Lemme A.3 D-M 2002)

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
    """Verifie que sharpsat (DP) et dnnf_count_recomputed (DAG) coincident.

    C2 (run 2 fix) : la saturation a LLONG_MAX cote DP peut etre invalidee
    par une simplification structurelle du DAG (AND avec FALSE -> FALSE),
    auquel cas le recompte sur le DAG est plus precis. On ne fait donc plus
    confiance au drapeau `dnnf_count_match` rapporte par le C (qui faisait
    une stricte egalite), on re-derive a partir des etats d'overflow :

        sharpsat_ovf | dnnf_ovf | verdict
        -------------+----------+--------------------------------------------
        T            | T        | OK (les deux satures, accord par convention)
        T            | F        | OK (DP saturated mais DAG simplifie au DP
                     |          |     incorrect ; DAG = verite, on le respecte)
        F            | T        | FAIL (anormal : si recompte overflow alors
                     |          |       le DP devrait aussi avoir overflow)
        F            | F        | OK ssi sharpsat == dnnf_count_recomputed
    """
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

    sharpsat_raw = row.get("sharpsat")
    dnnf_recomp_raw = row.get("dnnf_count_recomputed")
    sharpsat_ovf = _is_overflow(sharpsat_raw)
    dnnf_ovf = _is_overflow(dnnf_recomp_raw)

    if sharpsat_ovf and dnnf_ovf:
        return _Check(**base, status="OK",
                      expected="overflow~overflow",
                      observed="overflow~overflow",
                      message="DP et DAG tous deux satures, accord par convention")

    if sharpsat_ovf and not dnnf_ovf:
        # Cas C2 : DP a sature mais le DAG donne une valeur precise.
        # Le DAG est la source de verite (simplifications structurelles).
        return _Check(**base, status="OK",
                      expected=f"DAG = {dnnf_recomp_raw}",
                      observed=f"sharpsat=overflow dnnf={dnnf_recomp_raw}",
                      message=("DP saturated mais DAG simplifie a valeur "
                               "precise ; DAG = source de verite"))

    if not sharpsat_ovf and dnnf_ovf:
        # Anormal : si le DAG a overflow, le DP aussi devrait.
        return _Check(**base, status="FAIL",
                      expected="sharpsat aussi en overflow",
                      observed=f"sharpsat={sharpsat_raw} dnnf=overflow",
                      message="DAG en overflow mais DP a une valeur finie")

    # Cas nominal : aucun overflow, comparaison stricte.
    sharpsat = _parse_int(sharpsat_raw)
    dnnf = _parse_int(dnnf_recomp_raw)
    if sharpsat is None or dnnf is None:
        return _Check(**base, status="SKIPPED", expected="-",
                       observed=f"sharpsat={sharpsat_raw} dnnf={dnnf_recomp_raw}",
                       message="valeurs absentes ou non parsables")
    if sharpsat == dnnf:
        return _Check(**base, status="OK", expected=str(dnnf),
                      observed=str(sharpsat), message="")
    return _Check(**base, status="FAIL",
                   expected=str(dnnf), observed=str(sharpsat),
                   message=f"dnnf_count={dnnf} != sharpsat={sharpsat}")


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
    base = dict(
        instance_id=row["instance_id"],
        mode=mode,
        seed=int(row.get("seed") or 0),
        invariant="psw_within_family_bound",
        severity=configured_severity,
    )
    # B9 : la borne psw du papier (Saether/Telle/Vatshelle Sect. 6.1) porte
    # sur la decomposition OPTIMALE. Seul greedy est l'heuristique concue
    # pour s'en approcher. linear et random ne pretendent pas la respecter,
    # donc tester la borne sur ces modes n'a pas de sens theorique. On skip
    # explicitement (au lieu de WARN) pour ne pas polluer invariants.csv ni
    # le SUMMARY. Le constat empirique linear vs greedy reste visualise dans
    # le plot greedy_vs_linear.
    if mode != "greedy":
        return _Check(**base, status="SKIPPED",
                      expected="-", observed=str(row.get("ps_width")),
                      message="borne psw definie pour decompo optimale (greedy uniquement)")
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
    # C2 (run 2 fix) : determiner dp_sat avec preference au DAG quand le DP
    # a sature. La table de decision :
    #   sharpsat_ovf | dnnf_ovf | dp_sat
    #   -------------+----------+----------------------------------------
    #   T            | T        | True  (les deux satures = beaucoup de modeles)
    #   T            | F        | dnnf_count > 0 (DAG = verite, simplifications
    #                |          |   structurelles ont reduit le compte)
    #   F            | T        | True  (anormal mais a la limite, on prend SAT
    #                |          |   par defaut puisque qch est calcule cote DP)
    #   F            | F        | sharpsat > 0
    sharpsat_raw = row.get("sharpsat")
    dnnf_recomp_raw = row.get("dnnf_count_recomputed")
    sharpsat_ovf = _is_overflow(sharpsat_raw)
    dnnf_ovf = _is_overflow(dnnf_recomp_raw)

    if sharpsat_ovf and dnnf_ovf:
        dp_sat = True
    elif sharpsat_ovf and not dnnf_ovf:
        # DP sature mais DAG precis : DAG est source de verite.
        dnnf = _parse_int(dnnf_recomp_raw)
        if dnnf is None:
            return _Check(**base, status="SKIPPED", expected="-",
                           observed="-",
                           message="DP sature et DAG non parsable")
        dp_sat = dnnf > 0
    elif not sharpsat_ovf and dnnf_ovf:
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


def _check_enumerate_count_match(row: dict[str, Any]) -> _Check:
    """I8 : query_enum_count == dnnf_count_recomputed (Lemme A.3 D-M 2002).

    Verification stricte sur DAG lisse : ME multi (enumerate) doit produire
    exactement #SAT modeles distincts. Skip si enumerate a ete cape (par
    exemple si dnnf_count > 1e6) ou si le run a echoue.
    """
    base = dict(
        instance_id=row["instance_id"],
        mode=row["mode"],
        seed=int(row.get("seed") or 0),
        invariant="enumerate_count_match",
        severity="hard",
    )
    if row.get("status") != "ok":
        return _Check(**base, status="SKIPPED", expected="-", observed="-",
                       message="run non OK")
    if row.get("query_enum_count") in (None, ""):
        return _Check(**base, status="SKIPPED", expected="-", observed="-",
                       message="pas de bench query")
    skipped = _parse_bool(row.get("query_enum_all_skipped"))
    if skipped:
        return _Check(**base, status="SKIPPED", expected="-", observed="-",
                       message="enumerate cape (>seuil ou overflow)")
    enum_count = _parse_int(row["query_enum_count"])
    dnnf_count = _parse_int(row.get("dnnf_count_recomputed"))
    if enum_count is None or dnnf_count is None:
        return _Check(**base, status="SKIPPED", expected="-", observed="-",
                       message="parse error")
    if enum_count == dnnf_count:
        return _Check(**base, status="OK", expected=str(dnnf_count),
                       observed=str(enum_count), message="")
    return _Check(**base, status="FAIL", expected=str(dnnf_count),
                   observed=str(enum_count),
                   message=f"enumerate produit {enum_count} modeles vs "
                           f"dnnf_count={dnnf_count}")


def _check_entails_match_z3(row: dict[str, Any]) -> _Check:
    """I7 : F entraine sa propre 1ere clause (verdict trivial cote Z3).

    Le solveur en mode --json-with-queries interroge CE sur la 1ere clause de
    F par convention. F entraine trivialement chacune de ses propres clauses,
    donc query_ce_result doit etre 1 (YES) ou -1 (VACUOUSLY si F UNSAT).
    """
    base = dict(
        instance_id=row["instance_id"],
        mode=row["mode"],
        seed=int(row.get("seed") or 0),
        invariant="entails_match_z3",
        severity="hard",
    )
    if row.get("status") != "ok":
        return _Check(**base, status="SKIPPED", expected="-", observed="-",
                       message="run non OK")
    ce = _parse_int(row.get("query_ce_result"))
    if ce is None:
        return _Check(**base, status="SKIPPED", expected="-", observed="-",
                       message="pas de query_ce_result")
    # 1 (YES) ou -1 (VACUOUSLY si F UNSAT) sont sémantiquement YES.
    if ce in (1, -1):
        return _Check(**base, status="OK", expected="1 ou -1 (YES/VACUOUSLY)",
                       observed=str(ce), message="")
    return _Check(**base, status="FAIL",
                   expected="1 (YES, F entraine sa propre 1ere clause)",
                   observed=str(ce),
                   message="contradiction theorique")


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
                runner = row.get("runner")
                if runner not in ("dp", "dp_query"):
                    continue
                inst = instances.get(row["instance_id"])
                z3_max_row = z3_index.get((row["instance_id"], "maxsat"))
                z3_sat_row = z3_index.get((row["instance_id"], "sat"))

                # I1..I6 sur toutes les lignes DP (passes A et C).
                checks = [
                    _check_dnnf_count_match(row),
                    _check_dag_bound(row),
                    _check_maxsat_match_z3(row, z3_max_row),
                    _check_psw_bound(row, inst),
                    _check_consistency_match(row, z3_sat_row),
                    _check_phase_times_nonneg(row),
                ]
                # I7 et I8 uniquement sur les lignes de la passe C
                # (donnees query_* presentes).
                if runner == "dp_query":
                    checks.append(_check_entails_match_z3(row))
                    checks.append(_check_enumerate_count_match(row))
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
