/*********
  Marcelo Ferrarotti 2021
  Estación meteorológica portátil
  - Menú rotativo con el pulsador
  - Grafico de últimas 24hs de presion-humedad-temperatura, pantalla general, pantalla individual, bajo sampling del BME280 (según hoja de datos)
  - Ajuste de altura en metros según presión atmosférica
  - Apagar la pantalla y reactivar con el pulsador para mitigar consumo energético y vida útil del OLED
  - Guardar las matrices en la memoria flash (en caso de corte de energía)
  
  Falta:
   - Mejorar la calidad de los graficos? Hacer un cartesiano con clima general? Todavia no lo tengo definido
   - Hacer un deepSleep durante los periodos de espera (cuando la pantalla está apagada)

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

// Pulsador para cambiar o prender
#define pulsador 13
#define touch_1 27
#define touch_2 32

#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>

#define SEALEVELPRESSURE_HPA (1030.00) // Cambiado en Lucila del Mar día despejado con viento sur - temp 16 grados 21/09/2021

#define espera 60000 // 1 minuto para mostrar en el display
#define espera_array 669000 // 11m15s minutos para guardar las ultimas 24hs en el array y distribuirlas en 128 lineas verticales

#define espera_estado_pantalla 90000 // 1,5 minutos para el apagado del display por falta de uso
bool estado_pantalla = true;
unsigned long previousMillis_estado_pantalla = 0;

//#define espera 1000 // prueba rapida para mostrar en el display
//#define espera_array 1000 // prueba rapida para guardar las ultimas 24hs en el array

#define led 2

Adafruit_BME280 bme; // I2C
float temperature;
float pressure;
float humidity;
float altitude;

int pantalla = 1;

unsigned long previousMillis = 0;
unsigned long previousMillis_array = 0;

//array para guardar datos
int array_temperature [128];
int array_humidity [128];
int array_pressure [128];
int array_contador;

#include "FS.h"
#include "SPIFFS.h"

File file_temperature;
File file_humidity;
File file_pressure;

const char* file_temperature_path = "/temperature";
const char* file_humidity_path = "/humidity";
const char* file_pressure_path = "/pressure";

void write_arrays() {

  file_temperature = SPIFFS.open(file_temperature_path, FILE_WRITE);
  file_humidity = SPIFFS.open(file_humidity_path, FILE_WRITE);
  file_pressure = SPIFFS.open(file_pressure_path, FILE_WRITE);

  for (int n = 0; n <= 128; n++) {
    file_temperature.println(array_temperature[n]);
  }
  for (int n = 0; n <= 128; n++) {
    file_humidity.println(array_humidity[n]);
  }
  for (int n = 0; n <= 128; n++) {
    file_pressure.println(array_pressure[n]);
  }

  file_temperature.close();
  file_humidity.close();
  file_pressure.close();

  Serial.println("Se han guardado las arrays en la SPIFFS");
}

void read_arrays() {

  file_temperature = SPIFFS.open(file_temperature_path, FILE_READ);
  file_humidity = SPIFFS.open(file_humidity_path, FILE_READ);
  file_pressure = SPIFFS.open(file_pressure_path, FILE_READ);

  String string_temperature;
  String string_humidity;
  String string_pressure;

  int counter = 0;
  while (file_temperature.available()) {
    string_temperature = file_temperature.readStringUntil('\r');
    array_temperature[counter] = string_temperature.toInt();
    counter++;
  }

  counter = 0;
  while (file_humidity.available()) {
    string_humidity = file_humidity.readStringUntil('\r');
    array_humidity[counter] = string_humidity.toInt();
    counter++;
  }

  counter = 0;
  while (file_pressure.available()) {
    string_pressure = file_pressure.readStringUntil('\r');
    array_pressure[counter] = string_pressure.toInt();
    counter++;
  }

  file_temperature.close();
  file_humidity.close();
  file_pressure.close();

  Serial.println("Arrays llenas con datos anteriores");

}

void print_arrays() {

  Serial.println("Array temperature: ");
  for (int counter = 0; counter <= 128; counter++) {
    Serial.println(array_temperature[counter]);
  }

  Serial.println("Array humidity: ");
  for (int counter = 0; counter <= 128; counter++) {
    Serial.println(array_humidity[counter]);
  }

  Serial.println("Array pressure: ");
  for (int counter = 0; counter <= 128; counter++) {
    Serial.println(array_pressure[counter]);
  }

}

void print_files() {
  file_temperature = SPIFFS.open(file_temperature_path, FILE_READ);
  file_humidity = SPIFFS.open(file_humidity_path, FILE_READ);
  file_pressure = SPIFFS.open(file_pressure_path, FILE_READ);

  Serial.println("Contenido temperature: ");
  while (file_temperature.available()) {
    Serial.write(file_temperature.read());
  }

  Serial.println("Contenido humidity: ");
  while (file_humidity.available()) {
    Serial.write(file_humidity.read());
  }

  Serial.println("Contenido pressure: ");
  while (file_pressure.available()) {
    Serial.write(file_pressure.read());
  }
  file_temperature.close();
  file_humidity.close();
  file_pressure.close();
}

void setup() {
  // Init Serial Monitor
  Serial.begin(115200);

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Error while mounting SPIFFS");
    return;
  }

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

  read_arrays();

  print_arrays();

  print_files();
}

void loop() {

  // Lectura de datos BME280 cada un minuto en un solo sample de datos - según recomendación fabricante para meteorología
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= espera) {
    previousMillis = currentMillis;
    getReadings();
  }

  //Apagado de pantalla a los dos minutos
  unsigned long currentMillis_estado_pantalla = millis();
  if (currentMillis_estado_pantalla - previousMillis_estado_pantalla >= espera_estado_pantalla) {
    previousMillis_estado_pantalla = currentMillis_estado_pantalla;
    estado_pantalla = false;
  }

  // Se guardan los datos climáticos en los array correspondientes cada 11m15s, de manera de tener 128 lecturas (24hs de datos)
  unsigned long currentMillis_array = millis();
  if (currentMillis_array - previousMillis_array >= espera_array) {
    // save the last time you blinked the LED
    previousMillis_array = currentMillis_array;
    // getReadings(); //Redundante con la lectura anterior - puede afectar lectura en general?

    // guardar datos en array
    array_temperature[array_contador] = temperature;
    array_humidity[array_contador] = humidity;
    array_pressure[array_contador] = pressure;

    array_contador++;
    if (array_contador == 128) {
      array_contador = 0;
    }

    // Se guardan arrays en la SPIFFS
    write_arrays();
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
      clima_display_9();
      break;
    case 7:
      clima_display_7();
      break;
    case 8:
      clima_display_8();
      break;
    case 9:
      clima_display_6();
      break;
  }

  if (digitalRead(pulsador) == 0) {
    digitalWrite(led, !digitalRead(pulsador));
    estado_pantalla = true;
    while (!digitalRead(pulsador));
    pantalla++;
    if (pantalla == 10) {
      pantalla = 1;
    }
  } else {
    digitalWrite(led, !digitalRead(pulsador));
  }
}

void clima_display_1() {
  // Pantalla general
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
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
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_2() {
  // Pantalla temperatura
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
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
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_3() {
  // Pantalla presion
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
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
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_4() {
  // Pantalla humedad
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
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
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}
void clima_display_5() {
  // Pantalla altitud
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
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
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_6() {
  // Pantalla creditos
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
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
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_7() {
  // Pantalla histórico temperatura
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE, 0);
    display.setCursor(0, 0);
    display.print("Temperatura 24hs: ");
    for (int n = 0; n <= 128; n++) {
      display.drawLine(n, 40, n, 40 - array_temperature[n], WHITE);
    }
    // doble linea central
    display.drawLine(0, 41, 128, 41, BLACK);
    display.drawLine(0, 42, 128, 42, BLACK);
    display.display();
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_8() {
  //Pantalla histórico humedad
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    display.clearDisplay();

    for (int n = 0; n <= 128; n++) {
      display.drawLine(n, 64, n, map(array_humidity[n], 0, 100, 55, 0), WHITE);
    }
    display.setTextSize(1);
    display.setTextColor(SSD1306_BLACK, 0);
    display.setCursor(5, 55);
    display.print("Humedad 24hs ");
    display.display();
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_9() {
  //Pantalla histórico presión
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE, 0);
    display.setCursor(0, 0);
    display.print("Hectopascales 24hs ");
    for (int n = 0; n <= 128; n++) {
      display.drawLine(n, 44, n, 44 - (array_pressure[n] / 100), WHITE);
    }
    display.drawLine(0, 44, 128, 44, BLACK);
    display.display();
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
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
  Serial.println("Se ha realizado una lectura de datos en el BME280");
}

#define espera_estado_pantalla 90000 // 1,5 minutos para el apagado del display por falta de uso
bool estado_pantalla = true;
unsigned long previousMillis_estado_pantalla = 0;

// #define espera 1000 // prueba rapida para mostrar en el display
// #define espera_array 1000 // prueba rapida para guardar las ultimas 24hs en el array

#define led 2

Adafruit_BME280 bme; // I2C
float temperature;
float pressure;
float humidity;
float altitude;

int pantalla = 1;

unsigned long previousMillis = 0;
unsigned long previousMillis_array = 0;

//array para guardar datos
int array_temperature [128];
int array_humidity [128];
int array_pressure [128];
int array_contador;

#include "FS.h"
#include "SPIFFS.h"

File file_temperature;
File file_humidity;
File file_pressure;

const char* file_temperature_path = "/temperature";
const char* file_humidity_path = "/humidity";
const char* file_pressure_path = "/pressure";

void write_arrays() {

  file_temperature = SPIFFS.open(file_temperature_path, FILE_WRITE);
  file_humidity = SPIFFS.open(file_humidity_path, FILE_WRITE);
  file_pressure = SPIFFS.open(file_pressure_path, FILE_WRITE);

  for (int n = 0; n <= 128; n++) {
    file_temperature.println(array_temperature[n]);
  }
  for (int n = 0; n <= 128; n++) {
    file_humidity.println(array_humidity[n]);
  }
  for (int n = 0; n <= 128; n++) {
    file_pressure.println(array_pressure[n]);
  }

  file_temperature.close();
  file_humidity.close();
  file_pressure.close();

  Serial.println("Se han guardado las arrays en la SPIFFS");
}

void read_arrays() {

  file_temperature = SPIFFS.open(file_temperature_path, FILE_READ);
  file_humidity = SPIFFS.open(file_humidity_path, FILE_READ);
  file_pressure = SPIFFS.open(file_pressure_path, FILE_READ);

  String string_temperature;
  String string_humidity;
  String string_pressure;

  int counter = 0;
  while (file_temperature.available()) {
    string_temperature = file_temperature.readStringUntil('\n');
    array_temperature[counter] = string_temperature.toInt();
    counter++;
  }

  counter = 0;
  while (file_humidity.available()) {
    string_humidity = file_humidity.readStringUntil('\n');
    array_humidity[counter] = string_humidity.toInt();
    counter++;
  }

  counter = 0;
  while (file_pressure.available()) {
    string_pressure = file_pressure.readStringUntil('\n');
    array_pressure[counter] = string_pressure.toInt();
    counter++;
  }

  file_temperature.close();
  file_humidity.close();
  file_pressure.close();

  Serial.println("Arrays llenas con datos anteriores");

}

void print_arrays() {

  Serial.println("Array temperature: ");
  for (int counter = 0; counter <= 128; counter++) {
    Serial.println(array_temperature[counter]);
  }

  Serial.println("Array humidity: ");
  for (int counter = 0; counter <= 128; counter++) {
    Serial.println(array_humidity[counter]);
  }

  Serial.println("Array pressure: ");
  for (int counter = 0; counter <= 128; counter++) {
    Serial.println(array_pressure[counter]);
  }

}

void print_files() {
  file_temperature = SPIFFS.open(file_temperature_path, FILE_READ);
  file_humidity = SPIFFS.open(file_humidity_path, FILE_READ);
  file_pressure = SPIFFS.open(file_pressure_path, FILE_READ);

  Serial.println("Contenido temperature: ");
  while (file_temperature.available()) {
    Serial.write(file_temperature.read());
  }

  Serial.println("Contenido humidity: ");
  while (file_humidity.available()) {
    Serial.write(file_humidity.read());
  }

  Serial.println("Contenido pressure: ");
  while (file_pressure.available()) {
    Serial.write(file_pressure.read());
  }
  file_temperature.close();
  file_humidity.close();
  file_pressure.close();
}

void setup() {
  // Init Serial Monitor
  Serial.begin(115200);

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Error while mounting SPIFFS");
    return;
  }

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

  //read_arrays();

  print_arrays();

  print_files();
}

void loop() {

  // Lectura de datos BME280 cada un minuto en un solo sample de datos - según recomendación fabricante para meteorología
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= espera) {
    previousMillis = currentMillis;
    getReadings();
  }

  //Apagado de pantalla a los dos minutos
  unsigned long currentMillis_estado_pantalla = millis();
  if (currentMillis_estado_pantalla - previousMillis_estado_pantalla >= espera_estado_pantalla) {
    previousMillis_estado_pantalla = currentMillis_estado_pantalla;
    estado_pantalla = false;
  }

  // Se guardan los datos climáticos en los array correspondientes cada 11m15s, de manera de tener 128 lecturas (24hs de datos)
  unsigned long currentMillis_array = millis();
  if (currentMillis_array - previousMillis_array >= espera_array) {
    // save the last time you blinked the LED
    previousMillis_array = currentMillis_array;
    // getReadings(); //Redundante con la lectura anterior - puede afectar lectura en general?

    // guardar datos en array
    array_temperature[array_contador] = temperature;
    array_humidity[array_contador] = humidity;
    array_pressure[array_contador] = pressure;

    array_contador++;
    if (array_contador == 128) {
      array_contador = 0;
    }

    // Se guardan arrays en la SPIFFS
    write_arrays();
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
      clima_display_9();
      break;
    case 7:
      clima_display_7();
      break;
    case 8:
      clima_display_8();
      break;
    case 9:
      clima_display_6();
      break;
  }

  if (digitalRead(pulsador) == 0) {
    digitalWrite(led, !digitalRead(pulsador));
    estado_pantalla = true;
    while (!digitalRead(pulsador));
    pantalla++;
    if (pantalla == 10) {
      pantalla = 1;
    }
  } else {
    digitalWrite(led, !digitalRead(pulsador));
  }
}

void clima_display_1() {
  // Pantalla general
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
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
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_2() {
  // Pantalla temperatura
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
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
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_3() {
  // Pantalla presion
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
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
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_4() {
  // Pantalla humedad
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
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
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}
void clima_display_5() {
  // Pantalla altitud
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
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
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_6() {
  // Pantalla creditos
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
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
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_7() {
  // Pantalla histórico temperatura
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE, 0);
    display.setCursor(0, 0);
    display.print("Temperatura 24hs: ");
    for (int n = 0; n <= 128; n++) {
      display.drawLine(n, 40, n, 40 - array_temperature[n], WHITE);
    }
    // doble linea central
    display.drawLine(0, 41, 128, 41, BLACK);
    display.drawLine(0, 42, 128, 42, BLACK);
    display.display();
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_8() {
  //Pantalla histórico humedad
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    display.clearDisplay();

    for (int n = 0; n <= 128; n++) {
      display.drawLine(n, 64, n, map(array_humidity[n], 0, 100, 55, 0), WHITE);
    }
    display.setTextSize(1);
    display.setTextColor(SSD1306_BLACK, 0);
    display.setCursor(5, 55);
    display.print("Humedad 24hs ");
    display.display();
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_9() {
  //Pantalla histórico presión
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE, 0);
    display.setCursor(0, 0);
    display.print("Hectopascales 24hs ");
    for (int n = 0; n <= 128; n++) {
      display.drawLine(n, 44, n, 44 - (array_pressure[n] / 100), WHITE);
    }
    display.drawLine(0, 44, 128, 44, BLACK);
    display.display();
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
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
  Serial.println("Se ha realizado una lectura de datos en el BME280");
}
unsigned long previousMillis_estado_pantalla = 0;

// #define espera 1000 // prueba rapida para mostrar en el display
// #define espera_array 1000 // prueba rapida para guardar las ultimas 24hs en el array

#define led 2

Adafruit_BME280 bme; // I2C
float temperature;
float pressure;
float humidity;
float altitude;

int pantalla = 1;

unsigned long previousMillis = 0;
unsigned long previousMillis_array = 0;

//array para guardar datos
int array_temperature [128];
int array_humidity [128];
int array_pressure [128];
int array_contador;

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

  // Lectura de datos BME280 cada un minuto en un solo sample de datos - según recomendación fabricante para meteorología
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= espera) {
    previousMillis = currentMillis;
    getReadings();
  }

  //Apagado de pantalla a los dos minutos
  unsigned long currentMillis_estado_pantalla = millis();
  if (currentMillis_estado_pantalla - previousMillis_estado_pantalla >= espera_estado_pantalla) {
    previousMillis_estado_pantalla = currentMillis_estado_pantalla;
    estado_pantalla = false;
  }

  // Se guardan los datos climáticos en los array correspondientes cada 11m15s, de manera de tener 128 lecturas (24hs de datos)
  unsigned long currentMillis_array = millis();
  if (currentMillis_array - previousMillis_array >= espera_array) {
    // save the last time you blinked the LED
    previousMillis_array = currentMillis_array;
    // getReadings(); //Redundante con la lectura anterior - puede afectar lectura en general?

    // guardar datos en array
    array_temperature[array_contador] = temperature;
    array_humidity[array_contador] = humidity;
    array_pressure[array_contador] = pressure;

    array_contador++;
    if (array_contador == 128) {
      array_contador = 0;
    }
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
      clima_display_9();
      break;
    case 7:
      clima_display_7();
      break;
    case 8:
      clima_display_8();
      break;
    case 9:
      clima_display_6();
      break;
  }

  if (digitalRead(pulsador) == 0) {
    digitalWrite(led, !digitalRead(pulsador));
    estado_pantalla = true;
    while (!digitalRead(pulsador));
    pantalla++;
    if (pantalla == 10) {
      pantalla = 1;
    }
  } else {
    digitalWrite(led, !digitalRead(pulsador));
  }
}

void clima_display_1() {
  // Pantalla general
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
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
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_2() {
  // Pantalla temperatura
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
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
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_3() {
  // Pantalla presion
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
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
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_4() {
  // Pantalla humedad
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
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
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}
void clima_display_5() {
  // Pantalla altitud
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
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
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_6() {
  // Pantalla creditos
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
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
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_7() {
  // Pantalla histórico temperatura
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE, 0);
    display.setCursor(0, 0);
    display.print("Temperatura 24hs: ");
    for (int n = 0; n <= 128; n++) {
      display.drawLine(n, 40, n, 40 - array_temperature[n], WHITE);
    }
    // doble linea central
    display.drawLine(0, 41, 128, 41, BLACK);
    display.drawLine(0, 42, 128, 42, BLACK);
    display.display();
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_8() {
  //Pantalla histórico humedad
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    display.clearDisplay();

    for (int n = 0; n <= 128; n++) {
      display.drawLine(n, 64, n, map(array_humidity[n], 0, 100, 55, 0), WHITE);
    }
    display.setTextSize(1);
    display.setTextColor(SSD1306_BLACK, 0);
    display.setCursor(5, 55);
    display.print("Humedad 24hs ");
    display.display();
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

void clima_display_9() {
  //Pantalla histórico presión
  if (estado_pantalla == true) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE, 0);
    display.setCursor(0, 0);
    display.print("Hectopascales 24hs ");
    for (int n = 0; n <= 128; n++) {
      display.drawLine(n, 44, n, 44 - (array_pressure[n] / 100), WHITE);
    }
    display.drawLine(0, 44, 128, 44, BLACK);
    display.display();
  } else {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
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
  Serial.println("Se ha realizado una lectura de datos en el BME280");
}
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
