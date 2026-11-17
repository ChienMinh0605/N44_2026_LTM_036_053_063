#!/usr/bin/env bash
# Bảo đảm drone PX4 SITL đang bay giữ độ cao (hover) trước một lượt đo: đề bài yêu cầu đo khi hover.
# Bật Gateway tạm, đọc độ cao tương đối trên drone/001/altitude. Nếu thấp hơn MIN_ALT thì gửi lệnh
# takeoff qua MQTT (đường lệnh thật: commander -> Broker -> Gateway -> MAVSDK) và chờ tới ALT.
# Gọi khi KHÔNG có Gateway nào khác đang chạy (cùng client_id gateway-01 sẽ đá nhau).
#   ./scripts/hover.sh          # ALT=10 MIN_ALT=8 (mét)
# Cần PX4 SITL đang chạy: cd ~/PX4-Autopilot && make px4_sitl gz_x500
# và đặt một lần trong pxh: param set COM_DLL_EXCEPT 2 (PX4 tự lưu). Airframe gz_x500 đặt NAV_DLL_ACT 2:
# mất GCS quá 10 s là Return rồi hạ cánh, mà giữa hai lượt đo Gateway (GCS) có tắt. Giá trị 2 chỉ bỏ qua
# failsafe này khi drone ở chế độ Hold (hover, goto); các chế độ khác vẫn có failsafe.
set -euo pipefail
cd "$(dirname "$0")/.."

ALT="${ALT:-10}" MIN_ALT="${MIN_ALT:-8}"
MQTT=(-h 172.28.0.10 -u drone -P drone123)
TMP=$(mktemp -d)
./build/gateway/gateway --out "$TMP" >"$TMP/gateway.log" 2>&1 &
GW=$!
trap 'kill -INT $GW 2>/dev/null || true; wait $GW 2>/dev/null || true; rm -rf "$TMP"' EXIT

# Độ cao tương đối (m) từ tin altitude kế tiếp. Gateway chờ PX4 tối đa 10 s nên cho 15 s.
alt() {
  mosquitto_sub "${MQTT[@]}" -t drone/001/altitude -C 1 -W 15 | sed -nE 's/.*"rel_m":([-0-9.eE+]+).*/\1/p'
}
ge() { awk -v a="$1" -v b="$2" 'BEGIN { exit !(a >= b) }'; }

a=$(alt || true)
if [[ -z $a ]]; then
  echo "hover: không đọc được độ cao (PX4 chưa chạy?)" >&2
  tail -3 "$TMP/gateway.log" >&2
  exit 1
fi
if ge "$a" "$MIN_ALT"; then
  echo "hover: đang bay ở $a m"
  exit 0
fi

echo "hover: độ cao $a m, gửi lệnh takeoff $ALT m"
./build/commander/commander --action takeoff --alt "$ALT" --wait-ms 5000 || true
for _ in $(seq 1 120); do  # tối đa khoảng 60 s
  a=$(alt || true)
  if [[ -n $a ]] && ge "$a" "$(awk -v x="$ALT" 'BEGIN { print x - 0.5 }')"; then
    sleep 3  # để drone đứng yên hẳn ở độ cao giữ
    echo "hover: đã lên $(alt) m"
    exit 0
  fi
  sleep 0.5
done
echo "hover: cất cánh không lên tới $ALT m (độ cao cuối: ${a:-?} m)" >&2
exit 1
