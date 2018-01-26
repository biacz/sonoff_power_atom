#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "Secrets.h"

#define ROOM "office"
#define NAME "silberne_lampe"

const char* cmndTopic1 = "house/" ROOM "/" NAME "/set";
const char* cmndTopic2 = "house/group/power";
const char* statusTopic = "house/" ROOM "/" NAME "/status";

#define BUTTON_PIN 0
#define RELAY_PIN 12
#define LED_PIN 13
#define TRIGGER_PIN 14

volatile int desiredRelayState = 0;
volatile int relayState = 0;

WiFiClient wifiClient;
PubSubClient MQTTClient(wifiClient);
bool printedWifiToSerial = false;

void wifiSetup() {
  Serial.print("[WIFI] INFO: Devicename is ");
  Serial.println(NAME);
  Serial.print("[WIFI] INFO: Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("[WIFI] INFO: WiFi connected");
  Serial.print("[WIFI] INFO: IP address: ");
  Serial.println(WiFi.localIP());
}

void otaSetup() {
  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setHostname(NAME);
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.begin();
  Serial.println("[OTA] initialized!");
}

void mqttReconnect() {
  Serial.println("[MQTT] Check");
  if (MQTTClient.connected()) {
    Serial.println("[MQTT] OK");
  }
  else {
    if (WiFi.status() == WL_CONNECTED) {
      if (MQTTClient.connect(NAME, MQTT_USER, MQTT_PASS)) {
        Serial.println("[MQTT] Connected!");
        MQTTClient.subscribe(cmndTopic1);
        Serial.print("[MQTT] Subscribing to: ");
        Serial.println(cmndTopic1);
        MQTTClient.subscribe(cmndTopic2);
        Serial.print("[MQTT] Subscribing to: ");
        Serial.println(cmndTopic2);
      } else {
        Serial.print("[MQTT] failed, rc=");
        Serial.println(MQTTClient.state());
      }
    }
    else {
      Serial.println("[MQTT] Not connected to WiFI AP, abandoned connect.");
    }
  }
  if (MQTTClient.connected()) {
    digitalWrite(LED_PIN, HIGH);
  }
  else {
    digitalWrite(LED_PIN, LOW);
  }
}

void MQTTcallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("[MQTT] Message arrived [");
  Serial.print(topic);
  Serial.println("] ");

  if (!strcmp(topic, cmndTopic1) || !strcmp(topic, cmndTopic2)) {
    if ((char)payload[0] == '1' || ! strncasecmp_P((char *)payload, "ON", length)) {
      desiredRelayState = 1;
    }
    else if ((char)payload[0] == '0' || ! strncasecmp_P((char *)payload, "OFF", length)) {
      desiredRelayState = 0;
    }
    else if ( ! strncasecmp_P((char *)payload, "toggle", length)) {
      desiredRelayState = !desiredRelayState;
    }
  }
}

void shortPress() {
  desiredRelayState = !desiredRelayState;
}

void buttonChangeCallback() {
  if (digitalRead(0) == 1) {
    shortPress();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonChangeCallback, CHANGE);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  wifiSetup();
  otaSetup();

  MQTTClient.setServer(MQTT_SERVER, MQTT_PORT);
  MQTTClient.setCallback(MQTTcallback);
}

void loop() {
  yield();
  ArduinoOTA.handle();
  if(!MQTTClient.connected()) {
    mqttReconnect();
  }
  MQTTClient.loop();

  if (relayState != desiredRelayState) {
    digitalWrite(RELAY_PIN, desiredRelayState);
    relayState = desiredRelayState;

    Serial.print("[MQTT] Publish ");
    Serial.print(relayState);
    Serial.print(" to ");
    Serial.println(statusTopic);

    MQTTClient.publish(statusTopic, relayState == 0 ? "OFF" : "ON");
  }
}
