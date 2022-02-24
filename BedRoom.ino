/**************************** Library *********************************/
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include "MQ135.h"
#include <DHT.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>         //https://github.com/knolleary/pubsubclient
/******************* HARDWARE PIN DEFINITIONS (Set to match your hardware)***********************/
#define  fan          D0
#define  light         3 // RX
#define  sleep         1 // TX 
#define  switchLight  D4
#define  switchFan    D3
#define  switchSleep  D2
#define STEPPER_PIN_1 14
#define STEPPER_PIN_2 12
#define STEPPER_PIN_3 13
#define STEPPER_PIN_4 15
/**** Sensors *****/
#define DHTPIN  D1
#define DHTTYPE DHT11
#define PIN_MQ135     A0
DHT dht(DHTPIN, DHTTYPE);
MQ135 mq135_sensor = MQ135(PIN_MQ135);
/************* Flags and Counts***********************/
int step_number = 0;
int countOpen   = 100000;
int countClose  = 100000;
int checkBlink  = 0;
int checkBlink1 = 0;
int count    = 0;     //count the number of button presses to reset
int flag     = 0;      //check MQTT connection status (When the broker fails, the application will constantly reconnect, resulting in hardware failure.)
int countR   = 0;   //count reconnect
int flagMQTT = 0; //It reconnects repeatedly after 3 minutes of boot with no WiFi or MQTT connection.

static uint32_t  currentmls = millis(), intervalPM = currentmls, interval_MQTT = intervalPM, timeConnectMqtt = 180000;
/************* MQTT TOPICS (change these topics as you wish)  **************************/
/*********** COMMAND *********/
#define command_topic_bulb         "DoAn/BedRoom/Light"
#define command_topic_fan          "DoAn/BedRoom/Fan"
#define command_topic_light_sleep  "DoAn/BedRoom/Sleep"
#define command_topic_curtains     "DoAn/BedRoom/Curtains"

const char* turn_on_bulb = "ON";
const char* turn_off_bulb = "OFF";

const char* turn_on_fan = "ON";
const char* turn_off_fan = "OFF";

const char* turn_on_light_sleep = "ON";
const char* turn_off_light_sleep = "OFF";

const char* open_curtains = "Open";
const char* close_curtains = "Close";

/********************************** START SETUP*****************************************/
WiFiClient espClient2;
PubSubClient client(espClient2);
long lastMsg = 0;
char msg[50];
int value = 0;
//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40] = "vietnguyen.duckdns.org";
char mqtt_port[6] = "8889";
char username[34] = "vietnguyen";
char password[34] = "vanviet";
//flag for saving data
bool shouldSaveConfig = false;
//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}
/********************************** START MQTT CALLBACK*****************************************/

