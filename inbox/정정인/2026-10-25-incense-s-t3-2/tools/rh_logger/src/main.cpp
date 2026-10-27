// 키트 온습도 센서를 1 s마다 읽어 「ms,type,temp_c,rh」 한 줄씩 USB로 보낸다.
// 칩 표시가 없어 DHT11로 먼저 읽고, 5번 연속 실패하면 DHT22로 바꿔 본다(DHT22는 2 s 간격).
// 배선: 모듈 S(데이터) → D2, +(가운데) → 5V, - → GND
#include <Arduino.h>
#include <DHT.h>

constexpr uint8_t kPin = 2;
DHT dht11(kPin, DHT11);
DHT dht22(kPin, DHT22);
DHT* dht = &dht11;
const char* type_name = "DHT11";
unsigned long interval_ms = 1000;
int fails = 0;

void setup() {
  Serial.begin(115200);
  dht11.begin();
  dht22.begin();
  Serial.println(F("# ms,type,temp_c,rh"));
}

void loop() {
  static unsigned long last = 0;
  const unsigned long now = millis();
  if (now - last < interval_ms) return;
  last = now;
  const float rh = dht->readHumidity(true);  // true = 강제로 새로 읽기
  const float t = dht->readTemperature();
  if (isnan(rh) || isnan(t)) {
    Serial.print(now);
    Serial.print(',');
    Serial.print(type_name);
    Serial.println(F(",nan,nan"));
    if (++fails >= 5 && dht == &dht11) {  // DHT11로 안 읽히면 DHT22로
      dht = &dht22;
      type_name = "DHT22";
      interval_ms = 2000;
      fails = 0;
    }
    return;
  }
  fails = 0;
  Serial.print(now);
  Serial.print(',');
  Serial.print(type_name);
  Serial.print(',');
  Serial.print(t, 1);
  Serial.print(',');
  Serial.println(rh, 1);
}
