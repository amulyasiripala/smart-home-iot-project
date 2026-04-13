#include <WiFi.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define TRIG 9
#define ECHO 10
#define SERVO_PIN 6
#define DHTPIN 7
#define DHTTYPE DHT11
#define LDR A0
#define LED 4
#define BUZZER 8

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Servo doorServo;
DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const char* ssid = "Kalyan";
const char* password = "Kalyanpavan";
const char* mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient client(espClient);

String correctPassword = "1234";

bool doorOpen = false;
bool personDetected = false;

int wrongAttempts = 0;

long duration;
int distance;

unsigned long doorTimer = 0;
unsigned long messageTimer = 0;
bool messageActive = false;

void showIdleScreen() {

  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(30, 5);
  display.println("SMART LOCK");

  display.setTextSize(3);
  display.setCursor(45, 25);
  display.println(":)");

  display.display();
}

void showMessage(String msg) {

  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(10, 25);
  display.println(msg);

  display.display();

  messageTimer = millis();
  messageActive = true;
}

void setup_wifi() {

  Serial.println("Connecting WiFi...");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi Connected");
}

void callback(char* topic, byte* payload, unsigned int length) {

  String message = "";

  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == "smartlock/password") {

    if (message == correctPassword && personDetected) {

      doorServo.write(90);
      doorOpen = true;

      showMessage("WELCOME");

      wrongAttempts = 0;

      client.publish("smartlock/status", "Door Opened");
    }

    else {

      wrongAttempts++;

      showMessage("WRONG");

      client.publish("smartlock/status", "Wrong Password");

      if (wrongAttempts >= 3) {

        tone(BUZZER, 1000);
        delay(2000);
        noTone(BUZZER);

        showMessage("ALERT");

        client.publish("smartlock/status", "Intruder Alert");

        wrongAttempts = 0;
      }
    }
  }
}

void reconnect() {

  while (!client.connected()) {

    Serial.println("Connecting MQTT...");

    if (client.connect("SmartDoorClient")) {

      Serial.println("MQTT Connected");

      client.subscribe("smartlock/password");

    } else {

      Serial.print("MQTT Failed: ");
      Serial.println(client.state());

      delay(2000);
    }
  }
}

void setup() {

  Serial.begin(9600);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  doorServo.attach(SERVO_PIN);
  doorServo.write(0);

  dht.begin();

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  showIdleScreen();

  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  /* Ultrasonic Sensor */

  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG, LOW);

  duration = pulseIn(ECHO, HIGH, 20000);
  distance = duration * 0.034 / 2;

  /* LDR Light */

  int ldrValue = analogRead(LDR);

  if (ldrValue > 500) {

    digitalWrite(LED, HIGH);
    client.publish("smartlock/light", "ON");

  } else {

    digitalWrite(LED, LOW);
    client.publish("smartlock/light", "OFF");
  }

  /* Person detection */

  if (distance <= 10 && !personDetected) {

    personDetected = true;

    showMessage("ENTER");

    client.publish("smartlock/status", "Person Detected - Enter Password");
  }

  if (distance > 10 && personDetected) {

    personDetected = false;

    if (doorOpen) {
      doorTimer = millis();
    }
  }

  /* Door closing */

  if (doorOpen && !personDetected) {

    if (millis() - doorTimer > 2000) {

      doorServo.write(0);
      doorOpen = false;

      showMessage("BYE");

      client.publish("smartlock/status", "Door Closed");
    }
  }

  /* Temperature & Humidity */

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  String data = String(temp) + "," + String(hum);

  client.publish("smartlock/dht", data.c_str());

  /* OLED idle */

  if (messageActive && millis() - messageTimer > 5000) {

    messageActive = false;
    showIdleScreen();
  }

  delay(2000);
}