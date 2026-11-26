#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// --- Konfigurasi WiFi ---
const char* WIFI_SSID = "NAMA_WIFI_ANDA";
const char* WIFI_PASSWORD = "PASSWORD_WIFI_ANDA";

// --- Konfigurasi MQTT (EMQX Public Broker) ---
const char* MQTT_SERVER = "broker.emqx.io";
const int MQTT_PORT = 1883;
const char* TOPIC_DHT22 = "data/dht22";
const char* TOPIC_DISTANCE = "data/distance";

// --- Konfigurasi Pin Sensor ---
#define DHTPIN 4
#define DHTTYPE DHT22
#define TRIG_PIN 5
#define ECHO_PIN 18

// --- Interval Pengiriman (ms) ---
const unsigned long PUBLISH_INTERVAL = 5000;
unsigned long lastPublishTime = 0;

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

void setupWiFi() {
  delay(100);
  Serial.print("Menghubungkan ke WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi terhubung!");
  Serial.print("Alamat IP: ");
  Serial.println(WiFi.localIP());
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Menghubungkan ke broker.emqx.io...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println(" terhubung!");
    } else {
      Serial.print(" gagal, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" coba lagi dalam 3 detik");
      delay(3000);
    }
  }
}

float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) {
    return -1.0;
  }

  return (duration * 0.0343f) / 2.0f;
}

void publishSensorData() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  float distance = readDistanceCm();

  // Kirim plain text hasil bacaan DHT22: suhu,kelembapan
  if (!isnan(humidity) && !isnan(temperature)) {
    char dhtText[32];
    snprintf(dhtText, sizeof(dhtText), "%.2f,%.2f", temperature, humidity);

    bool okDht = mqttClient.publish(TOPIC_DHT22, dhtText);
    Serial.print("Publish [");
    Serial.print(TOPIC_DHT22);
    Serial.print("]: ");
    Serial.print(dhtText);
    Serial.println(okDht ? " (OK)" : " (GAGAL)");
  } else {
    Serial.println("Gagal membaca sensor DHT22!");
  }

  // Kirim plain text hasil bacaan ultrasonik: jarak (cm)
  if (distance >= 0) {
    char distText[16];
    snprintf(distText, sizeof(distText), "%.2f", distance);

    bool okDist = mqttClient.publish(TOPIC_DISTANCE, distText);
    Serial.print("Publish [");
    Serial.print(TOPIC_DISTANCE);
    Serial.print("]: ");
    Serial.print(distText);
    Serial.println(okDist ? " (OK)" : " (GAGAL)");
  } else {
    Serial.println("Pembacaan ultrasonik di luar jangkauan atau timeout.");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  dht.begin();
  setupWiFi();

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    setupWiFi();
  }

  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastPublishTime >= PUBLISH_INTERVAL) {
    lastPublishTime = now;
    publishSensorData();
  }
}
