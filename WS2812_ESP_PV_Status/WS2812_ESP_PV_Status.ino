/***************************************************
  Photovoltaic Status Monitor

  shows your generate electric power, consumption and battery state on a led stripe.
  This version is optimized for 60 pixels (with 10 kwp, 20 for production and 20 for consumption)

  Used hardware:
    * Wemos D1 mini 
    * Led stripe WS2812 with 60 Leds
    * AC/DC converter -> 5VDC power supply (>1A recommened)
    
  The code below has been modified by arotyramel and is based on
  
  Adafruit MQTT Library ESP8266 Example

  Must use ESP8266 Arduino from:
    https://github.com/esp8266/Arduino

  Works great with Adafruit's Huzzah ESP board & Feather
  ----> https://www.adafruit.com/product/2471
  ----> https://www.adafruit.com/products/2821

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Tony DiCola for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/
#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <Adafruit_NeoPixel.h>
#include "WS2812_Definitions.h"


/************************* WiFi Access Point *********************************/

// #define WLAN_SSID       "<YOUR_SSID_"
// #define WLAN_PASS       "<YOUR_WIFI_PASSWORD>"

// /************************* Adafruit.io Setup *********************************/

// #define AIO_SERVER      "<YOUR_MQTT_SERVER>"
// #define AIO_SERVERPORT  1883                   // use 8883 for SSL
// #define AIO_USERNAME    "<MQTT_USER>"
// #define AIO_KEY         "<MQTT_PASSWORD>"
// otherwise include file to set these values
#include "config.h"

/************************* LED Setup *********************************/
#define MAX_LED_COUNT 60


/************ Global State (you don't need to change this!) ******************/

Adafruit_NeoPixel leds = Adafruit_NeoPixel(MAX_LED_COUNT, D2, NEO_GRB + NEO_KHZ800);
// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// or... use WiFiFlientSecure for SSL
//WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/

// Setup a feed called 'onoff' for subscribing to changes.
Adafruit_MQTT_Subscribe pv_production = Adafruit_MQTT_Subscribe(&mqtt, "pv/yield");
Adafruit_MQTT_Subscribe total_consumption = Adafruit_MQTT_Subscribe(&mqtt, "pv/load");
Adafruit_MQTT_Subscribe battery_state = Adafruit_MQTT_Subscribe(&mqtt, "pv/battery");

/*************************** Sketch Code ************************************/

void MQTT_connect();

void pvProductionCallback(double production){
  Serial.print("Production: ");
  Serial.println(production);
  // Set yellow leds - production - Leds 40-59
  production = production * 2 / 1000;
  int led_count = min(int(production), 20);
  int offset = 40;
  //FULL LIGHTED PIXELS
  for (int i = offset; i < led_count + offset; i = i + 1) {
    leds.setPixelColor(i, YELLOW);
  }
  // PARTLY LIGHTED PIXELS
  double dimmed_factor = production-led_count;
  leds.setPixelColor(led_count + offset, leds.Color(255 * dimmed_factor, 255 * dimmed_factor, 0));
  
  //BLACK PIXELS
  for (int i = led_count + offset + 1; i < 20; i = i + 1) {
    leds.setPixelColor(i, BLACK);
  }
};
void totalConsumptionCallback(double load){
  Serial.print("Load: ");
  Serial.println(load);
  // Set blue led - consumption - Leds 20-39
  load = load * 2 / 1000;
  int max_led_count = MAX_LED_COUNT / 3;
  int led_count = min(int(load), max_led_count);
  int offset = 20;
  for (int i = offset; i < led_count + offset; i = i + 1) {
    leds.setPixelColor(i, BLUE);
  }
  // PARTLY LIGHTED PIXELS
  double dimmed_factor = load-led_count;
  leds.setPixelColor(led_count + offset, leds.Color(0,0,255* dimmed_factor));

  for (int i = led_count + offset + 1; i < offset + max_led_count; i = i + 1) {
    leds.setPixelColor(i, BLACK);
  }
};
void batteryStateCallback(double battery_state){
  Serial.print("Battery State: ");
  Serial.println(battery_state);
  // Set green leds - battery - Leds 0-19
  battery_state = battery_state / 5;
  int led_count = min(int(battery_state),20);
  int offset = 0;
  for (int i = offset; i < led_count + offset; i = i + 1) {
    leds.setPixelColor(i, GREEN);
  }
  // PARTLY LIGHTED PIXELS
  double dimmed_factor = battery_state-led_count;
  leds.setPixelColor(led_count+offset, leds.Color(0, 128 * dimmed_factor, 0));

  for (int i = led_count + offset + 1; i < offset + 20; i = i + 1) {
    leds.setPixelColor(i, BLACK);
  }
};

void setup() {
  Serial.begin(115200);

  pinMode(D2, OUTPUT);
  digitalWrite(D2, LOW);

  leds.begin();  
  clearLEDs();
  leds.setPixelColor(0, RED);
  leds.show();


  delay(10);


  // Connect to WiFi access point.
  Serial.println(F("Power/Energy Status Monitor"));
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());

  pv_production.setCallback(pvProductionCallback);
  total_consumption.setCallback(totalConsumptionCallback);
  battery_state.setCallback(batteryStateCallback);

  // Setup MQTT subscription for onoff feed.
  mqtt.subscribe(&pv_production);
  mqtt.subscribe(&total_consumption);
  mqtt.subscribe(&battery_state);

}


void loop() {
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();

  // this is our 'wait for incoming subscription packets' busy subloop
  // try to spend your time here

  Adafruit_MQTT_Subscribe *subscription;
  mqtt.processPackets(10000);

  leds.setBrightness(128);
  leds.show();
  if(! mqtt.ping()) {
    mqtt.disconnect();
  }

}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      while (1);
    }
  }
  Serial.println("MQTT Connected!");
  clearLEDs();
}

void clearLEDs()
{
  for (int i = 0; i < MAX_LED_COUNT; i++)
  {
    leds.setPixelColor(i, 0);
  }
  
}
