/* Choose NodeMCU 0.9 board in arduino MENU no matter what board you have (adafruit feather, nodemcu, wemos d1 mini) */
#include "DHT.h"  //temp sensor

// BEGIN CONFIG ///////////////////////////////////////////////////////////////////////////
#define HOST_NAME "WorkshopSensor3"

#define MIC_ENABLED 		false      /* mic connected on A0 pin */
#define LIGHT_ENABLED 		false    /* light sensor connected to A0 */
#define PRESSURE_ENABLED 	false /* atmospheric pressure sensor BMP085 */
#define DALLAS_ENABLED 		true    /* long wire waterproof thermometer */
#define RAIN_SENSOR_ENABLED false

#define TEMP_CELCIUS_OFFSET -3.0 /*cheap wonky sensor calibration offset */
#define HUMIDITY_OFFSET 	10.0     /*cheap wonky sensor calibration offset */

//#define DHTPIN 			D1      // what digital pin we're connected to		<<< on adafruit bluetooth feather board
//#define DHTPIN 			D3  	  // what digital pin we're connected to		<<< one NodeMCU 1.0 board (D1==20)
#define DHTPIN 				D4      // what digital pin we're connected to		<<< on stacked wemos d1 mini
#define DHTTYPE 			DHT22  // DHT11 (blue) DHT22 (white)

// END CONFIG //////////////////////////////////////////////////////////////////////////////

#define SPEAKER_PIN 		D8
#define ONE_WIRE_BUS 		D7
#define RAIN_SENSOR_PIN 	D5
#define ARDUINO_OTA 		true /* allow over the air update of code */
#define WATCHDOG_ENABLE 	false
const int sleepMS = 300;
const int sensorDataRate = 5000; //how often to read sensor data, in ms

// PIN Summary for wemos d1 mini stacks
// D1 D2 > bmp sensor (pressure)
// D2 > NeoPixel
// D4 > DHT
// D8 > Speaker
// D5 > Rain Sensor
// D7 > Dallas (Data wire is plugged into port 2 on the Arduino)

#include <SerialWebLog.h>
#include <ArduinoOTA.h>
#include "WifiPass.h"  //define wifi SSID & pass
#include "MusicalTones.h"

#if DALLAS_ENABLED
#include <OneWire.h>
#include <DallasTemperature.h>
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature dallasTempSensor(&oneWire);
#endif

SerialWebLog mylog;

// GLOBALS /////////////////////////////////////////////////////////////////////////////////

float tempCelcius = 0.0f;
float humidity = 0.0f;
float loudness = 0.0f;
float light = 0.0f;
float pressurePascal = 0.0f;
float tempCelcius2 = 0.0f;
float tempDallas = 0.0f;
int rain = 0;

int loopCounter = -1;

//global devices
DHT dht(DHTPIN, DHTTYPE);
String ID;

// WIFI ////////////////////////////////////////////////////////////////////////////////////

#if PRESSURE_ENABLED
#include <Adafruit_BMP085.h>
Adafruit_BMP085 bmp;
#endif

void playTune();

