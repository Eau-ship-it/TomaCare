#include "arduino_secrets.h"


// MOSFET activo HIGH binario: bomba1, PTC
// MOSFET activo HIGH PWM:     bomba2, LED grow
// RELÃ activo LOW:            humidificador, vent1, vent2

#include <Wire.h>
#include <BH1750.h>
#include <DHT.h>
#include <ESP32Servo.h>

// -------------------- Pines sensores --------------------
#define PINDHT22        15
#define PINMQ135        34
#define PINPHAGUA       35
#define PINHIGROMETRO   32
#define PINRS485TX      17
#define PINRS485RX      16
#define PINRS485DERE    5

// -------------------- Pines actuadores --------------------
#define PIN_BOMBA1      27   // MOSFET activo HIGH (binario)
#define PIN_PTC         33   // MOSFET activo HIGH (binario)
#define PIN_BOMBA2_PWM  26   // MOSFET activo HIGH (PWM)
#define PIN_LED_GROW    25   // MOSFET activo HIGH (PWM)
#define PIN_HUMIDIF     14
   // RELÃ activo LOW
#define PIN_VENT1       23   // RELÃ activo LOW
#define PIN_VENT2       13   // RELÃ activo LOW
#define PIN_SERVO       18

// -------------------- Config --------------------
#define DHTTYPE         DHT22
#define RS485BAUD       4800
#define RELAY_ON        LOW
#define RELAY_OFF       HIGH
#define MOSFET_ON       HIGH
#define MOSFET_OFF      LOW
#define PWM_FREQ        5000
#define PWM_RESOLUTION  8
#define INTERVALO_SENSORES   5000
#define INTERVALO_PRUEBA     30000

// -------------------- CalibraciÃ³n --------------------
#define PHOFFSET        0.0
#define HIGROSECO       3000
#define HIGROHUMEDO     1500
#define DHT_TEMP_OFFSET 0.0
#define DHT_HUM_OFFSET  0.0

// -------------------- MQ135 --------------------
#define MQ135_VREF      3.3
#define MQ135_RL        20000.0
#define MQ135_R0        13337.13
#define MQ135_A_CO2     466.0
#define MQ135_B_CO2    -0.35

BH1750 sensorLuz;
DHT dht(PINDHT22, DHTTYPE);
Servo servoPersiana;

struct DatosSensores {
  float luz, tempAire, humAire;
  int   rawCO2;
  float voltCO2, ppmCO2, valorPH, humSuelo;
  float humSueloRS, tempSuelo, phSuelo;
  int   ce;
  bool  rs485OK;
};

DatosSensores datos;
unsigned long ultimaLectura          = 0;
unsigned long ultimaPruebaActuadores = 0;
byte cmdRS485[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x04, 0x44, 0x09};

// ==================== Helpers ====================
void relayOn(int pin)   { digitalWrite(pin, RELAY_ON);   }
void relayOff(int pin)  { digitalWrite(pin, RELAY_OFF);  }
void mosfetOn(int pin)  { digitalWrite(pin, MOSFET_ON);  }
void mosfetOff(int pin) { digitalWrite(pin, MOSFET_OFF); }

void apagarTodoActuador() {
  mosfetOff(PIN_BOMBA1);        // MOSFET binario
  mosfetOff(PIN_PTC);           // MOSFET binario
  relayOff(PIN_HUMIDIF);        // RELÃ
  relayOff(PIN_VENT1);          // RELÃ
  relayOff(PIN_VENT2);          // RELÃ
  ledcWrite(PIN_BOMBA2_PWM, 0); // MOSFET PWM
  ledcWrite(PIN_LED_GROW,   0); // MOSFET PWM
  servoPersiana.write(0);
}

void fadePWM(int pin, int desde, int hasta, int paso, int pausaMs) {
  if (paso <= 0) paso = 1;
  if (desde < hasta) {
    for (int v = desde; v <= hasta; v += paso) { ledcWrite(pin, v); delay(pausaMs); }
  } else {
    for (int v = desde; v >= hasta; v -= paso) { ledcWrite(pin, v); delay(pausaMs); }
  }
}

