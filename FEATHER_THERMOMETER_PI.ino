#define MIC_ENABLED false
#define LIGHT_ENABLED false /*light sensor connected to A0*/
#define PRESSURE_ENABLED false
#define BONJOUR false
#define OTA_UPDATE false
#define DALLAS_ENABLED true

#define TEMP_CELCIUS_OFFSET -0.0 /*cheap wonky sensor calibration offset */
#define HUMIDITY_OFFSET 0      /*cheap wonky sensor calibration offset */

//temp sensor
#include "DHT.h"
#define DHTPIN 	14     // what digital pin we're connected to                <<< on adafruit bluetooth feather board
//#define DHTPIN D1  // what digital pin we're connected to                  <<< one NodeMCU 1.0 board (D1==20)
//#define DHTPIN 	D1     // what digital pin we're connected to                  <<< on stacked wemos d1 mini

#define DHTTYPE DHT22  // DHT22 (white)  DHT11 (blue)
#define BLUE_LED_PIN 2

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "WifiPass.h"  //define wifi SSID & pass

#if OTA_UPDATE
#include <ArduinoOTA.h>
#endif

#if DALLAS_ENABLED
#include <OneWire.h>
#include <DallasTemperature.h>
// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature dallasTempSensor(&oneWire);
#endif

// GLOBALS /////////////////////////////////////////////////////////////////////////////////

float tempCelcius = 0.0f;
float humidity = 0.0f;
float loudness = 0.0f;
float light = 0.0f;
float tempCelcius2 = 0.0f;
float pressurePascal = 0.0f;
float tempDallas = 0.0f;

int sleepMS = 1000;

//global devices
DHT dht(DHTPIN, DHTTYPE);
ESP8266WebServer server(80);
String ID;

// WIFI ////////////////////////////////////////////////////////////////////////////////////


#if PRESSURE_ENABLED
#include <Adafruit_BMP085.h>
Adafruit_BMP085 bmp;
#endif


void handleRoot() {
  static char json[128];
  sprintf(json, "{\"ID\":\"%s\", \"temperature\":%f, \"humidity\":%f}", ID, tempCelcius, humidity);
  server.send(200, "application/json", json);
}


void handleMetrics() {
  server.send(200, "text/plain", GenerateMetrics());
}


// RESET
void (*resetFunc)(void) = 0;  //declare reset function at address 0


void setup() {

  pinMode(LED_BUILTIN, OUTPUT);    //Initialize the LED_BUILTIN pin as an output
  digitalWrite(LED_BUILTIN, LOW);  //turn on red led
  pinMode(A0, INPUT);

  Serial.flush();
  Serial.begin(19200);
  Serial.flush();
  Serial.println("----------------------------------------------\n");
  ID = String(ESP.getChipId(), HEX);
  Serial.printf("Booting %s ......\n", ID.c_str());
  Serial.printf("HasMic:%d  HasLight:%d  HasPressure:%d  Dallas:%d  HasOTA:%d  Bonjour:%d\n", MIC_ENABLED, LIGHT_ENABLED, PRESSURE_ENABLED, DALLAS_ENABLED, OTA_UPDATE, BONJOUR);
  Serial.printf("TempSensorOffset: %.1f  HumiditySensorOffset: %.1f\n", TEMP_CELCIUS_OFFSET, HUMIDITY_OFFSET);
  //Serial.setDebugOutput(true);


  WiFi.setPhyMode(WIFI_PHY_MODE_11G);
  //WiFi.mode(WIFI_OFF);    //otherwise the module will not reconnect
  WiFi.mode(WIFI_STA);  //if it gets disconnected
  WiFi.disconnect();

  WiFi.begin(ssid, password);
  Serial.printf("Trying to connect to %s ...\n", ssid);
  int wifiAtempts = 0;
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    wifiAtempts++;
    if (wifiAtempts > 10) {
      Serial.printf("\nFailed to connect! Rebooting!\n");
      digitalWrite(LED_BUILTIN, HIGH);  //turn off red led
      ESP.restart();
    }
  }
  digitalWrite(LED_BUILTIN, HIGH);  //turn off red led
  Serial.printf("\nConnected to %s IP addres %s\n", ssid, WiFi.localIP().toString().c_str());

  WiFi.setAutoReconnect(true);
  //WiFi.persistent(true);
  //WiFi.forceSleepWake();
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

#if BONJOUR
  if (MDNS.begin(ID)) {
    Serial.println("MDNS responder started " + ID);
  }
