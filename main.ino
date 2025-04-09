#define ssidnum 1
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_chip_info.h"
#include <HTTPClient.h>
//......................................
const char* ssid = "KWIC-DEMO-ROOM";
const char* password = "1234Qwer";

// Firebase Functions 배포 후 받은 URL
const char* firebaseUrl = "https://devicehandler-dwjubrnesq-uc.a.run.app";

// 버튼 핀 설정 (예: GPIO0)

// Potentiometer is connected to GPIO 34 (Analog ADC1_CH6) 
const uint16_t potPin = 34;
// variable for storing the potentiometer value
uint16_t potValue = 0, lastpotValue = 0;

// Beacon Packet buffer
uint8_t packet[128] = { 
  0x80, 0x00,             // Frame Control
  0x00, 0x00,             // Duration
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,   // Destination address
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06,   // Source address
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06,   // BSSID
  0x00, 0x00,             // Sequence Control
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Timestamp
  0x64, 0x00,             // Beacon Interval
  0x31, 0x04,             // Capability info
  0x00                    // SSID Parameter
};

char ssids[ssidnum][32] = {
  "ESP32_Beacon"
};

bool broadcasting = false;

void beacon_mode() {
  WiFi.mode(WIFI_MODE_AP);
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_AP);
  esp_wifi_start();
}

void button_mode() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected");
}

void setup() {
  Serial.begin(115200);

  beacon_mode();
  delay(100);
  button_mode();
}

void loop() {
  broadcasting = true;

  potValue = (analogRead(potPin))/10; // adc를 통해 스위치 구분

  if (potValue>30 && lastpotValue<20) { // potValue-1st button:409, 2nd button : 228, 3rd button : 113, 4th button : 36
    Serial.println("🔘 Button pressed!");

    // MAC 주소 얻기
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.print("MAC: ");
    Serial.println(macStr);

    // Firebase Function 호출
    if (WiFi.status() == WL_CONNECTED) {
      String actions = ""; // 버튼 4 : 예비 버튼
      if (potValue>350) actions = "register"; // 버튼 1 기기 저장
      else if (potValue>150) actions = "delete"; // 버튼 2 기기 삭제
      else if (potValue>50) actions = "danger"; // 버튼 3 위험 신호
      HTTPClient http;
      http.begin(firebaseUrl);
      http.addHeader("Content-Type", "application/json");
      
      String body = "{\"action\":\"" + actions + "\",\"mac\":\"" + String(macStr) + "\",\"deviceId\":\"esp32-001\"}";
      Serial.println(body);
      int httpResponseCode = http.POST(body);

      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      Serial.println(http.getString());

      http.end();
    }

    delay(300); // 디바운스 대기
  }

  lastpotValue = potValue;
  
  // broadcasting wifi
  if (broadcasting) {
    for(int i = 0; i < ssidnum; i++) {
      uint8_t mac[6];
      esp_efuse_mac_get_default(mac);
      
      packet[10] = packet[16] = mac[0];
      packet[11] = packet[17] = mac[1];
      packet[12] = packet[18] = mac[2];
      packet[13] = packet[19] = mac[3];
      packet[14] = packet[20] = mac[4];
      packet[15] = packet[21] = mac[5];

      // Set SSID length
      int ssidLen = strlen(ssids[i]);
      packet[37] = ssidLen;
      
      // Set SSID
      for(int j = 0; j < ssidLen; j++) {
        packet[38 + j] = ssids[i][j];
      }
      
      // Send packet
      esp_wifi_80211_tx(WIFI_IF_AP, packet, sizeof(packet), false);
      delay(1);
    }
  }
}