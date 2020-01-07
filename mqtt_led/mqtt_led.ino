#include <ESP8266WiFi.h>           // Library for WiFi
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>           // Libraries for OTA updates
#include <WiFiUdp.h>               // Libraries for OTA updates
#include <ArduinoOTA.h>            // Libraries for OTA updates
#include <NTPClient.h>             // Library for connectiong to a time server
#include <Adafruit_NeoPixel.h>     // Library for the RGB LED
#include <Adafruit_MQTT.h>         // MQTT client
#include <Adafruit_MQTT_Client.h>  // MQTT client
#include <SPI.h>                   // Libraries for SD card
#include <SD.h>                    // Libraries for SD card
#include "credentials.h"           // File with personal credenials (WLAN_SSID, WLAN_PASSWORD, WEBHOOKS_KEY)

// Network Configurations
IPAddress ip(192, 168, 0, 103);   // Wemos D1 mini
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns1(190, 113, 220, 18); // DNS requiered for WiFiClientSecure to work with HTTPS and static IP
IPAddress dns2(190, 113, 220, 51);

// NTP congiguration
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "south-america.pool.ntp.org");

// Mosquitto client configuration
WiFiClient esp_client;
Adafruit_MQTT_Client mqtt(&esp_client, MQTT_SERVER, 1883, MQTT_USER, MQTT_PASSWORD);
Adafruit_MQTT_Subscribe current_temp = Adafruit_MQTT_Subscribe(&mqtt, "current_temp", MQTT_QOS_1);

// NeoPixel library configuration
#define PIN D2 // Set pin for the LED
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, PIN, NEO_GRB + NEO_KHZ800);

// SD card configuration
const int chipSelect = D8; // WeMos Micro SD Shield V1.0.0: D8
File myFile;

// WebServer configuration
ESP8266WebServer server(80);

// Globarl variables
String data;            // Variable to hold the MQTT message
String previous_data;
String home = "<h1>Download Log file!</h1>"
              "<p><a href=\"down\"><button>Download</button></a>";

void setup() {
  Serial.begin(115200);
  pixels.begin(); // This initializes the NeoPixel library.
  pixels.setPixelColor(0, pixels.Color(0, 150, 193)); // Blue light (State: Initializing)
  pixels.show();
  delay(1000);

  // Connect to WiFi network
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  WiFi.config(ip, gateway, subnet, dns1, dns2);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WLAN_SSID, WLAN_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    // Blink yellow light (State: Connecting)
    pixels.setPixelColor(0, pixels.Color(100, 100, 0));
    pixels.show();
    delay(500);
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();
    delay(500);
    Serial.print(".");
  }

  // Start OTA service
  ArduinoOTA.begin();

  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //Turn off the LED
  pixels.setPixelColor(0, pixels.Color(0, 0, 0));
  pixels.show();

  // Suscribe to topic
  mqtt.subscribe(&current_temp);

  // Initialize NTP client
  timeClient.begin();

  // Initialize SD card reader
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    return;
  }
  // Initialize WebServer
  server.on("/", handleRoot);
  server.on("/down", handleDownload);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // Start OTA handler
  ArduinoOTA.handle();

  // Update NTP
  timeClient.update();

  // Start WebServer handler
  server.handleClient();

  // Manage suscriptions
  MQTT_connect();
  Adafruit_MQTT_Subscribe *subscription;
  // This pauses execution of everthing outside the while-loop
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &current_temp) {
      Serial.print(F("Got: "));
      Serial.println((char *)current_temp.lastread);
      data = (char *)current_temp.lastread;

      if (data == String("hi_temp")) {
        pixels.setPixelColor(0, pixels.Color(255, 80, 0)); // Orange
        pixels.show();
      } else if (data == String("normal")) {
        pixels.setPixelColor(0, pixels.Color(0, 200, 0)); // Green
        pixels.show();
      } else if (data == String("low_temp")) {
        pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Blue
        pixels.show();
      }
      log_to_sd();
    }
  }

  // ping the server to keep the mqtt connection alive
  if (! mqtt.ping()) {
    mqtt.disconnect();
  }
}

// FUNCTIONS #########################################################################
// ###################################################################################

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

void log_to_sd() {
  if (data != previous_data) {
    myFile = SD.open("relay_log.csv", FILE_WRITE); // Open csv file
    if (myFile)
    {
      Serial.print("opened relay_log.csv...");
      myFile.print(timeClient.getFormattedDate());
      myFile.print(",");
      myFile.println(data);
      // close the file:
      myFile.close();
      Serial.println("closed relay_log.csv");
      previous_data = data;
    } else {
      // if the file didn't open, print an error:
      Serial.println("error opening relay_log.csv");
    }
  }
}

void handleRoot() {
  server.send(200, "text/html", home);
}

void handleDownload() {

  File dataFile = SD.open("relay_log.csv");
  int fsizeDisk = dataFile.size();
  Serial.print("fsizeDisk: ");
  Serial.println(fsizeDisk);

  size_t fsizeSent = server.streamFile(dataFile, "text/plain");

  Serial.print("fsizeSent: ");
  Serial.println(fsizeSent);

  dataFile.close();
}
