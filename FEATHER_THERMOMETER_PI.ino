/* Choose NodeMCU 0.9 board in arduino MENU no matter what board you have (adafruit feather, nodemcu, wemos d1 mini) */
// BEGIN CONFIG ///////////////////////////////////////////////////////////////////////////
#define HOST_NAME "RoofTop"

#define MIC_ENABLED false       /* mic connected on A0 pin */
#define LIGHT_ENABLED false    /* light sensor connected to A0 */
#define PRESSURE_ENABLED false /* atmospheric pressure sensor BMP085 */
#define DALLAS_ENABLED false /* long wire waterproof thermometer */
#define RAIN_SENSOR_ENABLED false
#define HAS_BEEP false
#define ARDUINO_OTA true /* allow over the air update of code */
#define WATCHDOG_ENABLE	true

//
int sleepMS = 500;
#define TEMP_CELCIUS_OFFSET -0.0 /*cheap wonky sensor calibration offset */
#define HUMIDITY_OFFSET 0.0     /*cheap wonky sensor calibration offset */

#include "DHT.h"        //temp sensor
//#define DHTPIN 	D1      // what digital pin we're connected to		<<< on adafruit bluetooth feather board
//#define DHTPIN D1  	  // what digital pin we're connected to		<<< one NodeMCU 1.0 board (D1==20)
#define DHTPIN D4    	// what digital pin we're connected to		<<< on stacked wemos d1 mini
#define DHTTYPE DHT22 	// DHT22 (white)  DHT11 (blue)

#define SPEAKER_PIN D8
#define ONE_WIRE_BUS D4
#define RAIN_SENSOR_PIN D5

// PIN Summary
// D1 D2 > bmp sensor (pressure)
// D2 > NeoPixel
// D4 > DHT
// D8 > Speaker
// D5 > Rain Sensor
// D4 > Dallas (Data wire is plugged into port 2 on the Arduino)

// END CONFIG //////////////////////////////////////////////////////////////////////////////

#include <SerialWebLog.h>
#include "WifiPass.h"  //define wifi SSID & pass

#include <ArduinoOTA.h>

#if HAS_BEEP
	#include "MusicalTones.h"
#endif

#if DALLAS_ENABLED
	#include <OneWire.h>
	#include <DallasTemperature.h>
	// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
	OneWire oneWire(ONE_WIRE_BUS);
	// Pass our oneWire reference to Dallas Temperature.
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

	pinMode(LED_BUILTIN, OUTPUT);    //Initialize the LED_BUILTIN pin as an output
	digitalWrite(LED_BUILTIN, LOW);  //turn on red led
	pinMode(A0, INPUT);

	mylog.setup(HOST_NAME, ssid, password);
	//WiFi.setSleepMode(WIFI_LIGHT_SLEEP);

	mylog.print("----------------------------------------------\n");
	ID = String(ESP.getChipId(), HEX);
	mylog.printf("Booting %s ......\n", ID.c_str());
	mylog.printf("HasMic:%d  HasLight:%d  HasPressure:%d  Dallas:%d  Rain:%d\n", MIC_ENABLED, LIGHT_ENABLED, PRESSURE_ENABLED, DALLAS_ENABLED, RAIN_SENSOR_ENABLED);
	mylog.printf("TempSensorOffset: %.1f  HumiditySensorOffset: %.1f\n", TEMP_CELCIUS_OFFSET, HUMIDITY_OFFSET);

	mylog.getServer()->on("/json", handleJSON);
	mylog.addHtmlExtraMenuOption("JsonOutput", "/json");
	
	mylog.getServer()->on("/metrics", handleMetrics);
	mylog.addHtmlExtraMenuOption("Metrics", "/metrics");
	
	#if HAS_BEEP
	mylog.getServer()->on("/beep", handleBeep);
	mylog.addHtmlExtraMenuOption("Beep", "/beep");
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

	dht.begin();
	delay(sleepMS / 2);
	updateSensorData();

	#if WATCHDOG_ENABLE
	//setup watchdog
	ESP.wdtDisable();
	ESP.wdtEnable(WDTO_8S);
	mylog.print("Watchdog Enabled!\n");
	#endif

	#if ARDUINO_OTA
	ArduinoOTA.setHostname(HOST_NAME);
	ArduinoOTA.setRebootOnSuccess(true);
	ArduinoOTA.begin();
	#endif

	mylog.printf("ID:%s   temperature:%f   humidity:%f\n", ID, tempCelcius, humidity);
}


void handleJSON() {
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
	str +=  ", \"light\": " + String(light,2);
	#endif

	#if RAIN_SENSOR_ENABLED
	str +=  ", \"rain\": " + String(rain);
	#endif

	str += " }";

	mylog.getServer()->send(200, "application/json", str.c_str());
}

void handleMetrics() {
	mylog.getServer()->send(200, "text/plain", GenerateMetrics());
}

#if DALLAS_ENABLED
void handlePool() {
	String str = "{\"temperature\":" + String(tempDallas, 2) + ", \"humidity\": 0.0 }" ;
	mylog.getServer()->send(200, "application/json", str.c_str());
}
#endif

#if HAS_BEEP
void handleBeep() {
	mylog.getServer()->send(200, "text/plain", "BEEP OK\n");
	playTune();
	delay(1000);
	playTune();
}
#endif

void loop() {

	mylog.update();
  	updateSensorData();
	
	delay(sleepMS);  //once per second

	#if WATCHDOG_ENABLE
	ESP.wdtFeed(); //feed watchdog frequently
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

void updateSensorData() {

	// Reading temperature or humidity takes about 250 milliseconds!
	// Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
	humidity = dht.readHumidity() + HUMIDITY_OFFSET;
	tempCelcius = dht.readTemperature() + TEMP_CELCIUS_OFFSET;
	if(humidity > 100) humidity = 100;
	if(humidity < 0) humidity = 0;

	if (isnan(humidity) || isnan(tempCelcius)) {
		mylog.print("Can't read from sensor! Resetting!\n");
		ESP.restart();
	}

	#if DALLAS_ENABLED
		//mylog.print("Requesting Dallas temperatures...\n");
		dallasTempSensor.requestTemperatures();  // Send the command to get temperatures
		//mylog.print("DONE\n");
		float tt = dallasTempSensor.getTempCByIndex(0);
		if (tt != DEVICE_DISCONNECTED_C) {
			tempDallas = tt;
		} else {
			mylog.print("Error: Could not read temperature data\n");
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
		mylog.printf("loud: %f\n", loudness);
	#endif

	#if (LIGHT_ENABLED)  //calc mic input gain //////////////////////
		light = analogRead(A0) / 1024.0f;
	#endif


	#if (PRESSURE_ENABLED)
		tempCelcius2 = bmp.readTemperature();
		pressurePascal = bmp.readPressure() / 100.0f;
	#endif

	#if RAIN_SENSOR_ENABLED
		rain = digitalRead(RAIN_SENSOR_PIN) == 0 ? 1 : 0;
	#endif

}

#if HAS_BEEP
void playTune() {

	mylog.print("playTune()\n");

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
#endif
