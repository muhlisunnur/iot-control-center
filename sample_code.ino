#include <WiFi.h>
#include <PubSubClient.h>

// ===================== KONFIGURASI WIFI =====================
const char* WIFI_SSID     = "YOUR_WIFI";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";

// ===================== KONFIGURASI MQTT =====================
// ESP32 menggunakan MQTT TCP port 1883.
// Dashboard menggunakan WebSocket wss://broker.emqx.io:8084/mqtt.
const char* MQTT_BROKER = "broker.emqx.io";
const int   MQTT_PORT   = 1883;

// Kosongkan jika broker tidak memerlukan autentikasi.
const char* MQTT_USERNAME = "";
const char* MQTT_PASSWORD = "";

// Topik harus sama dengan topik pada widget dashboard.
const char* TOPIC_SUHU      = "demo/iot/suhu";
const char* TOPIC_LEVEL     = "demo/iot/level";
const char* TOPIC_KELEMBAPAN = "demo/iot/kelembapan";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Nilai awal data dummy
float suhu = 27.0;
float level = 75.0;
float kelembapan = 60.0;

unsigned long lastPublish = 0;
unsigned long lastReconnectAttempt = 0;

const unsigned long PUBLISH_INTERVAL = 3000;
const unsigned long RECONNECT_INTERVAL = 5000;

void setupWiFi() {
  Serial.print("Menghubungkan ke WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi terhubung.");
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());
}

bool connectMQTT() {
  String clientId = "esp32-dummy-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  Serial.print("Menghubungkan ke MQTT sebagai ");
  Serial.println(clientId);

  bool connected;

  if (strlen(MQTT_USERNAME) > 0) {
    connected = mqttClient.connect(
      clientId.c_str(),
      MQTT_USERNAME,
      MQTT_PASSWORD
    );
  } else {
    connected = mqttClient.connect(clientId.c_str());
  }

  if (connected) {
    Serial.println("MQTT berhasil terhubung.");
  } else {
    Serial.print("Koneksi MQTT gagal, kode status: ");
    Serial.println(mqttClient.state());
  }

  return connected;
}

float updateDummyValue(float currentValue, float minimum,
                       float maximum, float maximumChange) {
  // Menghasilkan perubahan acak antara -maximumChange dan +maximumChange.
  float change = ((float)random(-100, 101) / 100.0) * maximumChange;
  float newValue = currentValue + change;

  return constrain(newValue, minimum, maximum);
}

void publishValue(const char* topic, float value) {
  char payload[16];

  // Mengirim angka sebagai teks, misalnya "27.35".
  snprintf(payload, sizeof(payload), "%.2f", value);

  bool success = mqttClient.publish(topic, payload);

  Serial.print(topic);
  Serial.print(" -> ");
  Serial.print(payload);

  if (success) {
    Serial.println(" [terkirim]");
  } else {
    Serial.println(" [gagal]");
  }
}

void publishDummyData() {
  // Data berubah sedikit setiap pengiriman agar terlihat realistis.
  suhu = updateDummyValue(suhu, 20.0, 35.0, 0.5);
  level = updateDummyValue(level, 0.0, 100.0, 2.0);
  kelembapan = updateDummyValue(kelembapan, 35.0, 90.0, 1.5);

  publishValue(TOPIC_SUHU, suhu);
  publishValue(TOPIC_LEVEL, level);
  publishValue(TOPIC_KELEMBAPAN, kelembapan);

  Serial.println("-----------------------------");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  randomSeed(esp_random());

  setupWiFi();

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setKeepAlive(60);

  connectMQTT();
}

void loop() {
  // Jika WiFi terputus, mulai ulang koneksi WiFi.
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi terputus. Menghubungkan ulang...");
    WiFi.disconnect();
    WiFi.reconnect();
    delay(2000);
    return;
  }

  // Reconnect MQTT tanpa memblokir loop terus-menerus.
  if (!mqttClient.connected()) {
    unsigned long now = millis();

    if (now - lastReconnectAttempt >= RECONNECT_INTERVAL) {
      lastReconnectAttempt = now;
      connectMQTT();
    }

    return;
  }

  // Menjaga koneksi MQTT dan memproses aktivitas jaringan.
  mqttClient.loop();

  unsigned long now = millis();

  if (now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;
    publishDummyData();
  }
}
