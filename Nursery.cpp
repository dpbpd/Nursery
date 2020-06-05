#include "Arduino.h"
#include <DHT.h> 
#include <Wire.h>
#include "ds3231.h"
#include <LiquidCrystal_I2C.h>

#define unusedUpstairsAnalogPin A1
#define unusedDigitalPinInHouse 52

#define unusedRelaySwitch 45 
#define Water 47
#define Light 49
#define Fan 51

#define SoilSensorPower 53
#define SSReadPin A15

#define DHTPIN 31         
#define DHTTYPE DHT22
#define PhotoResistor A14

// for the stepper motor
#define ENA 46
#define DIR 48
#define PUL 50

#define CalibrationButton 42

const int numOfSS = 6;
const int SSPins[numOfSS] = {43, 41, 39, 37, 35, 33};
const int PotPosition[numOfSS] = {20, 95, 125, 170, 240, 285};
const int waterNeeds[numOfSS] = {750, 750, 750, 750, 750, 750};
const int waterAmount[numOfSS] = {10000, 10000, 10000, 15000, 10000, 10000};

bool watered[numOfSS];
int soilSensorAve[3];
int soilSensorData[numOfSS];

bool dailyWatering = true;
bool checkAfterWatering = true;

int waterPosition;

uint8_t time[8];
unsigned long prev, interval = 1000;
int timeH;
int timeM;

LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
DHT dht(DHTPIN, DHTTYPE);

void setup() {
    
    Serial.begin(9600);
    while(!Serial);
    lcd.begin(20,4);
    dht.begin();
    Wire.begin();
    DS3231_init(DS3231_INTCN); // RTClock
    pinMode(Water, OUTPUT);
    digitalWrite(Water, HIGH);
    pinMode(Light, OUTPUT);
    digitalWrite(Light, HIGH);
    pinMode(Fan, OUTPUT);
    digitalWrite(Fan, HIGH);
    pinMode(SoilSensorPower, OUTPUT);
    digitalWrite(SoilSensorPower, LOW);
    pinMode(SSReadPin, INPUT); 

    //6 relay board
    digitalWrite(SoilSensorPower, HIGH);
    for(int i; i < numOfSS; i++){
        pinMode(SSPins[i], OUTPUT); 
        digitalWrite(SSPins[i], LOW);
        soilSensor(i);
        Serial.print("soilSensor ");
        Serial.print(i + 1);
        Serial.print(" ");
        Serial.print(soilSensorData[i]);
        Serial.println(" happened in setup");
    }
    //stepper motor{    
    pinMode(PUL, OUTPUT);
    pinMode(DIR, OUTPUT);
    pinMode(ENA, OUTPUT);
    pinMode(CalibrationButton, INPUT);
    //}

    calibrate();

}

void loop() {
    houseMain();
    nursery();
    displayData();
}
void displayData(){
    for(int i; i< numOfSS; i++){
        lcd.setCursor(0,3);
        lcd.print(i +1);
        lcd.print(": ");
        if(soilSensorData[i] < 1000){
            lcd.print(" ");
        }
        lcd.print(soilSensorData[i]);  
        delay(2500);
    }
}
void houseMain(){
    int houseTemp;
    unsigned long now = millis();
    struct ts t;
    
    DS3231_get(&t); //Get time
    timeH = t.hour; 
    timeM = t.min;
    houseTemp = DS3231_get_treg(); //Get temperature
    
    lcd.setCursor(0,0);
    lcd.print("HouseTemp ");  
    lcd.print(houseTemp);
    lcd.print((char)223);
    lcd.print("C ");
    if(t.hour<10) {
        lcd.print(" ");
    }
    lcd.print(timeH);
    lcd.print(":");
    if(t.min<10) {
        lcd.print("0");
    }
    lcd.print(t.min);

    prev = now;
}

