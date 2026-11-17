# Thư viện dùng chung cho Gateway và Logger:
#   message   - định dạng JSON của bản tin
#   mqtt_link - kết nối MQTT, bộ đệm, Last Will, tự kết nối lại, events.csv
add_library(px4mqtt_common STATIC
  src/message.cpp
  src/mqtt_link.cpp
)
target_include_directories(px4mqtt_common PUBLIC include)
target_link_libraries(px4mqtt_common PUBLIC nlohmann_json::nlohmann_json ${PAHO_CPP_LIB})

# Test không cần Broker hay PX4: ctest --test-dir build
add_executable(message_test test/message_test.cpp)
target_link_libraries(message_test PRIVATE px4mqtt_common)
foreach(c roundtrip keys malformed missing_field wrong_type status)
  add_test(NAME message.${c} COMMAND message_test ${c})
endforeach()

add_executable(options_test test/options_test.cpp)
target_include_directories(options_test PRIVATE include)
foreach(c defaults all_flags on_off last_wins)
  add_test(NAME options.${c} COMMAND options_test ${c})
endforeach()
# Tham số sai: parse_options phải thoát với mã khác 0
add_test(NAME options.bad_qos COMMAND options_test parse --qos 2)
add_test(NAME options.unknown_flag COMMAND options_test parse --qso 1)
add_test(NAME options.missing_value COMMAND options_test parse --run)
set_tests_properties(options.bad_qos options.unknown_flag options.missing_value PROPERTIES WILL_FAIL TRUE)
