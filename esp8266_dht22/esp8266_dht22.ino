#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <time.h>

// https://arduinojson.org/
#include "ArduinoJson.h"

#include "config.h"

//https://github.com/adafruit/DHT-sensor-library
#include "DHT.h"

#define VERSION "3"
#define DEV_TYPE "wemos_d1_mini"
#define DEEP_SLEEP_TIME 10*60*1e6 // 10 mins
#define UPDATE_INTERVAL_SECS 42*60

uint8_t DHTPIN = D4;
uint8_t DHTVcc = D6;
int DHTTYPE = DHT22;

DHT dht(DHTPIN, DHTTYPE);

int retries = 0;
bool ntp_sync_ok = false;

void setup() {
  Serial.begin(115200);
  pinMode(DHTVcc, OUTPUT);
  digitalWrite(DHTVcc, HIGH);
  delay(1000);
  dht.begin();
  
  Serial.println();
  Serial.println();
  connect_to_wifi();
  check_for_firmware_updates();
  sync_ntp();

  retries = 0;
}

void loop() {
  if (!ntp_sync_ok || retries > 100) {
    Serial.println("going to sleep");
    ESP.deepSleep(DEEP_SLEEP_TIME, WAKE_RF_DEFAULT);
  }
  Serial.println("Reading temp...");
  float temp = dht.readTemperature();
  delay(10);
  float humidity = dht.readHumidity();
  if (isnan(temp)) {
    Serial.println("Is NaN, restarting DHT.");
    dht_restart();
    retries++;
    return;
  }
  Serial.println(temp);
  Serial.println(humidity);
  Serial.println(retries);
  send_temp(temp);
  send_humidity(humidity);
  if (retries > 0) {
    send_retries(retries, WiFi.macAddress() + "-0");
  }
  Serial.println("going to sleep");
  ESP.deepSleep(DEEP_SLEEP_TIME, WAKE_RF_DEFAULT);
}

void connect_to_wifi() {
  // Connect to WiFi network
  Serial.println("------------------------------");
  Serial.print("Connecting to ");
  Serial.println(SSID);
 
  WiFi.begin(SSID, WIFI_PASSWORD);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Print the IP address
  Serial.print("Use this URL to connect: ");
  Serial.print(WiFi.localIP());
  Serial.println("/");
  Serial.println();
}

void sync_ntp() {
  Serial.println("------------------------------");
  Serial.println("Syncing NTP");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  unsigned timeout = 2 * 60 * 1000;
  unsigned start = millis();
  while (millis() - start < timeout) {
    Serial.print(".");
    time_t now = time(nullptr);
    if (now > (2016 - 1970) * 365 * 24 * 3600) {
      ntp_sync_ok = true;
      break;
    }
    delay(100);
  }
  Serial.println();
  {
    time_t now = time(nullptr);
    Serial.print("Sync finished with current time: ");
    Serial.println(ctime(&now));
  }
}

void send_temp(float temp) {
  String url = SERVER + "/api/temperature";

  StaticJsonDocument<200> doc;
  doc["timestamp"] = time(nullptr);
  doc["sensor"] = WiFi.macAddress() + "-0";
  doc["value"] = temp;
  doc["next_update"] = UPDATE_INTERVAL_SECS;
  String json = "";
  serializeJson(doc, json);
  
  send(url, json);
}

void send_humidity(float humidity) {
  String url = SERVER + "/api/humidity";

  StaticJsonDocument<200> doc;
  doc["timestamp"] = time(nullptr);
  doc["sensor"] = WiFi.macAddress() + "-1";
  doc["value"] = humidity;
  doc["next_update"] = UPDATE_INTERVAL_SECS;
  String json = "";
  serializeJson(doc, json);

  send(url, json);
}

void send_retries(int retries, String sensorId) {
  sensorId = remove_char(sensorId, ':');
  String url = SERVER + "/api/sensors/" + sensorId + "/logs";
  
  StaticJsonDocument<200> doc;
  doc["timestamp"] = time(nullptr);
  doc["message"] = "Retries for reading temp " + String(retries);
  String json = "";
  serializeJson(doc, json);

  send(url, json);
}

void send(const String url, const String json) {
  time_t now = time(nullptr);
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  Serial.println("------------------------------");
  Serial.println("Sending to server: " + json);
  int httpCode = http.POST(json);
  String payload = http.getString();
  Serial.println("Server replied with http code " + String(httpCode) + " and payload:");
  Serial.println(payload);
  http.end();
}

void dht_restart() {
  digitalWrite(DHTVcc, LOW);
  digitalWrite(DHTPIN, LOW);
  delay(1000);
  digitalWrite(DHTVcc, HIGH);
  digitalWrite(DHTPIN, HIGH);
  delay(1000);
}

String remove_char(String str, char charToRemove) {
  char c;
  for(int i=0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == charToRemove) {
      str.remove(i, 1);
      i--; // So we stay at the same index to handle multiple concurrent occurences of the same char.
    }
  }
  return str;
}

void check_for_firmware_updates() {
  String url = String(SERVER);
  url.concat("/api/firmware/updates");
  url.concat("?ver=" + String(VERSION));
  url.concat("&dev_type=" + String(DEV_TYPE));
  url.concat("&dev_id=" + remove_char(WiFi.macAddress(), ':'));

  Serial.println("Checking for updates @ " +url);
  t_httpUpdate_return ret = ESPhttpUpdate.update(url);

  switch (ret) {
    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;
    case HTTP_UPDATE_FAILED:
    default:
      Serial.println("HTTP_UPDATE_FAILED");
      break;
  }
}
