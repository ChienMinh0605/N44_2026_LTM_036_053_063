#!/usr/bin/env bash
# Đặt điều kiện mạng trên card eth0 của container Broker (mục 4 của kế hoạch).
# Mỗi lần chạy đều lấy lại PID, vì restart container làm PID đổi và mất netem.
#   Cách dùng: ./scripts/netem.sh N0|N1|N2|N3|show
#     N0: bình thường   N1: trễ 100 ms   N2: mất 5%   N3: trễ 100 ms + mất 5%
set -euo pipefail

COND="${1:-}"
PID=$(docker inspect -f '{{.State.Pid}}' broker)
if [ "$PID" = "0" ]; then
  echo "Container broker chưa chạy" >&2
  exit 1
fi
NS=(sudo nsenter -t "$PID" -n)

case "$COND" in
  N0) "${NS[@]}" tc qdisc del dev eth0 root 2>/dev/null || true ;;
  N1) "${NS[@]}" tc qdisc replace dev eth0 root netem delay 100ms ;;
  N2) "${NS[@]}" tc qdisc replace dev eth0 root netem loss 5% ;;
  N3) "${NS[@]}" tc qdisc replace dev eth0 root netem delay 100ms loss 5% ;;
  show) ;;
  *)
    echo "Cách dùng: $0 N0|N1|N2|N3|show" >&2
    exit 2
    ;;
esac

"${NS[@]}" tc qdisc show dev eth0
