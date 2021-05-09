// Libraries
#include <SPI.h>
#include <Ethernet.h>
#include <aREST.h>
#include <avr/wdt.h>
#include <dht.h> // DHTLib
#include "SimpleTimer.h"

// Enter a MAC address for your controller below.
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x2D };
byte gateway[] = { 192, 168, 0, 1 }; //               <------- PUT YOUR ROUTERS IP Address to which your shield is connected Here
byte subnet[] = { 255, 255, 255, 0 }; //                <------- It will be as it is in most of the cases
// IP address in case DHCP fails
IPAddress ip(192,168,0,102);

// Ethernet server
EthernetServer server(80);

// Create aREST instance
aREST rest = aREST();

// Variables to be exposed to the API
#define TEMP_HUM_SENSOR1 2 // temp/hum sensor
#define POMP_RELAY 3 // waterPomp
#define LAMP_VEG_RELAY 5 // vegLamp
#define LAMP_FRUIT_RELAY 6 // fruitLamp
#define FAN_IN 44
#define FAN_OUT 45

dht DHT;

// define api variables
int temperature;
int humidity;
int soilMoisture1;
int soilMoisture2;
long waterLevel;
bool pompStatus = HIGH;
int fanInValue = 0;
int fanOutValue = 0;
bool lampVegStatus = HIGH;
bool lampFruitStatus = HIGH;
const int wetSensor = 560;
const int drySensor = 770;
const int pingPin = 9; // Trigger Pin of Ultrasonic Sensor
const int echoPin = 8; // Echo Pin of Ultrasonic Sensor
//fan constants range
const int maxFan = 255;

// define pomp variables
int countdownRange = 5; // interval in s rulare pompa
int countdownSleep = 8; // interval in s pauza pompa
int countdown = 0;
boolean pompFirstRun = false;

SimpleTimer timer;


void setup(void)
{
//seet D44 and d45 freq to 31h
TCCR5B = TCCR5B & B11111000 | B00000101; 
  // Start Serial
  Serial.begin(115200);
  // pin modes (output or input)
  pinMode(POMP_RELAY, OUTPUT);
  pinMode(LAMP_VEG_RELAY, OUTPUT);
  pinMode(LAMP_FRUIT_RELAY, OUTPUT);
  pinMode(FAN_IN, OUTPUT);
  pinMode(FAN_OUT, OUTPUT);


  //At start set the relay to off;
  digitalWrite(POMP_RELAY, HIGH);
  digitalWrite(LAMP_VEG_RELAY, HIGH);
  digitalWrite(LAMP_FRUIT_RELAY, HIGH);
  analogWrite(FAN_IN, fanInValue);
  analogWrite(FAN_OUT, fanOutValue);
  
  //Define variables Object
  // Give name & ID to the device (ID should be 6 characters long)
  rest.set_id("1");
  rest.set_name("greenhouse");
  rest.variable("temperature",&temperature);
  rest.variable("humidity",&humidity);
  rest.variable("water_level",&waterLevel);
  rest.variable("soil_moisture_1",&soilMoisture1);
  rest.variable("soil_moisture_2",&soilMoisture2);
  rest.variable("fan_in",&fanInValue);
  rest.variable("fan_out",&fanOutValue);
  rest.variable("pomp_off",&pompStatus);
  rest.variable("veg_lamp_off",&lampVegStatus);
  rest.variable("fruit_lamp_off",&lampFruitStatus);
  
  // Function to be exposed
  rest.function("sleep",sleep);
  rest.function("pomp",pomp);
  rest.function("fanIn",startInFan);
  rest.function("fanOut",startOutFan);
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

      int chk = DHT.read22(TEMP_HUM_SENSOR1); 
      int chk2 = DHT.read22(TEMP_HUM_SENSOR1); 
      
      temperature = (int)DHT.temperature;
      humidity = (int)DHT.humidity;
      soilMoisture1 = analogRead(A0);
      soilMoisture2 = analogRead(A2);
      waterLevel = measureDistance();
      pompStatus = digitalRead(POMP_RELAY);
      lampVegStatus = digitalRead(LAMP_VEG_RELAY);
      lampFruitStatus = digitalRead(LAMP_FRUIT_RELAY);
    }
  timer.run();
  rest.handle(client);
  wdt_reset();
}

int procentToValue(int procent){
    Serial.println(procent);
     Serial.println(maxFan);
    Serial.println((procent * maxFan) / 100 );
    return (procent / 100) * maxFan;
  }

int startInFan(String command){
  
  int value = command.toInt();
  int setValue = procentToValue(value);
  if(value >= 0 && value <= 255){
      analogWrite(FAN_IN,value);
      fanInValue = value;
      return 1;  
  }else{
      return 0;
  }
}

int startOutFan(String command){
  
  int value = command.toInt();
  int setValue = procentToValue(value);
    if(value >= 0 && value <= 255){
      analogWrite(FAN_OUT,value);
      fanOutValue = value;
      return 1;  
  }else{
      return 0;
  }
}

// Custom function accessible by the API
int sleep(String command){
  int comm = command.toInt();
  
  if(comm == 1){
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

void setPwmFrequency(int pin, int divisor) {
  byte mode;
  if(pin == 5 || pin == 6 || pin == 9 || pin == 10) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 64: mode = 0x03; break;
      case 256: mode = 0x04; break;
      case 1024: mode = 0x05; break; Serial.println("set to 1024");
      default: return;
    }
    if(pin == 5 || pin == 6) {
      TCCR0B = TCCR0B & 0b11111000 | mode;
    } else {
      TCCR1B = TCCR1B & 0b11111000 | mode;
    }
  } else if(pin == 3 || pin == 11) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 32: mode = 0x03; break;
      case 64: mode = 0x04; break;
      case 128: mode = 0x05; break;
      case 256: mode = 0x06; break;
      case 1024: mode = 0x07; break;
      default: return;
    }
    TCCR2B = TCCR2B & 0b11111000 | mode;
    
  }
}

long measureDistance(){
  
   long  duration;
   pinMode(pingPin, OUTPUT);
   digitalWrite(pingPin, LOW);
   delayMicroseconds(2);
   digitalWrite(pingPin, HIGH);
   delayMicroseconds(10);
   digitalWrite(pingPin, LOW);
   pinMode(echoPin, INPUT);
   duration = pulseIn(echoPin, HIGH);
   return microsecondsToMillimeters(duration);
}

long microsecondsToMillimeters(long microseconds)
{return microseconds / 2.9 / 2;}
