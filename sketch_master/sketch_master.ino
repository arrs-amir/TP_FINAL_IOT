#include <WiFi.h>
#include <PubSubClient.h>
#include "Wire.h"
#define I2C_DEV_ADDR 0x55
// =====================================
// WIFI / MQTT (Raspberry Pi)
// =====================================
const char* ssid = "iot";
const char* password = "iotisis;";
const char* mqtt_server = "172.18.32.40";
WiFiClient espClient;
PubSubClient client(espClient);
// =====================================
// TABLE DES CODES ERREUR
// =====================================
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
// =====================================
// TIMER NON BLOQUANT
// =====================================
unsigned long lastRequest = 0;
const unsigned long interval = 2000;
// =====================================
// CORRESPONDANCE CODE / DESCRIPTION
// =====================================
const char* getErrorMessage(uint8_t code) {
switch (code) {
case ERR_NONE: return "Aucune erreur";
case ERR_WIFI_LOST: return "Perte du Wifi";
case ERR_SENSOR_DISCONNECT: return "Capteur non connecte";
case ERR_SENSOR_READ: return "Erreur de lecture du capteur";
case ERR_TEMP_LIMIT: return "Temperature hors limites";
case ERR_LOW_VOLTAGE: return "Tension alimentation insuffisante";
case ERR_SERIAL_LOST: return "Perte communication serie";
case ERR_LOW_MEMORY: return "Memoire insuffisante";
case ERR_I2C: return "Erreur communication I2C";
case ERR_WATCHDOG: return "Watchdog declenche";
case ERR_UNKNOWN: return "Erreur systeme inconnue";
default: return "Code erreur non repertorie";
}
}
// =====================================
// WIFI
// =====================================
void setup_wifi() {
Serial.print("\nConnexion au WiFi ");
Serial.println(ssid);
WiFi.begin(ssid, password);
while (WiFi.status() != WL_CONNECTED) {
delay(500);
Serial.print(".");
}
Serial.println("\nWiFi connecte");
Serial.print("Adresse IP : ");
Serial.println(WiFi.localIP());
}
// =====================================
// MQTT
// =====================================
void callback(char* topic, byte* message, unsigned int length) {
Serial.print("Message recu sur le topic : ");
Serial.println(topic);
String messageTemp;
for (unsigned int i = 0; i < length; i++) {
messageTemp += (char)message[i];
}
Serial.println(messageTemp);
}
void reconnect() {
while (!client.connected()) {
Serial.print("Connexion au broker MQTT...");
// Client ID différent du Slave ("ESP32Client_Global")
if (client.connect("ESP32Client_Master")) {
Serial.println("connecte !");
client.subscribe("esp32/master/output");
} else {
Serial.print("echec, rc=");
Serial.println(client.state());
delay(5000);
}
}
}
// =====================================
// INITIALISATION
// =====================================
void setup() {
Serial.begin(115200);
Wire.begin(21, 22);
Wire.setClock(100000);
setup_wifi();
client.setServer(mqtt_server, 1883);
client.setCallback(callback);
Serial.println("ESP32 Master initialise");
}
// =====================================
// BOUCLE PRINCIPALE
// =====================================
void loop() {
if (!client.connected()) {
reconnect();
}
client.loop();
unsigned long now = millis();
if (now - lastRequest < interval) {
return;
}
lastRequest = now;
// Demande d'un seul octet au Slave
uint8_t bytesReceived = Wire.requestFrom((uint8_t)I2C_DEV_ADDR, (uint8_t)1);
char codeBuffer[8];
if (bytesReceived == 1 && Wire.available()) {
uint8_t receivedCode = Wire.read();
const char* description = getErrorMessage(receivedCode);
// Affichage Moniteur Série
Serial.println("-----------------------------");
Serial.print("Code erreur : ");
Serial.println(receivedCode);
Serial.print("Description : ");
Serial.println(description);
Serial.println("-----------------------------");
// Publication MQTT vers le Raspberry Pi
snprintf(codeBuffer, sizeof(codeBuffer), "%d", receivedCode);
client.publish("esp32/master/errorCode", codeBuffer);
client.publish("esp32/master/errorMessage", description);
} else {
Serial.println("Erreur : aucune reponse du Slave");
// Signale une erreur I2C au broker MQTT
snprintf(codeBuffer, sizeof(codeBuffer), "%d", ERR_I2C);
client.publish("esp32/master/errorCode", codeBuffer);
client.publish("esp32/master/errorMessage", "Erreur : aucune reponse du Slave");
}
}