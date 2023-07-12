#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <max6675.h>
#include <TFT_eSPI.h>
#include <Button2.h>
#include <CountDown.h>

#define BUTTON_PIN_L 0   // Left Button
#define BUTTON_PIN_R 35  // Right Button
Button2 rightButton;
Button2 leftButton;

#define WIFI_RETRY_CONNECTION 10

int thermoDO = 26;
int thermoCS = 33;
int thermoCLK = 25;

MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

enum displayMode {
  info,
  count,
  temperatureOnly
};

TFT_eSPI tft = TFT_eSPI();
CountDown CD;

// WIFI Definitions
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

const int mqttPort = MQTT_PORT;
const char* mqttHost = MQTT_HOST;
const char* mqttUser = MQTT_USER;
const char *mqttPwd = MQTT_PASS;
const char* mqttTopic = MQTT_TOPIC;

AsyncMqttClient mqttClient;
static int previousTemperature = 0;
displayMode currentDisplayMode = info;
int maxCounterInSeconds = 0;
bool showCelsius = true;

void connectToMqtt()
{
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event)
{
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch (event)
  {
  case SYSTEM_EVENT_STA_GOT_IP:
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    connectToMqtt();
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    Serial.println("WiFi lost connection");

    break;
  }
}

void onMqttConnect(bool sessionPresent)
{
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);

}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  Serial.println("Disconnected from MQTT.");
}

void onMqttPublish(uint16_t packetId)
{
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}
 
void leftButtonPress(Button2 &btn)
{
  unsigned int time = btn.wasPressedFor();

  if (currentDisplayMode == temperatureOnly)
    currentDisplayMode = count;
  else
    currentDisplayMode = temperatureOnly;

  tft.setRotation(1); //0 - portrait   1 - landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(7);
}

void rightButtonPress(Button2 &btn)
{
  tft.fillScreen(TFT_BLACK);

  showCelsius = !showCelsius;
  
    tft.setCursor(5, 120);
    tft.setTextSize(1);
    tft.setTextFont(1);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    if (showCelsius)
      tft.println("CELSIUS");
    else
      tft.println("FAHRENHEIT");
}

void buttonPress(Button2 &btn)
{
  if (btn == rightButton)
  {
    Serial.println("RIGHT ");
    rightButtonPress(btn);
  }
  else if (btn == leftButton)
  {
    Serial.println("LEFT ");
    leftButtonPress(btn);
  }
  else
    Serial.println("NONE ");
}

void setup()
{
  Serial.begin(115200);

  leftButton.begin(BUTTON_PIN_L);
  leftButton.setPressedHandler(buttonPress);

  rightButton.begin(BUTTON_PIN_R);
  rightButton.setPressedHandler(buttonPress);

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);

  tft.print("Connecting to ");
  tft.println(ssid);

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onPublish(onMqttPublish);

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCredentials(mqttUser, mqttPwd);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int wifi_retry = 0;
  while (!WiFi.isConnected() && wifi_retry++ < WIFI_RETRY_CONNECTION)
  {
    Serial.printf(".");
    tft.print(".");
    delay(500);
  }

  if (WiFi.isConnected())
  {
    Serial.printf("WiFi Success\n");
    tft.print("Wifi Success");
  }
  else
  {
    ESP.restart();
  }

  tft.println("");
  tft.println("WiFi connected.");
  tft.println("IP address: ");
  tft.println(WiFi.localIP());
  tft.println(WiFi.macAddress());
  tft.println(WiFi.getHostname());

  Serial.println("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.macAddress());
  Serial.println(WiFi.getHostname());

  delay(2000);

  tft.setRotation(1); // 0 - portrait   1 - landscape
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);

  tft.setCursor(5, 30, 1);
  tft.print("IP: ");
  tft.println(WiFi.localIP());

  tft.setCursor(5, 50, 1);
  tft.print("Mac: ");
  tft.println(WiFi.macAddress());

  tft.setCursor(5, 70, 1);
  tft.print("Host: ");
  tft.println(WiFi.getHostname());

  CD.setResolution(CountDown::SECONDS);
  CD.start(0, 24, 0, 0);
  maxCounterInSeconds = CD.remaining();
  Serial.print("Seconds remaining: ");
  Serial.println(maxCounterInSeconds);
}

