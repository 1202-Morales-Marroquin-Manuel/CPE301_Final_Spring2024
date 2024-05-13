#include <I2C_RTC.h> // RTC Library
#include <dht.h> // Humidty/Temp Sensor Library
#include <LiquidCrystal.h> // LCD Screen
#include <Stepper.h> // Stepper motor library

// Serial Variables
#define RDA 0x80
#define TBE 0x20  

enum state { error = 0, disabled = 1, idle = 2, running = 3 };
enum led { red = 0, yellow = 1, green = 2, blue = 3 };
enum button { start = 5, stop = 6, reset = 7 };

// Useful for Displaying Current State (Prevent using IF statements)
char statesString[4][9] = {"Error\0", "Disabled\0", "Idle\0", "Running\0"};

// Red LED -- Pin 49 / PL0
// Yellow LED -- Pin 48 / PL1
// Green LED -- Pin 47 / PL2
// Blue LED -- Pin 46 / PL3
// PortL -- 0x10B
volatile unsigned char* ledPinsPort = 0x10B;
volatile unsigned char* ledPinsDDR = 0x10A;

// Stop -- Pin 12 -- PB6
// Reset -- Pin 13 -- PB7
// PortB -- 0x25
volatile unsigned char* buttonPinsDDR = 0x24;
volatile unsigned char* buttonPinsPin = 0x23;

// Keeps track of the current state of the swamp cooler
state currentState;
// Variable to keep track of what water level should allow the cooler to activate
double waterThreshold;
// Variable to keep track of what temperature to keep the temperature
double temperatureThreshold;
// Indicates when a state changed (for displaying)
bool stateChanged;
// Checks if reset button is able to be pressed
bool aboveWaterLevelThreshold;
// Used to immediately update display (bypass the interval timer)
bool transitionIntoIdle;

// From RTC lib, used for the Real Time Module
static DS1307 RTC;

// Interval w/o delay variables 
unsigned long previousMillis = 0;  // will store last time LCD was updated
const long interval = 60000;  // interval at which to blink (milliseconds)

// Humidity/Temperature Sensor Lib
static dht temperatureSensor;
// Pin used for the temperature sensor
int temperatureSensorPin = 50;

// // LCD pins <--> Arduino pins
const int RS = 9, EN = 10, D4 = 4, D5 = 5, D6 = 6, D7 = 7;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

// Water Sensor Variables
int waterSensorPin = 15; 

// FAN VARIABLES
// ENABLE - Pin 39 - PG2
// IN1 - Pin 40 - PG1
// IN2 - Pin 41 - PG0
volatile unsigned char* fanPinsPort = 0x34;
volatile unsigned char* fanPinsDDR = 0x33;

// Stepper Motor Variables
const int stepsPerRevolution = 2038;
Stepper stepper (stepsPerRevolution, 35, 33, 34, 32);

// Keeps track of the current direction (either positive 1 or negative -1 to toggle between the two positions)
int direction = 1;

// Vent Button
// Button PIN 52 - PB1
volatile unsigned char* ventButtonDDR = 0x24;
volatile unsigned char* ventButtonPin = 0x23;

void setup() {
  // Intialize Serial Baud Rate
  U0init(9600);
  // Initially set to disabled
  currentState = disabled;
  // Water level went to around 230 during the demo
  waterThreshold = 200;
  // Room temperature was around 23 deg celsius, components would head up enough after some time
  temperatureThreshold = 24;
  stateChanged = false;
  transitionIntoIdle = false;

  // Set LED Pins to Output
  *ledPinsDDR |= 0b1111;

  // Set Button Pins to Input
  *buttonPinsDDR &= ~0b11000000;

  // Attach interrupt to the start button
  attachInterrupt(digitalPinToInterrupt(2), startButtonRoutine, RISING);

  // Real Time Clock setup
  RTC.begin();
  RTC.setHourMode(CLOCK_H12);
  RTC.setMeridiem(HOUR_PM);

  // Uncomment when necessary to reset date and time
  // RTC.setDate(11,5,2024);
  // RTC.setTime(10,42,00);

  // LCD Screen Setup
  lcd.begin(16, 2);

  // Set Fan Pins to Output
  *fanPinsDDR |= 0b111;
  
  // Ensure Fan is Disabled (Enable set to low)
  disableFan();

  // Set Fan to Spin mode(IN1 = 1 IN2 = 0)
  *fanPinsPort &= ~0b11;
  *fanPinsPort |= 0b1;

  // Vent Button Set Input
  *ventButtonDDR &= ~0b10;

  // Intialize Serial Variables
  adc_init();
}

void loop() {
  detectButtonPress();
  
  detectStateChange();

  switch (currentState) {
    case error:
      // Error state should still monitor the water level (to know when to reset)
      // Monitoring temperature is required for ALL states except disabled
      monitorWaterLevel();
      monitorTemperature();
      break;
    case disabled:
      break;
    case idle:
      // Monitoring the water level if needed to redirect to error state
      monitorWaterLevel();
      // Detect if a stepper motor button presed to print to serial & update vent position
      detectStepperMotorPositionChange();
      // Determine the temperature to update it
      monitorTemperature();
      // Update the display after getting the temperature
      updateDisplay();
      break;
    case running:
      // Monitoring the water level if needed to redirect to error state
      monitorWaterLevel();
      // Detect if a stepper motor button presed to print to serial & update vent position
      detectStepperMotorPositionChange();
      // Determine the temperature to update it
      monitorTemperature();
      // Update the display after getting the temperature
      updateDisplay();
      break;
    default:
      break;
  }
}


void changeState(state state) {

}

void monitorWaterLevel() {

}

void monitorTemperature() {

}

void detectStateChange() {

}

void enableFan() {

}

void disableFan() {

}

void changeLED(led enabledLED) {

}

boolean buttonPressed(button button) {

}

void detectButtonPress() {

}

void startButtonRoutine() {
  // USED FOR ISR
}

void updateDisplay() {

}

void displayError() {

}

void printCurrentDate() {

}


void printCurrentTime() {

}