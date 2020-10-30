//temp sensor
#include "DHT.h"
#define DHTPIN 14     // what digital pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

// GLOBALS /////////////////////////////////////////////////////////////////////////////////

float tempCelcius = 0.0f;
float humidity = 0.0f;
static int count = 0;
int countMax = 1; //min - interval to send temp & humidity http updates to server


//global devices
DHT dht(DHTPIN, DHTTYPE);

// WIFI ////////////////////////////////////////////////////////////////////////////////////

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include "WifiPass.h" //define wifi SSID & pass
//const char* ssid = "XXXXX";
//const char* password = "XXXXX";

ESP8266WebServer server(80);

void handleRoot() {
	digitalWrite(LED_BUILTIN, LOW); //note on the huzza LED low is high!
	static char json[64];
	sprintf(json, "{\"temperature\":%f, \"humidity\":%f}", tempCelcius, humidity);
	server.send(200, "application/json", json);
	digitalWrite(LED_BUILTIN, HIGH);
}

// HTTP /////////////////////////////////////////////////////////////////////////////////////

#include <ESP8266HTTPClient.h>

/////////////////////////////////////////////////////////////////////////////////////////////


// RESET 
void(* resetFunc) (void) = 0;//declare reset function at address 0


void setup() {

	pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
	digitalWrite(LED_BUILTIN, HIGH);
	
	Serial.begin(115200);
	Serial.println("----------------------------------------------\n");

	dht.begin();

	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);

	while (WiFi.status() != WL_CONNECTED) { // Wait for connection
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.print("Connected to ");
	Serial.println(ssid);
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	if (MDNS.begin("tempSensor")) {
		Serial.println("MDNS responder started");
	}

	// Add service to MDNS-SD
	MDNS.addService("http", "tcp", 80);

	server.on("/", handleRoot);
	server.begin();
	Serial.println("HTTP server started");

	updateSensorData();
	sendHttpData();
}


void loop() {

	delay(1000); //once per second

	updateSensorData();
	server.handleClient();

	count++;
	if (count >= countMax * 10 || count == 0) { //every ~30 minutes, ping server with data
		count = 1;
		sendHttpData();
	}
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
}

void sendHttpData() {

	bool ok = false;
	HTTPClient http;
	char url[255];
	sprintf(url, "http://10.0.0.100:8080/sensorData/send?temp=%.1f&hum=%.1f", tempCelcius, humidity);
	//Serial.println(url);
	http.begin(url);
	int httpCode = http.GET();

	// httpCode will be negative on error
	if (httpCode > 0) {
		// HTTP header has been send and Server response header has been handled
		Serial.print("http code: ");
		Serial.println(httpCode);

		// file found at server
		if (httpCode == HTTP_CODE_OK) {
			String payload = http.getString();
			Serial.print("Response: ");
			Serial.println(payload);
			ok = true;
		}
	} else {
		Serial.print("[HTTP] GET... failed, error: ");
		Serial.println(http.errorToString(httpCode).c_str());
	}

	http.end();
}
