# Thư viện dùng chung cho Gateway và Logger:
#   message   - định dạng JSON của bản tin
#   mqtt_link - kết nối MQTT, bộ đệm, Last Will, tự kết nối lại, events.csv
add_library(px4mqtt_common STATIC
  src/message.cpp
  src/mqtt_link.cpp
)
target_include_directories(px4mqtt_common PUBLIC include)
target_link_libraries(px4mqtt_common PUBLIC nlohmann_json::nlohmann_json ${PAHO_CPP_LIB})
