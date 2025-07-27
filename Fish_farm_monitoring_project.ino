#include <BlynkSimpleShieldEsp8266.h>

// WiFi credentials
char ssid[] = "Khalid1";
char pass[] = "12345678";

// ESP8266 (ESP-01) communication on pins 2 (RX), 3 (TX)
SoftwareSerial EspSerial(2, 3);
ESP8266 wifi(&EspSerial);

// DS18B20 Setup
const byte ONE_WIRE_BUS = 4;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);

// Turbidity Sensor
const byte TURBIDITY_PIN = A0;
const byte NUM_SAMPLES = 100;

// pH Sensor
const int PH_PIN = A1;
const float V_THRESHOLD = 2.62;
const float V_BUFFER = 0.02; // ±0.02V deadband

// Actuator Pins
const byte HEATER_PIN = 8;
const byte COOLER_PIN = 9;
const byte HEATER_RELAY = 10;
const byte COOLER_RELAY = 11;

// Acid/Base pH Correction Motors
const byte ACID_MOTOR_PIN = 6;
const byte BASE_MOTOR_PIN = 7;

BlynkTimer timer;

// -------- Helper Functions --------
int readAverageAnalog(uint8_t pin, uint8_t samples = NUM_SAMPLES) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(3); // ~300ms total for 100 samples
  }
  return sum / samples;
}

int mapToPercent(int adcValue) {
  return map(adcValue, 0, 800, 100, 0); // Adjust based on sensor calibration
}

float calculatePH(float voltage) {
  float pH;
  if (voltage > V_THRESHOLD + V_BUFFER) {
    pH = -5.84 * voltage + 22.16;
  } else if (voltage < V_THRESHOLD - V_BUFFER) {
    pH = -6.27 * voltage + 23.28;
  } else {
    float pH1 = -5.84 * voltage + 22.16;
    float pH2 = -6.27 * voltage + 23.28;
    float weight = (voltage - (V_THRESHOLD - V_BUFFER)) / (2 * V_BUFFER);
    pH = pH1 * (1 - weight) + pH2 * weight;
  }
  return pH;
}

// -------- Main Sensor Update Function --------
void sendSensorData() {
  // Temperature
  tempSensor.requestTemperatures();
  float tempC = tempSensor.getTempCByIndex(0);

  // Turbidity
  int turbRaw = readAverageAnalog(TURBIDITY_PIN);
  int turbidityPercent = mapToPercent(turbRaw);

  // pH (moving average)
  int phRawAvg = readAverageAnalog(PH_PIN, 100);
  float phVoltage = phRawAvg * 5.0 / 1023.0;
  float pH = calculatePH(phVoltage);

  // Blynk Display
  Blynk.virtualWrite(V0, tempC);
  Blynk.virtualWrite(V1, pH);
  Blynk.virtualWrite(V2, turbidityPercent);

    // Acid/Base pH Correction Logic
  if (pH > 8.5 ) {
    digitalWrite(ACID_MOTOR_PIN, HIGH);
    delay(500);
    digitalWrite(ACID_MOTOR_PIN, LOW);

  } else if (pH < 6.5 ) {
    digitalWrite(BASE_MOTOR_PIN, HIGH);
    delay(500);
    digitalWrite(BASE_MOTOR_PIN, LOW);
  }
  else {
    digitalWrite(ACID_MOTOR_PIN, LOW);
    digitalWrite(BASE_MOTOR_PIN, LOW);
  }

  // Alerts
  if (tempC != DEVICE_DISCONNECTED_C && tempC > 30.0) {
    Blynk.logEvent("temperature_alert", "⚠️ High water temperature!");
  }
  if (turbidityPercent > 75) {
    Blynk.logEvent("ntu_alert", "⚠️ High turbidity level!");
  }
  if (pH < 5.5 || pH > 8.5) {
    Blynk.logEvent("ph_limit", "⚠️ Abnormal pH detected!");
  }

  // Serial Debug
  Serial.print(F("Temp: "));
  Serial.print(tempC == DEVICE_DISCONNECTED_C ? NAN : tempC, 2);
  Serial.print(F(" °C | Turbidity: "));
  Serial.print(turbRaw);
  Serial.print(F(" ("));
  Serial.print(turbidityPercent);
  Serial.print(F("%) | pH: "));
  Serial.print(pH, 2);
  Serial.print(" [");
  Serial.print(phVoltage, 3);
  Serial.println(" V]");


  // Heater/Cooler Logic
  if (tempC != DEVICE_DISCONNECTED_C) {
    if (tempC > 30.0) {
      digitalWrite(HEATER_PIN, HIGH);
      digitalWrite(COOLER_PIN, LOW);
      digitalWrite(HEATER_RELAY, LOW);
      digitalWrite(COOLER_RELAY, HIGH);
    } else if (tempC < 10.0) {
      digitalWrite(HEATER_PIN, LOW);
      digitalWrite(COOLER_PIN, HIGH);
      digitalWrite(HEATER_RELAY, HIGH);
      digitalWrite(COOLER_RELAY, LOW);
    } else {
      digitalWrite(HEATER_PIN, LOW);
      digitalWrite(COOLER_PIN, LOW);
      digitalWrite(HEATER_RELAY, HIGH);
      digitalWrite(COOLER_RELAY, HIGH);
    }
  }
}

void setup() {
  Serial.begin(38400);
  EspSerial.begin(38400);
  Blynk.begin(BLYNK_AUTH_TOKEN, wifi, ssid, pass);
  Serial.println(F("System Initialized"));

  tempSensor.begin();
  if (!tempSensor.getDeviceCount())
    Serial.println(F("⚠️ No DS18B20 detected"));

  pinMode(HEATER_PIN, OUTPUT);
  pinMode(COOLER_PIN, OUTPUT);
  pinMode(HEATER_RELAY, OUTPUT);
  pinMode(COOLER_RELAY, OUTPUT);
  pinMode(ACID_MOTOR_PIN, OUTPUT);
  pinMode(BASE_MOTOR_PIN, OUTPUT);

  digitalWrite(HEATER_PIN, LOW);
  digitalWrite(COOLER_PIN, LOW);
  digitalWrite(HEATER_RELAY, HIGH);
  digitalWrite(COOLER_RELAY, HIGH);
  digitalWrite(ACID_MOTOR_PIN, LOW);
  digitalWrite(BASE_MOTOR_PIN, LOW);

  timer.setInterval(5000L, sendSensorData);
}

void loop() {
  Blynk.run();
  timer.run();
}
