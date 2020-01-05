#include <ESP8266WiFi.h>        // Library for WiFi
#include <ESP8266WebServer.h>   // Library for the web server
#include <ESP8266mDNS.h>        // Libraries for OTA updates
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>        // Library for JSON objects
#include <OneWire.h>            // Library for DS18B20 sensor
#include <DallasTemperature.h>  // Library for DS18B20 sensor
#include <DHT.h>                // Library for the DHT11 sensor
#include <Adafruit_MQTT.h>      // MQTT client
#include <Adafruit_MQTT_Client.h>
#include "DataToMaker.h"        // Functions for connecting to Maker/IFTTT
#include "credentials.h"        // File with personal credenials (WLAN_SSID, WLAN_PASSWORD, WEBHOOKS_KEY)
#include "parameters.h"         // File with temperature parameters (upper_level, lower_level, upper_limit, lower_limit)

// Network Configurations
IPAddress ip(192, 168, 0, 102);   // Lolin D1 mini Pro
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns1(190, 113, 220, 18); // DNS requiered for WiFiClientSecure to work with HTTPS and static IP
IPAddress dns2(190, 113, 220, 51);

// MQTT client configuration
WiFiClient esp_client;
Adafruit_MQTT_Client mqtt(&esp_client, MQTT_SERVER, 1883, MQTT_USER, MQTT_PASSWORD);
Adafruit_MQTT_Publish current_temp = Adafruit_MQTT_Publish(&mqtt, "current_temp"); // Define MQTT topic

// Web Server Configuration
ESP8266WebServer server(80);         // Define web-server on port 80

// DS18B20 Sensor Libraries Configuration
OneWire ourWire(4);                  // Set D2(GPIO-4) pin as OneWire bus
DallasTemperature sensors(&ourWire); // Declare sensor object

// DHT library configuration
#define DHTPIN D4                    // What pin the sensor is connected to
#define DHTTYPE DHT11                // DHT11 or DHT12
DHT dht(DHTPIN, DHTTYPE);

// Relay configuration
const int relayPin = D1; //What pin the relay is connected to

// Declare maker events
DataToMaker low_temp_envent(WEBHOOKS_KEY, "low_temperature");
DataToMaker hi_temp_envent(WEBHOOKS_KEY, "hi_temperature");


// Global variables definition
float temp0;                         //Define variables to store temp readings
float temp1;
float temp2;
float temp_offset = 0.4733321;       //Offset relative to DS18B20 probe sensors
float humidity;
float heat_index;
unsigned long previousMillis = 0;    //Temporal variable
const long interval = 2000;          //Set minimal interval between readings
bool triggered = false;              //Flag variable for sending notifications
char* state;
char* previous_state;

void MQTT_connect();

void setup() {
  Serial.begin(115200);
  delay(10);

  pinMode(LED_BUILTIN, OUTPUT);       //Initialize builtin LED (starts on)

  // Connect to WiFi network
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  WiFi.config(ip, gateway, subnet, dns1, dns2);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WLAN_SSID, WLAN_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  // Start OTA service
  ArduinoOTA.begin();
  
  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Define server responses
  server.on("/reading", HTTP_GET, []() {
    read_sensor();

    char buff0[9];                    //Define character buffer variables to store converted values
    char buff1[9];
    char buff2[9];
    char buff3[9];
    char buff4[9];
    dtostrf(temp0, 8, 4, buff0);      //Convert float values to string for printing 7 character + sign + NULL
    dtostrf(temp1, 8, 4, buff1);
    dtostrf(temp2, 8, 4, buff2);
    dtostrf(humidity, 8, 4, buff3);
    dtostrf(heat_index, 8, 4, buff4);

    char webString[600];            //Define character variable for storing the HTML page
    snprintf(webString, 600,        //Write HTML page
             "\
<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>Temperature Server</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Sensor Data</h1>\
    <p>Temperature 01: %s &#176;C</p>\
    <p>Temperature 02: %s &#176;C</p>\
    <p>Temperature 03: %s &#176;C</p>\
    <p>Humidity: %s &#37;</p>\
    <p>Apparent Temperature: %s &#176;C</p>\
  </body>\
</html>\
", buff0, buff1, buff2, buff3, buff4);
    // Send response
    server.send(200, "text/html", webString);
  });

  server.on("/reading.json", []() {
    read_sensor();

    StaticJsonDocument<500> jsonBuffer;             //Create JSON document
    JsonObject root = jsonBuffer.to<JsonObject>();  //Create JSON object
    root["temperature-probe0"] = temp0;             //Add values to elements
    root["temperature-probe1"] = temp1;
    root["temperature-probe2"] = temp2;
    root["temperature-perceived"] = heat_index;
    root["humidity-board"] = humidity;

    String jsonString;
    serializeJson(jsonBuffer, jsonString);          //Serialize JSON for printing

    // Send response
    server.send(200, "application/json", jsonString);
  });

  server.begin();                                    //Start server
  Serial.println("Temp server started! Waiting for clients!");
  digitalWrite(LED_BUILTIN, HIGH);                  //Turn the LED off

  // Initialize sensors
  sensors.begin();                // Initialize DS18B20 sensor
  dht.begin();                    // Initialize DHT sensor
  pinMode(relayPin, OUTPUT);      // Initialize Relay pin
}

