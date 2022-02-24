/**************************** Library *********************************/
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>         //https://github.com/knolleary/pubsubclient
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <LiquidCrystal_I2C.h>
/******************* HARDWARE PIN DEFINITIONS (Set to match your hardware)***********************/
#define  fanKitchen 3    // RX
#define  lightKitchen 1 // TX
#define  bep D0 // 

#define  lightTho D7
#define  lightTolet D8

#define  lightTho3v D6
#define  mayNongLanh D5

#define  switchReset D3
#define  switchKhanCap D4
////LCD and MLX
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
LiquidCrystal_I2C lcd(0x27, 16, 2);

/************* Flags and Counts***********************/
int count = 0;     //count the number of button presses to reset
int flag = 0;      //check MQTT connection status (When the broker fails, the application will constantly reconnect, resulting in hardware failure.)
int countR = 0;   //count reconnect
int flagMQTT = 0; //It reconnects repeatedly after 3 minutes of boot with no WiFi or MQTT connection.
/************* SENSOR***********************/
int checkTempObjectKt = 0;
int checkAmbientTempKt = 0;
int oldTempAmbientKt = 0;
int oldTempObjectKt = 0;
int checkKhoiHuong = 0;

static uint32_t  currentmls = millis(), intervalPM = currentmls, interval_MQTT = intervalPM, intervalGas = interval_MQTT , timeConnectMqtt = 180000;

/************* MQTT TOPICS (change these topics as you wish)  **************************/
///COMMAND
#define command_topic_light_kitchen   "DoAn/Three/LightChitKen"
#define command_topic_fan_kitchen     "DoAn/Three/FanChitKen"
#define command_topic_bep             "DoAn/Three/Bep"
#define command_topic_light_dentho    "DoAn/Three/DenTho"
#define command_topic_light_Tolet     "DoAn/Three/LightTolet"
#define command_topic_light_DenTho_3V "DoAn/Three/LightDenTho3V"
#define command_topic_NongLanh        "DoAn/Three/MayNongLanh"
#define command_topic_KhanCap         "DoAn/Three/KhanCap"