void callback(char* topic, byte* payload, unsigned int length) {
  char* payload_str;
  payload_str = (char*) malloc(length + 1);
  memcpy(payload_str, payload, length);
  payload_str[length] = '\0';
  Serial.println(String(payload_str));

  if ( String(topic) == command_topic_bulb ) {
    if (String(payload_str) == turn_on_bulb)
    {
      digitalWrite(light, HIGH);
    }
    else if ( String(payload_str) == turn_off_bulb ) {
      digitalWrite(light, LOW);
    }
  }
  if ( String(topic) == command_topic_fan ) {
    if (String(payload_str) == turn_on_fan)
    {
      digitalWrite(fan, HIGH);
    }
    else if ( String(payload_str) == turn_off_fan ) {
      digitalWrite(fan, LOW);
    }
  }
  if ( String(topic) == command_topic_light_sleep ) {
    if (String(payload_str) == turn_on_light_sleep)
    {
      digitalWrite(sleep, HIGH);
    }
    else if ( String(payload_str) == turn_off_light_sleep ) {
      digitalWrite(sleep, LOW);
    }
  }
  if ( String(topic) == command_topic_curtains ) {
    if (String(payload_str) == open_curtains)
    {
      countOpen = 0;
    }
    else if ( String(payload_str) == close_curtains ) {
      countClose = 0;
    }
  }

  else {
    Serial.print("I do not know what to do with ");
    Serial.print(String(payload_str));
    Serial.print(" on topic ");
    Serial.println( String(topic));
  }
}
void setup() {
  // put your setup code here, to run once:
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("device is in Wake up mode");
  Serial.println();
  dht.begin();
  pinMode(STEPPER_PIN_1, OUTPUT);
  pinMode(STEPPER_PIN_2, OUTPUT);
  pinMode(STEPPER_PIN_3, OUTPUT);
  pinMode(STEPPER_PIN_4, OUTPUT);
  pinMode(light, OUTPUT);
  pinMode(fan, OUTPUT);
  pinMode(sleep, OUTPUT);
  pinMode(switchLight, INPUT_PULLUP);
  pinMode(switchFan, INPUT_PULLUP);
  pinMode(switchSleep, INPUT_PULLUP);
  if ((digitalRead(switchLight) == 0 ) && (digitalRead(switchFan) == 0 )) {
    flag = 1;
  }
  if ( ((digitalRead(switchLight) == 1 ) && (digitalRead(switchFan) == 1 )) || (digitalRead(switchLight) == 0 ) || (digitalRead(switchFan) == 0 ) )
  {
    //clean FS, for testing
    SPIFFS.format();
    //read configuration from FS json
    Serial.println("mounting FS...");

    if (SPIFFS.begin()) {
      Serial.println("mounted file system");
      if (SPIFFS.exists("/config.json")) {
        //file exists, reading and loading
        Serial.println("reading config file");
        File configFile = SPIFFS.open("/config.json", "r");
        if (configFile) {
          Serial.println("opened config file");
          size_t size = configFile.size();
          // Allocate a buffer to store contents of the file.
          std::unique_ptr<char[]> buf(new char[size]);

          configFile.readBytes(buf.get(), size);
          DynamicJsonBuffer jsonBuffer;
          JsonObject& json = jsonBuffer.parseObject(buf.get());
          json.printTo(Serial);
          if (json.success()) {
            Serial.println("\nparsed json");

            strcpy(mqtt_server, json["mqtt_server"]);
            strcpy(mqtt_port, json["mqtt_port"]);
            strcpy(username, json["username"]);
            strcpy(password, json["password"]);

          } else {
            Serial.println("failed to load json config");
          }
        }
      }
    } else {
      Serial.println("failed to mount FS");
    }
    WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
    WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);
    WiFiManagerParameter custom_username("username", "username", username, 32);
    WiFiManagerParameter custom_password("password", "password", password, 32);
    WiFiManager wifiManager;
    //reset settings
    if ( (digitalRead(switchLight) == 0 ) || (digitalRead(switchFan) == 0 ) )
    {
      digitalWrite(light, HIGH);
      delay(1000);
      wifiManager.resetSettings();
      digitalWrite(light, LOW);
    }
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    //add all your parameters here
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_username);
    wifiManager.addParameter(&custom_password);

    wifiManager.setTimeout(60);
    if (!wifiManager.autoConnect("Bed Room")) {
      Serial.println("failed to connect and hit timeout");
      flag = 1;
    }
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");

    //read updated parameters
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port, custom_mqtt_port.getValue());
    strcpy(username, custom_username.getValue());
    strcpy(password, custom_password.getValue());
    //save the custom parameters to FS
    if (shouldSaveConfig) {
      Serial.println("saving config");
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      json["mqtt_server"] = mqtt_server;
      json["mqtt_port"] = mqtt_port;
      json["username"] = username;
      json["password"] = password;

      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        Serial.println("failed to open config file for writing");
      }

      json.printTo(Serial);
      json.printTo(configFile);
      configFile.close();
      //end save
    }

    Serial.println("local ip");
    Serial.println(WiFi.localIP());

    // mqtt
    client.setServer(mqtt_server, atoi(mqtt_port)); // parseInt to the port
    client.setCallback(callback);
  }
}
/********************************** START RECONNECT*****************************************/
void reconnect() {

  //attempt to connect to the wifi if connection is lost
  if (WiFi.status() != WL_CONNECTED) {
    //loop while we wait for connection
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }

  }
  if (WiFi.status() == WL_CONNECTED) {
    // Loop until we're reconnected to the MQTT server
    while (!client.connected()) {
      Serial.print("Attempting MQTT connection...");
      // Attempt to connect
      if (client.connect("BedRoom", username, password)) { // username as client ID
        Serial.println("connected");
        client.subscribe(command_topic_bulb);
        client.subscribe(command_topic_fan);
        client.subscribe(command_topic_light_sleep);
        client.subscribe(command_topic_curtains);
      }
      else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 2 seconds");
        // Wait 5 seconds before retrying
        countR++;
        delay(2000);
        if (countR == 5) {
          flag = 1;
          flagMQTT = 0;
          Serial.println("Flag=");
          Serial.print(flag);
          Serial.println("CountR=");
          Serial.print(countR++);
          break;
        }
      }
    }
  }
}

