#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <DHT.h>

// OLED Includes
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const int DHTPIN = D4; //Initialize DHT11 Sensor Pin 

enum { DHTTYPE = DHT11 };
DHT dht(DHTPIN, DHTTYPE);

const char* AP_SSID = "Server_Monitoring_AP";
const char* AP_PASS = "cuadra1234";

const char* failedDataFile = "/failed_data.txt";
const char* serverPath = "http://192.168.4.2/server-room/api.php";

void saveFailedData(const String& data);
void resendStoredData();


const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 32;
const int OLED_RESET = -1;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

ESP8266WebServer server(80);

unsigned long previousMillis = 0;
const long interval = 2000; // Set Time Intervsl

float temperature = 0.0;
float humidity = 0.0;

void handleData() {
    if (!LittleFS.exists(failedDataFile)) {
        server.send(404, "text/plain", "File not found");
        return;
    }
    File file = LittleFS.open(failedDataFile, "r");
    server.streamFile(file, "text/plain");
    file.close();
}

void handleDelete() {
    if (LittleFS.exists(failedDataFile)) {
        LittleFS.remove(failedDataFile);
        server.send(200, "text/plain", "File deleted successfully");
    } else {
        server.send(404, "text/plain", "File not found");
    }
}

void handleDataRequest() {
    String jsonResponse = "{ \"temperature\": " + String(temperature, 1) + 
                          ", \"humidity\": " + String(humidity, 1) + " }";
    server.send(200, "application/json", jsonResponse);
}

void sendDataToServer() {
    HTTPClient http;
    WiFiClient client;
    
    String postData = "temperature=" + String(temperature, 1) + "&humidity=" + String(humidity, 1);

    http.begin(client, serverPath);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    int httpResponseCode = http.POST(postData);

    if (httpResponseCode > 0) {
        Serial.print("Server Response: ");
        Serial.println(http.getString());
        resendStoredData(); // Try resending any stored data
    } else {
        Serial.println("Error sending data. Storing for later.");
        saveFailedData(postData);
    }

    http.end();
}

void saveFailedData(const String& data) {
    File file = LittleFS.open(failedDataFile, "a");
    if (!file) {
        Serial.println("Failed to open file for writing!");
        return;
    }
    file.println(data);
    file.close();
    Serial.println("Failed data stored in LittleFS.");
}

void resendStoredData() {
    if (!LittleFS.exists(failedDataFile)) return;

    File file = LittleFS.open(failedDataFile, "r");
    if (!file) {
        Serial.println("Failed to open file for reading!");
        return;
    }

    String line;
    while (file.available()) {
        line = file.readStringUntil('\n');

        HTTPClient http;
        WiFiClient client;
        http.begin(client, serverPath);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");

        int httpResponseCode = http.POST(line);
        http.end();

        if (httpResponseCode > 0) {
            Serial.println("Resent stored data successfully!");
        } else {
            Serial.println("Failed to resend stored data. Keeping it for later.");
            file.close();
            return;
        }
    }

    file.close();
    LittleFS.remove(failedDataFile);
    Serial.println("All stored data sent and deleted.");
}

void setup() {
    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed!");
    } else {
        Serial.println("LittleFS mounted successfully.");
    }

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 OLED init failed!");
        while (true);
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 15);
    display.print("Initializing...");
    display.display();
    delay(1000);

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 15);
    display.print("Access Point Started");
    display.display();
    delay(5000);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);

    Serial.println("Access Point Started");
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 15);

    String ipAddress = "IP: " + WiFi.softAPIP().toString();
    display.println(ipAddress); 
    
    display.display();
    delay(1000);

    server.on("/index.php", HTTP_GET, handleDataRequest);
     server.on("/data", HTTP_GET, handleData);
    server.on("/delete", HTTP_GET, handleDelete);
    server.begin();
    Serial.println("Web server started!");

    dht.begin();
}

void updateOLED() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 10);
    display.print("Temp: ");
    display.print(temperature, 1);
    display.print(" C");

    display.setCursor(0, 20);
    display.print("Humidity: ");
    display.print(humidity, 1);
    display.print(" %");

    display.display();
}

void loop() {
    server.handleClient();
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        float newTemperature = dht.readTemperature();
        float newHumidity = dht.readHumidity();

        if (!isnan(newTemperature) && !isnan(newHumidity)) {
            temperature = newTemperature;
            humidity = newHumidity;

            Serial.printf("Temp: %.1f°C | Humidity: %.1f%%\n", temperature, humidity);

            updateOLED();
            sendDataToServer();
        } else {
            Serial.println("Failed to read from DHT sensor!");
        }
    }
}
