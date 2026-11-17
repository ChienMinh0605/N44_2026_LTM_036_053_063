# Phan B - cac tool C++ doc lap

add_executable(b_analyze analyze.cpp)
target_compile_features(b_analyze PRIVATE cxx_std_17)

# Test phan tinh toan cua analyze.cpp bang CSV nho: ctest --test-dir build
add_executable(analyze_test test/analyze_test.cpp)
target_compile_features(analyze_test PRIVATE cxx_std_17)
foreach(c percentile receive_rate duplicates ignore_unmatched cutoff qos_mismatch duplicate_sender bad_csv csv_parsing)
  add_test(NAME analyze.${c} COMMAND analyze_test ${c})
endforeach()

if(UNIX AND NOT APPLE)
  add_executable(b_experiment_runner experiment_runner.cpp)
  target_compile_features(b_experiment_runner PRIVATE cxx_std_17)
endif()
