#!/usr/bin/env bash
# Dựng Broker Mosquitto trong Docker với IP cố định 172.28.0.10 (mục 4 của kế hoạch)
# và tạo tài khoản MQTT chung. Chạy lại nhiều lần cũng không sao.
#   Cách dùng: ./scripts/setup_broker.sh
set -euo pipefail
cd "$(dirname "$0")/.."

MQTT_USER="drone"
MQTT_PASS="drone123"   # trùng với common/include/common/config.hpp

if ! docker network inspect mqttnet >/dev/null 2>&1; then
  docker network create --subnet 172.28.0.0/24 mqttnet
fi

# Mosquitto cần file mật khẩu tồn tại (có thể rỗng) để khởi động.
[ -e mosquitto/config/passwd ] || touch mosquitto/config/passwd

if docker container inspect broker >/dev/null 2>&1; then
  docker start broker >/dev/null
else
  docker run -d --name broker --restart unless-stopped --network mqttnet --ip 172.28.0.10 -p 1883:1883 \
    -v "$PWD/mosquitto/config:/mosquitto/config" \
    -v "$PWD/mosquitto/data:/mosquitto/data" \
    eclipse-mosquitto:2 >/dev/null
fi

# Tự bật lại Broker khi khởi động lại máy. "unless-stopped": nếu chính mình
# docker stop (thí nghiệm C) thì Broker vẫn tắt cho tới khi docker start.
docker update --restart unless-stopped broker >/dev/null

# Tạo/cập nhật tài khoản bên trong container, trả quyền file cho user mosquitto
# rồi khởi động lại để Broker đọc file mật khẩu mới.
docker exec broker mosquitto_passwd -b /mosquitto/config/passwd "$MQTT_USER" "$MQTT_PASS"
docker exec broker chown mosquitto:mosquitto /mosquitto/config/passwd
docker exec broker chmod 0600 /mosquitto/config/passwd
docker restart broker >/dev/null

echo "Broker đang chạy ở 172.28.0.10:1883, tài khoản: $MQTT_USER"
echo "Thử nhận tin:  docker exec broker mosquitto_sub -u $MQTT_USER -P $MQTT_PASS -v -t 'drone/#'"
echo "Thử gửi tin:   docker exec broker mosquitto_pub -u $MQTT_USER -P $MQTT_PASS -t drone/001/test -m hello"
echo "Lưu ý: restart container làm mất netem, chạy lại scripts/netem.sh nếu đang thí nghiệm."
