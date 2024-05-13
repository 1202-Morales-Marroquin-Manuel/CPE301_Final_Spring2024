// Group Project Members: Christian Culanag & Manuel Morales-Marroquin
// CPE 301 Final Project
// Due Date: 5/12/2024

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

// Serial Variables
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;

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

// ADC variables, used for analog reading
volatile unsigned char *my_ADMUX = (unsigned char *)0x7C;
volatile unsigned char *my_ADCSRB = (unsigned char *)0x7B;
volatile unsigned char *my_ADCSRA = (unsigned char *)0x7A;
volatile unsigned int *my_ADC_DATA = (unsigned int *)0x78;

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

// Changing state occurs often, useful to seperate the logic into a callable function - Christian
void changeState(state state) {
  // Based on the enum state, change the current state to the given parameter
  currentState = state;
  // Change the leds based on the state
  changeLED((led)state);
  // Update state variable (to intiate printing to serial monitor)
  stateChanged = true;
}

void monitorWaterLevel() {
  // Read the water sensor pin
  double currentWaterLevel = adc_read(waterSensorPin);

  // If the current water level is above the threshold, update the boolean to reflect such
  aboveWaterLevelThreshold = currentWaterLevel >= waterThreshold;
  
  // Go to an error state based on the state diagram
  if ((currentState == idle && currentWaterLevel <= waterThreshold)
  ||  (currentState == running && currentWaterLevel < waterThreshold)) {
    changeState(error);
    // Print out error state to LCD
    displayError();
    // Disable Fan Motor
    disableFan();
  } 
}

void monitorTemperature() {
  // Read the temperature sensor using the Arduino Library Methods
  temperatureSensor.read11(temperatureSensorPin);
  double currentTemperature = temperatureSensor.temperature;

  // Change states based on the temperature (logic based on the state diagram provided)
  if (currentState == idle && currentTemperature > temperatureThreshold) {
    changeState(running);
    enableFan();
  } else if (currentState == running && currentTemperature <= temperatureThreshold) {
    changeState(idle);
    disableFan();
  }
}

// Determine if vent button is pressed, update stepper motor pos and display to serial - Christian
void detectStepperMotorPositionChange() {
  // Display the current date and time
  // Vent Button Pressed
  if (*ventButtonPin & 0b10) {
    printCurrentDate();
    printCurrentTime();
    
    // Print to serial that the stepper motor was pressed
    char stepperMotorOn[16]= "Stepper Motor On";

    for (int i = 0; i < 16; i++) {
      U0putchar(stepperMotorOn[i]);   
    }
    U0putchar('\n');

    // Move the stepper motor only 1/4 of the entire revolution
    stepper.setSpeed(5);
    stepper.step(direction * (stepsPerRevolution / 4));
    // Flip direction
    direction *= -1;

    // Display the current date and time
    printCurrentDate();
    printCurrentTime();
    
    // Show that the stepper motor has been disabled
    char stepperMotorOff[17]= "Stepper Motor Off";

    for (int i = 0; i < 17; i++) {
      U0putchar(stepperMotorOff[i]);   
    }
    U0putchar('\n');
  }
}


// Necessary requirement to display every state transition to the side - Christian
void detectStateChange() {
  // Display the date and time on every state transition
  if (stateChanged) {
    // Display date and time
    printCurrentDate();
    printCurrentTime();

    // Generic state changed message
    char message[13]= "State Changed";

    for (int i = 0; i < 13; i++) {
      U0putchar(message[i]);   
    }

    U0putchar(' ');   
    U0putchar('(');   

    // State the specific state it was changed into based on the 2D array
    for (int i = 0; i < 9 && statesString[currentState][i] != '\0'; i++) {
      U0putchar(statesString[currentState][i]);   
    }
    U0putchar(')');   

    U0putchar('\n');
    
    stateChanged = false;
  }
}

void enableFan() {
  // Print the current date/time
  printCurrentDate();
  printCurrentTime();
  // Display the motor was turned on
  char message[12]= "Fan Motor On";

  for (int i = 0; i < 12; i++) {
    U0putchar(message[i]);   
  }
    U0putchar('\n');   
  // Set ENABLE to HIGH
  *fanPinsPort |= 0b100;
}

void disableFan() {
  // Print the current date/time
  printCurrentDate();
  printCurrentTime();
  // Display the motor was turned off
  char message[13]= "Fan Motor Off";

  for (int i = 0; i < 13; i++) {
    U0putchar(message[i]);   
  }
    U0putchar('\n');   

  // Set ENABLE to LOW
  *fanPinsPort &= ~0b100;
}

void changeLED(led enabledLED) {
  int mask = (0x1 << enabledLED);

  // Disable all pins
  *ledPinsPort &= 0;

  // Output high to desired led
  *ledPinsPort |= mask;
}

boolean buttonPressed(button button) {
  return *buttonPinsPin & (0x1 << button);
}

boolean buttonPressed(button button) {
  return *buttonPinsPin & (0x1 << button);
}

