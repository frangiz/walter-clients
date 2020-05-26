#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <time.h>
#include <PubSubClient.h>

// https://arduinojson.org/
#include "ArduinoJson.h"

#include "config.h"

//https://github.com/adafruit/DHT-sensor-library
#include "DHT.h"

#define VERSION "5"
#define DEV_TYPE "wemos_d1_mini"
#define DEEP_SLEEP_TIME 10*60*1e6 // 10 mins

#define MQTT_VERSION MQTT_VERSION_3_1_1

uint8_t DHTPIN = D4;
uint8_t DHTVcc = D6;
int DHTTYPE = DHT22;

DHT dht(DHTPIN, DHTTYPE);
WiFiClient wifiClient;
PubSubClient client(wifiClient);

int retries = 0;

void publishData(float temp, float humidity) {
  StaticJsonDocument<200> doc;
  doc["temperature"] = (String)temp;
  doc["humidity"] = (String)humidity;
  doc["firmware_version"] = VERSION;
  doc["retries"] = (String)retries;

  String json = "";
  serializeJson(doc, json);
  String topic = "sensor/" +deviceId();

  Serial.println("Sending to MQTT:");
  Serial.println("Topic: " +topic);
  Serial.println(json);

  boolean result = client.publish(topic.c_str(), json.c_str(), true);
  Serial.println(result);
  yield();
}

void setup() {
  Serial.begin(115200);
  pinMode(DHTVcc, OUTPUT);
  digitalWrite(DHTVcc, HIGH);
  delay(1000);
  dht.begin();
  
  Serial.println();
  Serial.println();
  connectToWiFi();
  checkForFirmwareUpdates();

  retries = 0;
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  Serial.println("Reading temp...");
  float temp = dht.readTemperature();
  delay(10);
  float humidity = dht.readHumidity();
  if (isnan(temp) || isnan(humidity)) {
    Serial.println("Is NaN, restarting DHT.");
    dhtRestart();
    retries++;
    return;
  }

  publishData(temp, humidity);

  Serial.println("Closing the MQTT connection.");
  client.disconnect();

  Serial.println("Closing the WiFi connection.");
  WiFi.disconnect();

  Serial.println("going to sleep");
  ESP.deepSleep(DEEP_SLEEP_TIME, WAKE_RF_DEFAULT);
}

String deviceId() {
  String id = String(ESP.getChipId());
  id.toLowerCase();
  return id;
}

void connectToWiFi() {
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
  Serial.println();

  // Init the MQTT connection
  client.setServer(MQTT_SERVER_IP, MQTT_SERVER_PORT);
  client.setCallback(callback);
  Serial.println();
}

void dhtRestart() {
  digitalWrite(DHTVcc, LOW);
  digitalWrite(DHTPIN, LOW);
  delay(1000);
  digitalWrite(DHTVcc, HIGH);
  digitalWrite(DHTPIN, HIGH);
  delay(1000);
}

// function called when a MQTT message arrived
void callback(char* topic, byte* payload, unsigned int length) {
}

void reconnect() {
  while(!client.connected()) {
    Serial.println("Connecting to MQTT broker.");
    if (client.connect(deviceId().c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("Connected");
    } else {
      Serial.print("Error: failed, rc=");
      Serial.print(client.state());
      Serial.print("Trying again in 5 seconds.");
      delay(5000);
    }
  }
}

String removeChar(String str, char charToRemove) {
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

void checkForFirmwareUpdates() {
  String url = String(WALTER_SERVER);
  url.concat("/api/firmware/updates");
  url.concat("?ver=" + String(VERSION));
  url.concat("&dev_type=" + String(DEV_TYPE));
  url.concat("&dev_id=" + deviceId());

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
