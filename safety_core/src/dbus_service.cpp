#include "dbus_service.h"

#include <chrono>
#include <cstdio>
#include <thread>

#include <sdbus-c++/sdbus-c++.h>

namespace {

void sleep_unless_stop(const std::atomic<bool>& stop, int ms) {
  for (int waited = 0; waited < ms && !stop; waited += 200) std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

}  // namespace

void run_dbus(const std::atomic<bool>& stop, const std::function<std::string()>& get_state) {
  while (!stop) {
    try {
      auto connection = sdbus::createSystemBusConnection(sdbus::ServiceName{kBusName});
      auto object = sdbus::createObject(*connection, sdbus::ObjectPath{"/com/atlas/HoodSeekSafetyCore1"});
      object
          ->addVTable(sdbus::registerMethod("GetState").withOutputParamNames("state").implementedAs([&get_state]() {
            return get_state();
          }))
          .forInterface(kBusName);
      connection->enterEventLoopAsync();
      std::printf("[dbus] %s 소유\n", kBusName);
      while (!stop) std::this_thread::sleep_for(std::chrono::milliseconds(200));
      connection->leaveEventLoop();
      return;
    } catch (const std::exception& e) {
      std::fprintf(stderr, "[dbus] 버스 이름 소유 실패, 5 s 뒤 재시도 (안전 루프는 계속): %s\n", e.what());
      sleep_unless_stop(stop, 5000);
    }
  }
}
