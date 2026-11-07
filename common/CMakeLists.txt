# Thư viện dùng chung cho Gateway và Logger.
# Phần MQTT chung (kết nối, bộ đệm, Last Will...) bạn A viết thêm vào đây.
add_library(px4mqtt_common STATIC
  src/message.cpp
)
target_include_directories(px4mqtt_common PUBLIC include)
target_link_libraries(px4mqtt_common PUBLIC nlohmann_json::nlohmann_json)
