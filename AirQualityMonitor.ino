#include <DHT.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


#define airQualityPin A0

#define buttonPin 3

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3D
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define millisecondsPerDay 86400000

int airQualitySensorValue;
int estimatedCO2;
char* quality;

int temp;
int humidity;

unsigned long currentMillis;
unsigned long previousMillis;
const unsigned int interval = 1000;
const unsigned int intervalsIn24Hours = millisecondsPerDay / interval;
int buttonState;

// The daily max array stores 5 of the highest co2 values throughout the day.
int dailyMax[5];
// The daily max stopwatch stores the amount of time since each max occurred.
int dailyMaxStopwatch[5];

int oledState;
// 0 --> main page (air quality and temp)
// 1 --> hum page (temp and humidity)
// 2 --> co2 page (air quality and co2)
// 3 --> daily max page (air quality and daily max co2)

void setup()
{

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();

  // Initialize variables and such.

  oledState = 0;

  Serial.begin(9600);

  pinMode(airQualityPin, INPUT);
  pinMode(buttonPin, INPUT);
  dht.begin();

  for (int i = 0; i < 5; i++) {
    dailyMax[i] = 0;
    dailyMaxStopwatch[i] = 0;
  }

  previousMillis = millis();
  
  display.setTextColor(SSD1306_WHITE);

  readAirQuality();
  readTemp();
  setDailyMax();
  displayInfo();

}

void loop()
{

  // Button must be readable at any time.
  operateButton();

  // Millis counts to ~49.7 days before rollover.
  currentMillis = millis();

  // New values are read and displayed every 10 seconds.
  // If the difference is negative, millis has rolled over back to 0.
  if (currentMillis - previousMillis >= interval || currentMillis - previousMillis < 0) {
    readAirQuality();
    readTemp();
    setDailyMax();
    displayInfo();

    previousMillis = currentMillis;
  }

}

void readAirQuality()
{

  // Read gas sensor pin and determine estimated CO2 ppm.
  // CO2 formula was calibrated in 'controlled' environment.
  airQualitySensorValue = analogRead(airQualityPin);
  estimatedCO2 = (int)((float)airQualitySensorValue * 4.68545 + 251.63835);
  
  // Set quality based on healthy levels.
  if (estimatedCO2 < 750) {
    quality = "Healthy";
  } else if (estimatedCO2 < 1000) {
    quality = "Moderate";
  } else if (estimatedCO2 < 2000) {
    quality = "Poor";
  } else if (estimatedCO2 < 4000) {
    quality = "Unhealthy";
  } else {
    quality = "Toxic";
  }

}

void readTemp()
{

  // Read DHT temperature and humidity values.
  temp = (int)(dht.readTemperature(true));
  humidity = (int)(dht.readHumidity());

  // Throw error message if values are undefined.
  if (isnan(humidity) || isnan(temp)) {
    Serial.println("Failed to read DHT.");
    return;
  }

}

void operateButton()
{

  // Read button state.
  buttonState = digitalRead(buttonPin);

  if (buttonState == LOW) {

    // Scroll through oled states.
    if (oledState < 3) {
      oledState++;
    } else {
      oledState = 0;
    }

    // Immediately display new page on button press.
    displayInfo();

    delay(1000);

  }

}

void setDailyMax()
{

  // Check for new daily max values
  if (estimatedCO2 > dailyMax[4]) {
    dailyMax[4] = estimatedCO2;
    int temp;

    // New daily max values and their stopwatches are shifted up as necessary.
    for (int i = 3; i >= 0; --i) {
      if (estimatedCO2 > dailyMax[i]) {
        temp = dailyMax[i];
        dailyMax[i] = estimatedCO2;
        dailyMax[i + 1] = temp;
        dailyMaxStopwatch[i + 1] = dailyMaxStopwatch[i];
      } else {
        dailyMaxStopwatch[i] = 0;
        break;
      }
      dailyMaxStopwatch[i] = 0;
    }

  }

  // Increment all stopwatches.
  for (int i = 0; i < 5; i++) {
   dailyMaxStopwatch[i]++;

    // Based on 24 hr stopwatches that have ended,
    // shift any daily max values and their stopwatches.
    if (dailyMaxStopwatch[i] >= intervalsIn24Hours) {
      for (int j = i + 1; j < 5; j++) {
        dailyMax[j - 1] = dailyMax[j];
        dailyMaxStopwatch[j - 1] = dailyMaxStopwatch[j];
      }

      // Reset last value.
      dailyMax[4] = 0;
      dailyMaxStopwatch[4] = 0;

    }
  }

}

void displayInfo()
{

  display.clearDisplay();
  display.setCursor(0,0);

  if (oledState != 1) {
    display.setTextSize(1);
    display.println(F("Air quality"));
    display.setTextSize(2);
    display.println(quality);
    display.println();
  }

  if (oledState <= 1) {
    display.setTextSize(1);
    display.println(F("Temp"));
    display.setTextSize(2);
    display.print(temp);
    display.println(F(" F"));
  }

  if (oledState == 1) {
    display.println();
    display.setTextSize(1);
    display.println(F("Humidity"));
    display.setTextSize(2);
    display.print(humidity);
    display.println(F("%"));

  } else if (oledState == 2) {
    display.setTextSize(1);
    display.println(F("Estimated CO2"));
    display.setTextSize(2);
    display.print(estimatedCO2);
    display.println(F(" ppm"));

  } else if (oledState == 3){
    display.setTextSize(1);
    display.println(F("24hr max CO2"));
    display.setTextSize(2);
    display.print(dailyMax[0]);
    display.println(F(" ppm"));

  }

  display.display();

}