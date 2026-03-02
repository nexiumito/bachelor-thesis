#!/bin/bash

BASE_DIR="experiments/time"

for exp_dir in "$BASE_DIR"/time_analysis_K*/; do
    CONFIG="$exp_dir/config.json"
    LOG_DIR="$exp_dir/logs"
    LOG_FILE="$LOG_DIR/logfile.log"

    mkdir -p "$LOG_DIR"
    
    echo "  Lancement de l'expérience : $CONFIG"
    nohup python3 code/main.py "$CONFIG" > "$LOG_FILE" 2>&1 &
done

echo "  Tous les jobs ont été lancés en arrière-plan."
