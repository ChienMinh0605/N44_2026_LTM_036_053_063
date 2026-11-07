add_executable(logger
  src/main.cpp
)
target_link_libraries(logger PRIVATE px4mqtt_common ${PAHO_CPP_LIB})