void Reset()
{
  if (  (digitalRead(switchLight) == 0 ) || (digitalRead(switchFan) == 0 )  || ((digitalRead(switchLight) == 0 ) && (digitalRead(switchFan) == 0 )) )
  {
    count++;
    Serial.print("Count:  ");
    Serial.print(count);
    if (count == 30) {
      ESP.reset();
    }
  }
}
void NhietDoAm()
{
  // Wait a few seconds between measurements.

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" *C ");
  Serial.print(f);
  Serial.print(" *F\t");
  Serial.print("Heat index: ");
  Serial.print(hic);
  Serial.print(" *C ");
  //  Serial.print(hif);
  //  Serial.println(" *F");
  Serial.print("Temperature in Celsius:");
  Serial.println(String(t).c_str());
  client.publish("DoAn/BedRoom/dht/temproom", String(t).c_str(), true);
  // Serial.print("Temperature in Fahrenheit:");
  // Serial.println(String(f).c_str());
  // client.publish(temperature_fahrenheit_topic, String(f).c_str(), true);
  Serial.print("Humidity:");
  Serial.println(String(h).c_str());
  client.publish("DoAn/BedRoom/dht/humidityroom", String(h).c_str(), true);
}

void PM()
{
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  Serial.println(h);
  float rzero = mq135_sensor.getRZero();
  float correctedRZero = mq135_sensor.getCorrectedRZero(t, h);
  float resistance = mq135_sensor.getResistance();
  float ppm = mq135_sensor.getPPM();
  float correctedPPM = mq135_sensor.getCorrectedPPM(t, h);
  Serial.print("\t Corrected PPM: ");
  Serial.print(correctedPPM);
  client.publish("DoAn/BedRoom/PM", String(correctedPPM).c_str());
  Serial.println("ppm");
}
/****** BUTTONS *******/
void Light()
{
  if (digitalRead(switchLight) == 0)
  {
    delay(150);
    while (digitalRead(switchLight) == 0);
    if (digitalRead(light) == LOW) {
      digitalWrite(light, HIGH);
      client.publish("DoAn/BedRoom/Light", "ON" );
    }
    else {
      digitalWrite(light, LOW);
      client.publish("DoAn/BedRoom/Light", "OFF" );
      count = 0;
    }
  }
}

