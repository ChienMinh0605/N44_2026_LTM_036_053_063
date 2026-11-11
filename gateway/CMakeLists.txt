add_executable(gateway
  src/main.cpp
  src/fake_source.cpp
  src/px4_source.cpp
)
target_link_libraries(gateway PRIVATE px4mqtt_common)

# Có MAVSDK thì bật nguồn PX4 thật (--source px4).
# MAVSDK v2 trở lên gộp mọi plugin vào một target MAVSDK::mavsdk.
if(MAVSDK_FOUND)
  target_compile_definitions(gateway PRIVATE HAVE_MAVSDK)
  target_link_libraries(gateway PRIVATE MAVSDK::mavsdk)
endif()
