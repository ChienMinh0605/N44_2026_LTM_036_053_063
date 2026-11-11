# Phan B - cac tool C++ doc lap
add_executable(b_fake_publisher fake_publisher.cpp)
target_link_libraries(b_fake_publisher PRIVATE px4mqtt_common)

add_executable(b_analyze analyze.cpp)
target_compile_features(b_analyze PRIVATE cxx_std_17)

if(UNIX AND NOT APPLE)
  add_executable(b_experiment_runner experiment_runner.cpp)
  target_compile_features(b_experiment_runner PRIVATE cxx_std_17)
endif()