void displayTemperatureOnly(int currentTemperature, char cf, uint16_t temperatureColour)
{
  tft.fillScreen(TFT_BLACK);
  
  if (currentTemperature > 99.99)
    tft.setCursor(8, 10);
  else
    tft.setCursor(25, 10);
  tft.setTextFont(7);
  tft.setTextSize(2);
  tft.setTextColor(temperatureColour, TFT_BLACK);

  tft.fillRect(200, 50, 120, 90, TFT_BLACK);
  if (previousTemperature > currentTemperature)
    tft.fillTriangle(223, 100, 213, 60, 233, 60, TFT_BLUE);
  else if (previousTemperature < currentTemperature)
    tft.fillTriangle(223, 60, 213, 100, 233, 100, TFT_RED);
  else
      tft.fillRect(203, 75, 35, 10, TFT_ORANGE);
  tft.print(currentTemperature);

  previousTemperature = currentTemperature;

  tft.setTextSize(3);
  tft.setTextFont(2);
  tft.print("`");
  tft.println(cf);

  int bars = 9; // Ten bars for 10 seconds
  tft.fillRect(130, 120, 120, 12, TFT_BLACK);
  for (int i = 0; i < bars + 1; i++)
  {
    tft.fillRect(150 + (i * 7), 120, 3, 10, TFT_BROWN);
    delay(1000);
  }
  leftButton.loop();
  rightButton.loop();
}

void displayTimer()
{
  tft.fillScreen(TFT_BLACK);

  int remaining = maxCounterInSeconds - CD.remaining();
  Serial.print(remaining);
  int s = remaining % 60;

  remaining = (remaining - s) / 60;
  int m = remaining % 60;

  remaining = (remaining - m) / 60;
  int h = remaining;

  tft.setCursor(15, 35);
  tft.setTextFont(7);
  tft.setTextSize(1);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);

  tft.printf("%0.2d:%0.2d:%0.2d", h, m, s);
}

void loop() {
  float tempReadC = thermocouple.readCelsius();
  float tempReadF = thermocouple.readFahrenheit();

  Serial.print("C = ");
  Serial.println(tempReadC);

  char tempString[8];
  dtostrf(tempReadC, 1, 2, tempString);
  mqttClient.publish(mqttTopic, 1, true, tempString);

  leftButton.loop();
  rightButton.loop();

  Serial.print("currentDisplayMode ");
  Serial.println(currentDisplayMode);
  if (currentDisplayMode == temperatureOnly)
  {
    u_int16_t temperatureColour = TFT_YELLOW;
    if (showCelsius){
      if (tempReadC < 120)
        temperatureColour = TFT_YELLOW;
      else if (tempReadC >= 120 && tempReadC < 145)
        temperatureColour = TFT_ORANGE;
      else
        temperatureColour = TFT_RED;
      displayTemperatureOnly(tempReadC, 'C', temperatureColour);
    }
    else {
      if (tempReadF < 120)
        temperatureColour = TFT_YELLOW;
      else if (tempReadF >= 120 && tempReadF < 145)
        temperatureColour = TFT_ORANGE;
      else
        temperatureColour = TFT_RED;
      displayTemperatureOnly(tempReadF, 'F', temperatureColour);
    }
  }
  else if (currentDisplayMode == count)
  {
    displayTimer();
  }
  else {
    // Coming from the default "info" screen. Wait and then show temperature screen
    delay(2000);
    currentDisplayMode = temperatureOnly;
  }

  delay(1000);
}