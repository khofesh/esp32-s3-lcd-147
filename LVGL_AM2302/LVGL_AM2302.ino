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

String header;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

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

// Function prototypes
void createDHTDisplay(lv_obj_t *parent);
void updateSensorValues(lv_timer_t *timer);

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
  
  // connect to wifi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }
  Serial.println(WiFi.localIP());
  server.begin();
  
  // Set RGB LED to blue initially
  Set_Color(0, 0, 255);
}

void loop() {
  WiFiClient client = server.accept();
  // LVGL handling
  Timer_Loop();
  
  // Cycle the RGB LED colors
  RGB_Lamp_Loop(50);

  if (client) 
  {
    showHttpToClient(&client);
  }
  
  // Give time for background tasks
  delay(5);
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

// Timer callback to update sensor values
void updateSensorValues(lv_timer_t *timer) {
  // Reading temperature and humidity
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();
  
  // Check if any reads failed
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  
  // Compute heat index
  heatIndex = dht.computeHeatIndex(temperature, humidity, false);
  
  // Update the display with new values
  char buffer[32];
  
  snprintf(buffer, sizeof(buffer), "%.1f °C", temperature);
  lv_label_set_text(temp_value, buffer);
  
  snprintf(buffer, sizeof(buffer), "%.1f %%", humidity);
  lv_label_set_text(humid_value, buffer);
  
  snprintf(buffer, sizeof(buffer), "%.1f °C", heatIndex);
  lv_label_set_text(heat_value, buffer);
  
  // Print values to serial monitor
  Serial.print(F("Humidity: "));
  Serial.print(humidity);
  Serial.print(F("%  Temperature: "));
  Serial.print(temperature);
  Serial.print(F("°C  Heat index: "));
  Serial.print(heatIndex);
  Serial.println(F("°C"));
  
  // Change RGB LED color based on temperature
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

void showHttpToClient(WiFiClient *client) {
  currentTime = millis();
  previousTime = currentTime;
  String currentLine = "";

  while (client->connected() && currentTime - previousTime <= timeoutTime) 
  {
    currentTime = millis();
    if (client->available()) 
    {
      char c = client->read();
      Serial.write(c);
      header += c;

      if (c == '\n')
      {
        if (currentLine.length() == 0)
        {
          client->println("HTTP/1.1 200 OK");
          client->println("Content-type:text/html");
          client->println("Connection: close");
          client->println();

          // Display the HTML web page
          client->println("<!DOCTYPE html><html>");
          client->println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
          client->println("<link rel=\"icon\" href=\"data:,\">");

          client->println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
          client->println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
          client->println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
          client->println(".button2 {background-color: #555555;}</style></head>");
            
          // Web Page Heading
          client->println("<body><h1>ESP32 S3 Web Server</h1>");

          char buffer[100];
          sprintf(buffer, "<p>Humidity: %.2f %%</p>", humidity);
          client->println(buffer);
          sprintf(buffer, "<p>Temperature: %.2f &deg;C</p>", temperature);
          client->println(buffer);
          sprintf(buffer, "<p>Heat Index: %.2f &deg;C</p>", heatIndex);
          client->println(buffer);

          client->println("</body></html>");

          client->println();
          break;
        }
        else {
          currentLine = "";
        }
      }
      else if (c != '\r') 
      {
        currentLine += c;
      }
    }
  }

  // Clear the header variable
  header = "";
  // Close the connection
  client->stop();
  Serial.println("Client disconnected.");
  Serial.println("");
}
