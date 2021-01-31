// Libraries
#include <SPI.h>
#include <Ethernet.h>
#include <aREST.h>
#include <avr/wdt.h>
#include <dht.h>
#include <ArduinoJson.h>

// Enter a MAC address for your controller below.
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x2D };
byte gateway[] = { 192, 168, 1, 254 }; //               <------- PUT YOUR ROUTERS IP Address to which your shield is connected Here
byte subnet[] = { 255, 255, 255, 0 }; //                <------- It will be as it is in most of the cases
// IP address in case DHCP fails
IPAddress ip(192,168,1,200);

// Ethernet server
EthernetServer server(80);

// Create aREST instance
aREST rest = aREST();

// Variables to be exposed to the API
#define TEMP_HUM_SENSOR 2 // temp/hum sensor
#define POMP_RELAY 3 // waterPomp
#define FAN_RELAY 4 // fan
#define LAMP_VEG_RELAY 5 // vegLamp
#define LAMP_FRUIT_RELAY 6 // fruitLamp

dht DHT;

// define variables
int temperature;
int humidity;
int soilMoisture;
bool pompStatus = HIGH;
bool fanStatus = HIGH;
bool lampVegStatus = HIGH;
bool lampFruitStatus = HIGH;

// Declare functions to be exposed to the API
int sonoffStatus(String command);

void setup(void)
{
  // Start Serial
  Serial.begin(115200);
  
  // pin modes (output or input)
  pinMode(POMP_RELAY, OUTPUT);
  pinMode(FAN_RELAY, OUTPUT);
  pinMode(LAMP_VEG_RELAY, OUTPUT);
  pinMode(LAMP_FRUIT_RELAY, OUTPUT);
  
  //At start set the relay to off;
  digitalWrite(POMP_RELAY, HIGH);
  digitalWrite(FAN_RELAY, HIGH);
  digitalWrite(LAMP_VEG_RELAY, HIGH);
  digitalWrite(LAMP_FRUIT_RELAY, HIGH);
  
  //Define variables Object
  rest.variable("temperature",&temperature);
  rest.variable("humidity",&humidity);
  rest.variable("soil_moisture",&soilMoisture);
  rest.variable("pomp_off",&pompStatus);
  rest.variable("fan_off",&fanStatus);
  rest.variable("veg_lamp_off",&lampVegStatus);
  rest.variable("fruit_lamp_off",&lampFruitStatus);
  // Function to be exposed
  rest.function("sonoff",sonoffStatus);

  // Give name & ID to the device (ID should be 6 characters long)
  rest.set_id("1");
  rest.set_name("greenhouse");

  // Start the Ethernet connection and the server
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    Ethernet.begin(mac, ip);
  }
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
  // Start watchdog
  wdt_enable(WDTO_4S);
}

void loop() {

  EthernetClient client = server.available();
  if (client) {

      int chk = DHT.read22(TEMP_HUM_SENSOR); 
      
      temperature = (int)DHT.temperature;
      humidity = (int)DHT.humidity;
      soilMoisture = analogRead(A3);
      pompStatus = digitalRead(POMP_RELAY);
      fanStatus = digitalRead(FAN_RELAY);
      lampVegStatus = digitalRead(LAMP_VEG_RELAY);
      lampFruitStatus = digitalRead(LAMP_FRUIT_RELAY);
      
    }
  rest.handle(client);
  wdt_reset();
}

// Custom function accessible by the API
int sonoffStatus(String command) {
 
  return 1;
}
