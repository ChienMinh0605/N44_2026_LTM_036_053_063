#!/usr/bin/env bash
# Alternates QoS order across repetitions and runs all 24 measurements.
set -euo pipefail
cd "$(dirname "$0")/.."
for run in 1 2 3; do
  for scenario in N0 N1 N2 N3; do
    if (( run % 2 == 1 )); then qos_order=(0 1); else qos_order=(1 0); fi
    for qos in "${qos_order[@]}"; do
      ./scripts/run_experiment.sh "$scenario" "$qos" "$run"
    done
  done
done
./build/scripts/b_analyze --input data/E --out data/plots --strict
