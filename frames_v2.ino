#include <Adafruit_NeoPixel.h>
#include <ArduinoOTA.h>
#include <BlynkSimpleEsp8266.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <math.h>
#include <SPI.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>

// add to .gitignore, holds BLYNK token.
#include "creds.h"

// ESP things
#define PIN D2
#define NUM_PIXELS 200

// Blynk things
#define BLYNK_PRINT Serial

// LED things
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_PIXELS, PIN, NEO_GRB + NEO_KHZ800);

double brightness = 1.0;  // global brightness off all colors
uint32_t sync_offset;     // used to sync multiple frames

struct color {
  double speed;
  int waveln;
  int phase;
  int pwr;
} red, grn, blu;

// sets red speed
BLYNK_WRITE(V0) {
  red.speed = (param[0].asDouble() * 256.0) / 500.0;
}
// sets green speed
BLYNK_WRITE(V1) {
  grn.speed = (param[0].asDouble() * 256.0) / 500.0;
}
// sets blue speed
BLYNK_WRITE(V2) {
  blu.speed = (param[0].asDouble() * 256.0) / 500.0;
}

// sets red wave length
BLYNK_WRITE(V3) {
  red.waveln = 256.0 / param[0].asInt();
}
// sets green frequency
BLYNK_WRITE(V4) {
  grn.waveln = 256.0 / param[0].asInt();
}
// sets blue frequency
BLYNK_WRITE(V5) {
  blu.waveln = 256.0 / param[0].asInt();
}

// sets red power
BLYNK_WRITE(V6) {
  red.pwr = param[0].asInt();
}
// sets green power
BLYNK_WRITE(V7) {
  grn.pwr = param[0].asInt();
}
// sets blue power
BLYNK_WRITE(V8) {
  blu.pwr = param[0].asInt();
}

// Sync devices
BLYNK_WRITE(V9) {
  if (param[0].asInt()) sync_offset = millis();
}

// Set brightness
BLYNK_WRITE(V10) {
  brightness = param[0].asDouble();
}

// 8bit sine wave approx
byte cos8(int x) {
  return (cos((x/127.5) * M_PI) * 127.5) + 127.5;
}

void setup() {
  Serial.begin(115200);

  Serial.println("Booting...");

  // LEDS
  pixels.begin();
  pixels.show();

  // WiFi
  WiFiManager wifiManager;
  wifiManager.autoConnect("Frame-AP");

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
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Blynk things
  String wifi_ssid = WiFi.SSID();
  String wifi_pass = WiFi.psk();
  char blynk_ssid[wifi_ssid.length()];
  char blynk_pass[wifi_pass.length()];
  wifi_ssid.toCharArray(blynk_ssid, wifi_ssid.length());
  wifi_pass.toCharArray(blynk_pass, wifi_pass.length());
  Blynk.begin(TOKEN, blynk_ssid, blynk_pass);

  // Sync settings to Blynk App except power so it turns on black.
  Blynk.syncAll();
  red.pwr = 0;
  grn.pwr = 0;
  blu.pwr = 0;

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
}
