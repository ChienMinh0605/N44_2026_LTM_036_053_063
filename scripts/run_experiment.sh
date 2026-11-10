#!/usr/bin/env bash
# Chạy một lần đo của thí nghiệm E (Nhật viết tiếp).
#   Cách dùng: ./scripts/run_experiment.sh <N0|N1|N2|N3> <qos 0|1> <run>
set -euo pipefail
cd "$(dirname "$0")/.."

SCENARIO="${1:?Thiếu điều kiện mạng N0..N3}"
QOS="${2:?Thiếu QoS 0|1}"
RUN="${3:?Thiếu số thứ tự lần chạy}"
OUT="data/E/${SCENARIO}_qos${QOS}_run${RUN}"
mkdir -p "$OUT"

# TODO theo đúng thứ tự ở mục 4 của kế hoạch:
#   1. ./scripts/netem.sh "$SCENARIO"           (script tự lấy lại PID)
#   2. ping -c 50 172.28.0.10 > "$OUT/ping.txt"  rồi kiểm tra RTT / tỷ lệ mất
#   3. ./build/logger/logger --clean             (xóa phiên cũ của Logger)
#   4. chạy Logger nền: --qos "$QOS" --scenario "$SCENARIO" --run "$RUN" --out "$OUT"
#   5. chạy Gateway 3 phút: timeout -s INT 180 ./build/gateway/gateway --qos "$QOS" ...
#   6. đợi thêm 15 giây rồi gửi SIGINT cho Logger
#   7. ./scripts/netem.sh N0

echo "TODO: chưa viết xong, xem ghi chú trong file" >&2
exit 1
