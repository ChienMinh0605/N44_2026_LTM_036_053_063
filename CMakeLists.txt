cmake_minimum_required(VERSION 3.16)
project(px4_mqtt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_GATEWAY "Build Gateway" ON)

find_package(nlohmann_json 3 REQUIRED)
find_package(PahoMqttCpp REQUIRED)

# Tên target của Paho C++ khác nhau giữa các phiên bản.
if(TARGET PahoMqttCpp::paho-mqttpp3)
  set(PAHO_CPP_LIB PahoMqttCpp::paho-mqttpp3)
elseif(TARGET PahoMqttCpp::paho-mqttpp3-shared)
  set(PAHO_CPP_LIB PahoMqttCpp::paho-mqttpp3-shared)
elseif(TARGET PahoMqttCpp::paho-mqttpp3-static)
  set(PAHO_CPP_LIB PahoMqttCpp::paho-mqttpp3-static)
else()
  message(FATAL_ERROR "Không tìm thấy target của Paho MQTT C++")
endif()

add_subdirectory(common)
add_subdirectory(logger)

if(BUILD_GATEWAY)
  # MAVSDK không bắt buộc: máy không cài vẫn build được Gateway, chỉ thiếu --source px4
  # (vẫn chạy được --source fake, đủ cho mọi thí nghiệm).
  find_package(MAVSDK QUIET)
  if(MAVSDK_FOUND)
    message(STATUS "Tìm thấy MAVSDK: Gateway hỗ trợ --source px4")
  else()
    message(STATUS "Không có MAVSDK: Gateway chỉ hỗ trợ --source fake")
  endif()
  add_subdirectory(gateway)
endif()

# Cong cu phan B (khong sua Gateway / Logger)
add_subdirectory(scripts)