void setup() {

	mylog.setup(HOST_NAME, ssid, password);
	pinMode(A0, INPUT);

	ID = String(ESP.getChipId(), HEX);
	mylog.printf("FEATHER_THERMOMETER_PI\n");
	mylog.print("*******************************************************************\n");
	mylog.printf("HostName: %s\n", HOST_NAME);
	mylog.printf("HasMic:%d  HasLight:%d  HasPressure:%d  Dallas:%d  Rain:%d\n", MIC_ENABLED, LIGHT_ENABLED, PRESSURE_ENABLED, DALLAS_ENABLED, RAIN_SENSOR_ENABLED);
	mylog.printf("DHT_Pin:%d DHT_Type:%d\n", DHTPIN, DHTTYPE);
	mylog.printf("TempSensorOffset: %.1f  HumiditySensorOffset: %.1f\n", TEMP_CELCIUS_OFFSET, HUMIDITY_OFFSET);
	mylog.print("*******************************************************************\n");
	mylog.printf("Sleep Interval: %dms\n", sleepMS);
	mylog.printf("Sensor readings Interval: %dms\n", sensorDataRate);

	mylog.getServer()->on("/json", handleJSON);
	mylog.addHtmlExtraMenuOption("JsonOutput", "/json");

	mylog.getServer()->on("/metrics", handleMetrics);
	mylog.addHtmlExtraMenuOption("Metrics", "/metrics");

	mylog.getServer()->on("/beep", handleBeep);
	mylog.addHtmlExtraMenuOption("Beep", "/beep");

	#if WATCHDOG_ENABLE
	ESP.wdtDisable();
	ESP.wdtEnable(WDTO_8S);
	mylog.print("Watchdog Enabled!\n");
	#endif

	#if DALLAS_ENABLED
	mylog.getServer()->on("/pool", handlePool);
	#endif

	#if PRESSURE_ENABLED
	if (!bmp.begin()) {
		mylog.print("Could not find a valid BMP085 sensor, check wiring! Resetting!\n");
		ESP.restart();
	}
	#endif

	#if DALLAS_ENABLED
	dallasTempSensor.begin();
	#endif

	#if RAIN_SENSOR_ENABLED
	pinMode(RAIN_SENSOR_PIN, INPUT);
	#endif

	//nan prevention
	//digitalWrite(DHTPIN, LOW); // sets output to gnd
	//pinMode(DHTPIN, OUTPUT); // switches power to DHT on
	//delay(1000); // delay necessary after power up for DHT to stabilize
	
	//sleep esp to reset DHT?
	//ESP.deepSleep(15e6);

	noInterrupts()
	dht.begin();
	updateSensorData();
	interrupts();

	mylog.printf("Temp: %.1f  Hum: %.1f\n", tempCelcius, humidity);

	#if ARDUINO_OTA
	ArduinoOTA.setHostname(HOST_NAME);
	ArduinoOTA.setRebootOnSuccess(true);
	ArduinoOTA.begin();
	#endif
}


void handleJSON() {
	String str = "{\"ID\":\"" + ID + "\", \"hostName\":\"" + HOST_NAME + "\", \"ip\":\"" + WiFi.localIP().toString();
	str += "\", \"temperature\":" + String(tempCelcius, 2) + ", \"humidity\":" + String(humidity, 2);

	//wifi signal
	str += ", \"wifi\": " + String((int)WiFi.RSSI());

	#if DALLAS_ENABLED
	str += ", \"tempDallas\": " + String(tempDallas, 2);
	#endif

	#if MIC_ENABLED
	str += ", \"loudness\": " + String(loudness, 4);
	#endif

	#if PRESSURE_ENABLED
	str += ", \"pressure\": " + String(pressurePascal, 1);
	#endif

	#if LIGHT_ENABLED
	str += ", \"light\": " + String(light, 3);
	#endif

	#if RAIN_SENSOR_ENABLED
	str += ", \"rain\": " + String(rain);
	#endif

	str += " }";

	mylog.getServer()->send(200, "application/json", str.c_str());
}

void handleMetrics() {
	mylog.getServer()->send(200, "text/plain", GenerateMetrics());
}

#if DALLAS_ENABLED
void handlePool() {
	String str = "{\"temperature\":" + String(tempDallas, 2) + ", \"humidity\": 0.0 }";
	mylog.getServer()->send(200, "application/json", str.c_str());
}
#endif

void handleBeep() {
	mylog.getServer()->send(200, "text/plain", "BEEP OK\n");
	playTune();
	delay(1000);
	playTune();
}

void loop() {

	mylog.update();
	delay(sleepMS);  
	updateSensorData();

	if (WiFi.RSSI() == 31) {  //wifi cant get RSSI, some sort of error... better restart
		ESP.restart();
	}

	#if WATCHDOG_ENABLE
	ESP.wdtFeed();  //feed watchdog frequently
	#endif

	#if ARDUINO_OTA
	ArduinoOTA.handle();
	#endif
}

String GenerateMetrics() {

	String message = "";
	String idString = "{id=\"" + ID + "\",ip=\"" + WiFi.localIP().toString() + "\"}";

	//message += "# HELP temp Temperature in C\n";
	//message += "# TYPE temp gauge\n";
	message += "temp";
	message += idString;
	message += String(tempCelcius, 3);
	message += "\n";

	//message += "# HELP hum Relative Humidity\n";
	//message += "# TYPE hum gauge\n";
	message += "hum";
	message += idString;
	message += String(humidity, 3);
	message += "\n";

	//message += "# HELP wifi Wifi Signal RSSI\n";
	//message += "# TYPE wifi gauge\n";
	message += "wifi";
	message += idString;
	message += String((int)WiFi.RSSI());
	message += "\n";

	#if MIC_ENABLED
	message += "loud";
	message += idString;
	message += String(loudness, 3);
	message += "\n";
	#endif

	#if LIGHT_ENABLED
	message += "light";
	message += idString;
	message += String(light, 3);
	message += "\n";
	#endif

	#if PRESSURE_ENABLED
	message += "pressure";
	message += idString;
	message += String(pressurePascal, 3);
	message += "\n";

	message += "temp2";
	message += idString;
	message += String(tempCelcius2, 3);
	message += "\n";
	#endif

	#if DALLAS_ENABLED
	message += "tempDallas";
	message += idString;
	message += String(tempDallas, 3);
	message += "\n";
	#endif

	#if RAIN_SENSOR_ENABLED
	message += "rain";
	message += idString;
	message += String(rain);
	message += "\n";
	#endif

	return message;
}

