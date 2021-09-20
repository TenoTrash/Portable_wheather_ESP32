/*********
  Marcelo Ferrarotti 2021
  Estación meteorológica portátil
  BME280 por I2C
  OLED SPI
*********/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_D1   23
#define OLED_D0   18
#define OLED_DC   16
#define OLED_CS   5
#define OLED_RES  17

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_D1, OLED_D0, OLED_DC, OLED_RES, OLED_CS);

#define pulsador 13
#define touch_1 27
#define touch_2 32

#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>

#define SEALEVELPRESSURE_HPA (1025.00)

#define espera 60000 // 1 minuto
#define led 2

Adafruit_BME280 bme; // I2C
float temperature;
float pressure;
float humidity;
float altitude;

int pantalla = 1;

unsigned long previousMillis = 0;

void setup() {
  // Init Serial Monitor
  Serial.begin(115200);

  pinMode(pulsador, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
    while (1);
  }

  pinMode(led, OUTPUT);

  getReadings();
}

void loop() {

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= espera) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    getReadings();
  }

  switch (pantalla) {
    case 1:
      clima_display_1();
      break;
    case 2:
      clima_display_2();
      break;
    case 3:
      clima_display_3();
      break;
    case 4:
      clima_display_4();
      break;
    case 5:
      clima_display_5();
      break;
    case 6:
      clima_display_6();
      break;
  }

  if (digitalRead(pulsador) == 0) {
    digitalWrite(led, !digitalRead(pulsador));

    while (!digitalRead(pulsador));
    pantalla++;
    if (pantalla == 7) {
      pantalla = 1;
    }
  } else {
    digitalWrite(led, !digitalRead(pulsador));
  }

  //Serial.print("Temperatura: ");
  //Serial.println(temperature);
  //Serial.print("Presión: ");
  //Serial.println(pressure);
  //Serial.println();
  //delay(espera);
}

void clima_display_1() {
  // Pantalla general
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, 0);
  display.setCursor(0, 0);
  display.print("Clima ~ Alt: ");
  display.print(altitude, 1);

  display.setCursor(0, 10);
  display.print("Temp: ");
  display.setTextSize(2);
  display.print(temperature, 1);
  display.print(" C");
  display.setTextSize(1);

  display.setCursor(0, 30);
  display.print("Pres: ");
  display.setTextSize(2);
  display.print(pressure, 1);
  display.setTextSize(1);
  display.print("hPa");
  display.setTextSize(1);

  display.setCursor(0, 50);
  display.print("Hum:  ");
  display.setTextSize(2);
  display.print(humidity, 0);
  display.print("%");
  display.display();
}

void clima_display_2() {
  // Pantalla temperatura
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, 0);
  display.setCursor(0, 0);
  display.print("Temperatura: ");
  display.setCursor(0, 20);
  display.setTextSize(3);
  display.print(temperature, 1);
  display.setTextSize(2);
  display.print(" C");
  display.display();
}

void clima_display_3() {
  // Pantalla presion
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, 0);
  display.setCursor(0, 0);
  display.print("Presion: ");
  display.setCursor(0, 20);
  display.setTextSize(3);
  display.println(pressure, 1);
  display.setTextSize(1);
  display.print("hPa");
  display.display();
}

void clima_display_4() {
  // Pantalla humedad
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, 0);
  display.setCursor(0, 0);
  display.print("Humedad: ");
  display.setCursor(0, 20);
  display.setTextSize(3);
  display.print(humidity, 1);
  display.setTextSize(2);
  display.print(" %");
  display.display();
}
void clima_display_5() {
  // Pantalla altitud
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, 0);
  display.setCursor(0, 0);
  display.print("Altitud: ");
  display.setCursor(0, 20);
  display.setTextSize(3);
  display.print(altitude, 1);
  display.setTextSize(1);
  display.print(" m");
  display.display();
}

void clima_display_6() {
  // Pantalla creditos
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, 0);
  display.setCursor(0, 0);
  display.println("Fabricado por: ");
  display.setCursor(0, 20);
  display.setTextSize(2);
  display.println("Marcelo");
  display.println("Ferrarotti");
  display.setTextSize(1);
  display.println("2021");
  display.display();

}

void getReadings() {
  //recomedado por Bosch para lectura de estación meteorológica
  bme.takeForcedMeasurement();
  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X1, // temperature
                  Adafruit_BME280::SAMPLING_X1, // pressure
                  Adafruit_BME280::SAMPLING_X1, // humidity
                  Adafruit_BME280::FILTER_OFF );
  temperature = bme.readTemperature();
  pressure = (bme.readPressure() / 100.0F);
  humidity = bme.readHumidity();
  altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
}
