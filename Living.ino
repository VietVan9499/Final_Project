/**************************** Library *********************************/
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <FastLED.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <Servo.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>         //https://github.com/knolleary/pubsubclient
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

/******************* HARDWARE PIN DEFINITIONS (Set to match your hardware)***********************/
#define  fan 3    // RX
#define  light 1 // TX
#define  lightSan D7
#define  switchLight D4
#define  switchFan D3
#define  switchSan D6
#define  door D2
#define  LED_PIN     D1
#define  NUM_LEDS    8

////config for Servo
Servo servo_1;       // servo controller (multiple can exist)
int servo_pin = D8; // PWM pin for servo control
int pos = 3;       // servo starting position
int angle = 178;
int angleOpen = 3;
int xxx = 0;
/// LED RGB
CRGB leds[NUM_LEDS];

// DHT 22
#include <DHT.h>
#define DHTPIN D5
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

/////// for sensor PM 2.5
int measurePin = A0;
int ledPower = D0;
/************* Flags and Counts***********************/

int checkOpenServo = 0;
int openServo = 0;
int count = 0;     //count the number of button presses to reset
int flag = 0;      //check MQTT connection status (When the broker fails, the application will constantly reconnect, resulting in hardware failure.)
int countR = 0;   //count reconnect
int flagMQTT = 0; //It reconnects repeatedly after 3 minutes of boot with no WiFi or MQTT connection.
/*************Checks***********************/
byte checkSensorDoor = 1;
int flagCheck = 0;
int dhtSensor, pmSensor = 0;
/////// for sensor PM 2.5
int samplingTime = 280;
int deltaTime = 40;
int sleepTime = 9620;
float voMeasured = 0;
float calcVoltage = 0;
int dustDensity = 0;
int   AQI = 0;
unsigned long voMeasuredTotal = 0;
int voCount = 0; //biến lấy mẫu
// Use the typical sensitivity in units of V per 100ug/m3.
const float K = 1;
float Voc = 0.6;

/*******--Begin-- For Servo **********/
unsigned elapsed_time = 0;
long previousMillis = 0;
long previousMillis2 = 0;
long servointerval = 15;
long interval = 1000;

/******** --End--  For Servo **********/

static uint32_t  currentmls = millis(), intervalPM = currentmls, interval_MQTT = intervalPM, intervalServo = interval_MQTT, timeConnectMqtt = 180000;

/************* MQTT TOPICS (change these topics as you wish)  **************************/
///COMMAND
#define command_topic_bulb_san "DoAn/LivingRoom/LightSan"
#define command_topic_bulb     "DoAn/LivingRoom/Light"
#define command_topic_fan      "DoAn/LivingRoom/Fan"
#define command_topic_door     "DoAn/LivingRoom/Door"
#define command_topic_servo    "DoAn/LivingRoom/Servo"
#define command_topic_rgb_reedbook "LEDS/Reed_Book"
#define command_topic_rgb_kara     "LEDS/Kara"
#define command_topic_rgb_sleep    "LEDS/Sleep"
#define command_topic_rgb_nomal    "LEDS/Normal"
#define command_topic_rgb_red      "LEDS/Red"
#define command_topic_rgb_off      "LEDS/OFF"