void updateSensorData() {

	static int timeAccumulator = sensorDataRate; //1st run always updates sensors
	timeAccumulator += sleepMS;
	if(timeAccumulator < sensorDataRate)
		return;
	
	mylog.print("updating sensors...\n");
	timeAccumulator = 0;

	humidity = float(HUMIDITY_OFFSET) + dht.readHumidity(false);
	tempCelcius = float(TEMP_CELCIUS_OFFSET) + dht.readTemperature(false, false);
	if (humidity > 100) humidity = 100;
	else if (humidity < 0) humidity = 0;

	if (isnan(humidity) || isnan(tempCelcius || ((int)humidity == 0 && (int)tempCelcius == 0))) {
		static bool warnedAboutDHT = false;
		if (!warnedAboutDHT) {
			mylog.printf("**** Can't read from DHT sensor! t:%f h:%f ****\n", humidity, tempCelcius);
			warnedAboutDHT = true;
		}
	}

	#if DALLAS_ENABLED /////////////////////////////////////////////////////////////////
	{
		static bool warnedAboutDallas = false;
		dallasTempSensor.requestTemperatures();  // Send the command to get temperatures
		float tt = dallasTempSensor.getTempCByIndex(0);
		if (tt != DEVICE_DISCONNECTED_C) {
			tempDallas = tt;
			warnedAboutDallas = false;
		} else {
			if(!warnedAboutDallas){
				mylog.print("**** Error: Could not read temperature data from DALLAS sensor ****\n");
				warnedAboutDallas = true;
			}
		}
	}
	#endif

	#if (MIC_ENABLED)  //calc mic input gain ///////////////////////////////////////////
	int mn = 1024;
	int mx = 0;
	for (int i = 0; i < 1024; ++i) {
		int val = analogRead(A0);
		mn = min(mn, val);
		mx = max(mx, val);
	}
	loudness = (mx - mn) / 1024.0f;  //note 2X gain to get a little more contrast
	                                 //mylog.printf("loud: %f\n", loudness);
	#endif

	#if (LIGHT_ENABLED)  //calc mic input gain //////////////////////////////////////////
	light = analogRead(A0) / 1024.0f;
	#endif

	#if (PRESSURE_ENABLED)
	{
		static int skipCounter = 0;
		skipCounter--;
		if (skipCounter <= 0) {
			tempCelcius2 = bmp.readTemperature();
			float altitute = 29;
			pressurePascal = bmp.readSealevelPressure(altitute) / 100.0f;
			if (pressurePascal > 1200) {  //reading is way off! reset!
				ESP.restart();
			}
			skipCounter = 10;
		}
	}
	#endif

	#if RAIN_SENSOR_ENABLED ////////////////////////////////////////////////////////////
	rain = digitalRead(RAIN_SENSOR_PIN) == 0 ? 1 : 0;
	#endif
}

void playTune() {

	mylog.print("playTune()\n");

	int tempo = 105;
	int melody[] = {
		// Pacman
		// Score available at https://musescore.com/user/85429/scores/107109
		NOTE_B4, 16, NOTE_B5, 16, NOTE_FS5, 16, NOTE_DS5, 16,  //1
		NOTE_B5, 32, NOTE_FS5, -16, NOTE_DS5, 8, NOTE_C5, 16,
		NOTE_C6, 16, NOTE_G6, 16, NOTE_E6, 16, NOTE_C6, 32, NOTE_G6, -16, NOTE_E6, 8,

		NOTE_B4, 16, NOTE_B5, 16, NOTE_FS5, 16, NOTE_DS5, 16, NOTE_B5, 32,  //2
		NOTE_FS5, -16, NOTE_DS5, 8, NOTE_DS5, 32, NOTE_E5, 32, NOTE_F5, 32,
		NOTE_F5, 32, NOTE_FS5, 32, NOTE_G5, 32, NOTE_G5, 32, NOTE_GS5, 32, NOTE_A5, 16, NOTE_B5, 8
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