void moverServoLento(int desde, int hasta, int paso, int pausaMs) {
  if (paso <= 0) paso = 1;
  if (desde < hasta) {
    for (int ang = desde; ang <= hasta; ang += paso) { servoPersiana.write(ang); delay(pausaMs); }
  } else {
    for (int ang = desde; ang >= hasta; ang -= paso) { servoPersiana.write(ang); delay(pausaMs); }
  }
}

// ==================== Prueba ====================
void pruebaActuadores() {
  Serial.println();
  Serial.println("======================================");
  Serial.println("INICIO PRUEBA DE ACTUADORES");
  Serial.println("======================================");
  apagarTodoActuador();
  delay(1000);

  Serial.println("[MOSFET] Bomba 1 -> 5 s");
  mosfetOn(PIN_BOMBA1); delay(5000); mosfetOff(PIN_BOMBA1);
  Serial.println("[MOSFET] Bomba 1 -> APAGADA");
  delay(1000);

  Serial.println("[MOSFET] PTC -> 5 s");
  mosfetOn(PIN_PTC); delay(5000); mosfetOff(PIN_PTC);
  Serial.println("[MOSFET] PTC -> APAGADO");
  delay(1000);

  Serial.println("[RELE] Humidificador -> 5 s");
  relayOn(PIN_HUMIDIF); delay(5000); relayOff(PIN_HUMIDIF);
  Serial.println("[RELE] Humidificador -> APAGADO");
  delay(1000);

  Serial.println("[RELE] Ventilador 1 -> 5 s");
  relayOn(PIN_VENT1); delay(5000); relayOff(PIN_VENT1);
  Serial.println("[RELE] Ventilador 1 -> APAGADO");
  delay(1000);

  Serial.println("[RELE] Ventilador 2 -> 5 s");
  relayOn(PIN_VENT2); delay(5000); relayOff(PIN_VENT2);
  Serial.println("[RELE] Ventilador 2 -> APAGADO");
  delay(1000);

  Serial.println("[MOSFET] Bomba 2 PWM -> fade subida ~3 s");
  fadePWM(PIN_BOMBA2_PWM, 0, 255, 2, 25); delay(1000);
  Serial.println("[MOSFET] Bomba 2 PWM -> fade bajada ~3 s");
  fadePWM(PIN_BOMBA2_PWM, 255, 0, 2, 25); delay(1000);

  Serial.println("[MOSFET] LED Grow PWM -> fade subida ~3 s");
  fadePWM(PIN_LED_GROW, 0, 255, 2, 25); delay(1000);
  Serial.println("[MOSFET] LED Grow PWM -> fade bajada ~3 s");
  fadePWM(PIN_LED_GROW, 255, 0, 2, 25); delay(1000);

  Serial.println("[SERVO] Persiana -> barrido 0-120");
  moverServoLento(0, 120, 1, 30); delay(800);
  moverServoLento(120, 0, 1, 30);

  apagarTodoActuador();
  Serial.println("======================================");
  Serial.println("FIN PRUEBA DE ACTUADORES");
  Serial.println("======================================");
  Serial.println();
}

// ==================== Setup ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("ChontoAgent --- Sensores + Actuadores prueba");

  Wire.begin(21, 22);
  if (sensorLuz.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("BH1750 OK");
  } else {
    Serial.println("BH1750 NO encontrado");
  }

  dht.begin();
  Serial.println("DHT22 iniciado");

  pinMode(PINRS485DERE, OUTPUT);
  digitalWrite(PINRS485DERE, LOW);
  Serial2.begin(RS485BAUD, SERIAL_8N1, PINRS485RX, PINRS485TX);
  Serial.println("RS485 iniciado");

  analogReadResolution(12);
  Serial.println("ADC 12 bits OK");

  // MOSFETs binarios â arrancan LOW (apagados)
  pinMode(PIN_BOMBA1, OUTPUT); mosfetOff(PIN_BOMBA1);
  pinMode(PIN_PTC,    OUTPUT); mosfetOff(PIN_PTC);

  // RelÃ©s â arrancan HIGH (apagados)
  pinMode(PIN_HUMIDIF, OUTPUT); relayOff(PIN_HUMIDIF);
  pinMode(PIN_VENT1,   OUTPUT); relayOff(PIN_VENT1);
  pinMode(PIN_VENT2,   OUTPUT); relayOff(PIN_VENT2);

  // MOSFETs PWM â arrancan en 0
  ledcAttach(PIN_BOMBA2_PWM, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(PIN_LED_GROW,   PWM_FREQ, PWM_RESOLUTION);
  ledcWrite(PIN_BOMBA2_PWM, 0);
  ledcWrite(PIN_LED_GROW,   0);

  servoPersiana.setPeriodHertz(50);
  servoPersiana.attach(PIN_SERVO, 500, 2400);
  servoPersiana.write(0);

  Serial.println("Lecturas cada 5 s | Prueba actuadores cada 30 s");
}

