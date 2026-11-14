#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHTesp.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <cmath>
#include "secrets.h"

const int DHT_PIN = 15;
const int LED_PIN = 2;
const unsigned long READ_INTERVAL = 10000;
const unsigned long MQTT_RETRY_INTERVAL = 3000;

const char* API_URL = "http://172.183.49.30/readings";
const char* BROKER = "135.232.203.144";
const char* STATUS_TOPIC = "ecse3038/alliyah/status";
const char* SETTINGS_TOPIC = "ecse3038/alliyah/fan/settings";
const char* STATE_TOPIC = "ecse3038/alliyah/fan/state";

DHTesp dht;
WiFiClient mqttNetwork;
PubSubClient mqtt(mqttNetwork);

String mac;
String fanMode = "off";
float maxHumidity = 0;
float lastHumidity = NAN;
bool fanOn = false;
bool statePending = false;
unsigned long lastRead = 0;
unsigned long lastMqttAttempt = 0;

void updateFan() {
    bool shouldBeOn = false;

    if (fanMode == "on") {
        shouldBeOn = true;
    } else if (fanMode == "auto" && std::isfinite(lastHumidity)) {
        shouldBeOn = lastHumidity > maxHumidity;
    }

    if (shouldBeOn != fanOn) {
        fanOn = shouldBeOn;
        digitalWrite(LED_PIN, fanOn ? HIGH : LOW);
        statePending = true;
        Serial.println(fanOn ? "Fan: on" : "Fan: off");
    }
}

void receiveSettings(char* topic, byte* payload, unsigned int length) {
    if (strcmp(topic, SETTINGS_TOPIC) != 0) return;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error || !doc["mode"].is<const char*>() ||
        !doc["max_humidity"].is<float>()) {
        Serial.println("Ignored invalid fan settings");
        return;
    }

    String newMode = doc["mode"].as<String>();
    float newLimit = doc["max_humidity"].as<float>();

    if ((newMode != "auto" && newMode != "on" && newMode != "off") ||
        !std::isfinite(newLimit) || newLimit < 0 || newLimit > 100) {
        Serial.println("Ignored invalid fan settings");
        return;
    }

    // Apply both fields only after the whole message passes validation.
    fanMode = newMode;
    maxHumidity = newLimit;
    Serial.printf("Settings: %s, limit %.1f%%\n", fanMode.c_str(), maxHumidity);
    updateFan();
}

void connectMqtt() {
    String clientId = "bathroom-fan-" + mac;

    if (mqtt.connect(clientId.c_str(), STATUS_TOPIC, 1, true, "offline")) {
        mqtt.publish(STATUS_TOPIC, "online", true);
        mqtt.subscribe(SETTINGS_TOPIC);
        Serial.println("MQTT connected");
    } else {
        Serial.printf("MQTT connection failed: %d\n", mqtt.state());
    }
}

void sendReading() {
    float humidity = dht.getHumidity();
    if (!std::isfinite(humidity)) {
        Serial.println("DHT22 humidity reading failed");
        return;
    }

    lastHumidity = humidity;
    updateFan();

    WiFiClient apiNetwork;
    HTTPClient http;
    if (!http.begin(apiNetwork, API_URL)) {
        Serial.println("Could not start HTTP request");
        return;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-Key", DEVICE_KEY);

    String body = "{\"mac\":\"" + mac + "\",\"humidity\":" +
                  String(humidity, 1) + "}";
    int status = http.POST(body);
    Serial.printf("Humidity: %.1f%% | POST /readings: %d\n", humidity, status);
    http.end();
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    dht.setup(DHT_PIN, DHTesp::DHT22);

    WiFi.mode(WIFI_STA);
    WiFi.begin("Wokwi-GUEST", "", 6);
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
    }

    mac = WiFi.macAddress();
    Serial.println("Board MAC: " + mac);

    mqtt.setServer(BROKER, 1883);
    mqtt.setCallback(receiveSettings);
    mqtt.setKeepAlive(15);
    connectMqtt();
    lastMqttAttempt = millis();
}

void loop() {
    unsigned long now = millis();

    if (!mqtt.connected() && now - lastMqttAttempt >= MQTT_RETRY_INTERVAL) {
        lastMqttAttempt = now;
        connectMqtt();
    }
    mqtt.loop();

    if (now - lastRead >= READ_INTERVAL) {
        lastRead = now;
        sendReading();
    }

    if (statePending && mqtt.connected()) {
        if (mqtt.publish(STATE_TOPIC, fanOn ? "on" : "off", true)) {
            statePending = false;
        }
    }
}