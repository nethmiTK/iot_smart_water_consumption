 #include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>   

#define FLOW_SENSOR_PIN D2  // GPIO4 (D2) of NodeMCU

volatile int pulseCount = 0;
float flowRate = 0.0;
float totalLiters = 0.0;
unsigned long oldTime = 0;
float dailyTotal = 0.0;

const char *ssid = "1234";
const char *password = "12345678";

// Firebase Credentials
#define FIREBASE_HOST "https://waterconsumptioniot-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "AIzaSyDY1fwkW1sVGytICfWd0Zn6he7Pp6u_Cws"

//kaweesha
//#define FIREBASE_HOST "https://iotproject4-4d100-default-rtdb.firebaseio.com/"
//#define FIREBASE_AUTH "AIzaSyBeYeDO5A3alzuaEa62NDiLaJgRiJ9L4xM"
//rash
//#define FIREBASE_HOST "https://smartflow-90170-default-rtdb.firebaseio.com/"
//#define FIREBASE_AUTH "AIzaSyCXDIZpz1__Ew-9dKAR409Nb1OvpbkK9aE"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 3600000); // 1-hour update interval

void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

void setup() {
  Serial.begin(9600);
  delay(100);


  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);

  connectWiFi();

  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  timeClient.begin();
  updateNTPTime(); // Initial time sync
}

void connectWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { // Limit retries
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi!");
  }
}

void updateNTPTime() {
  Serial.println("Updating NTP time...");
  if (!timeClient.update()) {
    Serial.println("Failed to get time from NTP!");
    return;
  }

  unsigned long epochTime = timeClient.getEpochTime();
  setTime(epochTime); // Set system time
  Serial.print("Updated Time: ");
  Serial.println(getFormattedDateTime());
}

String getFormattedDateTime() {
  unsigned long epochTime = timeClient.getEpochTime();
  if (epochTime == 0) return "Time_Error";

  setTime(epochTime);
  String formattedDate = String(day()) + "-" + String(month()) + "-" + String(year()) + "_" +
                         String(hour()) + "-" + String(minute()) + "-" + String(second());
  return formattedDate;
}

void loop() {
  if ((millis() - oldTime) > 1000) {  // Update every second
    detachInterrupt(FLOW_SENSOR_PIN);

    float litersPerSecond = (pulseCount / 450.0);
    flowRate = litersPerSecond * 60.0;  // Convert to L/min
    totalLiters += litersPerSecond;
    dailyTotal += litersPerSecond;

    Serial.print("Flow Rate: ");
    Serial.print(flowRate);
    Serial.println(" L/min");

    Serial.print("Total Volume: ");
    Serial.print(totalLiters);
    Serial.println(" Liters");

    pulseCount = 0;
    oldTime = millis();
    
    attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi Disconnected! Reconnecting...");
      connectWiFi();
    }

    if (WiFi.status() == WL_CONNECTED) {
      String dateTime = getFormattedDateTime();
      if (dateTime != "Time_Error") {
        Firebase.setFloat(fbdo, "/WaterFlow/FlowRate", flowRate);
        Firebase.setFloat(fbdo, "/WaterFlow/TotalVolume", totalLiters);
        Firebase.setFloat(fbdo, "/WaterFlow/DailyConsumption", dailyTotal);

        // Save historical data
        String historyPath = "/WaterFlow/History/" + dateTime;
        Firebase.setString(fbdo, historyPath + "/DateTime", dateTime);
        Firebase.setFloat(fbdo, historyPath + "/FlowRate", flowRate);
        Firebase.setFloat(fbdo, historyPath + "/TotalVolume", totalLiters);

        Serial.println("Data saved to Firebase: " + historyPath);
      } else {
        Serial.println("Invalid Time! Skipping Firebase update.");
      }
    }
  }
}
