add_executable(gateway
  src/main.cpp
)
# MAVSDK v2 trở lên gộp mọi plugin vào một target MAVSDK::mavsdk.
target_link_libraries(gateway PRIVATE px4mqtt_common ${PAHO_CPP_LIB} MAVSDK::mavsdk)