void loop() {

  // Start OTA handler
  ArduinoOTA.handle();
  
  server.handleClient();

  read_sensor();

  // CHECK TEMPERATURE LEVELS AND SET RELAY #########################################
  if ((temp0 > 0 && temp1 > 0) && (temp0 < lower_level || temp1 < lower_level)) {
    digitalWrite(relayPin, HIGH); // turn on relay with voltage HIGH
    state = "low_temp";
  } else if (temp0 > upper_level || temp1 > upper_level) {
    digitalWrite(relayPin, LOW);  // turn off relay with voltage LOW
    state = "hi_temp";
  } else {
    state = "normal";
  }

  // Send state via MQTT message
  MQTT_connect();
  if (state != previous_state) {
    Serial.print(F("\nSending state: "));
    Serial.print(state);
    Serial.print("...");
    if (! current_temp.publish(state)) {
      Serial.println(F("Failed"));
    } else {
      Serial.println(F("OK!"));
      previous_state = state;
    }
  }
  // ping the server to keep the mqtt connection alive
  if (! mqtt.ping()) {
    mqtt.disconnect();
  }
  // ################################################################################

  // CHECK TEMPERATURE LIMITS AND SEND NOTIFICATION #################################
  if (temp0 >= upper_limit || temp1 >= upper_limit) {
    if (!triggered) {
      hi_temp_envent.setValue(1, String(temp0, 2));
      hi_temp_envent.setValue(2, String(temp1, 2));
      hi_temp_envent.setValue(3, String(heat_index, 2));
      if (hi_temp_envent.connect()) {
        hi_temp_envent.post();
      }
      triggered = true;
    }
  } else if (temp0 <= lower_limit || temp1 <= lower_limit) {
    if (!triggered) {
      low_temp_envent.setValue(1, String(temp0, 2));
      low_temp_envent.setValue(2, String(temp1, 2));
      low_temp_envent.setValue(3, String(heat_index, 2));
      if (low_temp_envent.connect()) {
        low_temp_envent.post();
      }
      triggered = true;
    }
  } else {
    triggered = false;
  }
  // #################################################################################
}

// FUNCTIONS #########################################################################
// ###################################################################################

// Get sensor readings
void read_sensor() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    sensors.requestTemperatures();         //Request temp readings
    temp0 = sensors.getTempCByIndex(0);    //Request temp in C by index
    temp1 = sensors.getTempCByIndex(1);
    temp2 = sensors.getTempCByIndex(2) - temp_offset;
    humidity = dht.readHumidity();
    heat_index = dht.computeHeatIndex((temp0 + temp1) / 2, humidity, false);
  }
}

// MQTT functiones
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      while (1);
    }
  }
  Serial.println("MQTT Connected!");
}
