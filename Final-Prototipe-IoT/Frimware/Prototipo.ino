#include "arduino_secrets.h"



#define ENABLE_DATABASE

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <BH1750.h>
#include <DHT.h>

#include <FirebaseClient.h>
#include "ExampleFunctions.h"
#include <math.h>
#include <ESP32Servo.h>

// ==================== WiFi ====================
#define WIFI_SSID      "ssid"
#define WIFI_PASSWORD  "contraseÃ±a"

// ==================== Firebase ====================
#define DATABASE_URL   "Base de datos x"

// ==================== Pines sensores ====================
#define PINDHT22        15
#define PINMQ135        34
#define PINPHAGUA       35
#define PINHIGROMETRO   32
#define PINRS485TX      17
#define PINRS485RX      16
#define PINRS485DERE    5

// ==================== Pines actuadores ====================
#define PIN_BOMBA1      27
#define PIN_PTC         33
#define PIN_BOMBA2_PWM   4
#define PIN_LED_GROW    19
#define PIN_HUMIDIF     14
#define PIN_VENT1       23
#define PIN_VENT2       13
#define PIN_SERVO       18

// ==================== Config ====================
#define DHTTYPE         DHT22
#define RS485BAUD       4800

#define RELAY_ON        LOW
#define RELAY_OFF       HIGH
#define MOSFET_ON       HIGH
#define MOSFET_OFF      LOW

#define PWM_FREQ        5000
#define PWM_RESOLUTION  8

#define INTERVALO_SENSORES_MS   1000
#define INTERVALO_PRUEBA        700
#define INTERVALO_FIREBASE_TX   1000
#define INTERVALO_FIREBASE_RX   500
#define INTERVALO_WIFI_CHECK    1500

// ==================== Calibracion ====================
#define PHOFFSET         0.0
#define HIGROSECO        3000
#define HIGROHUMEDO      1500
#define DHT_TEMP_OFFSET  0.0
#define DHT_HUM_OFFSET   0.0

// ==================== MQ135 ====================
#define MQ135_VREF       3.3
#define MQ135_RL         20000.0
#define MQ135_R0         13337.13
#define MQ135_A_CO2      466.0
#define MQ135_B_CO2     -0.35

BH1750 sensorLuz;
DHT dht(PINDHT22, DHTTYPE);
Servo servoPersiana;

SSL_CLIENT ssl_client;
AsyncClientClass aClient(ssl_client);
NoAuth no_auth;
FirebaseApp app;
RealtimeDatabase Database;

struct DatosSensores {
  float luz = 0;
  float tempAire = 0;
  float humAire = 0;
  int   rawCO2 = 0;
  float voltCO2 = 0;
  float ppmCO2 = 0;
  float valorPH = 0;
  float humSuelo = 0;
  float humSueloRS = 0;
  float tempSuelo = 0;
  float phSuelo = 0;
  int   ce = 0;
  bool  rs485OK = false;
};

struct EstadoActuadores {
  int bomba1 = 0;
  int bomba2 = 0;
  int luzGrow = 0;
  int ptc = 0;
  int vent1 = 0;
  int vent2 = 0;
  int humidificador = 0;
  int persiana = 0;
};

DatosSensores datos;
EstadoActuadores estado;

unsigned long ultimaLectura = 0;
unsigned long ultimaPruebaActuadores = 0;
unsigned long ultimaLecturaFirebase = 0;
unsigned long ultimoEnvioFirebase = 0;
unsigned long ultimoChequeoWifi = 0;

byte cmdRS485[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x04, 0x44, 0x09};

// ==================== Helpers ====================
void relayOn(int pin)   { digitalWrite(pin, RELAY_ON); }
void relayOff(int pin)  { digitalWrite(pin, RELAY_OFF); }
void mosfetOn(int pin)  { digitalWrite(pin, MOSFET_ON); }
void mosfetOff(int pin) { digitalWrite(pin, MOSFET_OFF); }

int clamp255(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return v;
}

void apagarTodoActuador() {
  mosfetOff(PIN_BOMBA1);
  mosfetOff(PIN_PTC);
  relayOff(PIN_HUMIDIF);
  relayOff(PIN_VENT1);
  relayOff(PIN_VENT2);
  ledcWrite(PIN_BOMBA2_PWM, 0);
  ledcWrite(PIN_LED_GROW, 0);
  servoPersiana.write(0);
}

