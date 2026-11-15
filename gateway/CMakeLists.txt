add_executable(gateway
  src/main.cpp
  src/fake_source.cpp
  src/px4_source.cpp
  src/command_handler.cpp
)
target_link_libraries(gateway PRIVATE px4mqtt_common)

# Có MAVSDK thì bật nguồn PX4 thật (--source px4).
# MAVSDK v2 trở lên gộp mọi plugin vào một target MAVSDK::mavsdk.
if(MAVSDK_FOUND)
  target_compile_definitions(gateway PRIVATE HAVE_MAVSDK)
  target_link_libraries(gateway PRIVATE MAVSDK::mavsdk)
endif()

# Test kênh lệnh, không cần Broker: ctest --test-dir build
add_executable(command_handler_test test/command_handler_test.cpp src/command_handler.cpp src/fake_source.cpp)
target_include_directories(command_handler_test PRIVATE src)
target_link_libraries(command_handler_test PRIVATE px4mqtt_common)
add_test(NAME command_handler COMMAND command_handler_test)
