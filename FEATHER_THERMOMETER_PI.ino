/* Choose NodeMCU 0.9 board in arduino MENU no matter what board you have (adafruit feather, nodemcu, wemos d1 mini) */

// BEGIN CONFIG ///////////////////////////////////////////////////////////////////////////
#define BONJOUR false
#define OTA_UPDATE false
//
#define MIC_ENABLED false       /* mic connected on A0 pin */
#define LIGHT_ENABLED true    /* light sensor connected to A0 */
#define PRESSURE_ENABLED true /* atmospheric pressure sensor BMP085 */
#define DALLAS_ENABLED false /* long wire waterproof thermometer */
#define RGB_PIXEL_ENABLED false /* RGB led shows co2 leve of the house by querying co2 sensor through http*/
#define RAIN_SENSOR_ENABLED true 
//
#define TEMP_CELCIUS_OFFSET -4.2 /*cheap wonky sensor calibration offset */
#define HUMIDITY_OFFSET 17.0     /*cheap wonky sensor calibration offset */
#define RGB_LED_BRIGHTNESS 254 /*0..255*/

#include "DHT.h"        //temp sensor
//#define DHTPIN 	D1      // what digital pin we're connected to		<<< on adafruit bluetooth feather board
//#define DHTPIN D1  	  // what digital pin we're connected to		<<< one NodeMCU 1.0 board (D1==20)
#define DHTPIN D4    	// what digital pin we're connected to		<<< on stacked wemos d1 mini
#define DHTTYPE DHT11 	// DHT22 (white)  DHT11 (blue)

#define SPEAKER_PIN D8
#define ONE_WIRE_BUS D4
#define PIXEL_PIN D2
#define RAIN_SENSOR_PIN D5

// PIN Summary
// D1 D2 > bmp sensor (pressure)
// D2 > NeoPixel
// D4 > DHT
// D8 > Speaker
// D5 > Rain Sensor
// D4 > Dallas (Data wire is plugged into port 2 on the Arduino)

// END CONFIG //////////////////////////////////////////////////////////////////////////////

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "MusicalTones.h"
#include "WifiPass.h"  //define wifi SSID & pass

#if OTA_UPDATE
#include <ArduinoOTA.h>
#endif

#if DALLAS_ENABLED
#include <OneWire.h>
#include <DallasTemperature.h>

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
float pressurePascal = 0.0f;
float tempCelcius2 = 0.0f;
float tempDallas = 0.0f;
int rain = 0;

int sleepMS = 500;
int loopCounter = -1;

//global devices
DHT dht(DHTPIN, DHTTYPE);
ESP8266WebServer server(80);
String ID;

// WIFI ////////////////////////////////////////////////////////////////////////////////////

#if PRESSURE_ENABLED
#include <Adafruit_BMP085.h>
Adafruit_BMP085 bmp;
#endif

#if RGB_PIXEL_ENABLED
#include <Adafruit_NeoPixel.h>
#include <ESP8266HTTPClient.h>
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, PIXEL_PIN, NEO_GRB + NEO_KHZ800);
#endif

void (*resetFunc)(void) = 0;  //declare reset function at address 0
void playTune();

void handleRoot() {

  String str = "{\"ID\":\"" + ID + "\", \"temperature\":" + String(tempCelcius, 2) + ", \"humidity\":" + String(humidity, 2);
  
  #if DALLAS_ENABLED
  str +=  ", \"tempDallas\": " + String(tempDallas,2);
  #endif

  #if MIC_ENABLED
  str +=  ", \"loudness\": " + String(loudness,2);
  #endif

  #if PRESSURE_ENABLED
  str +=  ", \"pressure\": " + String(pressurePascal,2);
  #endif

  #if LIGHT_ENABLED
  str +=  " ,\"light\": " + String(light,2);
  #endif

  #if RAIN_SENSOR_ENABLED
  str +=  ", \"rain\": " + String(rain);
  #endif
  
  str += " }";

	server.send(200, "application/json", str.c_str());
}

void handleMetrics() {
	server.send(200, "text/plain", GenerateMetrics());
}

#if DALLAS_ENABLED
void handlePool() {
  String str = "{\"temperature\":" + String(tempDallas, 2) + ", \"humidity\": 0.0 }" ;
	server.send(200, "application/json", str.c_str());
}
#endif

void handleBeep() {
	server.send(200, "text/plain", "BEEP OK\n");
	playTune();
	delay(1000);
	playTune();
}


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
	Serial.printf("HasMic:%d  HasLight:%d  HasPressure:%d  Dallas:%d  RGB_Pixel:%d  Rain:%d  HasOTA:%d  Bonjour:%d\n", MIC_ENABLED, LIGHT_ENABLED, PRESSURE_ENABLED, DALLAS_ENABLED, RGB_PIXEL_ENABLED, RAIN_SENSOR_ENABLED, OTA_UPDATE, BONJOUR);
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
	server.on("/beep", handleBeep);
  
  #if DALLAS_ENABLED
  server.on("/pool", handlePool);
  #endif

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

  #if RGB_PIXEL_ENABLED
  pixels.begin();
  pixels.setPixelColor(0, pixels.Color(0,0,0));
  pixels.setBrightness(RGB_LED_BRIGHTNESS);
  pixels.show();
  #endif

  #if RAIN_SENSOR_ENABLED
  pinMode(RAIN_SENSOR_PIN, INPUT);
  #endif

	dht.begin();
	delay(sleepMS / 2);
	updateSensorData();

	char txtOut[64];
	sprintf(txtOut, "\nID:%s   temperature:%f   humidity:%f\n", ID, tempCelcius, humidity);
	Serial.println(txtOut);
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

  #if RAIN_SENSOR_ENABLED
  rain = digitalRead(RAIN_SENSOR_PIN) == 0 ? 1 : 0;
  #endif

  #if RGB_PIXEL_ENABLED
	loopCounter++;
	if(loopCounter > 10000) loopCounter = 0;
	if(loopCounter % 60 == 0){
	  handleCo2Led();
	}
  #endif
}

