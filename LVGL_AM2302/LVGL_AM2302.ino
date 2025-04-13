#include "DHT.h"
#include "Display_ST7789.h"
#include "LVGL_Driver.h"
#include "RGB_lamp.h"
#include "SD_Card.h"
#include "Wireless.h"

// DHT22 sensor setup
#define DHTPIN 4     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22 (AM2302), AM2321

// wifi cred
const char *ssid = "ssid";
const char *password = "password";

// server port number
WiFiServer server(80);

// non-blocking WiFi connection
WiFiEventId_t wifiConnectHandler;
bool wifiConnected = false;

// non-blocking client handling
WiFiClient client;
String currentLine = "";
String header = "";
unsigned long clientConnectTime = 0;
const long timeoutTime = 2000;
bool clientConnected = false;

// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Text objects for LVGL
lv_obj_t *temp_label;
lv_obj_t *temp_value;
lv_obj_t *humid_label;
lv_obj_t *humid_value;
lv_obj_t *heat_label;
lv_obj_t *heat_value;

// Variables to store sensor readings
float temperature = 0;
float humidity = 0;
float heatIndex = 0;

// sensor reading
bool readingInProgress = false;
unsigned long readStartTime = 0;
const long sensorReadInterval = 2000; // how often to read the sensor
unsigned long lastSensorReadTime = 0;

// Function prototypes
void createDHTDisplay(lv_obj_t *parent);
void updateSensorValues(lv_timer_t *timer);
void onWiFiConnected(WiFiEvent_t event, WiFiEventInfo_t info);
void handleClient();
void startSensorReading();
void finishSensorReading();

void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  Serial.println(F("DHT22 + ST7789 LCD Demo"));
  
  // Initialize DHT22 sensor
  dht.begin();
  
  // Initialize LCD
  LCD_Init();
  Set_Backlight(50); // Set brightness
  
  // RGB LED setup
  pinMode(PIN_NEOPIXEL, OUTPUT);
  
  // Initialize LVGL graphics library
  Lvgl_Init();
  
  // Create a display screen for sensor values
  lv_obj_t *main_screen = lv_scr_act();
  createDHTDisplay(main_screen);
  
  // Set up a timer to update the displayed values
  lv_timer_t *timer = lv_timer_create(updateSensorValues, 2000, NULL);
  
  // Initialize SD Card and test Flash
  SD_Init();
  Flash_test();
  
  // WiFi connection
  // https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiClientEvents/WiFiClientEvents.ino
  WiFi.mode(WIFI_STA);
  wifiConnectHandler = WiFi.onEvent(onWiFiConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  
  // Set RGB LED to blue initially
  Set_Color(0, 0, 255);
  
  // start the server only after WiFi is connected (handled by event)
}

void loop() {
  // LVGL handling
  Timer_Loop();
  
  // Cycle the RGB LED colors
  RGB_Lamp_Loop(50);

  // WiFi client handling
  if (wifiConnected) {
    if (!clientConnected) {
      // Check for new client
      client = server.accept();
      if (client) {
        Serial.println("New client connected");
        clientConnected = true;
        clientConnectTime = millis();
        currentLine = "";
        header = "";
      }
    } else {
      // handle existing client
      handleClient();
    }
  }
  
  // sensor reading at regular intervals
  unsigned long currentMillis = millis();
  if (!readingInProgress && currentMillis - lastSensorReadTime >= sensorReadInterval) {
    startSensorReading();
  }
  
  if (readingInProgress && currentMillis - readStartTime >= 250) {
    // DHT readings can take 250ms, check if enough time has passed
    finishSensorReading();
  }
}

// WiFi connected event handler
void onWiFiConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("Connected to WiFi!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  wifiConnected = true;
  server.begin();
  Serial.println("HTTP server started");
}

// Start the sensor reading process
void startSensorReading() {
  Serial.println("Starting sensor reading...");
  readingInProgress = true;
  readStartTime = millis();
}

