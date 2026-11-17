#!/usr/bin/env bash
# Bắt gói MQTT cho phần Wireshark của báo cáo, lưu vào data/pcap/. Cần sudo (tcpdump, netem).
#   1_qos1.pcap      CONNECT/CONNACK, SUBSCRIBE/SUBACK, PUBLISH QoS 1 + PUBACK, DISCONNECT
#   2_qos0.pcap      PUBLISH QoS 0: không có PUBACK (để so với 1)
#   3_mat_goi_N2.pcap  netem mất 5 % (N2), QoS 1: TCP Dup ACK và truyền lại
#   4_last_will.pcap Gateway bị kill -9: Broker phát Last Will "offline" tới Logger
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
cleanup() {
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

sudo -v
scene 1_qos1.pcap 1 3 INT
scene 2_qos0.pcap 0 3 INT
./scripts/netem.sh N2 | tail -1
scene 3_mat_goi_N2.pcap 1 30 INT
./scripts/netem.sh N0 >/dev/null
scene 4_last_will.pcap 1 3 KILL
ls -la "$OUT"
