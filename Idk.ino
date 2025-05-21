#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <DHTesp.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define DHT11_PIN 18
#define DS18B20 5
#define REPORTING_PERIOD_MS 1000
#define BUFFER_SIZE 100

float temperature, humidity, bodytemperature;
int32_t BPM, SpO2;

const char* ssid = "****";  
const char* password = "****";  

DHTesp dht;
MAX30105 particleSensor;
uint32_t tsLastReport = 0;
OneWire oneWire(DS18B20);
DallasTemperature sensors(&oneWire);

WebServer server(80);

uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];
int bufferLength = 0;

void setup() {
  server.on("/", handle_Root);

  Serial.begin(115200);
  pinMode(19, OUTPUT);
  delay(100);

  dht.setup(DHT11_PIN, DHTesp::DHT11);

  Serial.println("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Configura el endpoint JSON
  server.on("/data", handle_Data);
  server.onNotFound(handle_NotFound);
  server.begin();
  Serial.println("HTTP server started");

  if (!particleSensor.begin()) {
    Serial.println("MAX30105 was not found. Please check wiring/power.");
    while (1);
  }
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeIR(0x0A);

  sensors.begin();
}

void loop() {
  server.handleClient();

  // Leer datos de MAX30105
  bufferLength = BUFFER_SIZE;
  for (int i = 0; i < BUFFER_SIZE; i++) {
    while (!particleSensor.check()) {
      delay(1);
    }
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
  }

  int8_t spo2Valid;
  int8_t heartRateValid;
  maxim_heart_rate_and_oxygen_saturation(irBuffer, BUFFER_SIZE, redBuffer, &SpO2, &spo2Valid, &BPM, &heartRateValid);

  sensors.requestTemperatures();
  bodytemperature = sensors.getTempCByIndex(0);

  TempAndHumidity data = dht.getTempAndHumidity();
  temperature = data.temperature;
  humidity = data.humidity;

  if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
    Serial.print("Temperatura Ambiental: ");
    Serial.print(temperature);
    Serial.println(" °C");

    Serial.print("Humedad del Ambiente: ");
    Serial.print(humidity);
    Serial.println(" %");

    Serial.print("BPM: ");
    Serial.println(heartRateValid ? BPM : 0);

    Serial.print("SpO2: ");
    Serial.println(spo2Valid ? SpO2 : 0);

    Serial.print("Temperatura Corporal: ");
    Serial.print(bodytemperature);
    Serial.println(" °C");

    Serial.println("*********************************");
    tsLastReport = millis();
  }
}

void handle_Data() {
  String json = "{";
  json += "\"temperature\":" + String(temperature, 1) + ",";
  json += "\"humidity\":" + String(humidity, 1) + ",";
  json += "\"BPM\":" + String(BPM) + ",";
  json += "\"SpO2\":" + String(SpO2) + ",";
  json += "\"bodytemperature\":" + String(bodytemperature, 1);
  json += "}";
  server.send(200, "application/json", json);
}

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}
void handle_Root() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <title>Monitor de Salud ESP32</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body {
      font-family: 'Segoe UI', sans-serif;
      background-color: #7e7e7e;
      text-align: center;
      padding: 20px;
      color: #333;
    }
    h1 {
      color: rgb(255, 255, 255);
      margin-bottom: 30px;
    }
    .container {
      display: flex;
      flex-wrap: wrap;
      justify-content: center;
      gap: 20px;
    }
    .card {
      background-color: white;
      border-radius: 15px;
      padding: 20px;
      box-shadow: 5px 5px 15px rgba(0,0,0,0.9);
      width: 250px;
      color: white;
      background: #1d1d1d;
      text-align: center;
      position: relative;
    }
    .card::after, .card::before{
      --angle: 0deg;
      content:'';
      position: absolute;
      height: 100%;
      width: 100%;
      background-image: conic-gradient(#32ff03,#7aff6e,#90e7a3,#afffcd,#9ffafd,#32ff03);
      top:50%;
      left: 50%;
      translate: -50% -50%;
      z-index: -1;
      padding: 3px;
      border-radius: 25px;
      filter: blur(0.2rem);
    }
    .card::before {
      filter: blur(1.5rem);
      opacity: 0.5;
    }
    .title {
      font-size: 18px;
      margin-bottom: 10px;
      font-weight: bold;
      color: white;
    }
    .value {
      font-size: 36px;
      font-weight: 600;
    }
    .unit {
      font-size: 18px;
      color: #777;
    }
    .auxilio{
      color: white;
      text-shadow: 2px 2px 5px #32ff03;
    }
  </style>
</head>
<body>

  <h1 class="auxilio">Sistema de Monitoreo de Salud con ESP32</h1>

  <div class="container">
    <div class="card">
      <div class="title">Temperatura Ambiental</div>
      <div class="value" id="temperature">-- <span class="unit">°C</span></div>
    </div>
    <div class="card">
      <div class="title">Humedad</div>
      <div class="value" id="humidity">-- <span class="unit">%</span></div>
    </div>
    <div class="card">
      <div class="title">Ritmo Cardiaco</div>
      <div class="value" id="BPM">-- <span class="unit">BPM</span></div>
    </div>
    <div class="card">
      <div class="title">Oxígeno en Sangre</div>
      <div class="value" id="SpO2">-- <span class="unit">%</span></div>
    </div>
    <div class="card">
      <div class="title">Temperatura Corporal</div>
      <div class="value" id="bodytemperature">-- <span class="unit">°C</span></div>
    </div>
  </div>

  <script>
    const ESP32_IP = window.location.origin;

    async function getData() {
      try {
        const response = await fetch(`${ESP32_IP}/data`);
        if (!response.ok) throw new Error("Error en respuesta HTTP");

        const data = await response.json();

        document.getElementById("temperature").innerHTML = `${data.temperature} <span class="unit">°C</span>`;
        document.getElementById("humidity").innerHTML = `${data.humidity} <span class="unit">%</span>`;
        document.getElementById("BPM").innerHTML = `${data.BPM} <span class="unit">BPM</span>`;
        document.getElementById("SpO2").innerHTML = `${data.SpO2} <span class="unit">%</span>`;
        document.getElementById("bodytemperature").innerHTML = `${data.bodytemperature} <span class="unit">°C</span>`;
      } catch (error) {
        console.error("Error al obtener datos del ESP32:", error);
        alert("No se pudo obtener datos del ESP32. Verifica que esté conectado.");
      }
    }

    setInterval(getData, 2000);
    getData();
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

