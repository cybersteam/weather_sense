/*
HARDWARE
    Arduino Uno
    BME280 - sensor for temp, humidity & barometric
    16 x 2 LCD display
*/
//SPI lib for communicating with the BME280

//#include <SPI.h>
//include <Wire.h>
//#include <Adafruit_BME280.h>
//#include <LiquidCrystal.h>

//#define BME_SCK 13
//#define BME_MISO 12
//#define BME_MOSI 11
//#define BME_CS 10

//Adafruit_BME280 bme;
//LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

#define temperature
#define humidity

/*
Irradiance Sensor or Pyranometer.
Ambient Temperature Sensor.
Module Temperature Sensor.
Wind Speed Sensor.
Wind Direction Sensor.
Humidity Sensor.
*/

float temperature;
float humidity;
float pressure;

void setup() {
  //lcd.begin(16, 2);
  //if (!bme.begin(BME_CS)) {
    Serial.println("I sesne somethng");
    while (1);
}

Serial.begin(9600);
Serial.println("Starting test");
}

void readWeatherData() {
    temperature = bme.readTemperature();
    humidity = bme.readHumidity();
    pressure = bme.readPressure() / 100.0F;
}
