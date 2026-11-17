add_executable(gateway
  src/main.cpp
  src/px4_source.cpp
  src/command_handler.cpp
)
# MAVSDK v2 trở lên gộp mọi plugin vào một target MAVSDK::mavsdk.
target_link_libraries(gateway PRIVATE px4mqtt_common MAVSDK::mavsdk)

# Test kênh lệnh, không cần Broker hay PX4: ctest --test-dir build
add_executable(command_handler_test test/command_handler_test.cpp src/command_handler.cpp)
target_include_directories(command_handler_test PRIVATE src)
target_link_libraries(command_handler_test PRIVATE px4mqtt_common)
add_test(NAME command_handler COMMAND command_handler_test)
