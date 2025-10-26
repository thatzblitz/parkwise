#include <Servo.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ----- Display Definitions -----
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ----- Gate Definitions -----
#define ENTRY_IR_SENSOR 2      // Entry IR sensor connected to D2
#define ENTRY_SERVO_PIN 9      // Entry gate servo connected to D9
#define EXIT_IR_SENSOR 3       // Exit IR sensor connected to D3
#define EXIT_SERVO_PIN 10      // Exit gate servo connected to D10

// WS2812B LED strip for both gate and parking indicators:
#define LED_STRIP_PIN 6        // WS2812B LED strip output pin (D6)
#define NUM_LEDS_TOTAL 6       // Total LEDs: 2 for gates + 4 for parking spots

// ----- Parking Spot Definitions -----
// IR sensors for four parking spots
#define PARKING_IR1 4          // Parking Spot 1 IR sensor on D4
#define PARKING_IR2 5          // Parking Spot 2 IR sensor on D5
#define PARKING_IR3 7          // Parking Spot 3 IR sensor on D7
#define PARKING_IR4 8          // Parking Spot 4 IR sensor on D8

// Orange indicator LEDs for the parking spots
#define ORANGE_LED_1 11        // Parking Spot 1 orange LED on D11
#define ORANGE_LED_2 12        // Parking Spot 2 orange LED on D12
#define ORANGE_LED_3 A0        // Parking Spot 3 orange LED on A0
#define ORANGE_LED_4 A1        // Parking Spot 4 orange LED on A1

// Arrays for parking spot handling:
const int parkingIRPins[4] = { PARKING_IR1, PARKING_IR2, PARKING_IR3, PARKING_IR4 };
const int orangeLEDPins[4] = { ORANGE_LED_1, ORANGE_LED_2, ORANGE_LED_3, ORANGE_LED_4 };
const int wsLedIndices[4] = { 1, 2, 3, 4 };        // For the WS2812B LED strip, we assume indices 1 to 4 are reserved for parking spots
const char *parkingSpotNames[4] = { "P1", "P2", "P3", "P4" };        // Names for display purposes

// ----- Global Objects -----
Servo entryServo, exitServo;
Adafruit_NeoPixel ledStrip(NUM_LEDS_TOTAL, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(9600);
  
  // ----- Setup Gate Hardware -----
  pinMode(ENTRY_IR_SENSOR, INPUT);
  entryServo.attach(ENTRY_SERVO_PIN);
  entryServo.write(5);  // Start with gate closed
  
  pinMode(EXIT_IR_SENSOR, INPUT);
  exitServo.attach(EXIT_SERVO_PIN);
  exitServo.write(0);   // Start with gate closed
  
  ledStrip.begin();
  ledStrip.show();  // Initialize and turn off all LEDs
  
  // Set default gate LED colors: Red (no car detected)
  setLED(0, 255, 0, 0);  // Entry gate LED (index 0) = Red
  setLED(5, 255, 0, 0);  // Exit gate LED (index 5) = Red
  
  // ----- Setup Parking Spot Hardware -----
  // Parking IR sensors as inputs
  pinMode(PARKING_IR1, INPUT);
  pinMode(PARKING_IR2, INPUT);
  pinMode(PARKING_IR3, INPUT);
  pinMode(PARKING_IR4, INPUT);
  
  // Orange indicator LEDs as outputs
  pinMode(ORANGE_LED_1, OUTPUT);
  pinMode(ORANGE_LED_2, OUTPUT);
  pinMode(ORANGE_LED_3, OUTPUT);
  pinMode(ORANGE_LED_4, OUTPUT);
  
  // Optionally, initialize parking spot WS2812B indicators (indices 1–4) to green (free)
  for (int i = 1; i < NUM_LEDS_TOTAL - 1; i++) {
    ledStrip.setPixelColor(i, ledStrip.Color(0, 255, 0)); // Free = green
  }
  ledStrip.show();
  
  // ----- Setup OLED Display -----
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(16, 25);
  display.println("ParkWise");
  display.display();
  delay(200);
}

void loop() {
  // ----- Gate Handling -----
  // Entry Gate
  int entrySensorState = digitalRead(ENTRY_IR_SENSOR);
  if (entrySensorState == LOW) {  // Car detected at Entry
    entryServo.write(25);         // Open gate
    setLED(0, 0, 255, 0);         // Set Entry LED (index 0) to Green
    Serial.println("Entry Gate Open - LED Green");
  } else {
    entryServo.write(5);          // Close gate
    setLED(0, 255, 0, 0);         // Set Entry LED (index 0) to Red
    Serial.println("Entry Gate Closed - LED Red");
  }
  
  // Exit Gate
  int exitSensorState = digitalRead(EXIT_IR_SENSOR);
  if (exitSensorState == LOW) {   // Car detected at Exit
    exitServo.write(22);          // Open gate
    setLED(5, 0, 255, 0);         // Set Exit LED (index 5) to Green
    Serial.println("Exit Gate Open - LED Green");
  } else {
    exitServo.write(0);           // Close gate
    setLED(5, 255, 0, 0);         // Set Exit LED (index 5) to Red
    Serial.println("Exit Gate Closed - LED Red");
  }
  
  delay(100);  // Small delay for gate stability
  
  // ----- Parking Spot Handling -----

  updateParkingStatus();
}

// ----- Functions -----

// Function to set the color of a specific pixel on the WS2812B LED strip
void setLED(int ledIndex, int red, int green, int blue) {
  ledStrip.setPixelColor(ledIndex, ledStrip.Color(red, green, blue));
  ledStrip.show();
}

// Provided blink function for an indicator LED (blocking delays)
void blinkState(int pin) {
  digitalWrite(pin, HIGH);
  delay(200); // LED on for 200 ms
  digitalWrite(pin, LOW);
  delay(200); // LED off for 200 ms
}

// Function to update a parking spot indicator based on its IR sensor reading
void updateParkingStatus() {
  // Array to store the status strings ("FULL" or "EMPTY") for each spot
  char statuses[4][6]; // each string can hold up to 5 characters + null terminator
  
  // Loop through each parking spot
  for (int i = 0; i < 4; i++) {
    display.setTextSize(1);
    int sensorState = digitalRead(parkingIRPins[i]);
    
    if (sensorState == LOW) {  // Car detected = Occupied
      digitalWrite(orangeLEDPins[i], LOW);      // Turn off the discrete (orange) LED
      ledStrip.setPixelColor(wsLedIndices[i], ledStrip.Color(255, 0, 0));      // Set the WS2812B LED to red
      strcpy(statuses[i], "FULL");
      Serial.print(parkingSpotNames[i]);
      Serial.println(": Occupied");
    } else {  // No car detected = Free
      blinkState(orangeLEDPins[i]); // Blink the discrete orange LED
      ledStrip.setPixelColor(wsLedIndices[i], ledStrip.Color(0, 255, 0));      // Set the WS2812B LED to green
      strcpy(statuses[i], "EMPTY");
      Serial.print(parkingSpotNames[i]);
      Serial.println(": Free");
    }
  }
  
  ledStrip.show(); // Updating the WS2812B LED strip
  
  // updating the OLED display
  display.clearDisplay();
  display.setCursor(19, 15);
  display.println("Parking Status:");
  display.println("");
  display.print("P1: ");
  display.print(statuses[0]);
  display.print("  P2: ");
  display.println(statuses[1]);
  display.print("P3: ");
  display.print(statuses[2]);
  display.print("  P4: ");
  display.println(statuses[3]);
  display.display();
}