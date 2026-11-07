cmake_minimum_required(VERSION 3.16)
project(px4_mqtt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Máy không cài MAVSDK (không chạy PX4) thì build với -DBUILD_GATEWAY=OFF,
# vẫn build được common và logger.
option(BUILD_GATEWAY "Build Gateway (cần MAVSDK)" ON)

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
  find_package(MAVSDK REQUIRED)
  add_subdirectory(gateway)
endif()
