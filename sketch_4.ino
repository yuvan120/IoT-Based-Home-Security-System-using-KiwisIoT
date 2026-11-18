#include <WiFi.h>
#include <PubSubClient.h>

// Wi-Fi credentials
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

// KiwisIoT MQTT configuration
const char* mqtt_server = "YOUR_MQTT_BROKER";
const int mqtt_port = YOUR_MQTT_PORT;
const char* mqtt_topic = "YOUR_DEVICE_TOPIC";

// ESP32 pin configuration
#define PIR_PIN 27
#define DOOR_PIN 26
#define BUZZER_PIN 25
#define LED_PIN 2

WiFiClient espClient;
PubSubClient client(espClient);

bool lastDoorState = HIGH;
bool lastPirState = LOW;

bool alarmActive = false;

unsigned long alarmStart = 0;
unsigned long lastPublish = 0;

const unsigned long ALARM_DURATION = 3000;
const unsigned long PUBLISH_INTERVAL = 10000;

void connectWiFi() {
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("WiFi Connected");
}

void reconnectMQTT() {
  while (!client.connected()) {
    if (client.connect("HomeSecurityESP32")) {
      Serial.println("MQTT Connected");
    } else {
      delay(2000);
    }
  }
}

void triggerAlarm(const char* eventName) {

  Serial.print("SECURITY ALERT: ");
  Serial.println(eventName);

  alarmActive = true;
  alarmStart = millis();

  digitalWrite(BUZZER_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);

  if (client.connected()) {

    char payload[160];

    snprintf(payload, sizeof(payload),
      "{\"security_event\":\"%s\","
      "\"alarm_status\":\"ON\","
      "\"system_status\":\"ARMED\"}",
      eventName
    );

    client.publish(mqtt_topic, payload);
  }
}

void setup() {

  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(DOOR_PIN, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  connectWiFi();

  client.setServer(mqtt_server, mqtt_port);

  // Allow PIR sensor to stabilize
  Serial.println("PIR sensor warming up...");
  delay(30000);

  lastDoorState = digitalRead(DOOR_PIN);
  lastPirState = digitalRead(PIR_PIN);

  Serial.println("Home Security System Armed");
}

void loop() {

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!client.connected()) {
    reconnectMQTT();
  }

  client.loop();

  bool pirState = digitalRead(PIR_PIN);
  bool doorState = digitalRead(DOOR_PIN);

  // Detect new motion
  if (pirState == HIGH && lastPirState == LOW) {
    triggerAlarm("MOTION_DETECTED");
  }

  // Reed switch opens when door opens
  if (doorState == HIGH && lastDoorState == LOW) {
    triggerAlarm("DOOR_OPENED");
  }

  lastPirState = pirState;
  lastDoorState = doorState;

  // Stop local alarm after 3 seconds
  if (alarmActive &&
      millis() - alarmStart >= ALARM_DURATION) {

    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);

    alarmActive = false;
  }

  // Publish periodic security status
  if (millis() - lastPublish >= PUBLISH_INTERVAL) {

    lastPublish = millis();

    char payload[160];

    snprintf(payload, sizeof(payload),
      "{\"motion\":\"%s\","
      "\"door\":\"%s\","
      "\"alarm\":\"%s\","
      "\"system\":\"ARMED\"}",
      pirState ? "DETECTED" : "CLEAR",
      doorState ? "OPEN" : "CLOSED",
      alarmActive ? "ON" : "OFF"
    );

    client.publish(mqtt_topic, payload);
  }

  delay(50);
}