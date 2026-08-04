#include "arduino_secrets.h"

// ChontoAgent --- Lectura de Sensores + RS485 ZTS-3002 (v1.x corregido)

// ------- Includes -------
#include <Wire.h>
#include <BH1750.h>
#include <DHT.h>

// ------- Pines -------
#define PINDHT22       15
#define PINMQ135       34
#define PINPHAGUA      35
#define PINHIGROMETRO  32

#define PINRS485TX     17
#define PINRS485RX     16
#define PINRS485DERE    5   // DE/RE del mÃ³dulo RS485

// ------- ConfiguraciÃ³n -------
#define DHTTYPE        DHT22
#define RS485BAUD      4800

// CalibraciÃ³n rÃ¡pida
#define PHOFFSET       0.0
#define HIGROSECO      3000
#define HIGROHUMEDO    1500

// ------- Objetos -------
BH1750 sensorLuz;
DHT dht(PINDHT22, DHTTYPE);

// Estructura de datos
struct DatosSensores {
  float luz;

  float tempAire;
  float humAire;

  int   rawCO2;
  float voltCO2;

  float valorPH;     // pH agua
  float humSuelo;    // higrÃ³metro analÃ³gico
  float humSueloRS;  // humedad RS485
  float tempSuelo;   // temperatura RS485
  int   ce;          // CE RS485
  float phSuelo;     // pH suelo RS485

  bool rs485OK;
};

DatosSensores datos;

unsigned long ultimaLectura = 0;
#define INTERVALOMS 5000

// Comando RS485: esclavo 0x01, func 0x03, desde 0x0000, 4 regs, CRC 0x4409
byte cmdRS485[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x04, 0x44, 0x09};

// ------- Setup -------
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("ChontoAgent --- Sensores + RS485 v1.x");
  Serial.println();

  // I2C BH1750
  Wire.begin(21, 22);
  if (sensorLuz.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("BH1750 OK");
  } else {
    Serial.println("BH1750 NO encontrado");
  }

  // DHT22
  dht.begin();
  Serial.println("DHT22 iniciado");

  // RS485
  pinMode(PINRS485DERE, OUTPUT);
  digitalWrite(PINRS485DERE, LOW); // RX por defecto

  Serial2.begin(RS485BAUD, SERIAL_8N1, PINRS485RX, PINRS485TX);
  Serial.println("RS485 iniciado");

  // ADC 12 bits
  analogReadResolution(12);
  Serial.println("ADC 12 bits OK");

  Serial.println();
  Serial.println("Lecturas cada 5 segundos");
  Serial.println();
}

// ------- Loop -------
void loop() {
  unsigned long ahora = millis();
  if (ahora - ultimaLectura > INTERVALOMS) {
    ultimaLectura = ahora;
    leerTodosSensores();
    imprimirDatos();
  }
}

// ------- BH1750 -------
void leerBH1750() {
  datos.luz = sensorLuz.readLightLevel();
}

// ------- DHT22 -------
void leerDHT22() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  datos.tempAire = isnan(t) ? -999 : t;
  datos.humAire  = isnan(h) ? -999 : h;
}

// ------- MQ-135 -------
void leerMQ135() {
  datos.rawCO2  = analogRead(PINMQ135);
  datos.voltCO2 = datos.rawCO2 * 3.3 / 4095.0;
}

// ------- pH Agua -------
void leerPHAgua() {
  long suma = 0;
  for (int i = 0; i < 10; i++) {
    suma += analogRead(PINPHAGUA);
    delay(10);
  }

  float volt = (suma / 10.0) * 3.3 / 4095.0;
  datos.valorPH = constrain(7.0 + (2.5 - volt) / 0.18 + PHOFFSET, 0.0, 14.0);
}

// ------- HigrÃ³metro analÃ³gico -------
void leerHigrometro() {
  long suma = 0;
  for (int i = 0; i < 5; i++) {
    suma += analogRead(PINHIGROMETRO);
    delay(10);
  }

  datos.humSuelo = constrain(
                     map(suma / 5, HIGROSECO, HIGROHUMEDO, 0, 100),
                     0, 100
                   );
}

