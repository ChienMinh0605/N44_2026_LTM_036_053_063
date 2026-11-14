add_executable(commander src/main.cpp)
target_link_libraries(commander PRIVATE px4mqtt_common)