// ==================== Loop ====================
void loop() {
  unsigned long ahora = millis();
  if (ahora - ultimaLectura >= INTERVALO_SENSORES) {
    ultimaLectura = ahora;
    leerTodosSensores();
    imprimirDatos();
  }
  if (ahora - ultimaPruebaActuadores >= INTERVALO_PRUEBA) {
    ultimaPruebaActuadores = ahora;
    pruebaActuadores();
  }
}

// ==================== Sensores ====================
void leerBH1750() { datos.luz = sensorLuz.readLightLevel(); }

void leerDHT22() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  datos.tempAire = isnan(t) ? -999 : (t + DHT_TEMP_OFFSET);
  datos.humAire  = isnan(h) ? -999 : (h + DHT_HUM_OFFSET);
}

void leerMQ135() {
  int raw = analogRead(PINMQ135);
  float volt = raw * MQ135_VREF / 4095.0;
  datos.rawCO2  = raw;
  datos.voltCO2 = volt;
  datos.ppmCO2  = -1;
  if (MQ135_R0 > 0.0 && volt > 0.1) {
    float Rs = MQ135_RL * (MQ135_VREF / volt - 1.0);
    float ratio = Rs / MQ135_R0;
    datos.ppmCO2 = MQ135_A_CO2 * pow(ratio, MQ135_B_CO2);
  }
}

void leerPHAgua() {
  long suma = 0;
  for (int i = 0; i < 10; i++) { suma += analogRead(PINPHAGUA); delay(10); }
  float volt = (suma / 10.0) * 3.3 / 4095.0;
  datos.valorPH = constrain(7.0 + (2.5 - volt) / 0.18 + PHOFFSET, 0.0, 14.0);
}

void leerHigrometro() {
  long suma = 0;
  for (int i = 0; i < 5; i++) { suma += analogRead(PINHIGROMETRO); delay(10); }
  datos.humSuelo = constrain(map(suma / 5, HIGROSECO, HIGROHUMEDO, 0, 100), 0, 100);
}