// Too crowded in the main loop, moved it to its own function - Christian
void detectButtonPress() {
  // If the button pressed is stop, and it is on a disable state, perform the following actions
  if (buttonPressed(stop) && currentState != disabled) {
    changeState(disabled);
    disableFan();
    lcd.clear();
  }

  // If the button reset is pressed, ensure it is currently in an error state and that the system is above the water level
  if (buttonPressed(reset) && currentState == error && aboveWaterLevelThreshold) {
    changeState(idle);
    transitionIntoIdle = true;
  }
}

// Start button must use an ISR, needed by final project - Christian
void startButtonRoutine() {
  // USED FOR ISR
    if (currentState == disabled) {
    changeState(idle);
    transitionIntoIdle = true;
  }
}

void updateDisplay() {
  // Get the current time elapsed
  unsigned long currentMillis = millis();

  // Only update if the interval has passed, we've transitioned into an idle state, and the current state != error
  if (((currentMillis - previousMillis >= interval) || transitionIntoIdle) && currentState != error) {
    previousMillis = currentMillis;
    lcd.setCursor(0,0); 
    lcd.print("Temp: ");
    lcd.print(temperatureSensor.temperature);
    lcd.print((char)223);
    lcd.print("C");
    lcd.setCursor(0,1);
    lcd.print("Humidity: ");
    lcd.print(temperatureSensor.humidity);
    lcd.print("%");

    transitionIntoIdle = false;
  }
}

void displayError() {
  // Write to LCD
  lcd.clear();
  lcd.setCursor(0,0); 
  lcd.write("ERROR");
}

void displayError() {

}

// Seperated date print to its own function due to its commonality -- Christian
void printCurrentDate() {
  // Get the individual numeric components of each time variable
  char yearArray[4] = {'\0'};
  char monthArray[2] = {'\0'};
  char dayArray[2] = {'\0'};

  int year = RTC.getYear();
  int month = RTC.getMonth();
  int day = RTC.getDay();

  // Split it into arrays based on some algebra logic
  yearArray[0] = (year / 1000) + '0'; year %= 1000;
  yearArray[1] = (year / 100) + '0'; year %= 100;
  yearArray[2] = (year / 10) + '0'; year %= 10;
  yearArray[3] = year + '0'; 

  monthArray[0] = (month / 10) + '0'; month %= 10;
  monthArray[1] = month + '0'; 

  dayArray[0] = (day / 10) + '0'; day %= 10;
  dayArray[1] = day + '0'; 

  // Loop through the array for each number, add dashes as separators
  for (int i = 0; i < 4 && yearArray[i] != '\0'; i++) {
    U0putchar(yearArray[i]);
  }

  U0putchar('-');

  for (int i = 0; i < 2 && monthArray[i] != '\0'; i++) {
    U0putchar(monthArray[i]);
  }

  U0putchar('-');

  for (int i = 0; i < 2 && dayArray[i] != '\0'; i++) {
    U0putchar(dayArray[i]);
  }
  U0putchar(' ');
}

// Same logic as date, except time will always be no more than two digits, and it's seperated by colons instead -- Christian
void printCurrentTime() {
  // Get the individual numeric components of each time variable
  char hoursArray[2] = {'\0'};
  char minutesArray[2] = {'\0'};
  char secondsArray[2] = {'\0'};

  int hours = RTC.getHours();
  int minutes = RTC.getMinutes();
  int seconds = RTC.getSeconds();

  // Split it into arrays based on some algebra logic
  hoursArray[0] = (hours / 10) + '0'; hours %= 10;
  hoursArray[1] = hours + '0'; 

  minutesArray[0] = (minutes / 10) + '0'; minutes %= 10;
  minutesArray[1] = minutes + '0'; 

  secondsArray[0] = (seconds / 10) + '0'; seconds %= 10;
  secondsArray[1] = seconds + '0'; 

  // Loop through the array for each number, add colons as separators

  for (int i = 0; i < 2 && hoursArray[i] != '\0'; i++) {
    U0putchar(hoursArray[i]);
  }

  U0putchar(':');

  for (int i = 0; i < 2 && minutesArray[i] != '\0'; i++) {
    U0putchar(minutesArray[i]);
  }

  U0putchar(':');

  for (int i = 0; i < 2 && secondsArray[i] != '\0'; i++) {
    U0putchar(secondsArray[i]);
  }

  // Add a dash to serpreate events
  U0putchar(' ');
  U0putchar('-');
  U0putchar(' ');
}

void U0init(unsigned long U0baud) {
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 // Same as (FCPU / (16 * U0baud)) - 1;
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}
//
// Read USART0 RDA status bit and return non-zero true if set
//
unsigned char U0kbhit() {
  return *myUCSR0A & RDA;
}
//
// Read input character from USART0 input buffer
//
unsigned char U0getchar() {
  return *myUDR0;
}
//
// Wait for USART0 (myUCSR0A) TBE to be set then write character to
// transmit buffer
//
void U0putchar(unsigned char U0pdata) {
  while (!(*myUCSR0A & TBE));
  *myUDR0 = U0pdata;
}
