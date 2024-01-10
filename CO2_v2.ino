#include "s300i2c.h"
#include <Wire.h>
#include <WiFi.h>
#include "ThingSpeak.h"

String apiKey = "LN7YKZ2MOLMNNI3H"; // Enter your Write API key from ThingSpeak
const char *ssid = "MASHARRIA"; // replace with your wifi ssid and wpa2 key
const char *pass = "machungwa";
const char* server = "api.thingspeak.com";

WiFiClient  client;

S300I2C s3(Wire);

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
   {
    delay(500);
    Serial.print(".");
   }
 Serial.println("");
 Serial.println("WiFi connected");

 Serial.begin(115200);
 Wire.begin();
 s3.begin(S300I2C_ADDR);
 delay(1000); // 10sec wait.

 s3.wakeup();
 s3.end_mcdl();
 s3.end_acdl();
 Serial.println("START S300I2C");

 ThingSpeak.begin(client); //Initialize ThingSpeak
}

void loop() {
  unsigned int co2;
  co2 = s3.getCO2ppm();
  Serial.print("CO2=");
  Serial.println(co2);
  delay(3000); // 30sec wait

 if (client.connect(server, 80)) // "184.106.153.149" or api.thingspeak.com
{
String postStr = apiKey;
postStr += "&field1=";
postStr += String(co2);
postStr += "r\n";
client.print("POST /update HTTP/1.1\n");
client.print("Host: api.thingspeak.com\n");
client.print("Connection: close\n");
client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
client.print("Content-Type: application/x-www-form-urlencoded\n");
client.print("Content-Length: ");
client.print(postStr.length());
client.print("\n\n");
client.print(postStr);
Serial.print("Gas Level: ");
Serial.println(co2);
Serial.println("Data Send to Thingspeak");
}
delay(500);
client.stop();
Serial.println("Waiting...");
 
// thingspeak needs minimum 15 sec delay between updates.
delay(1500);
  
}
