#!/bin/bash

# ───── Valeurs par défaut ─────
DEFAULT_K_VALUES="25"
DEFAULT_NB_MULTS=10
DEFAULT_SIMPLIFIE=false
DEFAULT_BLOAT=false

# ───── Lecture des arguments ─────
EXPERIMENT_NAME="$1"
TYPE_ANALYSIS="$2"
K_VALUES="${3:-$DEFAULT_K_VALUES}"
NB_MULTS="${4:-$DEFAULT_NB_MULTS}"
SIMPLIFIE="${5:-$DEFAULT_SIMPLIFIE}"
BLOAT="${6:-$DEFAULT_BLOAT}"

# ───── Vérification des arguments obligatoires ─────
if [ -z "$EXPERIMENT_NAME" ] || [ -z "$TYPE_ANALYSIS" ]; then
  echo "  Utilisation : ./new_experiment.sh <nom_experience> <type_analysis> [k_values] [nb_mults] [simplifie] [bloat]"
  echo "  Exemple : ./new_experiment.sh test_k50 time 25 20 true false"
  echo "  Types possibles : time, complexity, varClause, randomModifiedVar, uselessDeletedVar, etc."
  exit 1
fi

# ───── Création de l'arborescence ─────
BASE_DIR="experiments/$TYPE_ANALYSIS/$EXPERIMENT_NAME"
mkdir -p "$BASE_DIR"/{dimacs,graphs,logs,csv}

# ───── Génération du fichier config.json ─────
cat << EOF > "$BASE_DIR/config.json"
{
  "experiment_name": "$EXPERIMENT_NAME",
  "type": "$TYPE_ANALYSIS",
  "k_values": $K_VALUES,
  "nb_mults": $NB_MULTS,
  "simplifie": $SIMPLIFIE,
  "bloat": $BLOAT
}
EOF

echo "✅ Expérience '$EXPERIMENT_NAME' de type '$TYPE_ANALYSIS' créée dans $BASE_DIR"
