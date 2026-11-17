#!/usr/bin/env bash
# Chạy thí nghiệm C và F theo kế hoạch (mục "Chiến – Logger"). Không cần sudo.
#
#   ./scripts/run_CF.sh c     # C: docker stop Broker 15 s giữa lượt; QoS 1; bộ đệm Gateway off/on × 3 lần
#   ./scripts/run_CF.sh f     # F: dừng Logger 15 s rồi chạy lại cùng client_id; QoS 0/1 × 3 lần
#   ./scripts/run_CF.sh all
#
# Mỗi lượt: xoá phiên Logger → Logger chạy trước 3 s → Gateway phát DUR giây (sự cố ở giây
# OFF_AT, kéo dài OFF giây) → dừng Gateway → Logger nhận thêm 15 s. Thứ tự hai mức đảo
# giữa các lần như thí nghiệm E. Kết quả: $ROOT/C/buffer-<on|off>_run<n>/, $ROOT/F/qos<0|1>_run<n>/
# Biến môi trường: RUNS=3, DUR=180, OFF_AT=60, OFF=15, ROOT=data, FORCE=1 (chạy lại thư mục đã có).
# Tổng hợp: ./scripts/summarize_CF.sh
set -euo pipefail
cd "$(dirname "$0")/.."

GW=./build/gateway/gateway
LG=./build/logger/logger
RUNS="${RUNS:-3}" DUR="${DUR:-180}" OFF_AT="${OFF_AT:-60}" OFF="${OFF:-15}" ROOT="${ROOT:-data}"
MQTT=(-h 172.28.0.10 -u drone -P drone123)

[[ -x $GW && -x $LG ]] || { echo "Build trước: cmake -B build && cmake --build build -j" >&2; exit 1; }

GW_PID="" LG_PID=""
stop() {  # stop <pid>: Ctrl+C rồi chờ thoát hẳn
  [[ -n $1 ]] || return 0
  kill -INT "$1" 2>/dev/null || true
  wait "$1" 2>/dev/null || true
}
start_logger() {  # start_logger <thư mục> <qos> <scenario> <run>
  "$LG" --qos "$2" --scenario "$3" --run "$4" --out "$1" >>"$1/logger.log" 2>&1 &
  LG_PID=$!
}
wait_broker() {
  for _ in $(seq 1 50); do
    mosquitto_pub "${MQTT[@]}" -t drone/001/probe -n -q 0 2>/dev/null && return 0
    sleep 0.2
  done
  echo "Broker không lên lại" >&2; return 1
}
cleanup() {
  stop "$GW_PID"; stop "$LG_PID"
  docker start broker >/dev/null 2>&1 || true  # bị ngắt giữa lúc Broker đang tắt
}
trap cleanup EXIT INT TERM

prepare() {  # prepare <thư mục>: trả về 1 nếu lượt này đã có dữ liệu
  local d="$1"
  if [[ -s $d/logger.csv && ${FORCE:-0} != 1 ]]; then
    echo "  bỏ qua $d (đã có logger.csv, đặt FORCE=1 để chạy lại)"; return 1
  fi
  rm -rf "$d"; mkdir -p "$d"
  wait_broker
  ./scripts/hover.sh >"$d/hover.txt" 2>&1 || { cat "$d/hover.txt" >&2; exit 1; }  # đo khi drone hover
  "$LG" --clean >"$d/clean.log" 2>&1  # Logger không nhận lại tin của lượt trước
}

# Một lượt: <thư mục> <qos> <buffer on|off> <scenario> <run> <hàm gây sự cố> <hàm khôi phục>
one_run() {
  local d="$1" qos="$2" buf="$3" sc="$4" run="$5" fail="$6" recover="$7"
  start_logger "$d" "$qos" "$sc" "$run"
  sleep 3
  "$GW" --qos "$qos" --buffer "$buf" --scenario "$sc" --run "$run" --out "$d" \
    >>"$d/gateway.log" 2>&1 &
  GW_PID=$!
  local t0=$SECONDS
  sleep "$OFF_AT"
  date +%s%3N >"$d/off_ms.txt"; "$fail" "$d" "$qos" "$sc" "$run"
  sleep "$OFF"
  "$recover" "$d" "$qos" "$sc" "$run"; date +%s%3N >"$d/on_ms.txt"
  sleep $((DUR - (SECONDS - t0) > 0 ? DUR - (SECONDS - t0) : 0))
  stop "$GW_PID"; GW_PID=""
  sleep 15
  stop "$LG_PID"; LG_PID=""
  printf 'scenario=%s\nqos=%s\nbuffer=%s\nrun=%s\npublish_sec=%s\noff_at_sec=%s\noff_sec=%s\n' \
    "$sc" "$qos" "$buf" "$run" "$DUR" "$OFF_AT" "$OFF" >"$d/scenario.txt"
  tail -1 "$d/gateway.log"
}

broker_down() { docker stop broker >/dev/null; }
broker_up() { docker start broker >/dev/null; wait_broker; }
logger_down() { stop "$LG_PID"; LG_PID=""; }
logger_up() { start_logger "$@"; }

c() {
  for run in $(seq 1 "$RUNS"); do
    if (( run % 2 == 1 )); then order=(off on); else order=(on off); fi
    for buf in "${order[@]}"; do
      local d="$ROOT/C/buffer-${buf}_run${run}"
      echo "== C $d"
      prepare "$d" || continue
      one_run "$d" 1 "$buf" "C-buffer-$buf" "$run" broker_down broker_up
    done
  done
}

f() {
  for run in $(seq 1 "$RUNS"); do
    if (( run % 2 == 1 )); then order=(0 1); else order=(1 0); fi
    for qos in "${order[@]}"; do
      local d="$ROOT/F/qos${qos}_run${run}"
      echo "== F $d"
      prepare "$d" || continue
      one_run "$d" "$qos" on "F-qos$qos" "$run" logger_down logger_up
    done
  done
}

case "${1:-}" in
  c) c ;;
  f) f ;;
  all) c; f ;;
  *) sed -n 2,11p "$0"; exit 2 ;;
esac
