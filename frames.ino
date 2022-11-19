#include <Adafruit_NeoPixel.h>
#include <ArduinoOTA.h>
#include <BlynkSimpleEsp8266.h>
#include <math.h>
#include <WiFiManager.h>

// add to .gitignore, holds BLYNK token.
#include "creds.h"

// ESP things
#define NUM_PIXELS 200

// Blynk things
#define BLYNK_PRINT Serial
#ifndef TOKEN
  #error "define TOKEN in creds.h"
#endif
// LED things
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_PIXELS, D2, NEO_GRB + NEO_KHZ800);

double brightness = 1.0;  // global brightness off all colors
uint32_t sync_offset;     // used to sync multiple frames

#define TIMEOUT_MS  (4 * 60 * 60 * 1000)  // 4 hours in ms
unsigned long timer_last_change;

struct color {
  double speed;
  int waveln;
  int phase;
  int pwr;
} red, grn, blu;

// sets red speed
BLYNK_WRITE(V0) {
  red.speed = (param[0].asDouble() * 256.0) / 500.0;
  timer_last_change = millis();
}
// sets green speed
BLYNK_WRITE(V1) {
  grn.speed = (param[0].asDouble() * 256.0) / 500.0;
  timer_last_change = millis();
}
// sets blue speed
BLYNK_WRITE(V2) {
  blu.speed = (param[0].asDouble() * 256.0) / 500.0;
  timer_last_change = millis();
}

// sets red wave length
BLYNK_WRITE(V3) {
  red.waveln = 256.0 / param[0].asInt();
  timer_last_change = millis();
}
// sets green frequency
BLYNK_WRITE(V4) {
  grn.waveln = 256.0 / param[0].asInt();
  timer_last_change = millis();
}
// sets blue frequency
BLYNK_WRITE(V5) {
  blu.waveln = 256.0 / param[0].asInt();
  timer_last_change = millis();
}

// sets red power
BLYNK_WRITE(V6) {
  red.pwr = param[0].asInt();
  timer_last_change = millis();
}
// sets green power
BLYNK_WRITE(V7) {
  grn.pwr = param[0].asInt();
  timer_last_change = millis();
}
// sets blue power
BLYNK_WRITE(V8) {
  blu.pwr = param[0].asInt();
  timer_last_change = millis();
}

// Sync devices
BLYNK_WRITE(V9) {
  if (param[0].asInt()) sync_offset = millis();
  timer_last_change = millis();
}

// Set brightness
BLYNK_WRITE(V10) {
  brightness = param[0].asDouble();
  timer_last_change = millis();
}

// Syncs all settings with app
void blynk_sync_all() {
  Blynk.syncVirtual(V0);
  Blynk.syncVirtual(V1);
  Blynk.syncVirtual(V2);
  Blynk.syncVirtual(V3);
  Blynk.syncVirtual(V4);
  Blynk.syncVirtual(V5);
  Blynk.syncVirtual(V6);
  Blynk.syncVirtual(V7);
  Blynk.syncVirtual(V8);
  Blynk.syncVirtual(V9);
  Blynk.syncVirtual(V10);
}

// Turns of power to all colors
void blynk_pwr_off() {
  Blynk.virtualWrite(V6,0);
  Blynk.virtualWrite(V7,0);
  Blynk.virtualWrite(V8,0);
  red.pwr = 0;
  grn.pwr = 0;
  blu.pwr = 0;
}

// 8bit sine wave approx
byte cos8(int x) {
  return (cos((x/127.5) * M_PI) * 127.5) + 127.5;
}

void setup() {
  Serial.begin(115200);

  Serial.println("Booting...");
  uint32_t ESP_ID = ESP.getChipId();
  Serial.print("ESP8266 ID:"); Serial.println(ESP_ID);

  // LEDS
  pixels.begin();

  // WiFi
  WiFiManager wifiManager;
  //wifiManager.resetSettings();
  wifiManager.setTimeout(180);  // 3min timeout

  // Create unique SSID
  char buf[16];
  sprintf(buf, "Frame-AP-%u", ESP_ID);

  // Connect to WiFi, create AP if fails, reset if timeout
  if(!wifiManager.autoConnect(buf)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    while(1);
  }

  // OTA things
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Blynk things
  Serial.println("Connecting to Blynk...");
  String wifi_ssid = WiFi.SSID();
  String wifi_pass = WiFi.psk();
  char blynk_ssid[wifi_ssid.length()];
  char blynk_pass[wifi_pass.length()];
  wifi_ssid.toCharArray(blynk_ssid, wifi_ssid.length());
  wifi_pass.toCharArray(blynk_pass, wifi_pass.length());
  Blynk.begin(TOKEN, blynk_ssid, blynk_pass);

  // Sync wave settings from Blynk App
  blynk_sync_all();
  timer_last_change = millis();
  // Wait 100ms for sync to finish (prevents displaying a partial sync)
  while (millis()- timer_last_change < 100) {
    Blynk.run();
  }

  Serial.println("Starting main loop...");
}

void loop() {
  ArduinoOTA.handle();
  uint32_t t = millis() - sync_offset;
  for (int i = 0; i < NUM_PIXELS; i++) {
    uint32_t r = brightness * (red.pwr * cos8((uint32_t ((red.speed * t) + (i*red.waveln))) % 256));
    uint32_t g = brightness * (grn.pwr * cos8((uint32_t ((grn.speed * t) + (i*grn.waveln))) % 256));
    uint32_t b = brightness * (blu.pwr * cos8((uint32_t ((blu.speed * t) + (i*blu.waveln))) % 256));
    pixels.setPixelColor(i, pixels.Color(r,g,b));
  }
  pixels.show();
  Blynk.run();

  if (millis() - timer_last_change > TIMEOUT_MS) {
    blynk_pwr_off();
    timer_last_change = millis();
  }
}