void aplicarActuadores() {
  int pwmB1 = estado.bomba1 ? 255 : 0;
  int pwmB2 = clamp255(estado.bomba2);
  int pwmLed = clamp255(estado.luzGrow);

  if (estado.bomba1) mosfetOn(PIN_BOMBA1); else mosfetOff(PIN_BOMBA1);
  if (estado.ptc) mosfetOn(PIN_PTC); else mosfetOff(PIN_PTC);

  ledcWrite(PIN_BOMBA2_PWM, pwmB2);
  ledcWrite(PIN_LED_GROW, pwmLed);

  digitalWrite(PIN_HUMIDIF, estado.humidificador ? LOW : HIGH);
  digitalWrite(PIN_VENT1, estado.vent1 ? LOW : HIGH);
  digitalWrite(PIN_VENT2, estado.vent2 ? LOW : HIGH);

  servoPersiana.write(constrain(estado.persiana, 0, 180));

  Serial.println("---- ACTUADORES ----");
  Serial.printf("Firebase bomba1=%d -> %s\n", estado.bomba1, estado.bomba1 ? "ON" : "OFF");
  Serial.printf("Firebase bomba2=%d -> PWM=%d\n", estado.bomba2, pwmB2);
  Serial.printf("Firebase luzGrow=%d -> PWM=%d\n", estado.luzGrow, pwmLed);
  Serial.printf("Firebase ptc=%d\n", estado.ptc);
  Serial.printf("Firebase humidificador=%d\n", estado.humidificador);
  Serial.printf("Firebase vent1=%d\n", estado.vent1);
  Serial.printf("Firebase vent2=%d\n", estado.vent2);
  Serial.printf("Firebase persiana=%d\n", estado.persiana);
}

void conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Conectando WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi NO conectado");
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
  datos.rawCO2 = raw;
  datos.voltCO2 = volt;
  datos.ppmCO2 = -1;

  if (MQ135_R0 > 0.0 && volt > 0.1) {
    float Rs = MQ135_RL * (MQ135_VREF / volt - 1.0);
    float ratio = Rs / MQ135_R0;
    datos.ppmCO2 = MQ135_A_CO2 * pow(ratio, MQ135_B_CO2);
  }
}

void leerPHAgua() {
  long suma = 0;
  for (int i = 0; i < 10; i++) {
    suma += analogRead(PINPHAGUA);
    delay(10);
  }
  float volt = (suma / 10.0) * 3.3 / 4095.0;
  datos.valorPH = constrain(7.0 + (2.5 - volt) / 0.18 + PHOFFSET, 0.0, 14.0);
}

void leerHigrometro() {
  long suma = 0;
  for (int i = 0; i < 5; i++) {
    suma += analogRead(PINHIGROMETRO);
    delay(10);
  }
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
    datos.rs485OK = false;
    return;
  }

  uint16_t rawHum  = ((uint16_t)resp[3] << 8) | resp[4];
  uint16_t rawTemp = ((uint16_t)resp[5] << 8) | resp[6];
  uint16_t rawCE   = ((uint16_t)resp[7] << 8) | resp[8];
  uint16_t rawPH   = ((uint16_t)resp[9] << 8) | resp[10];

  datos.humSueloRS = rawHum / 10.0;
  datos.tempSuelo   = (int16_t)rawTemp / 10.0;
  datos.ce          = rawCE;
  datos.phSuelo     = rawPH / 10.0;
  datos.rs485OK     = true;
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
  Serial.println("â           LECTURA DE SENSORES       â");
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

// ==================== Firebase ====================
void initFirebase() {
  set_ssl_client_insecure_and_buffer(ssl_client);
  initializeApp(aClient, app, getAuth(no_auth), auth_debug_print, "rtdbTask");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);
}

int leerEnteroSeguro(const String& ruta, int valorActual) {
  int v = Database.get<int>(aClient, ruta);
  if (aClient.lastError().code() != 0) {
    Serial.printf("Error leyendo %s -> code=%d\n", ruta.c_str(), aClient.lastError().code());
    return valorActual;
  }
  return v;
}

void publicarSensoresFirebase() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!app.ready()) return;

  Database.set<number_t>(aClient, "/sensores/luz", number_t(datos.luz, 1));
  Database.set<number_t>(aClient, "/sensores/tempAire", number_t(datos.tempAire, 1));
  Database.set<number_t>(aClient, "/sensores/humAire", number_t(datos.humAire, 1));
  Database.set<int>(aClient, "/sensores/rawCO2", datos.rawCO2);
  Database.set<number_t>(aClient, "/sensores/voltCO2", number_t(datos.voltCO2, 2));
  Database.set<number_t>(aClient, "/sensores/ppmCO2", number_t(datos.ppmCO2, 0));
  Database.set<number_t>(aClient, "/sensores/pHAgua", number_t(datos.valorPH, 2));
  Database.set<number_t>(aClient, "/sensores/humSueloCap", number_t(datos.humSuelo, 1));
  Database.set<number_t>(aClient, "/sensores/humSueloRS", number_t(datos.humSueloRS, 1));
  Database.set<number_t>(aClient, "/sensores/tempSuelo", number_t(datos.tempSuelo, 1));
  Database.set<int>(aClient, "/sensores/ceSuelo", datos.ce);
  Database.set<number_t>(aClient, "/sensores/phSuelo", number_t(datos.phSuelo, 1));
  Database.set<bool>(aClient, "/sensores/rs485OK", datos.rs485OK);
}