// Complete the sensor reading process
void finishSensorReading() {
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();
  
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println(F("Failed to read from DHT sensor!"));
  } else {
    heatIndex = dht.computeHeatIndex(temperature, humidity, false);
    
    Serial.print(F("Humidity: "));
    Serial.print(humidity);
    Serial.print(F("%  Temperature: "));
    Serial.print(temperature);
    Serial.print(F("°C  Heat index: "));
    Serial.print(heatIndex);
    Serial.println(F("°C"));
    
    // update LVGL labels (will be handled by the updateSensorValues timer)
    
    // Adjust RGB LED color based on temperature
    if (temperature > 30) {
      Set_Color(255, 0, 0); // Red for hot
    } else if (temperature > 25) {
      Set_Color(255, 165, 0); // Orange for warm
    } else if (temperature > 20) {
      Set_Color(0, 255, 0); // Green for comfortable
    } else {
      Set_Color(0, 0, 255); // Blue for cool
    }
  }
  
  readingInProgress = false;
  lastSensorReadTime = millis();
}

void handleClient() {
  if (client.connected()) {
    unsigned long currentTime = millis();
    
    // check if client timed out
    if (currentTime - clientConnectTime > timeoutTime) {
      Serial.println("Client timeout!");
      client.stop();
      clientConnected = false;
      return;
    }
    
    if (client.available()) {
      char c = client.read();
      header += c;
      
      if (c == '\n') {
        if (currentLine.length() == 0) {
          // HTTP headers ended, send response
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println("Connection: close");
          client.println();
          
          // Display the HTML web page
          client.println("<!DOCTYPE html><html>");
          client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
          client.println("<link rel=\"icon\" href=\"data:,\">");
          
          client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
          client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
          client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
          client.println(".button2 {background-color: #555555;}</style></head>");
          
          // Web Page Heading
          client.println("<body><h1>ESP32 S3 Web Server</h1>");
          
          char buffer[100];
          sprintf(buffer, "<p>Humidity: %.2f %%</p>", humidity);
          client.println(buffer);
          sprintf(buffer, "<p>Temperature: %.2f &deg;C</p>", temperature);
          client.println(buffer);
          sprintf(buffer, "<p>Heat Index: %.2f &deg;C</p>", heatIndex);
          client.println(buffer);
          
          client.println("</body></html>");
          client.println();
          
          // Finish and close the connection
          client.stop();
          clientConnected = false;
          Serial.println("Client disconnected");
        } else {
          currentLine = "";
        }
      } else if (c != '\r') {
        currentLine += c;
      }
    }
  } else {
    // Client disconnected
    clientConnected = false;
  }
}

// Create the display layout
void createDHTDisplay(lv_obj_t *parent) {
  // Create a panel for the DHT sensor values
  lv_obj_t *panel = lv_obj_create(parent);
  lv_obj_set_size(panel, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
  
  // Title
  lv_obj_t *title = lv_label_create(panel);
  lv_label_set_text(title, "DHT22 Sensor");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
  
  // Temperature section
  temp_label = lv_label_create(panel);
  lv_label_set_text(temp_label, "Temp:");
  lv_obj_align(temp_label, LV_ALIGN_TOP_LEFT, 10, 40);
  
  temp_value = lv_label_create(panel);
  lv_label_set_text(temp_value, "---");
  lv_obj_align(temp_value, LV_ALIGN_TOP_RIGHT, -10, 40);
  
  // Humidity section
  humid_label = lv_label_create(panel);
  lv_label_set_text(humid_label, "Hum:");
  lv_obj_align(humid_label, LV_ALIGN_TOP_LEFT, 10, 80);
  
  humid_value = lv_label_create(panel);
  lv_label_set_text(humid_value, "---");
  lv_obj_align(humid_value, LV_ALIGN_TOP_RIGHT, -10, 80);
  
  // Heat Index section
  heat_label = lv_label_create(panel);
  lv_label_set_text(heat_label, "Heat Idx:");
  lv_obj_align(heat_label, LV_ALIGN_TOP_LEFT, 10, 120);
  
  heat_value = lv_label_create(panel);
  lv_label_set_text(heat_value, "---");
  lv_obj_align(heat_value, LV_ALIGN_TOP_RIGHT, -10, 120);
}

// Timer callback to update displayed sensor values
void updateSensorValues(lv_timer_t *timer) {
  // Update the display with current values
  char buffer[32];
  
  snprintf(buffer, sizeof(buffer), "%.1f °C", temperature);
  lv_label_set_text(temp_value, buffer);
  
  snprintf(buffer, sizeof(buffer), "%.1f %%", humidity);
  lv_label_set_text(humid_value, buffer);
  
  snprintf(buffer, sizeof(buffer), "%.1f °C", heatIndex);
  lv_label_set_text(heat_value, buffer);
}
