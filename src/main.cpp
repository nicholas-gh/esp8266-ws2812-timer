
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <restclient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <fauxmoESP.h>

#include "settings.h"

extern "C" {
 #include "user_interface.h"
}

#define BUTTON_PIN 0
#define PIXEL_PIN 2
#define PIXEL_COUNT 41
#define TIMER_LENGTH 5*60*1000

restclient hue(HUE_IP,HUE_PORT);

fauxmoESP fauxmo;

const char LIGHTS_ON[] = "{\"on\":true}";
const char LIGHTS_OFF[] = "{\"on\":false}";
const char EFFECT_COLORLOOP[] = "{\"effect\":\"colorloop\"}";
const char EFFECT_NONE[] = "{\"effect\":\"none\"}";

bool running = false;
bool finished = false;
int litPixels = 0;

os_timer_t myTimer;
bool tickOccured;
void timerCallback(void *pArg) {
      tickOccured = true;
}

void user_init(void) {
  os_timer_setfn(&myTimer, timerCallback, NULL);
  os_timer_arm(&myTimer, (TIMER_LENGTH)/(PIXEL_COUNT), true);
}

Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ400);


void blinkLED(int dlay, int count = 1)
{
  for (int c=0; c < count; c++) {
    delay(dlay);
    digitalWrite(BUILTIN_LED, HIGH);
    delay(dlay);
    digitalWrite(BUILTIN_LED, LOW);
  }
}

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  strip.begin();
  //strip.setPixelColor(1, strip.Color(200, 0, 0));
  strip.show(); // Initialize all pixels to 'off'
  tickOccured = false;
  user_init();
  // Set WiFi mode to station (client)
  WiFi.mode(WIFI_STA);

  // Initiate connection with SSID and PSK
  WiFi.hostname("bustimer");
  WiFi.begin(WIFI_SSID, WIFI_PSK);

  // Blink LED while we wait for WiFi connection
  while (WiFi.status() != WL_CONNECTED) blinkLED(100);
  if (WiFi.status() != WL_CONNECTED) while(1) blinkLED(1000);

  // Serial.begin(115200);
  ArduinoOTA.onStart([]() {
    String type;

    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
// all serial parts commented out, since the LED shares the TX pin
//    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
//    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
  //  Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
/*    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
*/
  });
  ArduinoOTA.begin();


  fauxmo.addDevice("bus lights");
  fauxmo.onMessage([](unsigned char device_id, const char * device_name, bool state) {
      if (state) {
        running = true;
      } else {
        running = false;
        for (int i = 0; i < PIXEL_COUNT; i++) {
          strip.setPixelColor(i, strip.Color(0, 0, 0));
        }
        litPixels = 0;
        strip.show();
      }
    });

}

void loop() {
  ArduinoOTA.handle();
  fauxmo.handle();

  int val = digitalRead(BUTTON_PIN);
  digitalWrite(BUILTIN_LED, val);
  if (running) {
    if (val == false) {
      running = false;
      for (int i = 0; i < PIXEL_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 0));
      }
      litPixels = 0;
      strip.show();
      delay(500);
      return;
    } else {
      if (tickOccured == true) {
        tickOccured = false;
        if (litPixels < PIXEL_COUNT) {
         finished = false;
	 int red = (int)((float)200-(((float)litPixels / ((float)PIXEL_COUNT))*(float)200));
	 int green = (int)(((float)litPixels / (((float)PIXEL_COUNT)))*(float)255);
	 int blue = (int)(((float)litPixels / (((float)PIXEL_COUNT)))*(float)20);
	 /*
	 Serial.print(red);
	 Serial.print(" ");
	 Serial.print(green);
	 Serial.print(" ");
	 Serial.print(blue);
	 Serial.print("\n");
	 */
	 strip.setPixelColor(litPixels, strip.Color(red,
                                                    green,
                                                    blue));
	 //         strip.setPixelColor(litPixels, strip.Color(200-((litPixels / ((PIXEL_COUNT)*1.0))*100),
         //                                           (litPixels / ((PIXEL_COUNT)*1.0))*255,
         //                                           0));
         strip.show();
	 if (litPixels == 30) {
	   hue.put("/api/" HUE_USERNAME "/groups/0/action", LIGHTS_ON);
	   hue.put("/api/" HUE_USERNAME "/groups/0/action", EFFECT_COLORLOOP);
	 }
         litPixels++;
       } else {
         if (finished == false) {
          strip.setPixelColor(0, strip.Color(0, 0, 50));
          strip.show();
          hue.put("/api/" HUE_USERNAME "/groups/0/action", LIGHTS_OFF);  // Turn off all lights
          finished = true;
        }
       }
      }
    }
  } else {
    if (val == false) {
      running = true;
      litPixels = 0;
      strip.setPixelColor(1, strip.Color(255, 255, 255));
      strip.setPixelColor(PIXEL_COUNT-1, strip.Color(0, 0, 50));
      strip.show();
      delay(500);
      return;
    };
  }
  yield();
}
