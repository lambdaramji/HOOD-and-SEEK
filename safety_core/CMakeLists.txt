cmake_minimum_required(VERSION 3.14)
# 프로젝트 이름 = 실행 파일 이름 = serviceinfo.json "entry" (arc 규칙: 소문자·숫자만, #46 실측)
project(safetycore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

# 기기(arc build) = ON. 호스트(Mac·CI)에는 sdbus-c++가 없으므로 -DHOODSEEK_WITH_DBUS=OFF로 빌드한다.
option(HOODSEEK_WITH_DBUS "D-Bus 버스 이름 소유·GetState, root 실행 확인" ON)

find_package(Threads REQUIRED)

# third_party/nlohmann/json.hpp — nlohmann/json 3.11.3 단일 헤더(MIT), 설계서 §5.1 「헤더 1개 vendored」
set(CORE_SOURCES
  src/csv_log.cpp
  src/sensors.cpp
  src/uart.cpp
  src/tcp_server.cpp
  src/message.cpp
  src/profile_loader.cpp
  src/policy.cpp)

# MLX90640 Melexis API (third_party/mlx90640, Apache-2.0). I2C 드라이버는 src/sensors.cpp가 Linux에서만 구현하므로
# Linux(기기·Linux 호스트)에서만 넣는다. 남의 코드라 경고는 끈다.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list(APPEND CORE_SOURCES third_party/mlx90640/MLX90640_API.cpp)
  set_source_files_properties(third_party/mlx90640/MLX90640_API.cpp PROPERTIES COMPILE_OPTIONS "-w")
endif()

add_executable(${PROJECT_NAME} src/main.cpp ${CORE_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE src third_party third_party/mlx90640)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
if(HOODSEEK_WITH_DBUS)
  find_package(sdbus-c++ 2.0 REQUIRED)
  target_sources(${PROJECT_NAME} PRIVATE src/dbus_service.cpp)
  target_compile_definitions(${PROJECT_NAME} PRIVATE HOODSEEK_WITH_DBUS)
  target_link_libraries(${PROJECT_NAME} PRIVATE SDBusCpp::sdbus-c++)
endif()

# 하드웨어 없이 PC에서 도는 시험: 수신 검증·state 직렬화·profile 로더.
# 판정 시험(policy_test)은 tests/(#57, CI safety_core_policy job)에 따로 있다.
if(NOT CMAKE_CROSSCOMPILING)
  enable_testing()
  add_executable(message_test core_tests/message_test.cpp ${CORE_SOURCES})
  target_include_directories(message_test PRIVATE src third_party third_party/mlx90640)
  target_link_libraries(message_test PRIVATE Threads::Threads)
  # 계약 v1 예제(main contracts/examples)를 Core 파서로 전수 시험 — 계약이 바뀌면 시험도 따라간다
  target_compile_definitions(message_test PRIVATE CONTRACTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../contracts")
  add_test(NAME message_test COMMAND message_test)

  # UART 링크 시험 (#56): 가상 터미널(pty) 한 쌍으로 ESP32 흉내 — 즉시 송신·재전송·ACK 일치·만료·송신 중단
  add_executable(uart_test core_tests/uart_test.cpp src/uart.cpp)
  target_include_directories(uart_test PRIVATE src third_party)
  target_link_libraries(uart_test PRIVATE Threads::Threads)
  if(NOT APPLE)
    target_link_libraries(uart_test PRIVATE util)  # openpty (glibc libutil)
  endif()
  add_test(NAME uart_test COMMAND uart_test)

  # SPS30 샘플 분류 시험 (#65 PM 팔로업 2번): 정상·포화·버림
  add_executable(sensor_test core_tests/sensor_test.cpp)
  target_include_directories(sensor_test PRIVATE src third_party)
  add_test(NAME sensor_test COMMAND sensor_test)

  # CSV 판정 로그 시험 (#88): 열 수가 머리줄과 같은지, 목록·빈 값 형식, 같은 초 재시작에 덮어쓰지 않는지
  add_executable(csv_log_test core_tests/csv_log_test.cpp src/csv_log.cpp src/policy.cpp)
  target_include_directories(csv_log_test PRIVATE src third_party)
  add_test(NAME csv_log_test COMMAND csv_log_test)
endif()
