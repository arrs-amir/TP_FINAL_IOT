/*********
Rui Santos
Complete project details at https://randomnerdtutorials.com [https://randomnerdtutorials.com/]
Modified by Alexis Combes
*********/
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
// Identifiants Wi-Fi
const char *ssid = "iot";
const char *password = "iotisis;";
// Adresse IP du broker MQTT
const char *mqtt_server = "172.18.32.40";
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
// Broches (Pins)
const int ledPin = 25;
const int buttonPin = 26; // À remplacer par la nouvelle broche du bouton
void setup_wifi();
void callback(char *topic, byte *message, unsigned int length);
void setup()
{
Serial.begin(115200);
setup_wifi();
client.setServer(mqtt_server, 1883);
client.setCallback(callback);
// Initialisation des deux broches
pinMode(ledPin, OUTPUT);
pinMode(buttonPin, INPUT_PULLUP);
}
void setup_wifi()
{
delay(10);
Serial.println();
Serial.print("Connecting to ");
Serial.println(ssid);
WiFi.begin(ssid, password);
while (WiFi.status() != WL_CONNECTED)
{
delay(500);
Serial.print(".");
}
Serial.println("");
Serial.println("WiFi connected");
Serial.println("IP address: ");
Serial.println(WiFi.localIP());
}
void callback(char *topic, byte *message, unsigned int length)
{
Serial.print("Message arrived on topic: ");
Serial.print(topic);
Serial.print(". Message: ");
String messageTemp;
for (int i = 0; i < length; i++)
{
Serial.print((char)message[i]);
messageTemp += (char)message[i];
}
Serial.println();
// Contrôle de la LED via le topic esp32/output
if (String(topic) == "esp32/output")
{
Serial.print("Changing output to ");
if (messageTemp == "on")
{
Serial.println("LED on");
digitalWrite(ledPin, HIGH);
}
else if (messageTemp == "off")
{
Serial.println("LED off");
digitalWrite(ledPin, LOW);
}
}
}
void reconnect()
{
while (!client.connected())
{
Serial.print("Attempting MQTT connection...");
// ID client unique (à personnaliser si d'autres ESP32 utilisent le même broker)
if (client.connect("ESP32Client_BtnLed"))
{
Serial.println("connected");
client.subscribe("esp32/output");
}
else
{
Serial.print("failed, rc=");
Serial.print(client.state());
Serial.println(" try again in 5 seconds");
delay(5000);
}
}
}
void loop()
{
if (!client.connected())
{
reconnect();
}
client.loop();
unsigned long now = millis();
if (now - lastMsg > 1000)
{
lastMsg = now;
// Lecture de l'état du bouton et publication MQTT
int buttonState = digitalRead(buttonPin);
if (buttonState == LOW)
{
Serial.print("Publish message: ");
String payload = "down";
Serial.println(payload);
client.publish("esp32/button1", payload.c_str());
}
else
{
Serial.print("Publish message: ");
String payload = "up";
Serial.println(payload);
client.publish("esp32/button1", payload.c_str());
}
}
}