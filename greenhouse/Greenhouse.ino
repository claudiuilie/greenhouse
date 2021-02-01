// Libraries
#include <SPI.h>
#include <Ethernet.h>
#include <aREST.h>
#include <avr/wdt.h>
#include <dht.h>
#include <ArduinoJson.h>
#include <SimpleTimer.h>

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

// define api variables
int temperature;
int humidity;
int soilMoisture;
bool pompStatus = HIGH;
bool fanStatus = HIGH;
bool lampVegStatus = HIGH;
bool lampFruitStatus = HIGH;

// define soil moisture sensor range
const int dry = 650;
const int wet = 530;

// define pomp variables
int countdownRange = 5; // interval in s rulare pompa
int countdownSleep = 8; // interval in s pauza pompa
int countdown = 0;
boolean pompFirstRun = false;

SimpleTimer timer;


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
  // Give name & ID to the device (ID should be 6 characters long)
  rest.set_id("1");
  rest.set_name("greenhouse");
  rest.variable("temperature",&temperature);
  rest.variable("humidity",&humidity);
  rest.variable("soil_moisture",&soilMoisture);
  rest.variable("pomp_off",&pompStatus);
  rest.variable("fan_off",&fanStatus);
  rest.variable("veg_lamp_off",&lampVegStatus);
  rest.variable("fruit_lamp_off",&lampFruitStatus);
  
  // Function to be exposed
  rest.function("sleep",sleep);
  rest.function("pomp",pomp);
  rest.function("fan",fan);
  rest.function("vegPhase",lampVegPhase);
  rest.function("fruitPhase",lampFruitPhase);

  
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
      soilMoisture = analogRead(A0);
      pompStatus = digitalRead(POMP_RELAY);
      fanStatus = digitalRead(FAN_RELAY);
      lampVegStatus = digitalRead(LAMP_VEG_RELAY);
      lampFruitStatus = digitalRead(LAMP_FRUIT_RELAY);
      
    }
  timer.run();
  rest.handle(client);
  wdt_reset();
}

// Custom function accessible by the API
int sleep(String command){
  int comm = command.toInt();
  
  if(comm == 1){
      digitalWrite(FAN_RELAY, HIGH);
      delay(50);
      digitalWrite(LAMP_VEG_RELAY, HIGH);
      delay(50);
      digitalWrite(LAMP_FRUIT_RELAY, HIGH);
      delay(50);
      return 1;
    }else{
      return 0;   
   }
  }

int pomp(String command) {
  //example ?params=1&10:5 (status&runtime:sleeptime)
  bool pompOff = digitalRead(POMP_RELAY); 

  String xval = getValue(command, '&', 0);
  String yval = getValue(command, '&', 1);
  String x = getValue(yval, ':', 0);
  String y = getValue(yval, ':', 1);
  int comm = xval.toInt();

  switch(comm){

    case 0:
      digitalWrite(POMP_RELAY, HIGH);
      timer.disable(0);
      timer.disable(1);
      timer.disable(2);
      pompFirstRun = false;
      countdown = 0;
      Serial.println("Pomp stopped.");
      return 1;
    break;

    case 1:
      if(pompOff) {
          countdownSleep = y.toInt();
          countdownRange = x.toInt();
          startPomp();
        return 1;
      }else{
        Serial.println("Pomp already active!");
        return 0;
      }
    break;

    default:
      return 0;
    }
}

void runPomp(){

  if(countdown < countdownRange - 1){
    countdown += 1;
    Serial.print("Running: "); 
    Serial.print(countdown);
    Serial.print(" s");
    Serial.println();
  }else{
    Serial.println("Timer exausted.");
    if (pompFirstRun == false){
      countdown = 0;
      pompFirstRun = true;
      stopPomp();
      timer.setTimer(1000, sleepPomp, countdownSleep );
    }else{
      stopPomp();
      countdown = 0;
      pompFirstRun = false;
    }
    Serial.println(countdown);
  }
}

void sleepPomp() {
  
  if(countdown < countdownSleep - 1){
    countdown += 1;
    Serial.print("Sleeping: "); 
    Serial.print(countdown);
    Serial.print(" s");
    Serial.println();
  }else{
    Serial.println("Timer exausted.");
    countdown = 0;
    startPomp();
  }
}

void startPomp() {
  
  int pompOff = digitalRead(POMP_RELAY);
  if(pompOff && countdown == 0){
       timer.setTimer(1000, runPomp, countdownRange );
       digitalWrite(POMP_RELAY,LOW);
       Serial.println("Pomp Started");
    }else{ 
    Serial.println("Pomp is already active");  
  }
}

void stopPomp() {
  digitalWrite(POMP_RELAY,HIGH);
  Serial.println("Pomp Stopped");
}


int fan(String input){  
  int comm = input.toInt();
  switch(comm){
      case 0:
        digitalWrite(FAN_RELAY, HIGH);
        return 1;
      break;
        
      case 1:
        digitalWrite(FAN_RELAY, LOW);
        return 1;
      break;

      default:
        return 0;
    }
}

int lampVegPhase(String command){
    int comm = command.toInt();
    
    digitalWrite(LAMP_VEG_RELAY, HIGH);
    digitalWrite(LAMP_FRUIT_RELAY, HIGH);
    
    switch(comm){
      case 0:
        digitalWrite(LAMP_VEG_RELAY, HIGH);
        return 1;
      break;

      case 1:
        digitalWrite(LAMP_VEG_RELAY, LOW);
        return 1;
      break;
      
      default:
        return 0;
    }
}

int lampFruitPhase(String command){
    int comm = command.toInt();
    
    switch(comm){
      case 0:
        digitalWrite(LAMP_FRUIT_RELAY, HIGH);
        return 1;
      break;

      case 1:
        digitalWrite(LAMP_FRUIT_RELAY, LOW);
        digitalWrite(LAMP_VEG_RELAY, LOW);
        return 1;
      break;
      
      default:
        return 0;
    }
}

String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