void leerRS485() {
  while (Serial2.available()) Serial2.read();
  digitalWrite(PINRS485DERE, HIGH);
  delay(10);
  Serial2.write(cmdRS485, sizeof(cmdRS485));
  Serial2.flush();
  delay(10);
  digitalWrite(PINRS485DERE, LOW);
  delay(500);

  if (Serial2.available() < 13) {
    datos.rs485OK = false;
    Serial.print("RS485 sin respuesta --- bytes = ");
    Serial.println(Serial2.available());
    return;
  }

  byte resp[13];
  Serial2.readBytes(resp, 13);

  Serial.print("RAW: ");
  for (int i = 0; i < 13; i++) {
    Serial.print("["); Serial.print(i); Serial.print("]=0x");
    if (resp[i] < 0x10) Serial.print("0");
    Serial.print(resp[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  if (resp[2] != 0x08) {
    Serial.println("Frame RS485 desalineado, IGNORADO");
    return;
  }

  uint16_t rawHum  = ((uint16_t)resp[3] << 8) | resp[4];
  uint16_t rawTemp = ((uint16_t)resp[5] << 8) | resp[6];
  uint16_t rawCE   = ((uint16_t)resp[7] << 8) | resp[8];
  uint16_t rawPH   = ((uint16_t)resp[9] << 8) | resp[10];

  datos.humSueloRS = rawHum  / 10.0;
  datos.tempSuelo  = (int16_t)rawTemp / 10.0;
  datos.ce         = rawCE;
  datos.phSuelo    = rawPH / 10.0;
  datos.rs485OK    = true;
}

void leerTodosSensores() {
  leerBH1750();
  leerDHT22();
  leerMQ135();
  leerPHAgua();
  leerHigrometro();
  leerRS485();
}

void imprimirDatos() {
  Serial.println();
  Serial.println("âââââââââââââââââââââââââââââââââââââââ");
  Serial.println("â         LECTURA DE SENSORES         â");
  Serial.println("âââââââââââââââââââââââââââââââââââââââ¤");
  Serial.print("â Luz:          "); Serial.print(datos.luz, 1); Serial.println(" lux");
  Serial.print("â Temp aire:    ");
  if (datos.tempAire == -999) Serial.println("ERROR");
  else { Serial.print(datos.tempAire, 1); Serial.println(" C"); }
  Serial.print("â Hum aire:     ");
  if (datos.humAire == -999) Serial.println("ERROR");
  else { Serial.print(datos.humAire, 1); Serial.println(" %"); }
  Serial.print("â CO2 raw:      ");
  Serial.print(datos.rawCO2);
  Serial.print(" ("); Serial.print(datos.voltCO2, 2); Serial.print(" V)");
  if (datos.ppmCO2 > 0) { Serial.print(" ~ "); Serial.print(datos.ppmCO2, 0); Serial.print(" ppm"); }
  Serial.println();
  Serial.print("â pH agua:      "); Serial.println(datos.valorPH, 2);
  Serial.print("â Hum suelo:    "); Serial.print(datos.humSuelo, 1); Serial.println(" %");
  Serial.println("âââââââââââââââââââââââââââââââââââââââ¤");
  if (datos.rs485OK) {
    Serial.print("â Hum suelo RS: "); Serial.print(datos.humSueloRS, 1); Serial.println(" %");
    Serial.print("â Temp suelo:   "); Serial.print(datos.tempSuelo, 1); Serial.println(" C");
    Serial.print("â CE:           "); Serial.print(datos.ce); Serial.println(" uS/cm");
    Serial.print("â pH suelo:     "); Serial.println(datos.phSuelo, 1);
  } else {
    Serial.println("â   RS485 sin respuesta              â");
  }
  Serial.println("âââââââââââââââââââââââââââââââââââââââ");
}

void debugHigrometro() {
  long suma = 0;
  for (int i = 0; i < 20; i++) { suma += analogRead(PINHIGROMETRO); delay(10); }
  Serial.print("HigrÃ³metro RAW = "); Serial.println(suma / 20);
}

void debugPHAgua() {
  long suma = 0;
  for (int i = 0; i < 20; i++) { suma += analogRead(PINPHAGUA); delay(10); }
  float raw  = suma / 20.0;
  float volt = raw * 3.3 / 4095.0;
  float pH   = constrain(7.0 + (2.5 - volt) / 0.18 + PHOFFSET, 0.0, 14.0);
  Serial.println("=== Debug pH agua SEN0161 ===");
  Serial.print("RAW ADC = "); Serial.println(raw);
  Serial.print("Volt    = "); Serial.println(volt, 4);
  Serial.print("pH calc = "); Serial.println(pH, 2);
}

void calibrarMQ135EnAireLimpio() {
  const int N = 100;
  long sumaRaw = 0;
  for (int i = 0; i < N; i++) { sumaRaw += analogRead(PINMQ135); delay(50); }
  float rawMedio = sumaRaw / (float)N;
  float volt = rawMedio * MQ135_VREF / 4095.0;
  float Rs   = MQ135_RL * (MQ135_VREF / volt - 1.0);
  float R0   = Rs / 3.6;
  Serial.println("=== Calibracion MQ135 en aire limpio ===");
  Serial.print("rawMedio = "); Serial.println(rawMedio);
  Serial.print("volt     = "); Serial.println(volt, 3);
  Serial.print("Rs       = "); Serial.println(Rs);
  Serial.print("R0 aprox = "); Serial.println(R0);
  Serial.println("Copia este valor en #define MQ135_R0");
}