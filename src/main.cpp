#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include "credentials.h"
#include "logo.h"

#define oled

#if defined(oled)
  #include <Adafruit_GFX.h>
  #include <Adafruit_SH110X.h>
  #include <Fonts/FreeMonoBold18pt7b.h>
  #define i2c_Address   0x3c
  #define SCREEN_WIDTH   128
  #define SCREEN_HEIGHT   64
  #define OLED_RESET      -1
  Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#endif

#define DHTPIN  22
#define DHTTYPE DHT22
#define STATUS_LED  LED_BUILTIN
DHT dht(DHTPIN, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
int read_interval = 30000;

const char* hostname = "picow-iot-node1";

void setup_wifi() {
  delay(100);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  if (!MDNS.begin(hostname)) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_ota(){
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else
        type = "filesystem";
      Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
  ArduinoOTA.begin();
}

void callback(char* topic, byte* payload, unsigned int length) {

}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "PicoW-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      String hostnameGreet = hostname;
      hostnameGreet += " connected"; 
      client.publish("home/connections", hostnameGreet.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  pinMode(STATUS_LED, OUTPUT);
  Serial.begin(115200);
  delay(100);
  dht.begin();
  #if defined(oled)
    display.begin(i2c_Address, true);
    display.clearDisplay();
    display.display();
    display.setTextColor(SH110X_WHITE);
  #endif
  setup_wifi();
  setup_ota();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  ArduinoOTA.handle();
  
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  StaticJsonDocument<32> doc;
  char output[55];

  long now = millis();
  if (now - lastMsg > read_interval) {
    digitalWrite(STATUS_LED, HIGH);
    lastMsg = now;

/*  
    float temp = dht.readTemperature();
    float humidity = dht.readHumidity(); 
*/
    
    float temp = 21.3;
    float humidity = 60.50; 
    
    doc["t"] = temp;
    doc["h"] = humidity;
    
    Serial.println("Read");
    serializeJson(doc, output);
    Serial.println(output);
    client.publish("home/sensors/salon2", output);
    Serial.println("Sent");
    #if defined(oled)
      display.clearDisplay();
      display.drawBitmap(0, 0, logo_bmp, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);
      display.setFont(&FreeMonoBold18pt7b);
      display.setTextColor(SH110X_WHITE);
      display.setCursor(45, 28);
      display.print((int)temp);
      display.drawCircle(92, 8, 3, SH110X_WHITE);
      display.setCursor(100, 27);
      display.print("C");
      display.setCursor(45, 62);
      display.print((int)humidity);
      display.print("%");
      display.display();
    #endif

    digitalWrite(STATUS_LED, LOW);
  }
}