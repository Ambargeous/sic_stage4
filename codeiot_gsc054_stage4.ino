#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "UbidotsEsp32Mqtt.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C

const char *UBIDOTS_TOKEN = "BBUS-KmrMzdP8sReCgMznESwEMQMSrWuzv4";
const char *WIFI_SSID = "Abrisam Lubis";
const char *WIFI_PASS = "PakeAja.";
const char *DEVICE_LABEL = "smart_class_system";
const char *VARIABLE_LABEL = "heart_rate";
const char *TEMP_VARIABLE_LABEL = "temperature";
const char *SISWA_VARIABLE_LABEL = "siswa";
const int PUBLISH_FREQUENCY = 5000;

unsigned long timer;

Ubidots ubidots(UBIDOTS_TOKEN);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
MAX30105 particleSensor;

const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;

float beatsPerMinute;
int beatAvg;
float currentTemperature;
int siswa = 1;

// Tombol AI Gemini
const int button1 = 34;
const int button2 = 35;
const int button3 = 32;

void callback(char *topic, byte *payload, unsigned int length) {
  // No relay control used in this project
}

void sendToMongo(float bpm, float temp, int siswa) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://192.168.0.43:5000/data");
    http.addHeader("Content-Type", "application/json");

    String json = "{\"heart_rate\": " + String(bpm, 2) +
                  ", \"temperature\": " + String(temp, 2) +
                  ", \"siswa\": " + String(siswa) + "}";

    int httpCode = http.POST(json);
    String payload = http.getString();

    if (httpCode > 0) {
      Serial.println("[MongoDB] Data terkirim: " + payload);
    } else {
      Serial.println("[MongoDB] Gagal mengirim data. Kode HTTP: " + String(httpCode));
    }

    http.end();
  } else {
    Serial.println("[MongoDB] WiFi tidak terhubung. Data tidak dikirim.");
  }
}


void sendToGemini(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://192.168.0.43:5000/ask_gemini";
    Serial.println("[Gemini] Mengirim permintaan ke: " + url);
    Serial.println("[Gemini] Pesan: " + message);

    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String json = "{\"question\": \"" + message + "\"}";
    Serial.println("[Gemini] Payload: " + json);

    int httpCode = http.POST(json);
    String payload = http.getString();

    if (httpCode > 0) {
      Serial.println("[Gemini] Kode HTTP: " + String(httpCode));
      Serial.println("[Gemini] Respon: " + payload);
    } else {
      Serial.println("[Gemini] Gagal mengirim ke Gemini. Kode HTTP: " + String(httpCode));
    }
    http.end();
  } else {
    Serial.println("[Gemini] Tidak terhubung ke WiFi.");
  }
}

void setup() {
  Serial.begin(115200);
  ubidots.connectToWifi(WIFI_SSID, WIFI_PASS);
  ubidots.setCallback(callback);
  ubidots.setup();
  ubidots.reconnect();
  timer = millis();

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30105 was not found. Please check wiring/power.");
    while (1);
  }

  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeGreen(0);
  particleSensor.enableDIETEMPRDY();

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();

  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);
  pinMode(button3, INPUT_PULLUP);
}

void loop() {
  long irValue = particleSensor.getIR();
  currentTemperature = particleSensor.readTemperature();

  if (checkForBeat(irValue) == true) {
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20) {
      rates[rateSpot++] = (byte)beatsPerMinute;
      rateSpot %= RATE_SIZE;

      beatAvg = calculateAverage(rates, RATE_SIZE);
      updateDisplay(beatsPerMinute, beatAvg, currentTemperature);
    //  animateHeartbeat();

      if ((millis() - timer) > PUBLISH_FREQUENCY) {
        ubidots.add(VARIABLE_LABEL, beatsPerMinute);
        ubidots.add(TEMP_VARIABLE_LABEL, currentTemperature);
        ubidots.add(SISWA_VARIABLE_LABEL, siswa);
        ubidots.publish(DEVICE_LABEL);

        sendToMongo(beatsPerMinute, currentTemperature, siswa);

        timer = millis();
      }
    }
  }

  if (!ubidots.connected()) {
    ubidots.reconnect();
  }
  ubidots.loop();

  if (digitalRead(button1) == LOW) {
    sendToGemini("siswa stress apakah ada solusi yang bisa dilakukan ringan di kelas");
    delay(1000);
  }
  if (digitalRead(button2) == LOW) {
    sendToGemini("siswa demam apakah ada solusi yang bisa dilakukan ringan di kelas");
    delay(1000);
  }
  if (digitalRead(button3) == LOW) {
    sendToGemini("siswa kurang tidur apakah ada solusi yang bisa dilakukan ringan di kelas");
    delay(1000);
  }
}

int calculateAverage(byte *arrayToAverage, int arraySize) {
  int sum = 0;
  for (int i = 0; i < arraySize; i++) {
    sum += arrayToAverage[i];
  }
  return sum / arraySize;
}

void updateDisplay(float bpm, int avgBpm, float tempC) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  //display.printf("BPM: %.1f", bpm);

  //display.setCursor(0, 15);
  display.printf("BPM: %d", avgBpm);

  display.setCursor(0, 30);
  display.printf("Temp: %.1f", int(tempC));

  display.display();
}