const char* open_door = "OPEN";
const char* close_door = "CLOSE";
const char* turn_on_bulb = "ON";
const char* turn_off_bulb = "OFF";
const char* turn_on_bulb_san = "ON";
const char* turn_off_bulb_san = "OFF";
const char* turn_on_fan = "ON";
const char* turn_off_fan = "OFF";
//// RGB
const char* turn_off_rgb = "OFF";
const char* on_rgb_readbook = "ON";
const char* on_rgb_kara = "ON";
const char* on_rgb_nomarl = "ON";
const char* on_rgb_sleep = "ON";
const char* on_rgb_red = "ON";
//// RGB

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
  if ( String(topic) == command_topic_servo ) {
    if ( ( String(payload_str) == open_door ) && (flagCheck == 0) ) {
      openServo = 0;
      flagCheck = 1;
    }
    else if ( (String(payload_str) == close_door) && (flagCheck == 1) ) {
      openServo = 1;
      flagCheck = 0;
    }
  }
  if ( String(topic) == command_topic_bulb_san ) {
    if (String(payload_str) == turn_on_bulb_san)
    {
      digitalWrite(lightSan, HIGH);
    }
    else if ( String(payload_str) == turn_off_bulb_san ) {
      digitalWrite(lightSan, LOW);
    }
  }
  if ( String(topic) == command_topic_bulb ) {
    if (String(payload_str) == turn_on_bulb)
    {
      digitalWrite(light, LOW);
    }
    else if ( String(payload_str) == turn_off_bulb ) {
      digitalWrite(light, HIGH);
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
  if (String(topic) == command_topic_rgb_off ) {
    if ( String(payload_str) == turn_off_rgb ) {
      for (int i = 0; i <= 7; i++) {
        leds[i] = CRGB (0, 0, 0);
        FastLED.show();
        delay(10);
      }
    }
  }
  if (String(topic) == command_topic_rgb_reedbook ) {
    if ( String(payload_str) == on_rgb_readbook ) {
      for (int i = 0; i <= 7; i++) {
        leds[i] = CRGB (128, 128, 0);
        FastLED.show();
      }
    }
  }
  if (String(topic) == command_topic_rgb_kara ) {
    if ( String(payload_str) == on_rgb_kara ) {
      for (int i = 0; i <= 7; i++) {
        leds[i] = CRGB (5, 1, 0);
        FastLED.show();
      }
    }
  }
  if (String(topic) == command_topic_rgb_sleep ) {
    if ( String(payload_str) == on_rgb_sleep) {
      for (int i = 0; i <= 7; i++) {
        leds[i] = CRGB (255, 0, 255);
        FastLED.show();
      }
    }
  }
  if (String(topic) == command_topic_rgb_nomal ) {
    if ( String(payload_str) == on_rgb_nomarl ) {
      for (int i = 0; i <= 7; i++) {
        leds[i] = CRGB (0, 25, 255);
        FastLED.show();
      }
    }
  }
  if (String(topic) == command_topic_rgb_red ) {
    if ( String(payload_str) == on_rgb_red ) {
      for (int i = 0; i <= 7; i++) {
        leds[i] = CRGB (255, 0, 0);
        FastLED.show();
      }
    }
  }
  else {
    Serial.print("I do not know what to do with ");
    Serial.print(String(payload_str));
    Serial.print(" on topic ");
    Serial.println( String(topic));
  }
}
/********************************** START SETUP*****************************************/
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("device is in Wake up mode");
  servo_1.attach(servo_pin); // start servo control
  dht.begin();
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  pinMode(light, OUTPUT);
  pinMode(lightSan, OUTPUT);
  pinMode(switchLight, INPUT_PULLUP);
  pinMode(switchFan, INPUT_PULLUP);
  pinMode(switchSan, INPUT_PULLUP);
  pinMode(door, INPUT_PULLUP);
  pinMode(fan, OUTPUT);
  pinMode(ledPower, OUTPUT);
  digitalWrite(light, HIGH);
  /*******--Begin-- For Servo **********/

  /******** --End--  For Servo **********/

  if ((digitalRead(switchLight) == 0 ) && (digitalRead(switchFan) == 0 )) {
    flag = 1;
  }
  //clean FS, for testing
  if ( ((digitalRead(switchLight) == 1 ) && (digitalRead(switchFan) == 1 )) || (digitalRead(switchLight) == 0 ) || (digitalRead(switchFan) == 0 ) )
  {
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
    //end read

    // The extra parameters to be configured (can be either global or just in the setup)
    // After connecting, parameter.getValue() will get you the configured value
    // id/name placeholder/prompt default length
    WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
    WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);
    WiFiManagerParameter custom_username("username", "username", username, 32);
    WiFiManagerParameter custom_password("password", "password", password, 32);
    WiFiManager wifiManager;
    // reset settings
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

    wifiManager.setTimeout(120);
    if (!wifiManager.autoConnect("Living Room")) {
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
      if (client.connect("Living", username, password)) { // username as client ID
        Serial.println("connected");
        client.subscribe(command_topic_bulb_san);
        client.subscribe(command_topic_bulb);
        client.subscribe(command_topic_fan);
        client.subscribe(command_topic_servo);
        client.subscribe(command_topic_rgb_reedbook);
        client.subscribe(command_topic_rgb_kara);
        client.subscribe(command_topic_rgb_sleep);
        client.subscribe(command_topic_rgb_nomal);
        client.subscribe(command_topic_rgb_red);
        client.subscribe(command_topic_rgb_off);
      }
      else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 2 seconds");
        // Wait 5 seconds before retrying
        countR++;
        delay(2000);
        if (countR == 10) {
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

void Light()
{
  if (digitalRead(switchLight) == 0)
  {
    delay(150);
    while (digitalRead(switchLight) == 0);
    if (digitalRead(light) == LOW) {
      digitalWrite(light, HIGH);
      client.publish("DoAn/LivingRoom/Light", "OFF" );
    }
    else {
      digitalWrite(light, LOW);
      client.publish("DoAn/LivingRoom/Light", "ON" );
      count = 0;
    }
  }
}

void Fan()
{
  if (digitalRead(switchFan) == 0)
  {
    Serial.println("Button Quat");
    delay(150);
    while (digitalRead(switchFan) == 0);
    if (digitalRead(fan) == LOW) {
      digitalWrite(fan, HIGH);
      client.publish("DoAn/LivingRoom/Fan", "ON" );
    }
    else {
      digitalWrite(fan, LOW);
      client.publish("DoAn/LivingRoom/Fan", "OFF" );
      count = 0;
    }
  }
}
void San()
{
  if (digitalRead(switchSan) == 0)
  {
    Serial.println("Button San");
    delay(150);
    while (digitalRead(switchSan) == 0);
    if (digitalRead(lightSan) == LOW) {
      digitalWrite(lightSan, HIGH);
      client.publish("DoAn/LivingRoom/LightSan", "ON" );
    }
    else {
      digitalWrite(lightSan, LOW);
      client.publish("DoAn/LivingRoom/LightSan", "OFF" );
      count = 0;
    }
  }
}
void openDoor()
{
  if ( (digitalRead(door) == 0) && (checkSensorDoor == 1))
  {
    client.publish("DoAn/LivingRoom/Door", "close" );
    Serial.print("close");
    checkSensorDoor = 0;
    count = 0;
  }
  if ( (digitalRead(door) == 1 ) && (checkSensorDoor == 0))
  {
    client.publish("DoAn/LivingRoom/Door", "open" );
    Serial.print("open");
    checkSensorDoor = 1;
    count = 0;
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
  int h = dht.readHumidity();

  // Read temperature as Celsius (the default)
  int t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  int f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  int hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  int hic = dht.computeHeatIndex(t, h, false);

  if (t != dhtSensor)
  {
    dhtSensor = t;
    Serial.print("Temperature in Celsius:");
    Serial.println(String(t).c_str());
    client.publish("DoAn/LivingRoom/dht/temproom", String(t).c_str(), true);
    Serial.print("Humidity:");
    Serial.println(String(h).c_str());
    client.publish("DoAn/LivingRoom/dht/humidityroom", String(h).c_str(), true);
  }
}

void PM()
{
  voCount = 0;
  voMeasuredTotal = 0;
  while (voCount <= 100) {
    digitalWrite(ledPower, LOW);     // Bật IR LED
    delayMicroseconds(samplingTime);   //Delay 0.28ms
    voMeasured = analogRead(measurePin); // Đọc giá trị ADC V0 mất khoảng 0.1ms
    digitalWrite(ledPower, HIGH);    // Tắt LED
    delayMicroseconds(sleepTime);     //Delay 9.62ms
    voMeasuredTotal += voMeasured;    // Tính tổng lần lấy mẫu
    voCount ++;              // Đếm số lần lấy mẫu
  }
  voMeasured = 1.0 * voMeasuredTotal / 100; //Tính trung bình
  //****************************

  calcVoltage = voMeasured / 1024 * 5; //Tính điện áp Vcc của cảm biến (5.0 hoặc 3.3)
  dustDensity = calcVoltage / K * 100.0;
  if (dustDensity != pmSensor)
  {
    pmSensor = dustDensity;
    AQI = dustDensity * 2;
    client.publish("DoAn/LivingRoom/PM", String(dustDensity).c_str());
    client.publish("DoAn/LivingRoom/AQI", String(AQI).c_str());
  }
}

void doorServo() {
  if (openServo == 0 && xxx == 0) {
    if ( angleOpen <= pos <= angle) {
      pos++;
      servo_1.write(pos);
      Serial.println("POS OPEN");
      Serial.println(pos);
      delay(0.8);
      if(pos == (angle - 1) ) {
        xxx = 1;
      }
    }
  }
  if (openServo == 1 &&  xxx == 1) {
    if ( angleOpen <= pos <= angle) {
      pos--;
      servo_1.write(pos);
      Serial.println("POS CLOSE");
      Serial.println(pos);
      delay(0.8);
       if( pos == (angleOpen + 1)) {
        xxx = 0;
      }
    }
  }
}
/********************************** START MAIN LOOP***************************************/
void loop() {
  Reset();
  San();
  Fan();
  Light();
  openDoor();
  doorServo();
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