#if RGB_PIXEL_ENABLED
void handleCo2Led(){
  Serial.println("handleCo2Led()");
  WiFiClient client;
  HTTPClient http;
  http.begin(client, "http://10.0.0.104/co2"); //connect to co2 device, ask for co2 level
  int httpResponseCode = http.GET();
  pixels.setPixelColor(0, pixels.Color(0,0,0));

  if (httpResponseCode == 200) {
	int co2Level = http.getString().toInt();
	//Serial.printf("co2: %d\n", co2Level);

	if(co2Level > 0){
	  if(co2Level < 600){
		pixels.setPixelColor(0, pixels.Color(0,255,0));
	  }else if(co2Level < 800){
		pixels.setPixelColor(0, pixels.Color(128,255,0));
	  }else if(co2Level < 800){
		pixels.setPixelColor(0, pixels.Color(255,255,0));
	  }else if(co2Level < 900){
		pixels.setPixelColor(0, pixels.Color(255,128,0));
	  }else if(co2Level < 1000){
		pixels.setPixelColor(0, pixels.Color(255,0,0));
	  }else if(co2Level < 1100){
		pixels.setPixelColor(0, pixels.Color(255,0,128));
	  }else{
		pixels.setPixelColor(0, pixels.Color(255,0,255));
	  }      
	}
  }
  pixels.show();
}
#endif

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

  #if RAIN_SENSOR_ENABLED
	message += "# HELP rain in range 0..1\n";
	message += "# TYPE rain gauge\n";
	message += "rain";
	message += idString;
	message += String(rain);
	message += "\n";
  #endif

	return message;
}

void playTune() {

	Serial.println("playTune()");

	int tempo = 105;
	int melody[] = {
		// Pacman
		// Score available at https://musescore.com/user/85429/scores/107109
		NOTE_B4, 16, NOTE_B5, 16, NOTE_FS5, 16, NOTE_DS5, 16, //1
		NOTE_B5, 32, NOTE_FS5, -16, NOTE_DS5, 8, NOTE_C5, 16,
		NOTE_C6, 16, NOTE_G6, 16, NOTE_E6, 16, NOTE_C6, 32, NOTE_G6, -16, NOTE_E6, 8,

		NOTE_B4, 16,  NOTE_B5, 16,  NOTE_FS5, 16,   NOTE_DS5, 16,  NOTE_B5, 32,  //2
		NOTE_FS5, -16, NOTE_DS5, 8,  NOTE_DS5, 32, NOTE_E5, 32,  NOTE_F5, 32,
		NOTE_F5, 32,  NOTE_FS5, 32,  NOTE_G5, 32,  NOTE_G5, 32, NOTE_GS5, 32,  NOTE_A5, 16, NOTE_B5, 8
	};

	int numNotes = sizeof(melody) / sizeof(melody[0]) / 2;
	int wholenote = (60000 * 4) / tempo;
	int divider = 0, noteDuration = 0;

	for (int thisNote = 0; thisNote < numNotes * 2; thisNote = thisNote + 2) {

		divider = melody[thisNote + 1];
		if (divider > 0) {
			noteDuration = (wholenote) / divider;
		} else if (divider < 0) {
			// dotted notes are represented with negative durations!!
			noteDuration = (wholenote) / abs(divider);
			noteDuration *= 1.5;  // increases the duration in half for dotted notes
		}

		// we only play the note for 90% of the duration, leaving 10% as a pause
		tone(SPEAKER_PIN, melody[thisNote], noteDuration * 0.9);
		delay(noteDuration);
		noTone(SPEAKER_PIN);
	}
}

void updateSensorData() {

	// Reading temperature or humidity takes about 250 milliseconds!
	// Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
	humidity = dht.readHumidity() + HUMIDITY_OFFSET;
	tempCelcius = dht.readTemperature() + TEMP_CELCIUS_OFFSET;
  if(humidity > 100) humidity = 100;
  if(humidity < 0) humidity = 0;

	if (isnan(humidity) || isnan(tempCelcius)) {
		Serial.println("Can't read from sensor! Resetting!");
		resetFunc();  //sensor is acting up - force hw reset!
	}

#if DALLAS_ENABLED
	//Serial.print("Requesting Dallas temperatures...");
	dallasTempSensor.requestTemperatures();  // Send the command to get temperatures
	//Serial.println("DONE");
	float tt = dallasTempSensor.getTempCByIndex(0);
	if (tt != DEVICE_DISCONNECTED_C) {
		tempDallas = tt;
	} else {
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
