#include <DHT.h>

// ---------- Pin map ----------
#define DHT_PIN   2
#define DHT_TYPE  DHT22
#define GAS_PIN   A0
#define LED_GREEN 8
#define LED_YELLOW 9
#define LED_RED   10
#define BUZZER    11

// ---------- Thresholds (tune these while testing) ----------
const int   GAS_WARN    = 250;   // ADC counts (0-1023)
const int   GAS_ALARM   = 350;
const float TEMP_WARN   = 40.0;  // deg C
const float TEMP_ALARM  = 55.0;
const int   GAS_HYST    = 10;    // hysteresis so the state does not flicker
const float TEMP_HYST   = 2.0;

const unsigned long SAMPLE_MS = 1000;  // sample once per second

enum State { NORMAL, WARNING, ALARM };

DHT dht(DHT_PIN, DHT_TYPE);
State state = NORMAL;
unsigned long lastSample = 0;

// Average 8 ADC readings to reduce noise
int readGasAverage() {
  long sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += analogRead(GAS_PIN);
    delay(2);
  }
  return (int)(sum / 8);
}

// Pure function: easy to unit test on a laptop
State nextState(State s, int gas, float temp) {
  switch (s) {
    case NORMAL:
      if (gas >= GAS_ALARM || temp >= TEMP_ALARM) return ALARM;
      if (gas >= GAS_WARN  || temp >= TEMP_WARN)  return WARNING;
      return NORMAL;
    case WARNING:
      if (gas >= GAS_ALARM || temp >= TEMP_ALARM) return ALARM;
      if (gas < GAS_WARN - GAS_HYST && temp < TEMP_WARN - TEMP_HYST) return NORMAL;
      return WARNING;
    case ALARM:
      if (gas < GAS_ALARM - GAS_HYST && temp < TEMP_ALARM - TEMP_HYST) return WARNING;
      return ALARM;
  }
  return NORMAL;
}

void setOutputs(State s) {
  digitalWrite(LED_GREEN,  s == NORMAL);
  digitalWrite(LED_YELLOW, s == WARNING);
  digitalWrite(LED_RED,    s == ALARM);
  if (s == ALARM) {
    tone(BUZZER, 1000);              // continuous tone
  } else if (s == WARNING) {
    tone(BUZZER, 500, 150);          // short beep each sample
  } else {
    noTone(BUZZER);
  }
}

const char* stateName(State s) {
  return s == NORMAL ? "NORMAL" : (s == WARNING ? "WARNING" : "ALARM");
}

void setup() {
  Serial.begin(9600);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  dht.begin();
  setOutputs(NORMAL);
  Serial.println("Safety monitor started");
}

void loop() {
  unsigned long now = millis();
  if (now - lastSample < SAMPLE_MS) return;   // non-blocking timing
  lastSample = now;

    float temp = dht.readTemperature();
  int gas = readGasAverage();

  // Sensor fault handling: a bad temperature must not block gas monitoring
  if (isnan(temp)) {
    Serial.println("ERROR: temperature sensor read failed");
    temp = -100.0;   // sentinel: temperature cannot trigger anything
  }

  state = nextState(state, gas, temp);
  setOutputs(state);
  
  Serial.print("Temp: ");  Serial.print(temp);
  Serial.print(" C | Gas: "); Serial.print(gas);
  Serial.print(" | State: "); Serial.println(stateName(state));
}
