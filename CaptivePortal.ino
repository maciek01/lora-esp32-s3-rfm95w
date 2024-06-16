
//WIFI
#include <WiFi.h>
#include <DNSServer.h>
#include <ESP32Ping.h>
#include <EMailSender.h>

//LORA
#include <SPI.h>
#include <LoRa.h>

// Define the pins used by the LoRa module
const int csPin = 4;     // LoRa radio chip select
const int resetPin = 3;  // LoRa radio reset
const int irqPin = 8;    // Must be a hardware interrupt pin

const int miso = 6;//MISO;
const int mosi = 5;//MOSI
const int sck = 7;

int loraUp = 1;
// Message counter
byte msgCount = 0;


//WIFI

const byte DNS_PORT = 53;
IPAddress apIP(8,8,4,4); // The default android DNS
DNSServer dnsServer;
WiFiServer server(80);

EMailSender emailSend("user1@kolesnik.biz", "changeme");


String responseHTML = ""
  "<!DOCTYPE html><html><head><title>CaptivePortal</title></head><body>"
  "<h1>Hello World!</h1><p>This is a captive portal example. All requests will "
  "be redirected here.</p></body></html>";

void setup() { 

  delay(3000);

  //start serial connection
  Serial.begin(9600);

  Serial.println("Configuring access point...");

  //WiFi.mode(WIFI_AP);
  WiFi.mode(WIFI_MODE_APSTA);

  if (!WiFi.softAP("FBI_Surveillance_Van_5")) {
    Serial.println("Soft AP creation failed.");
    while(1);
  }
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  Serial.println("access point created");

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);

  //wifi server on 80
  server.begin();


  //connect to router
  Serial.print("connecting to...");
  Serial.println("kolesnik-nest");

  WiFi.setHostname("loranode1");
  WiFi.begin("kolesnik-nest", "changeme");

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.print(" connected to router ");
  Serial.println(WiFi.localIP());

  testPing();

  //configure pin 2 as an input and enable the internal pull-up resistor
  pinMode(2, INPUT_PULLUP);
  pinMode(13, OUTPUT);


  //LORA

  // Setup LoRa module
  LoRa.setPins(csPin, resetPin, irqPin);
 
  Serial.println("LoRa Sender Test");
 
  // Start LoRa module at local frequency
  // 433E6 for Asia
  // 866E6 for Europe
  // 915E6 for North America
 
  SPI.begin(sck, miso, mosi, -1);//SS==CS?

  if (!LoRa.begin(915E6)) {
    Serial.println("Starting LoRa failed!");
    loraUp = 0;
  } else {
    Serial.println("LoRa is UP!");
  }

  // Set Receive Call-back function
  //LoRa.onReceive(onReceive);
 
  // Place LoRa in Receive Mode
  //LoRa.receive();

}

void testPing() {

  Serial.print("pinging 8.8.8.8, stat is ... ");
  int ret = Ping.ping("8.8.8.8", 5);
  Serial.println(ret);
  int avg_time_ms = Ping.averageTime();
  Serial.print("avg time ms ");
  Serial.println(avg_time_ms);
    if(ret) {
    Serial.println("Success!!");
  } else {
    Serial.println("Error :(");
  }

}


void sendEmail(String _message) {

  EMailSender::EMailMessage message;
  message.subject = "new connect";
  message.message = _message;

  EMailSender::Response resp = emailSend.send("maciek@kolesnik.biz", message);

  Serial.println("Sending status: ");

  Serial.println(resp.status);
  Serial.println(resp.code);
  Serial.println(resp.desc);

}

//capture time to we can mamage lora message interval
unsigned long myTime = millis();

//time led was last activated by LORA RX
unsigned long ledOn = millis();

void loop() {
  dnsServer.processNextRequest();
  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {

    String currentRequest = "";
    String currentLine = "";

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.print(responseHTML);
            break;
          } else {
            currentRequest += currentLine;
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }

    sendEmail(currentRequest);
    
    client.stop();
  }


  //read the pushbutton value into a variable
  int sensorVal = digitalRead(2);

  // Keep in mind the pull-up means the pushbutton's logic is inverted. It goes
  // HIGH when it's open, and LOW when it's pressed. Turn on pin 13 when the
  // button's pressed, and off when it's not:
  if (sensorVal == HIGH) {
    if (millis() - ledOn > 1000) {
      digitalWrite(13, LOW);
    }
  } else {
    digitalWrite(13, HIGH);
  }


  //LORA RX

  if (millis() - ledOn > 1000) {
    digitalWrite(13, LOW);
    ledOn = millis(); //to ensure we dont flip too often
  }

  //LORA TX

  if (millis() - myTime > 1000 && loraUp == 1) {
    Serial.print("Sending packet: ");
    Serial.println(msgCount);
  
    // Send packet
    LoRa.beginPacket();
    LoRa.print("Packet ");
    LoRa.print(msgCount);
    LoRa.endPacket();
  
    // Increment packet counter
    msgCount++;
  
    // 5-second delay
    //delay(5000);
    myTime = millis();
  }

  // parse for a packet, and call onReceive with the result:
  onReceive(LoRa.parsePacket());

}


// Receive Callback Function
void onReceive(int packetSize) {
  if (packetSize == 0) return;  // if there's no packet, return
 
  // Read packet header bytes:
  byte recipient = LoRa.read();        // recipient address
  byte sender = LoRa.read();          // sender address
  byte incomingMsgId = LoRa.read();   // incoming msg ID
  byte incomingLength = LoRa.read();  // incoming msg length
 
  String incoming = "";  // payload of packet
 
  while (LoRa.available()) {        // can't use readString() in callback, so
    incoming += (char)LoRa.read();  // add bytes one by one
  }
 
  // if (incomingLength != incoming.length()) {  // check length for error
  //  Serial.println("error: message length does not match length");
  //  return;  // skip rest of function
  // }
 
  //If the recipient isn't this device or broadcast,
  // if (recipient != localAddress && recipient != 0xFF) {
  //  Serial.println("This message is not for me.");
  //  return;  // skip rest of function
  // }
 
  // If message is for this device, or broadcast, print details:
  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Sent to: 0x" + String(recipient, HEX));
  Serial.println("Message ID: " + String(incomingMsgId));
  Serial.println("Message length: " + String(incomingLength));
  Serial.println("Message: " + incoming);
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println("Snr: " + String(LoRa.packetSnr()));
  Serial.println();
  
  // Drive LED
  digitalWrite(13, HIGH);
  ledOn = millis();

}