const char* turn_on = "ON";
const char* turn_off = "OFF";

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

  if ( String(topic) == command_topic_fan_kitchen ) {
    if (String(payload_str) == turn_on)
    {
      digitalWrite(fanKitchen, HIGH);
    }
    else if ( String(payload_str) == turn_off ) {
      digitalWrite(fanKitchen, LOW);
    }
  }
  if ( String(topic) == command_topic_light_kitchen ) {
    if (String(payload_str) == turn_on)
    {
      digitalWrite(lightKitchen, HIGH);
    }
    else if ( String(payload_str) == turn_off ) {
      digitalWrite(lightKitchen, LOW);
    }
  }

  if ( String(topic) == command_topic_bep  ) {
    if (String(payload_str) == turn_on)
    {
      digitalWrite(bep, HIGH);
    }
    else if ( String(payload_str) == turn_off ) {
      digitalWrite(bep, LOW);
    }
  }

  if ( String(topic) == command_topic_light_dentho ) {
    if (String(payload_str) == turn_on)
    {
      digitalWrite(lightTho, HIGH);
    }
    else if ( String(payload_str) == turn_off ) {
      digitalWrite(lightTho, LOW);
    }
  }

  if ( String(topic) == command_topic_light_Tolet ) {
    if (String(payload_str) == turn_on)
    {
      digitalWrite(lightTolet, HIGH);
    }
    else if ( String(payload_str) == turn_off ) {
      digitalWrite(lightTolet, LOW);
    }
  }

  if ( String(topic) == command_topic_light_DenTho_3V ) {
    if (String(payload_str) == turn_on)
    {
      digitalWrite(lightTho3v, HIGH);
    }
    else if ( String(payload_str) == turn_off ) {
      digitalWrite(lightTho3v, LOW);
    }
  }

  if ( String(topic) == command_topic_NongLanh ) {
    if (String(payload_str) == turn_on)
    {
      digitalWrite(mayNongLanh, HIGH);
    }
    else if ( String(payload_str) == turn_off ) {
      digitalWrite(mayNongLanh, LOW);
    }
  }

  if ( String(topic) == command_topic_KhanCap ) {
    if (String(payload_str) == turn_on)
    {
      digitalWrite(switchKhanCap, HIGH);
    }
    else if ( String(payload_str) == turn_off ) {
      digitalWrite(switchKhanCap, LOW);
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
  lcd.begin();
  mlx.begin();
  pinMode(fanKitchen, OUTPUT);
  pinMode(lightKitchen, OUTPUT);
  pinMode(lightTho, OUTPUT);
  pinMode(lightTolet, OUTPUT);
  pinMode(bep, OUTPUT);
  pinMode(lightTho3v, OUTPUT);
  pinMode(mayNongLanh, OUTPUT);
  pinMode(switchReset, INPUT_PULLUP);
  pinMode(switchKhanCap, INPUT_PULLUP);
  count = 0;
  //SPIFFS.format();
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

  //clean FS, for testing

  //  wifiManager.resetSettings();


  wifiManager.setSaveConfigCallback(saveConfigCallback);
  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_username);
  wifiManager.addParameter(&custom_password);

  wifiManager.setTimeout(60);
  if (!wifiManager.autoConnect("THREE ROOM")) {
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
      if (client.connect("ThreeRom", username, password)) { // username as client ID
        Serial.println("connected");
        client.subscribe(command_topic_light_kitchen);
        client.subscribe(command_topic_fan_kitchen);
        client.subscribe(command_topic_bep);
        client.subscribe(command_topic_light_dentho);
        client.subscribe(command_topic_light_Tolet);
        client.subscribe(command_topic_light_DenTho_3V);
        client.subscribe(command_topic_NongLanh);
        client.subscribe(command_topic_KhanCap);
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



void Reset()
{
  if (digitalRead(switchReset) == 0 )
  {
    count++;
    Serial.print("Count:  ");
    Serial.print(count);
    if (count == 30) {
      client.publish("DoAn/Three/KhanCap", "OFF" );
      delay(1000);
      ESP.reset();
    }
  }
}

void Emergency()
{
  if (digitalRead(switchKhanCap) == 0)
  {
    delay(150);
    while (digitalRead(switchKhanCap) == 0);
    client.publish("DoAn/Three/KhanCap", "ON" );
  }
}
void KitChenTempObject() {
  int tempKtObject = mlx.readObjectTempC();
  if ( (tempKtObject != oldTempObjectKt ) &&  ( tempKtObject < 150 )) {
    oldTempObjectKt = tempKtObject;
    client.publish("DoAn/KitchenRoom/TempObject", String(tempKtObject).c_str());
  }
}

void KitChenTempAmbient() {
  // save the last time you updated the DHT values
  int tempKtAmbient = mlx.readAmbientTempC();
  if ( tempKtAmbient != oldTempAmbientKt ) {
    oldTempAmbientKt = tempKtAmbient;
    client.publish("DoAn/KitchenRoom/TempAmbient", String(tempKtAmbient).c_str());
    if ( 2 < tempKtAmbient < 22 ) {
      client.publish("DoAn/Bep/Sua", "on");
      lcd.setCursor(0, 1);
      lcd.print("SUA");
    }
    if (  5 <  tempKtAmbient < 25 ) {
      client.publish("DoAn/Bep/Rau", "on");
      lcd.setCursor(1, 5);
      lcd.print("RAU");
      if ( tempKtAmbient > 22) {
        client.publish("DoAn/Bep/Sua", "off");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("NHIET DO:");
        lcd.setCursor(10, 0);
        lcd.print(  tempKtAmbient);
        lcd.print("\337C");
        lcd.setCursor(0, 1);
        lcd.print("RAU");
      }
    }
    if ( 5 < tempKtAmbient < 27 ) {
      client.publish("DoAn/Bep/Food", "on");
      lcd.setCursor(9, 1);
      lcd.print("DO CHIN");
      if ( tempKtAmbient > 25) {
        client.publish("DoAn/Bep/Rau", "off");
        client.publish("DoAn/Bep/Sua", "off");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("NHIET DO:");
        lcd.setCursor(10, 0);
        lcd.print(  tempKtAmbient);
        lcd.print("\337C");
        lcd.setCursor(0, 1);
        lcd.print("RAU");
        lcd.setCursor(4, 1);
        lcd.print("DO CHIN");
      }
    }
    if (tempKtAmbient > 30) {
      lcd.setCursor(0, 0);
      lcd.print("NHIET DO:");
      lcd.setCursor(10, 0);
      lcd.print(tempKtAmbient);
      lcd.print("\337C");
      lcd.setCursor(0, 1);
      lcd.print("NHIET DO CAO !!!");
    }
  }
}

void KhoiHuong() {
  int value = analogRead(A0);
  if ( (value > 550) && (checkKhoiHuong == 0)) {
    client.publish("DoAn/Three/Huong", "on");
    checkKhoiHuong = 1;
  } else if ((value < 550) && (checkKhoiHuong == 1)) {
    client.publish("DoAn/Three/Huong", "off");
    checkKhoiHuong = 0;
  }
}
/********************************** START MAIN LOOP***************************************/
void loop() {
  Reset();
  Emergency();

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
  if (millis() - currentmls > 2000) {
    // save the last time you updated the DHT values
    currentmls = millis();
    KitChenTempObject();
  }

  if ( millis() - intervalPM > 2000 ) {
    // save the last time you updated the PM 2.5 values
    intervalPM = millis();
    KitChenTempAmbient();
  }

  if ( millis() - intervalGas > 2000 ) {
    // save the last time you updated the PM 2.5 values
    intervalGas = millis();
    KhoiHuong();
  }
}
