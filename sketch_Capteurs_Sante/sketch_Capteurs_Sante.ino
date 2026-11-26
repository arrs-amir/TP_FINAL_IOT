#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "DHT.h"
#include "DFRobot_BloodOxygen_S.h"
// ======================================
// ADRESSE I2C DU SLAVE (pour le Master)
// ======================================
#define I2C_DEV_ADDR 0x55
// ======================================
// WIFI / MQTT
// ======================================
const char *ssid = "iot";
const char *password = "iotisis;";
const char *mqtt_server = "172.18.32.40";
WiFiClient espClient;
PubSubClient client(espClient);
// ======================================
// DHT11 (pin 27)
// ======================================
#define DHTPIN 27
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
// ======================================
// MPU6050 sur I2C_2 (SDA 18, SCL 19)
// ======================================
TwoWire I2C_2 = TwoWire(1);
Adafruit_MPU6050 mpu;
// ======================================
// MAX30102 (SEN0518) sur UART Serial2 (25, 26)
// ======================================
DFRobot_BloodOxygen_S_HardWareUart MAX30102(&Serial2, 9600);
// ======================================
// CODES ERREUR
// ======================================
#define ERR_NONE 0
#define ERR_WIFI_LOST 1
#define ERR_SENSOR_DISCONNECT 2
#define ERR_SENSOR_READ 3
#define ERR_TEMP_LIMIT 4
#define ERR_LOW_VOLTAGE 5
#define ERR_SERIAL_LOST 6
#define ERR_LOW_MEMORY 7
#define ERR_I2C 8
#define ERR_WATCHDOG 9
#define ERR_UNKNOWN 10
volatile uint8_t errorCode = ERR_NONE;
// ======================================
// LED / TIMER
// ======================================
const int ledPin = 4;
const int ledVerte = 9;
const int ledJaune = 10;
int lastHeartRate = 0;
unsigned long lastBlink = 0;
const unsigned long blinkInterval = 500;
bool blinkState = false;
unsigned long lastMsg = 0;
const unsigned long interval = 4000;
// ======================================
// CALLBACK I2C SLAVE (réponse au Master)
// ======================================
void onI2CRequest() {
Wire.write(errorCode);
}
// ======================================
// WIFI
// ======================================
void setup_wifi() {
Serial.print("\nConnecting to ");
Serial.println(ssid);
WiFi.begin(ssid, password);
while (WiFi.status() != WL_CONNECTED) {
delay(500);
Serial.print(".");
}
Serial.println("\nWiFi connected");
Serial.print("IP address: ");
Serial.println(WiFi.localIP());
}
// ======================================
// MQTT
// ======================================
void callback(char *topic, byte *message, unsigned int length) {
Serial.print("Message arrived on topic: ");
Serial.println(topic);
String messageTemp;
for (unsigned int i = 0; i < length; i++) messageTemp += (char)message[i];
Serial.println(messageTemp);
}
void reconnect() {
while (!client.connected()) {
Serial.print("Attempting MQTT connection...");
if (client.connect("ESP32Client_Global")) {
Serial.println("connected");
client.subscribe("esp32/output");
} else {
Serial.print("failed, rc=");
Serial.println(client.state());
delay(5000);
}
}
}
// ======================================
// SETUP
// ======================================
void setup() {
Serial.begin(115200);
pinMode(ledPin, OUTPUT);
pinMode(ledVerte, OUTPUT);
pinMode(ledJaune, OUTPUT);
digitalWrite(ledVerte, LOW);
digitalWrite(ledJaune, LOW);
// 1. Bus I2C 1 (21, 22) : Mode SLAVE pour répondre au Master
Wire.onRequest(onI2CRequest);
Wire.begin((uint8_t)I2C_DEV_ADDR, 21, 22, 100000);
Serial.println("ESP32 Slave I2C initialise sur 0x55");
// 2. Bus I2C 2 (18, 19) : Mode MASTER pour le MPU6050 seul
I2C_2.begin(18, 19, 100000);
Serial.println("Initializing MPU6050...");
if (!mpu.begin(0x68, &I2C_2)) {
Serial.println("MPU6050 not detected!");
errorCode = ERR_SENSOR_DISCONNECT;
while (1) delay(10);
}
Serial.println("MPU6050 detected!");
mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
mpu.setGyroRange(MPU6050_RANGE_500_DEG);
mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
// 3. DHT11
dht.begin();
Serial.println("DHT11 initialized!");
// 4. MAX30102 (SEN0518) en UART sur 25 (RX) et 26 (TX)
Serial2.begin(9600, SERIAL_8N1, 25, 26);
Serial.println("Initializing MAX30102 (UART)...");
while (!MAX30102.begin()) {
Serial.println("MAX30102 init fail!");
errorCode = ERR_SENSOR_DISCONNECT;
delay(1000);
}
Serial.println("MAX30102 init success!");
MAX30102.sensorStartCollect();
// 5. WiFi + MQTT
setup_wifi();
client.setServer(mqtt_server, 1883);
client.setCallback(callback);
Serial.println("All sensors initialized!");
}
// ======================================
// LOOP
// ======================================
void loop() {
if (!client.connected()) reconnect();
client.loop();
unsigned long now = millis();
// Clignotement des LEDs selon le dernier heartRate mesure
if (now - lastBlink >= blinkInterval) {
lastBlink = now;
blinkState = !blinkState;
if (lastHeartRate >= 60 && lastHeartRate <= 90) {
digitalWrite(ledVerte, blinkState ? HIGH : LOW);
digitalWrite(ledJaune, LOW);
} else if (lastHeartRate > 90) {
digitalWrite(ledVerte, LOW);
digitalWrite(ledJaune, blinkState ? HIGH : LOW);
} else {
digitalWrite(ledVerte, LOW);
digitalWrite(ledJaune, LOW);
}
}
if (now - lastMsg < interval) return;
lastMsg = now;
char buffer[20];
uint8_t currentError = ERR_NONE;
if (WiFi.status() != WL_CONNECTED) currentError = ERR_WIFI_LOST;
if (ESP.getFreeHeap() < 10000) currentError = ERR_LOW_MEMORY;
// ---------- 1. DHT11 ----------
float humidity = dht.readHumidity();
float temperature = dht.readTemperature();
Serial.println("\n========== DHT11 ==========");
if (!isnan(humidity) && !isnan(temperature)) {
Serial.printf("Temperature: %.2f C\nHumidity: %.2f %%\n", temperature, humidity);
snprintf(buffer, sizeof(buffer), "%.2f", temperature);
client.publish("esp32/DHT11/temperature", buffer);
snprintf(buffer, sizeof(buffer), "%.2f", humidity);
client.publish("esp32/DHT11/humidity", buffer);
if (temperature > 50) currentError = ERR_TEMP_LIMIT;
} else {
Serial.println("DHT11 reading error!");
currentError = ERR_SENSOR_READ;
}
// ---------- 2. MPU6050 ----------
sensors_event_t accel, gyro, temp;
mpu.getEvent(&accel, &gyro, &temp);
Serial.println("\n========== MPU6050 ==========");
Serial.printf("Accel X: %.3f Y: %.3f Z: %.3f\n",
accel.acceleration.x, accel.acceleration.y, accel.acceleration.z);
Serial.printf("Gyro X: %.3f Y: %.3f Z: %.3f\n",
gyro.gyro.x, gyro.gyro.y, gyro.gyro.z);
snprintf(buffer, sizeof(buffer), "%.3f", accel.acceleration.x);
client.publish("esp32/MPU6050/accelX", buffer);
snprintf(buffer, sizeof(buffer), "%.3f", accel.acceleration.y);
client.publish("esp32/MPU6050/accelY", buffer);
snprintf(buffer, sizeof(buffer), "%.3f", accel.acceleration.z);
client.publish("esp32/MPU6050/accelZ", buffer);
snprintf(buffer, sizeof(buffer), "%.3f", gyro.gyro.x);
client.publish("esp32/MPU6050/gyroX", buffer);
snprintf(buffer, sizeof(buffer), "%.3f", gyro.gyro.y);
client.publish("esp32/MPU6050/gyroY", buffer);
snprintf(buffer, sizeof(buffer), "%.3f", gyro.gyro.z);
client.publish("esp32/MPU6050/gyroZ", buffer);
// ---------- 3. MAX30102 ----------
Serial.println("\n========== HEART RATE & SPO2 ==========");
MAX30102.getHeartbeatSPO2();
int heartRate = MAX30102._sHeartbeatSPO2.Heartbeat;
int spo2 = MAX30102._sHeartbeatSPO2.SPO2;
float boardTemp = MAX30102.getTemperature_C();
lastHeartRate = heartRate;
Serial.printf("Heart Rate: %d BPM\nSpO2: %d %%\nBoard temp: %.2f C\n",
heartRate, spo2, boardTemp);
if (heartRate > 0) {
snprintf(buffer, sizeof(buffer), "%d", heartRate);
client.publish("esp32/DFRobot/heartRate", buffer);
} else {
Serial.println("Heart rate not available yet!");
}
if (spo2 > 0) {
snprintf(buffer, sizeof(buffer), "%d", spo2);
client.publish("esp32/DFRobot/spo2", buffer);
} else {
Serial.println("SpO2 not available yet!");
}
snprintf(buffer, sizeof(buffer), "%.2f", boardTemp);
client.publish("esp32/DFRobot/boardTemp", buffer);
// ---------- 4. Code erreur ----------
errorCode = currentError;
snprintf(buffer, sizeof(buffer), "%d", errorCode);
client.publish("esp32/errorCode", buffer);
Serial.printf("\nCode erreur : %d\n", errorCode);
digitalWrite(ledPin, errorCode != ERR_NONE ? HIGH : LOW);
Serial.println("======================================");
}