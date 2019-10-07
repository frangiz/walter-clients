#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <time.h>

// https://arduinojson.org/
#include "ArduinoJson.h"

#include "config.h"

//https://github.com/adafruit/DHT-sensor-library
#include "DHT.h"

uint8_t DHTPIN = D4;
uint8_t DHTVcc = D6;
int DHTTYPE = DHT22;

DHT dht(DHTPIN, DHTTYPE);


void setup() {
  Serial.begin(115200);
  //pinMode(A0, INPUT);
  pinMode(DHTVcc, OUTPUT);
  digitalWrite(DHTVcc, HIGH);
  delay(1000);
  dht.begin();
  
  Serial.println();
  Serial.println();
  connect_to_wifi();
  sync_ntp();
}

void loop() {
  Serial.println("Reading temp...");
  //float temp = calc_temp(analogRead(A0));
  float temp = dht.readTemperature();
  delay(10);
  float humidity = dht.readHumidity();
  if (isnan(temp)) {
    Serial.println("Is NaN, restarting DHT.");
    dht_restart();
    return;
  }
  Serial.println(temp);
  Serial.println(humidity);
  send_temp(temp);
  send_humidity(humidity);
  Serial.println("going to sleep");
  ESP.deepSleep(10*60*1e6, WAKE_RF_DEFAULT);
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
  unsigned timeout = 5000;
  unsigned start = millis();
  while (millis() - start < timeout) {
    Serial.print(".");
    time_t now = time(nullptr);
    if (now > (2016 - 1970) * 365 * 24 * 3600) {
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
  time_t now = time(nullptr);
  HTTPClient http;
  http.begin(SERVER + "/api/temperature");
  http.addHeader("Content-Type", "application/json");
  StaticJsonDocument<200> doc;
  doc["timestamp"] = now;
  doc["sensor"] = WiFi.macAddress() + "-0";
  doc["value"] = temp;
  doc["next_update"] = 12 * 60;
  String json = "";
  serializeJson(doc, json);
  Serial.println("------------------------------");
  Serial.println("Sending to server: " + json);
  int httpCode = http.POST(json);
  String payload = http.getString();
  Serial.println("Server replied with http code " + String(httpCode) + " and payload:");
  Serial.println(payload);
  http.end();
}

void send_humidity(float humidity) {
  time_t now = time(nullptr);
  HTTPClient http;
  http.begin(SERVER + "/api/humidity");
  http.addHeader("Content-Type", "application/json");
  StaticJsonDocument<200> doc;
  doc["timestamp"] = now;
  doc["sensor"] = WiFi.macAddress() + "-1";
  doc["value"] = humidity;
  doc["next_update"] = 12 * 60;
  String json = "";
  serializeJson(doc, json);
  Serial.println("------------------------------");
  Serial.println("Sending to server: " + json);
  int httpCode = http.POST(json);
  String payload = http.getString();
  Serial.println("Server replied with http code " + String(httpCode) + " and payload:");
  Serial.println(payload);
  http.end();
}

// TMP36 specific
// Converting from 10mv per degree with 500mV offset
// to degrees ((voltage - 500mV) times 100)
float calc_temp(int reading) {
    Serial.print("Reading: ");
    Serial.println(reading);
    float voltage = reading * 3.1;  
    voltage /= 1024.0;
    return (voltage - 0.5) * 100;
}

void dht_restart() {
  digitalWrite(DHTVcc, LOW);
  digitalWrite(DHTPIN, LOW);
  delay(1000);
  digitalWrite(DHTVcc, HIGH);
  digitalWrite(DHTPIN, HIGH);
  delay(1000);
}