#endif

  server.on("/", handleRoot);
  server.on("/metrics", handleMetrics);
  server.begin();
  Serial.println("HTTP server started");

#if BONJOUR
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
#endif

#if PRESSURE_ENABLED
  if (!bmp.begin()) {
    Serial.println("Could not find a valid BMP085 sensor, check wiring! Resetting!");
    resetFunc();  //sensor is acting up - force hw reset!
  }
#endif

#if OTA_UPDATE
  setupOTA();
#endif

#if DALLAS_ENABLED
  dallasTempSensor.begin();
#endif

  dht.begin();
  updateSensorData();
}


void loop() {

  delay(sleepMS);  //once per second

#if BONJOUR
  MDNS.update();
#endif
  updateSensorData();
  server.handleClient();

#if OTA_UPDATE
  ArduinoOTA.handle();
#endif
}


String GenerateMetrics() {
  String message = "";
  String idString = "{id=\"" + ID + "\",mac=\"" + WiFi.macAddress().c_str() + "\"}";

  message += "# HELP temp Temperature in C\n";
  message += "# TYPE temp gauge\n";
  message += "temp";
  message += idString;
  message += String(tempCelcius, 3);
  message += "\n";

  message += "# HELP hum Relative Humidity\n";
  message += "# TYPE hum gauge\n";
  message += "hum";
  message += idString;
  message += String(humidity, 3);
  message += "\n";

#if MIC_ENABLED
  message += "# HELP loud Microphone Loudness\n";
  message += "# TYPE loud gauge\n";
  message += "loud";
  message += idString;
  message += String(loudness, 3);
  message += "\n";
#endif

#if LIGHT_ENABLED
  message += "# HELP light sensor brightness\n";
  message += "# TYPE light gauge\n";
  message += "light";
  message += idString;
  message += String(light, 3);
  message += "\n";
#endif

#if PRESSURE_ENABLED
  message += "# HELP pressure Atmospheric Pressure in Pascals\n";
  message += "# TYPE pressure gauge\n";
  message += "pressure";
  message += idString;
  message += String(pressurePascal, 3);
  message += "\n";

  message += "# HELP temp2 Temperature in C\n";
  message += "# TYPE temp2 gauge\n";
  message += "temp2";
  message += idString;
  message += String(tempCelcius2, 3);
  message += "\n";
#endif

#if DALLAS_ENABLED
  message += "# HELP tempDallas Temperature in C\n";
  message += "# TYPE tempDallas gauge\n";
  message += "tempDallas";
  message += idString;
  message += String(tempDallas, 3);
  message += "\n";
#endif

  return message;
}


void updateSensorData() {

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  humidity = dht.readHumidity() + HUMIDITY_OFFSET;
  tempCelcius = dht.readTemperature() + TEMP_CELCIUS_OFFSET;

  if (isnan(humidity) || isnan(tempCelcius)) {
    Serial.println("Can't read from sensor! Resetting!");
    resetFunc();  //sensor is acting up - force hw reset!
  }

#if DALLAS_ENABLED
  //Serial.print("Requesting Dallas temperatures...");
  dallasTempSensor.requestTemperatures(); // Send the command to get temperatures
  //Serial.println("DONE");
  float tt = dallasTempSensor.getTempCByIndex(0);
  if(tt != DEVICE_DISCONNECTED_C) {
    tempDallas = tt;
  }else{
    Serial.println("Error: Could not read temperature data");
  }
#endif

#if (MIC_ENABLED)  //calc mic input gain //////////////////////
  int mn = 1024;
  int mx = 0;
  for (int i = 0; i < 4000; ++i) {
    int val = analogRead(A0);
    mn = min(mn, val);
    mx = max(mx, val);
  }
  loudness = (mx - mn) / 1024.0f;  //note 2X gain to get a little more contrast
                                   //Serial.printf("loud: %f\n", loudness);
#endif

#if (LIGHT_ENABLED)  //calc mic input gain //////////////////////
  light = analogRead(A0) / 1024.0f;
#endif


#if (PRESSURE_ENABLED)
  tempCelcius2 = bmp.readTemperature();
  pressurePascal = bmp.readPressure() / 100.0f;
#endif
}

///////////////// OTA UPDATES //////////////////////////////////////////////////////

#if OTA_UPDATE
void setupOTA() {
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
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
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });

  ArduinoOTA.begin();
}
#endif
