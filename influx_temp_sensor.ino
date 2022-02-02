#include "DHTesp.h"
#include <EEPROM.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_task_wdt.h>
#include <secrets.h>

#define WDT_TIMEOUT 100

DHTesp dht;

void tempTask(void *pvParameters);
bool getTemperature();
void triggerGetTemp();
String deviceName = "";

/** Task handle for the light value read task */
TaskHandle_t tempTaskHandle = NULL;

/** Comfort profile */
ComfortState cf;

/** Pin number for DHT22 data pin */
const int dhtPin = 27;

String readString(char add)
{
  int i;
  char data[100]; //Max 100 Bytes
  int len=0;
  unsigned char k;
  k=EEPROM.read(add);
  while(k != '\0' && len<500)   //Read until null character
  {    
    k=EEPROM.read(add+len);
    Serial.println(k);
    data[len]=k;
    len++;
  }
  Serial.println("null to finish");
  data[len]='\0';
  return String(data);
}

String translateEncryptionType(wifi_auth_mode_t encryptionType) {

  switch (encryptionType) {
    case (WIFI_AUTH_OPEN):
      return "Open";
    case (WIFI_AUTH_WEP):
      return "WEP";
    case (WIFI_AUTH_WPA_PSK):
      return "WPA_PSK";
    case (WIFI_AUTH_WPA2_PSK):
      return "WPA2_PSK";
    case (WIFI_AUTH_WPA_WPA2_PSK):
      return "WPA_WPA2_PSK";
    case (WIFI_AUTH_WPA2_ENTERPRISE):
      return "WPA2_ENTERPRISE";
  }
}

void scanNetworks() {

  int numberOfNetworks = WiFi.scanNetworks();

  Serial.print("Number of networks found: ");
  Serial.println(numberOfNetworks);

  for (int i = 0; i < numberOfNetworks; i++) {

    Serial.print("Network name: ");
    Serial.println(WiFi.SSID(i));

    Serial.print("Signal strength: ");
    Serial.println(WiFi.RSSI(i));

    Serial.print("MAC address: ");
    Serial.println(WiFi.BSSIDstr(i));

    Serial.print("Encryption type: ");
    String encryptionTypeDescription = translateEncryptionType(WiFi.encryptionType(i));
    Serial.println(encryptionTypeDescription);
    Serial.println("-----------------------");

  }
}

void connectToNetwork() {
  while (true){
    WiFi.setHostname(HOSTNAME);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
    for(int i = 0; i < 10; i++){
      if (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Establishing connection to WiFi.." + String(i));
        Serial.println(String(WiFi.status()));
      }
      else{
        Serial.println("Connected to network");
        return;   
      }
    }
  }
}

/**
   initTemp
   Setup DHT library
   Setup task and timer for repeated measurement
   @return bool
      true if task and timer are started
      false if task or timer couldn't be started
*/
bool initTemp() {
  byte resultValue = 0;
  // Initialize temperature sensor
  dht.setup(dhtPin, DHTesp::DHT22);
  Serial.println("DHT initiated");

  return true;
}

void sendDataToInflux(char* tag, float value) {
  int streamReturn;
  
  Serial.println("4: " + String(tag));
  Serial.println("5: " + String(value));
  if(WiFi.status() != WL_CONNECTED) {
    connectToNetwork();
  }
  Serial.println("verified connected to wifi");
  String debugString = "";
  debugString += String(tag);
  debugString += String(value);
  HTTPClient http;
  String serverPath = INFLUXDB_URL;
  debugString += ("start send: " + String(INFLUXDB_URL));
  Serial.println("6: " + debugString);
  http.begin(serverPath);
  debugString += ("begin done: " + String(serverPath));
  Serial.println("7: " + debugString);
  http.addHeader("Content-Type", "text/plain");
  debugString += ("header added");
  Serial.println("8: " + debugString);
  http.setAuthorization(INFLUXDB_USER, INFLUXDB_PASSWORD);
  debugString += ("auth set: " + String(INFLUXDB_USER) + ", " + String(INFLUXDB_PASSWORD));
  
  Serial.println("9: " + debugString);
  String influxData = "";
  influxData += "hrv " + String(tag) + "=" + String(value);

  debugString += ("data: " + String(influxData));
  
  Serial.println("10: " + debugString);

  // The first attempt to send returns -1
  int httpCode = -1;
  while(httpCode == -1){
    debugString += ("influx send loop: " + influxData);
    httpCode = http.POST(influxData);
    debugString += String(httpCode);
    Serial.println("11: " + debugString);
    debugString += ("post done");
    Serial.println("11.1: " + debugString);
    //streamReturn = http.writeToStream(&Serial);
    //debugString += String(streamReturn);
    Serial.println("12: " + debugString);
    /*
    
    debugString += ("httpCode: " + String(httpCode));
    String response = http.getString();
    debugString += ("response: " + response);
    
    Serial.println("3: " + debugString);

    if (httpCode != 204) {
      Serial.println(debugString);
    }
    else {
      Serial.println("204");
    }
    */
  }
  http.end();
  
  Serial.println("14: " + debugString);
}

void setup()
{
  EEPROM.begin(512);
  Serial.begin(115200);
  delay(2000);
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.println("--------- Starting ---------");
  //deviceName = readString(0);
  deviceName = "ngarage";
  Serial.println("deviceName: " + String(deviceName));
  initTemp();

  Serial.println("Scanning networks");
  scanNetworks();
  Serial.println("First connect to network");
  connectToNetwork();

  Serial.println("MAC Address");
  Serial.println(WiFi.macAddress());
  Serial.println("Local IP");
  Serial.println(WiFi.localIP());

  //  WiFi.disconnect(true);
  //  Serial.println("Disconnecting: " + String(WiFi.localIP()));

  // Wait 2 seconds to let system settle down
  delay(2000);

  Serial.println("Staring watchdog");
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch
}

void loop() {
  // Reading temperature for humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  TempAndHumidity newValues = dht.getTempAndHumidity();
  float temp = newValues.temperature * 9 / 5 + 32.0;
  String output = "Temp: " + String(temp) + "  Humidity: " + String(newValues.humidity);
  Serial.println(output);

  sendDataToInflux("ngarage_temp", temp);
  sendDataToInflux("ngarage_humidity", newValues.humidity);
  Serial.println("Resetting WDT");
  esp_task_wdt_reset();
  int sleepValue = 60000;
  
  Serial.println("waiting " + String(sleepValue));
  delay(sleepValue);
}
