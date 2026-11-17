cmake_minimum_required(VERSION 3.16)
project(px4_mqtt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

enable_testing()

add_subdirectory(common)
add_subdirectory(logger)
add_subdirectory(commander)  # gửi lệnh điều khiển, đo khứ hồi (thí nghiệm H)

# Gateway đọc PX4 qua MAVSDK (đã build với MAVSDK 3.17.5)
find_package(MAVSDK REQUIRED)
add_subdirectory(gateway)

# Cong cu phan B (khong sua Gateway / Logger)
add_subdirectory(scripts)
