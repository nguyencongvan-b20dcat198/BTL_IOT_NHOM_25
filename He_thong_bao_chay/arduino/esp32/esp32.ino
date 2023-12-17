#include <iostream>
#include <map>

#include "DHTesp.h"

#include <WiFi.h>
#include <ESP_Mail_Client.h>
#include <PubSubClient.h>

using MyMap = std::map<String, String>;

const int dht11_pin = 13;
     
const int gas_analog = 35;    
const int gas_digital = 34;   

const int pump_pin = 32;
const int fan_pin = 33;
const int bell_pin = 14;

const int green_pin = 25;
const int yellow_pin = 26;
const int red_pin = 27;

const String ssid = "nguyencongvan";
const String password = "van12345";

#define MQTT_SERVER "192.168.1.4"
#define MQTT_PORT 1883

#define SMTP_server "smtp.gmail.com"
#define SMTP_Port 587
#define sender_email "esp32wroom32u@gmail.com"
#define sender_password "btmcnrvdamxjfidk"
#define Recipient_email "vannguyencong2002@gmail.com"
#define Recipient_name ""

DHTesp dhtSensor;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

SMTPSession smtp;

unsigned long lastMsg = 0;

int totalTime = 0;

int pumpState = HIGH;
int fanState = HIGH;
int bellState = LOW;
int ok = 0;


void setup() {
    Serial.begin(115200);

    dhtSensor.setup(dht11_pin, DHTesp::DHT11);

    pinMode(gas_digital, INPUT);

    pinMode(pump_pin, OUTPUT);
    pinMode(fan_pin, OUTPUT);
    digitalWrite(pump_pin, HIGH);
    digitalWrite(fan_pin, HIGH);

    
    pinMode(bell_pin, OUTPUT);

    pinMode(green_pin,OUTPUT);
    digitalWrite(green_pin, HIGH);
    pinMode(yellow_pin,OUTPUT);
    pinMode(red_pin,OUTPUT);

    Serial.print("Connecting...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { 
      Serial.print(".");
      delay(500);
    }
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println();
  
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.connect("esp32");
    mqttClient.setCallback(sub_data);
}

void sub_data(char *topic, byte *payload, unsigned int length) {
  if (strcmp(topic, "button") == 0) {
    String s = "";
    for (int i = 0; i < length; i++)
      s += (char)payload[i];
    if (s.equalsIgnoreCase("pump|on")) {
      pumpState = LOW;
    }
    if (s.equalsIgnoreCase("pump|off")) {
      pumpState = HIGH;
    }
    if (s.equalsIgnoreCase("fan|on")) {
      fanState = LOW;
    }
    if (s.equalsIgnoreCase("fan|off")) {
      fanState = HIGH;
    }
    if (s.equalsIgnoreCase("bell|on")) {
      bellState = HIGH;
    }
    if (s.equalsIgnoreCase("bell|off")) {
      bellState = LOW;
    }
    digitalWrite(pump_pin, pumpState);
    digitalWrite(fan_pin, fanState);
    digitalWrite(bell_pin, bellState);
    Serial.println(s);
    mqttClient.publish("action", s.c_str());
  }
}


MyMap dht11Handle() {
    MyMap dht11Map;
    TempAndHumidity  data = dhtSensor.getTempAndHumidity();
    
    dht11Map["temp"] = String(data.temperature, 2);
    dht11Map["humi"] = String(data.humidity, 1);
    
    if (data.temperature > 50 && data.humidity < 30) {
      dht11Map["fire"] = "yes";
    }
    else {
      dht11Map["fire"] = "no";
    }
    
    return dht11Map;
}

MyMap mq2Handle() {
    MyMap mq2Map;
    int gassensorAnalog = analogRead(gas_analog);
    int gassensorDigital = digitalRead(gas_digital);
  
    mq2Map["gasSensor"] = String(gassensorAnalog);
    mq2Map["gasClass"] = String(gassensorDigital);
    
    if (gassensorAnalog > 1000) {
      mq2Map["gas"] = "yes";
    }
    else {
      mq2Map["gas"] = "no";
    }
    
    return mq2Map;
}

void emailHandle(MyMap dht11Map, MyMap mq2Map) {
    smtp.debug(1);
    ESP_Mail_Session session;
    session.server.host_name = SMTP_server ;
    session.server.port = SMTP_Port;
    session.login.email = sender_email;
    session.login.password = sender_password;
    session.login.user_domain = "";
    
    /* Declare the message class */
    
    SMTP_Message message;
    message.sender.name = "ESP32";
    message.sender.email = sender_email;
    message.subject = "Fire alarm";
    message.addRecipient(Recipient_name,Recipient_email);
    
     //Send HTML message
    
    String htmlMsg = "<div style='color:red;'><h1>Temperature: " + dht11Map["temp"] + "°C</h1><h1>Humidity: " + dht11Map["humi"] + "%</h1><h1>Mq2 sensor value: " + mq2Map["gasSensor"] + "</h1></div>";
    message.html.content = htmlMsg.c_str();
    message.html.content = htmlMsg.c_str();
    message.text.charSet = "us-ascii";
    message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
    
    if (!smtp.connect(&session)) {
      return;
    }
    
    if (!MailClient.sendMail(&smtp, &message))
      Serial.println("Error sending Email, " + smtp.errorReason());  
}

void redLight(int state) {
  digitalWrite(red_pin, state);
  digitalWrite(green_pin, !state);
  digitalWrite(yellow_pin, !state);
}

void greenLight(int state) {
  digitalWrite(red_pin, !state);
  digitalWrite(green_pin, state);
  digitalWrite(yellow_pin, !state);
}

void yellowLight(int state) {
  digitalWrite(red_pin, !state);
  digitalWrite(green_pin, !state);
  digitalWrite(yellow_pin, state);
}

void reconnect() { 
  Serial.println("Connecting to MQTT Broker...");
  while (!mqttClient.connected()) {
    Serial.println("Reconnecting to MQTT Broker...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
  
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("Connected");
    } 
  }
}


void loop() {

  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();
  mqttClient.subscribe("button");
  unsigned long now = millis();
  if(now - lastMsg > 2000) {
    lastMsg = now;
    MyMap dht11Map = dht11Handle();
    MyMap mq2Map = mq2Handle();
    
//      Serial.println("dht11: " + dht11Map["temp"] + "|" + dht11Map["humi"] + "|" + dht11Map["fire"]);
//      Serial.println("mq2: " + mq2Map["gasSensor"] + "|" + mq2Map["gasClass"] + "|" + mq2Map["gas"]);

    String data = "DHT11, MQ2|" + dht11Map["humi"] + "|" + dht11Map["temp"] + "|" + mq2Map["gasSensor"];
    Serial.println(data);
    mqttClient.publish("sensor", data.c_str());
    
    if(dht11Map["fire"] == "yes" || mq2Map["gas"] == "yes") {
      if(ok == 0) {
        ok = 1;
        redLight(1);
        mqttClient.publish("button", "bell|on");
        mqttClient.publish("button", "pump|on");
        mqttClient.publish("button", "fan|on");
      }
      
       totalTime += 2000;
        if(totalTime >= 60000) {
          totalTime = 0;
          yellowLight(1);
          emailHandle(dht11Map, mq2Map);
          redLight(1);
        }
      
    }else {
        totalTime = 0;

      if(ok == 1) {
        ok = 0;
        greenLight(1);
        mqttClient.publish("button", "bell|off");
        mqttClient.publish("button", "pump|off");
        mqttClient.publish("button", "fan|off");
      }
        
    }
  }
  
}