void nursery(){
    lightTimer();
    dailyWater();
    int humidity = dht.readHumidity();
    int temp = dht.readTemperature();
    if(humidity == 0 || isnan(temp)){
        // sensor failure
    }
    lcd.setCursor(0,1);
    lcd.print("Nursery T ");
    lcd.print(temp);
    lcd.print((char)223);
    lcd.print("C ");
    lcd.setCursor(15,1);
    lcd.print("H ");
    lcd.print(humidity);
    lcd.println("%");
    
}
int lightTimer(){
    if(timeH >=3 && timeH <= 23){
        digitalWrite(Light, LOW);
        digitalWrite(Fan, LOW);
    } else {
        digitalWrite(Light, HIGH);
        digitalWrite(Fan, HIGH);
    }
    lcd.setCursor(0,2);
    lcd.print("Light "); 
    if(lightSensor() < 100){
        lcd.print("On "); 
    } else {
        lcd.print("Off");
    }
    if(lightSensor() < 100){
        // light is on
    } else {
        // light is off
    }
}

int lightSensor(){
    int v = analogRead(PhotoResistor);
    return v;
}

int soilSensor(int sensor){
    int sum = 0;
    digitalWrite(SoilSensorPower, HIGH);
    delay(1000);
    digitalWrite(SSPins[sensor], HIGH);
    delay(1000);
    for(int i = 0; i < 3; i++){
        sum = sum + analogRead(SSReadPin);
        delay(500);
    }
    soilSensorData[sensor] = sum / 3;
    digitalWrite(SSPins[sensor], LOW);
    digitalWrite(SoilSensorPower, LOW); 
    return soilSensorData[sensor];
}

void dailyWater(){
    if(timeH == 18 && dailyWatering){
        for(int i = 0; i < numOfSS; i++){
            int currentWaterAmount = soilSensor(i);
            int potPosition = PotPosition[i];
            Serial.print("soil sensor ");
            Serial.print(i + 1);
            Serial.print(" data: ");
            Serial.println(soilSensorData[i]);
            if(currentWaterAmount >= waterNeeds[i]){
                if(waterPosition > potPosition){
                    moveDir("right", waterPosition - potPosition);
                    water(waterAmount[i]);
                }else if(waterPosition < potPosition){
                    moveDir("left", potPosition - waterPosition);
                    water(waterAmount[i]);
                }else {
                    water(waterAmount[i]); 
                }
                watered[i] = true;
                soilSensor(i);
                Serial.print("watered sensor ");
                Serial.print(i + 1); 
                Serial.print(" New value ");
                Serial.println(soilSensorData[i]);
            } else {
                watered[i] = false;
            }          
        }
        dailyWatering = false;
    }
    if(timeH == 23){
        dailyWatering = true;
    }
}
void water(int amount){
    digitalWrite(Water, LOW);
    delay(amount);
    digitalWrite(Water, HIGH);
    delay(5000);
}

void calibrate(){
    int counter = 0;
    Serial.println("calibrate test 1");
    while(digitalRead(CalibrationButton) == LOW){
        Serial.println("calibrate test 2");
        moveDir("right",1);
        counter++;
    }
    moveDir("left", 5);
    Serial.print("calibration offset ");
    Serial.println(waterPosition - counter);
    
    waterPosition = 5;
    Serial.print("waterPosition ");
    Serial.println(waterPosition);
    Serial.print("counter ");
    Serial.println(counter);
}
void moveDir(String dir, int steps){
    
    if(dir == "left"){
        for(int i = 0; i < steps; i++){
            for (int i=0; i<400; i++){
                digitalWrite(DIR,HIGH);
                digitalWrite(ENA,HIGH);
                digitalWrite(PUL,HIGH);
                delayMicroseconds(50);
                digitalWrite(PUL,LOW);
                delayMicroseconds(50);
            } 
            waterPosition++;
        } 
        digitalWrite(ENA,LOW);
    }
    if(dir == "right"){
        for(int i = 0; i < steps; i++){
            for (int i=0; i<400; i++){
                digitalWrite(DIR,LOW);
                digitalWrite(ENA,HIGH);
                digitalWrite(PUL,HIGH);
                delayMicroseconds(50);
                digitalWrite(PUL,LOW);
                delayMicroseconds(50);
            }
            waterPosition--; 
        } 
        digitalWrite(ENA,LOW);
    }
}