// ------- RS485 ZTS-3002 (4 registros) -------
void leerRS485() {
  // Limpiar buffer previo
  while (Serial2.available()) Serial2.read();

  // Enviar comando
  digitalWrite(PINRS485DERE, HIGH);
  delay(10);
  Serial2.write(cmdRS485, sizeof(cmdRS485));
  Serial2.flush();
  delay(10);
  digitalWrite(PINRS485DERE, LOW);

  // Esperar respuesta
  delay(500);

  if (Serial2.available() < 13) {
    datos.rs485OK = false;
    Serial.print("RS485 sin respuesta --- bytes = ");
    Serial.println(Serial2.available());
    return;
  }

  byte resp[13];
  Serial2.readBytes(resp, 13);

  // DEBUG RAW
  Serial.print("RAW: ");
  for (int i = 0; i < 13; i++) {
    Serial.print("[");
    Serial.print(i);
    Serial.print("]=0x");
    if (resp[i] < 0x10) Serial.print("0");
    Serial.print(resp[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // Validar formato: cuando el frame estÃ¡ bien alineado, resp[2] = 0x08
  if (resp[2] != 0x08) {
    Serial.println("Frame RS485 desalineado, IGNORADO");
    // NO tocamos rs485OK ni los datos: mantenemos la Ãºltima lectura vÃ¡lida
    return;
  }

  // Estructura "buena": 00 XX 08 02EE 00D7 003A/3B 005A CRC CRC
  // resp[3..4] -> humedad
  // resp[5..6] -> temperatura
  // resp[7..8] -> CE
  // resp[9..10]-> pH
  uint16_t rawHum  = ((uint16_t)resp[3] << 8) | resp[4];
  uint16_t rawTemp = ((uint16_t)resp[5] << 8) | resp[6];
  uint16_t rawCE   = ((uint16_t)resp[7] << 8) | resp[8];
  uint16_t rawPH   = ((uint16_t)resp[9] << 8) | resp[10];

  datos.humSueloRS = rawHum / 10.0;            // %
  datos.tempSuelo  = (int16_t)rawTemp / 10.0;  // Â°C
  datos.ce         = rawCE;                    // uS/cm
  datos.phSuelo    = rawPH / 10.0;             // pH

  datos.rs485OK = true;
}

// ------- Orquestador -------
void leerTodosSensores() {
  leerBH1750();
  leerDHT22();
  leerMQ135();
  leerPHAgua();
  leerHigrometro();
  leerRS485();
}

// ------- ImpresiÃ³n -------
void imprimirDatos() {
  Serial.println();
  Serial.println("âââââââââââââââââââââââââââââââââââââââ");
  Serial.println("â         LECTURA DE SENSORES        â");
  Serial.println("âââââââââââââââââââââââââââââââââââââââ¤");

  Serial.print("â Luz:          ");
  Serial.print(datos.luz, 1);
  Serial.println(" lux");

  Serial.print("â Temp aire:    ");
  if (datos.tempAire == -999) Serial.println("ERROR");
  else {
    Serial.print(datos.tempAire, 1);
    Serial.println(" C");
  }

  Serial.print("â Hum aire:     ");
  if (datos.humAire == -999) Serial.println("ERROR");
  else {
    Serial.print(datos.humAire, 1);
    Serial.println(" %");
  }

  Serial.print("â CO2 raw:      ");
  Serial.print(datos.rawCO2);
  Serial.print(" (");
  Serial.print(datos.voltCO2, 2);
  Serial.println(" V)");

  Serial.print("â pH agua:      ");
  Serial.println(datos.valorPH, 2);

  Serial.print("â Hum suelo:    ");
  Serial.print(datos.humSuelo, 1);
  Serial.println(" %");

  Serial.println("âââââââââââââââââââââââââââââââââââââââ¤");

  if (datos.rs485OK) {
    Serial.print("â Hum suelo RS: ");
    Serial.print(datos.humSueloRS, 1);
    Serial.println(" %");

    Serial.print("â Temp suelo:   ");
    Serial.print(datos.tempSuelo, 1);
    Serial.println(" C");

    Serial.print("â CE:           ");
    Serial.print(datos.ce);
    Serial.println(" uS/cm");

    Serial.print("â pH suelo:     ");
    Serial.println(datos.phSuelo, 1);
  } else {
    Serial.println("â   RS485 sin respuesta              â");
  }

  Serial.println("âââââââââââââââââââââââââââââââââââââââ");
}