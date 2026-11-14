// Các hằng số theo mục 2 và 3 của kế hoạch. Cả nhóm dùng chung file này,
// muốn đổi giá trị nào thì sửa ở đây (qua pull request), không viết cứng chỗ khác.
#pragma once

namespace px4mqtt {

// Broker (mục 2)
inline constexpr const char* kBrokerUri = "tcp://172.28.0.10:1883";
inline constexpr const char* kMqttUser = "drone";
inline constexpr const char* kMqttPassword = "drone123";  // trùng với scripts/setup_broker.sh

// Client id cố định
inline constexpr const char* kGatewayClientId = "gateway-01";
inline constexpr const char* kLoggerClientId = "logger-01";

// Topic (mục 2)
inline constexpr const char* kDroneId = "001";
inline constexpr const char* kTopicPosition = "drone/001/position";
inline constexpr const char* kTopicAltitude = "drone/001/altitude";
inline constexpr const char* kTopicVelocity = "drone/001/velocity";
inline constexpr const char* kTopicBattery = "drone/001/battery";
inline constexpr const char* kTopicStatus = "drone/001/status";
inline constexpr const char* kTopicAll = "drone/001/#";

// Tần số phát: 4 topic telemetry, mỗi topic 5 lần/giây = 20 tin/giây
inline constexpr int kPublishPeriodMs = 200;

// Kết nối và bộ đệm (mục 3a)
inline constexpr int kKeepAliveSec = 10;
inline constexpr int kReconnectMinSec = 1;
inline constexpr int kReconnectMaxSec = 8;
inline constexpr int kMaxBufferedMessages = 1000;

// Kênh lệnh điều khiển (docs/command_channel.md)
//   drone/001/cmd      : chiều xuống, người điều khiển -> Gateway
//   drone/001/cmd/ack  : chiều lên, Gateway trả kết quả từng lệnh
inline constexpr const char* kTopicCmd = "drone/001/cmd";
inline constexpr const char* kTopicCmdAck = "drone/001/cmd/ack";
// Lệnh không ghi ttl_ms thì dùng hạn này. Quá hạn thì Gateway từ chối, để lệnh
// bị Broker giữ lại trong lúc mất kết nối không bị thực hiện muộn.
inline constexpr int kCmdDefaultTtlMs = 3000;
// Quyền điều khiển: người gửi lệnh đầu tiên giữ quyền; không gửi gì trong khoảng này
// thì quyền được nhả cho người khác (giống "proof of life" của PX4 Offboard).
inline constexpr int kCmdLeaseMs = 5000;

// MAVSDK bản mới dùng udpin://, bản cũ dùng udp://:14540
inline constexpr const char* kMavlinkUrl = "udpin://0.0.0.0:14540";

}  // namespace px4mqtt