void leerActuadoresFirebase() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!app.ready()) return;

  estado.bomba1 = leerEnteroSeguro("/actuadores/pwm/bomba1", estado.bomba1);
  estado.bomba2 = leerEnteroSeguro("/actuadores/pwm/bomba2", estado.bomba2);
  estado.luzGrow = leerEnteroSeguro("/actuadores/pwm/luzGrow", estado.luzGrow);

  estado.ptc = leerEnteroSeguro("/actuadores/digitales/ptc", estado.ptc);
  estado.vent1 = leerEnteroSeguro("/actuadores/digitales/vent1", estado.vent1);
  estado.vent2 = leerEnteroSeguro("/actuadores/digitales/vent2", estado.vent2);
  estado.humidificador = leerEnteroSeguro("/actuadores/digitales/humidificador", estado.humidificador);

  estado.persiana = leerEnteroSeguro("/actuadores/servo/persiana", estado.persiana);

  aplicarActuadores();
}

void imprimirEstadoSerial() {
  Serial.println();
  Serial.println("=========== SENSORES ===========");
  Serial.printf("Luz: %.1f lux\n", datos.luz);
  Serial.printf("Temp aire: %.1f C\n", datos.tempAire);
  Serial.printf("Hum aire: %.1f %%\n", datos.humAire);
  Serial.printf("CO2 raw: %d | Volt: %.2f V | ppm: %.0f\n", datos.rawCO2, datos.voltCO2, datos.ppmCO2);
  Serial.printf("pH agua: %.2f\n", datos.valorPH);
  Serial.printf("Hum suelo: %.1f %%\n", datos.humSuelo);
  Serial.printf("Hum suelo RS485: %.1f %%\n", datos.humSueloRS);
  Serial.printf("Temp suelo: %.1f C\n", datos.tempSuelo);
  Serial.printf("CE suelo: %d uS/cm\n", datos.ce);
  Serial.printf("pH suelo: %.1f\n", datos.phSuelo);
  Serial.printf("RS485: %s\n", datos.rs485OK ? "OK" : "SIN COM");

  Serial.println("========= ACTUADORES =========");
  Serial.printf("bomba1=%d | bomba2=%d | luzGrow=%d\n", estado.bomba1, estado.bomba2, estado.luzGrow);
  Serial.printf("ptc=%d | vent1=%d | vent2=%d | humidificador=%d | persiana=%d\n",
                estado.ptc, estado.vent1, estado.vent2, estado.humidificador, estado.persiana);
}

// ==================== Setup ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("TomaCare --- ESP32 + Firebase");

  Wire.begin(21, 22);

  if (sensorLuz.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("BH1750 OK");
  } else {
    Serial.println("BH1750 NO encontrado");
  }

  dht.begin();

  pinMode(PINRS485DERE, OUTPUT);
  digitalWrite(PINRS485DERE, LOW);
  Serial2.begin(RS485BAUD, SERIAL_8N1, PINRS485RX, PINRS485TX);

  analogReadResolution(12);

  pinMode(PIN_BOMBA1, OUTPUT); mosfetOff(PIN_BOMBA1);
  pinMode(PIN_PTC, OUTPUT); mosfetOff(PIN_PTC);

  pinMode(PIN_HUMIDIF, OUTPUT); relayOff(PIN_HUMIDIF);
  pinMode(PIN_VENT1, OUTPUT); relayOff(PIN_VENT1);
  pinMode(PIN_VENT2, OUTPUT); relayOff(PIN_VENT2);

  ledcAttach(PIN_BOMBA2_PWM, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(PIN_LED_GROW, PWM_FREQ, PWM_RESOLUTION);
  ledcWrite(PIN_BOMBA2_PWM, 0);
  ledcWrite(PIN_LED_GROW, 0);

  servoPersiana.setPeriodHertz(50);
  servoPersiana.attach(PIN_SERVO, 500, 2400);
  servoPersiana.write(0);

  conectarWiFi();
  initFirebase();

  apagarTodoActuador();
  Serial.println("Sistema listo");
}

// ==================== Loop ====================
void loop() {
  app.loop();
  Database.loop();

  unsigned long ahora = millis();

  if (ahora - ultimoChequeoWifi >= INTERVALO_WIFI_CHECK) {
    ultimoChequeoWifi = ahora;
    if (WiFi.status() != WL_CONNECTED) conectarWiFi();
  }

  if (ahora - ultimaLecturaFirebase >= INTERVALO_FIREBASE_RX) {
    ultimaLecturaFirebase = ahora;
    leerActuadoresFirebase();
  }

  if (ahora - ultimaLectura >= INTERVALO_SENSORES_MS) {
    ultimaLectura = ahora;
    leerTodosSensores();
    imprimirDatos();
  }

  if (ahora - ultimoEnvioFirebase >= INTERVALO_FIREBASE_TX) {
    ultimoEnvioFirebase = ahora;
    publicarSensoresFirebase();
  }
}