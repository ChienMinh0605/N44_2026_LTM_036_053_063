#!/usr/bin/env bash
# Bắt gói MQTT cho phần Wireshark của báo cáo, lưu vào data/pcap/. Cần sudo (tcpdump, netem).
#   1_qos1.pcap      CONNECT/CONNACK, SUBSCRIBE/SUBACK, PUBLISH QoS 1 + PUBACK, DISCONNECT
#   2_qos0.pcap      PUBLISH QoS 0: không có PUBACK (để so với 1)
#   3_mat_goi_N2.pcap  netem mất 5 % (N2), QoS 1: TCP Dup ACK và truyền lại
#   4_last_will.pcap Gateway bị kill -9: Broker phát Last Will "offline" tới Logger
# Hai cảnh chạy riêng (không ghi đè 4 file trên, ảnh trong báo cáo đánh số gói theo chúng):
#   ./scripts/capture_pcap.sh lenh     5_lenh_qos1_N0.pcap: 20 lệnh hold QoS 1 ở N0 (khứ hồi 42 ms: Nagle + delayed ACK?)
#   ./scripts/capture_pcap.sh nodelay  6_N1_nodelay.pcap: telemetry QoS 1 ở N1, Broker bật set_tcp_nodelay true
# Gateway đọc PX4 thật: chạy PX4 SITL trước (cd ~/PX4-Autopilot && make px4_sitl gz_x500).
# Bắt trên bridge Docker của mạng mqttnet (giữa máy và Broker), chỉ cổng 1883.
# Lưu ý: gói CONNECT chứa mật khẩu MQTT dạng chữ thường (drone/drone123, đã có sẵn trong repo).
# Chạy trong terminal thường của máy: trong tab Terminal của app Claude, AppArmor chặn tín hiệu tới tcpdump.
set -euo pipefail
cd "$(dirname "$0")/.."

OUT=data/pcap
TMP=$(mktemp -d)
IF="br-$(docker network inspect mqttnet -f '{{.Id}}' | cut -c1-12)"
GW=./build/gateway/gateway LG=./build/logger/logger
mkdir -p "$OUT"
CONF_BAK=""  # cảnh nodelay sửa tạm mosquitto.conf của Broker; trả lại khi thoát
restore_conf() {
  [[ -n $CONF_BAK ]] || return 0
  cp "$CONF_BAK" "$CONF"; CONF_BAK=""
  docker restart broker >/dev/null
}
cleanup() {
  restore_conf || true
  ./scripts/netem.sh N0 >/dev/null 2>&1 || true
  pkill -INT -x gateway 2>/dev/null || true
  pkill -INT -x logger 2>/dev/null || true
  rm -rf "$TMP"
}
trap cleanup EXIT

CAP=""
cap_start() {  # cap_start <tên file>
  sudo tcpdump -i "$IF" -U -s 0 -w "$OUT/$1" 'tcp port 1883' 2>/dev/null &
  CAP=$!
  sleep 2  # chờ tcpdump sẵn sàng, để không lỡ gói CONNECT
}
# Gửi thẳng cho tcpdump: sudo bỏ qua tín hiệu đến từ cùng nhóm tiến trình với nó, nên kill $CAP không có tác dụng.
cap_stop() { sleep 1; sudo pkill -INT -x tcpdump; wait "$CAP" || true; echo "  -> $OUT/$1"; }

# scene <file> <qos> <số giây Gateway chạy> <cách dừng Gateway: INT|KILL>
scene() {
  echo "== $1"
  $LG --clean >/dev/null 2>&1
  cap_start "$1"
  $LG --qos "$2" --out "$TMP/$1" >/dev/null 2>&1 & local lg=$!
  sleep 1
  $GW --qos "$2" --out "$TMP/$1" >/dev/null 2>&1 & local gw=$!
  sleep "$3"
  kill -"$4" $gw; wait $gw 2>/dev/null || true
  sleep 2  # đủ để Broker phát Last Will (khi KILL) tới Logger
  kill -INT $lg; wait $lg 2>/dev/null || true
  cap_stop "$1"
}

# cmd_scene <file>: Gateway + commander gửi 20 lệnh hold QoS 1, mỗi 200 ms
cmd_scene() {
  echo "== $1"
  mkdir -p "$TMP/$1"
  $GW --out "$TMP/$1" >/dev/null 2>&1 & local gw=$!
  sleep 3  # chờ Gateway nối PX4 và subscribe drone/001/cmd
  cap_start "$1"
  ./build/commander/commander --action hold --count 20 --interval-ms 200 --qos 1 --out "$TMP/$1"
  cp "$TMP/$1/cmd_rtt.csv" "$OUT/${1%.pcap}_cmd_rtt.csv"
  cap_stop "$1"
  kill -INT $gw; wait $gw 2>/dev/null || true
}

sudo -v
case "${1:-all}" in
  all)
    scene 1_qos1.pcap 1 3 INT
    scene 2_qos0.pcap 0 3 INT
    ./scripts/netem.sh N2 | tail -1
    scene 3_mat_goi_N2.pcap 1 30 INT
    ./scripts/netem.sh N0 >/dev/null
    scene 4_last_will.pcap 1 3 KILL
    ;;
  lenh)
    cmd_scene 5_lenh_qos1_N0.pcap
    ;;
  nodelay)
    CONF="$(docker inspect -f '{{range .Mounts}}{{if eq .Destination "/mosquitto/config"}}{{.Source}}{{end}}{{end}}' broker)/mosquitto.conf"
    cp "$CONF" "$TMP/mosquitto.conf.bak"; CONF_BAK="$TMP/mosquitto.conf.bak"
    echo "set_tcp_nodelay true" >>"$CONF"
    docker restart broker >/dev/null; sleep 2
    ./scripts/netem.sh N1 | tail -1  # đặt sau restart: restart làm mất netem
    scene 6_N1_nodelay.pcap 1 30 INT
    ./scripts/netem.sh N0 >/dev/null
    restore_conf
    ;;
  *) echo "Cách dùng: $0 [all|lenh|nodelay]" >&2; exit 2 ;;
esac
ls -la "$OUT"