void Fan()
{
  if (digitalRead(switchFan) == 0)
  {
    delay(150);
    while (digitalRead(switchFan) == 0);
    if (digitalRead(fan) == LOW) {
      digitalWrite(fan, HIGH);
      client.publish("DoAn/BedRoom/Fan", "ON" );
    }
    else {
      digitalWrite(fan, LOW);
      client.publish("DoAn/BedRoom/Fan", "OFF" );
      count = 0;
    }
  }
}
void sleepLight()
{
  if (digitalRead(switchSleep) == 0)
  {
    delay(150);
    while (digitalRead(switchSleep) == 0);
    if (digitalRead(sleep) == LOW) {
      digitalWrite(sleep, HIGH);
      client.publish("DoAn/BedRoom/Sleep", "ON" );
    }
    else {
      digitalWrite(sleep, LOW);
      client.publish("DoAn/BedRoom/Sleep", "OFF" );
      count = 0;
    }
  }
}
/******* Curtains ********/
void Curtains()
{
  if ( (countOpen <= 4800) && (checkBlink == 0))
  {
    countOpen++;
    Serial.println("Opem");
    Serial.println(countOpen);
    OneStep(false);
    delay(2);
    if (countOpen  >= 4800) {
      checkBlink = 1;
      countClose = 50000;
    }
  }
  if ( (countClose <= 6500) && (checkBlink == 1) )
  {
    countClose++;
    Serial.println("Close");
    Serial.println(countClose);
    OneStep(true);
    delay(2);
    if (countClose >= 6500) {
      checkBlink = 0;
      countOpen = 50000;
    }
  }
}
void OneStep(bool dir) {
  if (dir) {
    switch (step_number) {
      case 0:
        digitalWrite(STEPPER_PIN_1, HIGH);
        digitalWrite(STEPPER_PIN_2, LOW);
        digitalWrite(STEPPER_PIN_3, LOW);
        digitalWrite(STEPPER_PIN_4, LOW);
        break;
      case 1:
        digitalWrite(STEPPER_PIN_1, LOW);
        digitalWrite(STEPPER_PIN_2, HIGH);
        digitalWrite(STEPPER_PIN_3, LOW);
        digitalWrite(STEPPER_PIN_4, LOW);
        break;
      case 2:
        digitalWrite(STEPPER_PIN_1, LOW);
        digitalWrite(STEPPER_PIN_2, LOW);
        digitalWrite(STEPPER_PIN_3, HIGH);
        digitalWrite(STEPPER_PIN_4, LOW);
        break;
      case 3:
        digitalWrite(STEPPER_PIN_1, LOW);
        digitalWrite(STEPPER_PIN_2, LOW);
        digitalWrite(STEPPER_PIN_3, LOW);
        digitalWrite(STEPPER_PIN_4, HIGH);
        break;
    }
  } else {
    switch (step_number) {
      case 0:
        digitalWrite(STEPPER_PIN_1, LOW);
        digitalWrite(STEPPER_PIN_2, LOW);
        digitalWrite(STEPPER_PIN_3, LOW);
        digitalWrite(STEPPER_PIN_4, HIGH);
        break;
      case 1:
        digitalWrite(STEPPER_PIN_1, LOW);
        digitalWrite(STEPPER_PIN_2, LOW);
        digitalWrite(STEPPER_PIN_3, HIGH);
        digitalWrite(STEPPER_PIN_4, LOW);
        break;
      case 2:
        digitalWrite(STEPPER_PIN_1, LOW);
        digitalWrite(STEPPER_PIN_2, HIGH);
        digitalWrite(STEPPER_PIN_3, LOW);
        digitalWrite(STEPPER_PIN_4, LOW);
        break;
      case 3:
        digitalWrite(STEPPER_PIN_1, HIGH);
        digitalWrite(STEPPER_PIN_2, LOW);
        digitalWrite(STEPPER_PIN_3, LOW);
        digitalWrite(STEPPER_PIN_4, LOW);


    }
  }
  step_number++;
  if (step_number > 3) {
    step_number = 0;
  }
}
void loop() {
  // put your main code here, to run repeatedly:
  sleepLight();
  Reset();
  Curtains();
  Fan();
  Light();
    if (flag == 0)
  {
    if ( !client.connected()) { //NẾU HÀM BOOLE NGHĨA LÀ NẾU NGƯỜI LẠI CỦA CONECT THÌ RECONECT
      reconnect();
    }
    client.loop();
    countR = 0;
  }
  if ( ( millis() - interval_MQTT > timeConnectMqtt ) && ( flagMQTT == 0)) {
    interval_MQTT = millis();
    if (interval_MQTT > timeConnectMqtt)
    {
      timeConnectMqtt = 120000;
    }
    if (flag == 1) {
      flagMQTT = 1;
      countR = 0;
      Serial.println("MQTT RECONECT:");
      Serial.println(flagMQTT);
      Serial.println("CountR:");
      Serial.println(countR);
      flag = 0;
    } else {
      flagMQTT = 1;
    }
  }

  if (millis() - currentmls > 30000) {
    // save the last time you updated the DHT values
    currentmls = millis();
    NhietDoAm();
  }

  if ( millis() - intervalPM > 30000 ) {
    // save the last time you updated the PM 2.5 values
    intervalPM = millis();
    PM();
  }
}
