cmake_minimum_required(VERSION 3.14)
# 판정 시험 — 하드웨어 없이 PC·CI에서 (system.yml safety_core_policy job).
# policy.cpp + profile 로더 + 시험만 빌드한다. 센서·UART·TCP·D-Bus 코드는 들어오지 않는다.
project(safetycore_policy_test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(policy_test policy_test.cpp ../src/policy.cpp ../src/profile_loader.cpp)
target_compile_options(policy_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_include_directories(policy_test PRIVATE ../src)
target_include_directories(policy_test SYSTEM PRIVATE ../third_party)
# 시험은 저장소의 profiles/default.json을 실제 로더로 읽어 그 값으로 돈다 → 시험 코드에 임계 숫자를 쓰지 않는다.
target_compile_definitions(policy_test PRIVATE
  HOODSEEK_DEFAULT_PROFILE="${CMAKE_CURRENT_SOURCE_DIR}/../../profiles/default.json"
  HOODSEEK_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")

enable_testing()
add_test(NAME policy_test COMMAND policy_test)
