#include <SPI.h>
#include <Ethernet.h>
#include <EthernetClient.h>
#include <PubSubClient.h>
#include <SD.h>
#include <Time.h>
#include <EthernetUdp.h>
#include <NTPClient.h>

// Define the MAC address, IP address, and MQTT broker details
byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
IPAddress ip(192, 168, 0, 105);
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "lcabs1993/arduino";
const char* mqtt_topic_sala = "lcabs1993/sala";
const char* mqtt_topic_escritorio = "lcabs1993/escritorio";
const char* mqtt_topic_quarto = "lcabs1993/quarto";

EthernetClient ethClient;
PubSubClient client(ethClient);

const int buzzerPin = 8; // Changed to pin 8
const int outputPin = 11; // Set as an output with initial LOW state
const int clappingSensorPin = 2; // Modify to the actual pin you're using for the sensor

// SD card
const int chipSelect = 4; // Change this to your SD card's chip select pin
File dataFile;

// NTP
EthernetUDP udp;
NTPClient ntpClient(udp, "pool.ntp.org", -3 * 3600, 60000); // GMT -3

bool clapDetected = false;
unsigned long lastPayloadTime = 0;

void setup() {
  // Start Serial communication
  Serial.begin(9600);

  // Initialize Ethernet and MQTT
  Ethernet.begin(mac, ip);
  delay(1500);  // Allow time for Ethernet to initialize
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Initialize the buzzer pin as an output (active HIGH)
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, HIGH); // Set the buzzer to initial HIGH state

  // Initialize the output pin as an output with initial LOW state
  pinMode(outputPin, OUTPUT);
  digitalWrite(outputPin, LOW); // Set the output pin to initial LOW state

  // Initialize the clapping sensor pin as an input
  pinMode(clappingSensorPin, INPUT);

  // Initialize the SD card
  if (SD.begin(chipSelect)) {
    Serial.println("SD card initialized.");
    // Beep the buzzer twice rapidly (active HIGH)
    beepBuzzer();
  } else {
    Serial.println("SD card initialization failed.");
    return;
  }

  // Connect to MQTT broker and subscribe to topics
  connectToMqtt();
}

void loop() {
  // Maintain MQTT connection
  if (!client.connected()) {
    connectToMqtt();
  }
  client.loop();

  // Check Ethernet connection and ping www.google.com
  if (Ethernet.linkStatus() == LinkON) {
    if (pingGoogle()) {
      // Ping successful, generate three short beeps (active HIGH)
      for (int i = 0; i < 3; i++) {
        beepBuzzer();
        delay(500); // Delay between beeps
      }
    }
  }

  // Update NTP client
  ntpClient.update();

  // Check for clapping sensor input
  int sensorValue = digitalRead(clappingSensorPin);
  if (sensorValue == HIGH) {
    // Clap detected, generate a short beep (active HIGH)
    beepBuzzer();
    clapDetected = true;
  }

  // Check if a payload with "salaoff" was received
  if (clapDetected && (millis() - lastPayloadTime) <= 10000) {
    // Within 10 seconds of receiving "salaoff," publish "salaon" to topic "lcabs1993/sala"
    client.publish(mqtt_topic_sala, "salaon");
    clapDetected = false; // Reset clap detection
  }
}

bool pingGoogle() {
  EthernetClient client;
  if (client.connect("www.google.com", 80)) {
    client.println("GET /search?q=arduino HTTP/1.0");
    client.println();
    return true;
  } else {
    return false;
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);

  // Activate the buzzer for a short beep (active HIGH)
  beepBuzzer();

  // Get the current timestamp
  lastPayloadTime = millis();

  Serial.print("Payload: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Write the payload and timestamp to the SD card
  dataFile = SD.open("mqtt_log.txt", FILE_WRITE);
  if (dataFile) {
    char timestamp[20];
    snprintf(timestamp, sizeof(timestamp), "[%lu] ", lastPayloadTime);
    dataFile.print(timestamp);
    dataFile.print("Topic: ");
    dataFile.println(topic);
    dataFile.print("Payload: ");
    for (int i = 0; i < length; i++) {
      dataFile.write(payload[i]);
    }
    dataFile.println();
    dataFile.close();
  } else {
    Serial.println("Error opening file.");
  }
}

void connectToMqtt() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ArduinoClient")) {
      Serial.println("Connected to MQTT");
      // Subscribe to additional topics
      client.subscribe(mqtt_topic_sala);
      client.subscribe(mqtt_topic_escritorio);
      client.subscribe(mqtt_topic_quarto);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void beepBuzzer() {
  digitalWrite(buzzerPin, HIGH);
  delay(100); // Beep duration
  digitalWrite(buzzerPin, LOW);
}
