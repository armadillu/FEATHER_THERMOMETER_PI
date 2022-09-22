#define MIC_ENABLED			true
#define LIGHT_ENABLED		false
#define PRESSURE_ENABLED	false

//temp sensor
#include "DHT.h"
//#define DHTPIN 	14     // what digital pin we're connected to                <<< on adafruit bluetooth feather board
#define DHTPIN 		D1 		// what digital pin we're connected to                  <<< one NodeMCU 1.0 board - D1 - 20
//#define DHTPIN 	D4     // what digital pin we're connected to                  <<< on stacked wemos d1 mini

#define DHTTYPE DHT22   // DHT22 (white)  DHT11 (blue)
#define BLUE_LED_PIN	2

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "WifiPass.h" //define wifi SSID & pass


// GLOBALS /////////////////////////////////////////////////////////////////////////////////

float tempCelcius = 0.0f;
float humidity = 0.0f;
float loudness = 0.0f;
float light = 0.0f;
float tempCelcius2 = 0.0f;
float pressurePascal = 0.0f;

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
	server.send(200, "text/plain", GenerateMetrics() );
}


// RESET 
void(* resetFunc) (void) = 0;//declare reset function at address 0


void setup() {

	pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
	pinMode(A0, INPUT);
	
	Serial.begin(9600);
	Serial.println("----------------------------------------------\n");
	ID = String(ESP.getChipId(),HEX);
	Serial.printf("\nBooting %s ......\n", ID);
	Serial.printf("HasMic:%d  HasLight:%d  HasPressure:%d\n", MIC_ENABLED, LIGHT_ENABLED, PRESSURE_ENABLED);
	//Serial.setDebugOutput(true);
	

	WiFi.setPhyMode(WIFI_PHY_MODE_11G);
  	//WiFi.mode(WIFI_OFF);    //otherwise the module will not reconnect
  	WiFi.mode(WIFI_STA);    //if it gets disconnected
	WiFi.disconnect();
	WiFi.begin(ssid, password);
	Serial.printf("Trying to connect to %s ...\n", ssid);

	while (WiFi.status() != WL_CONNECTED) { // Wait for connection
		delay(250);
		Serial.print(".");
	}
	digitalWrite(LED_BUILTIN, HIGH); //turn off red led
	Serial.printf("\nConnected to %s IP addres %s\n", ssid, WiFi.localIP().toString().c_str());

	//WiFi.setAutoReconnect(true);
	//WiFi.persistent(true);
	//WiFi.forceSleepWake();
	//WiFi.setSleepMode(WIFI_NONE_SLEEP);

	if (MDNS.begin(ID)) {
		Serial.println("MDNS responder started " + ID);
	}

	server.on("/", handleRoot);
	server.on("/metrics", handleMetrics);
	server.begin();
	Serial.println("HTTP server started");

	// Add service to MDNS-SD
	MDNS.addService("http", "tcp", 80);

	#if PRESSURE_ENABLED
	if (!bmp.begin()) {
		Serial.println("Could not find a valid BMP085 sensor, check wiring! Resetting!");
		resetFunc(); //sensor is acting up - force hw reset!
  	}
  	#endif

	dht.begin();

	updateSensorData();
}


void loop() {

	delay(sleepMS); //once per second
	
	MDNS.update();
	updateSensorData();
	server.handleClient();
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

    #if (PRESSURE_ENABLED)
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

  return message;
}


void updateSensorData(){

	// Reading temperature or humidity takes about 250 milliseconds!
	// Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
	humidity = dht.readHumidity();
	tempCelcius = dht.readTemperature();

	if(isnan(humidity) || isnan(tempCelcius)){
		Serial.println("Can't read from sensor! Resetting!");
		resetFunc(); //sensor is acting up - force hw reset!
	}

	#if (MIC_ENABLED) //calc mic input gain //////////////////////	
		int mn = 1024;
		int mx = 0;
		for (int i = 0; i < 10; ++i) {
			int val = analogRead(A0);
			mn = min(mn, val);
			mx = max(mx, val);
			if(i % 500 == 1){
				yield();
			}
		}
		float vol = (mx - mn) / 1024.0f;
		if(loudness <= 0.001f){ //first value
			loudness = vol;
		}else{
			if(vol > loudness){ //quick raise
				loudness = vol;
			}else{ //slow decay
				loudness = 0.9f * loudness + 0.1f * vol;
			}			
		}		
	#endif

	#if (LIGHT_ENABLED) //calc mic input gain //////////////////////	
		light = analogRead(A0) / 1024.0f;
	#endif
	

	#if (PRESSURE_ENABLED)
	tempCelcius2 = bmp.readTemperature();
	pressurePascal = bmp.readPressure() / 100.0f;
	#endif
}
