
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <ArduinoJson.h>

#define PIN_POT_SPECTRAL 34

#define PIN_LED_RED      4
#define PIN_LED_GREEN    16
#define PIN_LED_BLUE     17

#define PIN_BUZZER       23
#define BUZZER_FREQ      2000

#define LCD_I2C_ADDR     0x27
#define LCD_COLS         16
#define LCD_ROWS         2

const byte KEYPAD_ROWS = 4;
const byte KEYPAD_COLS = 4;
byte rowPins[KEYPAD_ROWS] = {13, 14, 18, 19};
byte colPins[KEYPAD_COLS] = {25, 26, 27, 32};

char keys[KEYPAD_ROWS][KEYPAD_COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

const char* WIFI_SSID     = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";
const char* BACKEND_URL   = "http://host.wokwi.internal:8000/readings";
const char* DEVICE_ID     = "ESP32-FRESHFRUIT-01";
const char* API_KEY       = "freshfruit-secret-key-2026";

#define NUM_CHANNELS 18
#define SMOOTHING_SAMPLES 5

const int SPECTRAL_WAVELENGTHS[NUM_CHANNELS] = {
  410, 435, 460, 485, 510, 535, 560, 585, 610,
  645, 680, 705, 730, 760, 810, 860, 900, 940
};

enum SystemState {
  STATE_IDLE,
  STATE_NORMAL,
  STATE_ATTENTION,
  STATE_CRITICAL,
  STATE_FAULT
};

enum MaturityStage {
  STAGE_UNKNOWN,
  STAGE_IMMATURE,
  STAGE_RIPE,
  STAGE_OVERRIPE,
  STAGE_SPOILED
};

struct FruitConfig {
  const char* name;
  float minBrix;
  float maxBrix;
  float minChlorophyll;
  float maxChlorophyll;
  float maxShelfLifeDays;
  float normalMaturityMin;
  float normalMaturityMax;
  float criticalMaturityMax;
};

const FruitConfig FRUITS[7] = {
  {"Banana",  8.0, 22.0, 15.0, 90.0,  7.0, 45.0, 80.0, 92.0},
  {"Manga",   7.0, 18.0, 20.0, 85.0, 10.0, 40.0, 78.0, 90.0},
  {"Mamao",   6.0, 15.0, 18.0, 80.0,  8.0, 42.0, 76.0, 88.0},
  {"Maca",   10.0, 17.0, 10.0, 70.0, 20.0, 35.0, 75.0, 88.0},
  {"Uva",    12.0, 24.0, 12.0, 65.0, 12.0, 45.0, 82.0, 92.0},
  {"Laranja", 8.0, 14.0, 25.0, 80.0, 15.0, 38.0, 75.0, 88.0},
  {"Abacate", 4.0, 10.0, 30.0, 95.0,  9.0, 40.0, 78.0, 90.0}
};

struct SpectralIndicators {
  float channels[NUM_CHANNELS];
  float chlorophyllProxy;
  float solubleSolidsProxy;
  float relativeMoistureProxy;
  float maturityIndex;
  float estimatedShelfLifeDays;
  MaturityStage stage;
  SystemState state;
};

LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS);

int selectedFruitIndex = -1;
bool fruitConfirmed = false;
bool readingValid = false;

float rawHistory[SMOOTHING_SAMPLES];
int historyIndex = 0;
int historyCount = 0;

SpectralIndicators currentIndicators;

unsigned long lastReadTime = 0;
const unsigned long READ_INTERVAL = 400;

unsigned long lastDisplayTime = 0;
const unsigned long DISPLAY_INTERVAL = 2500;
int displayPage = 0;

unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 4000;

unsigned long lastBuzzerToggle = 0;
bool buzzerState = false;
bool faultLedState = false;

void initPeripherals();
void handleKeypad();
float readSpectralData();
bool validateReadings(float rawPotValue);
float smoothReadings(float newValue);
void calculateIndicators(float smoothedPot, int fruitIdx, SpectralIndicators &ind);
void classifyMaturity(int fruitIdx, SpectralIndicators &ind);
void updateDisplay();
void updateAlerts();
void printTelemetry();
void sendReading();
void setRgbColor(bool red, bool green, bool blue);
void beep(unsigned int ms);
void buzzerOff();

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println(F("\n========================================================"));
  Serial.println(F("   FRESH FRUIT - SISTEMA DE ESPECTROMETRIA E BORDA      "));
  Serial.println(F("       FIAP - GLOBAL SOLUTION 2026.2 - CITIZENTECH       "));
  Serial.println(F("========================================================"));
  Serial.println(F("Integrantes:"));
  Serial.println(F(" - Antonio do Nascimento Ferreira de Sousa (RM: 573706)"));
  Serial.println(F(" - Guilherme Pereira Ruiz da Silva (RM: 573360)"));
  Serial.println(F(" - Gustavo Leal (RM: 569361)"));
  Serial.println(F(" - Matheus Mendes Duarte da Silva (RM: 569559)"));
  Serial.println(F(" - Matheus Sato Oliveira do Prado (RM: 569392)"));
  Serial.println(F("========================================================\n"));

  initPeripherals();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print(F("[WIFI] Conectando a "));
  Serial.println(WIFI_SSID);

  unsigned long startWifi = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startWifi < 2500) {
    delay(100);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("\n[WIFI] Conectado com sucesso!"));
    Serial.print(F("[WIFI] IP: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("\n[WIFI] Aviso: Wi-Fi offline. Operacao autonoma (Edge Computing) ativada."));
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  FRESH  FRUIT  ");
  lcd.setCursor(0, 1);
  lcd.print("GS 2026.2 FIAP  ");
  delay(1800);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Selecione Fruta:");
  lcd.setCursor(0, 1);
  lcd.print("1..7 e tecle #  ");

  setRgbColor(false, false, true);
}

void loop() {
  unsigned long currentMillis = millis();

  handleKeypad();

  if (currentMillis - lastReadTime >= READ_INTERVAL) {
    lastReadTime = currentMillis;

    if (fruitConfirmed && selectedFruitIndex >= 0) {
      float rawPot = readSpectralData();
      readingValid = validateReadings(rawPot);

      if (readingValid) {
        float smoothedPot = smoothReadings(rawPot);
        calculateIndicators(smoothedPot, selectedFruitIndex, currentIndicators);
        classifyMaturity(selectedFruitIndex, currentIndicators);
      } else {
        currentIndicators.state = STATE_FAULT;
        currentIndicators.stage = STAGE_UNKNOWN;
      }

      printTelemetry();
    }
  }

  updateAlerts();

  if (currentMillis - lastDisplayTime >= DISPLAY_INTERVAL) {
    lastDisplayTime = currentMillis;
    displayPage = (displayPage + 1) % 2;
    updateDisplay();
  }

  if (fruitConfirmed && currentIndicators.state != STATE_IDLE &&
      (currentMillis - lastSendTime >= SEND_INTERVAL)) {
    lastSendTime = currentMillis;
    sendReading();
  }
}

void initPeripherals() {
  pinMode(PIN_POT_SPECTRAL, INPUT);

  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_BLUE, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
}

void beep(unsigned int ms) {
  tone(PIN_BUZZER, BUZZER_FREQ);
  delay(ms);
  noTone(PIN_BUZZER);
}

void buzzerOff() {
  if (buzzerState) {
    noTone(PIN_BUZZER);
    buzzerState = false;
  }
}

void handleKeypad() {
  char key = keypad.getKey();
  if (key == NO_KEY) return;

  Serial.print(F("[TECLADO] Tecla pressionada: "));
  Serial.println(key);

  if (key >= '1' && key <= '7') {
    selectedFruitIndex = key - '1';
    fruitConfirmed = false;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Fruta: ");
    lcd.print(FRUITS[selectedFruitIndex].name);
    lcd.setCursor(0, 1);
    lcd.print("Confirme com #  ");

    setRgbColor(false, false, true);
    beep(40);
  }
  else if (key == '#') {
    if (selectedFruitIndex >= 0) {
      fruitConfirmed = true;
      historyCount = 0;
      currentIndicators.state = STATE_IDLE;
      currentIndicators.stage = STAGE_UNKNOWN;
      lastReadTime = 0;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Analisando:     ");
      lcd.setCursor(0, 1);
      lcd.print(FRUITS[selectedFruitIndex].name);

      Serial.print(F("[SISTEMA] Fruta confirmada: "));
      Serial.println(FRUITS[selectedFruitIndex].name);

      beep(80);
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Selecione 1 a 7 ");
      lcd.setCursor(0, 1);
      lcd.print("antes de teclar#");
    }
  }
  else if (key == '*') {
    selectedFruitIndex = -1;
    fruitConfirmed = false;
    currentIndicators.state = STATE_IDLE;
    currentIndicators.stage = STAGE_UNKNOWN;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Analise cancelada");
    lcd.setCursor(0, 1);
    lcd.print("Selecione 1..7  ");

    setRgbColor(false, false, true);
    beep(120);

    Serial.println(F("[SISTEMA] Selecao cancelada."));
  }
}

float readSpectralData() {
  return (float)analogRead(PIN_POT_SPECTRAL);
}

bool validateReadings(float rawPotValue) {
  return !(isnan(rawPotValue) || rawPotValue < 0.0 || rawPotValue > 4095.0);
}

float smoothReadings(float newValue) {
  rawHistory[historyIndex] = newValue;
  historyIndex = (historyIndex + 1) % SMOOTHING_SAMPLES;
  if (historyCount < SMOOTHING_SAMPLES) {
    historyCount++;
  }

  float sum = 0.0;
  for (int i = 0; i < historyCount; i++) {
    sum += rawHistory[i];
  }
  return sum / historyCount;
}

void calculateIndicators(float smoothedPot, int fruitIdx, SpectralIndicators &ind) {
  const FruitConfig &fruit = FRUITS[fruitIdx];

  float normValue = smoothedPot / 4095.0;

  for (int i = 0; i < NUM_CHANNELS; i++) {
    int wl = SPECTRAL_WAVELENGTHS[i];
    float baseReflectance = 20.0 + (wl - 400.0) * 0.06;

    if (wl >= 510 && wl <= 560) {
      ind.channels[i] = baseReflectance + (1.0 - normValue) * 35.0;
    }
    else if (wl == 680) {
      ind.channels[i] = baseReflectance + normValue * 25.0 - (1.0 - normValue) * 15.0;
    }
    else if (wl >= 705 && wl <= 760) {
      ind.channels[i] = baseReflectance + 20.0 + sin((float)wl * 0.01) * 5.0;
    }
    else if (wl >= 810 && wl <= 860) {
      ind.channels[i] = baseReflectance + normValue * 30.0;
    }
    else if (wl >= 900 && wl <= 940) {
      ind.channels[i] = baseReflectance + (1.0 - normValue * 0.4) * 22.0;
    }
    else {
      ind.channels[i] = baseReflectance + normValue * 15.0;
    }

    if (ind.channels[i] < 1.0) ind.channels[i] = 1.0;
    if (ind.channels[i] > 99.0) ind.channels[i] = 99.0;
  }

  ind.chlorophyllProxy = fruit.maxChlorophyll - normValue * (fruit.maxChlorophyll - fruit.minChlorophyll);
  if (ind.chlorophyllProxy < fruit.minChlorophyll) ind.chlorophyllProxy = fruit.minChlorophyll;

  ind.solubleSolidsProxy = fruit.minBrix + normValue * (fruit.maxBrix - fruit.minBrix);
  ind.relativeMoistureProxy = 88.0 - (normValue * 22.0);
  ind.maturityIndex = normValue * 100.0;

  float remainingMaturity = 1.0 - normValue;
  if (remainingMaturity < 0.0) remainingMaturity = 0.0;
  ind.estimatedShelfLifeDays = remainingMaturity * fruit.maxShelfLifeDays;
}

void classifyMaturity(int fruitIdx, SpectralIndicators &ind) {
  const FruitConfig &fruit = FRUITS[fruitIdx];
  float mat = ind.maturityIndex;

  if (mat < fruit.normalMaturityMin) {
    ind.stage = STAGE_IMMATURE;
    ind.state = STATE_ATTENTION;
  }
  else if (mat <= fruit.normalMaturityMax) {
    ind.stage = STAGE_RIPE;
    ind.state = STATE_NORMAL;
  }
  else if (mat <= fruit.criticalMaturityMax) {
    ind.stage = STAGE_OVERRIPE;
    ind.state = STATE_ATTENTION;
  }
  else {
    ind.stage = STAGE_SPOILED;
    ind.state = STATE_CRITICAL;
  }
}

void updateDisplay() {
  if (!fruitConfirmed || selectedFruitIndex < 0) {
    return;
  }

  if (currentIndicators.state == STATE_IDLE) {
    return;
  }

  if (currentIndicators.state == STATE_FAULT) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ERRO DE LEITURA ");
    lcd.setCursor(0, 1);
    lcd.print("Sensor Invalido ");
    return;
  }

  lcd.clear();
  if (displayPage == 0) {
    lcd.setCursor(0, 0);
    lcd.print(FRUITS[selectedFruitIndex].name);
    lcd.print(": ");

    switch (currentIndicators.stage) {
      case STAGE_IMMATURE: lcd.print("Verde");  break;
      case STAGE_RIPE:     lcd.print("Maduro"); break;
      case STAGE_OVERRIPE: lcd.print("MtMad");  break;
      case STAGE_SPOILED:  lcd.print("Passad"); break;
      default:             lcd.print("Analis"); break;
    }

    lcd.setCursor(0, 1);
    lcd.print("Mat:");
    lcd.print((int)currentIndicators.maturityIndex);
    lcd.print("% ");

    switch (currentIndicators.state) {
      case STATE_NORMAL:    lcd.print("NORMAL");  break;
      case STATE_ATTENTION: lcd.print("ATENCAO"); break;
      case STATE_CRITICAL:  lcd.print("CRITICO"); break;
      default:              lcd.print("AGUARDA"); break;
    }
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Brix: ");
    lcd.print(currentIndicators.solubleSolidsProxy, 1);
    lcd.print(" *Bx");

    lcd.setCursor(0, 1);
    lcd.print("Val:");
    lcd.print(currentIndicators.estimatedShelfLifeDays, 1);
    lcd.print("d Um:");
    lcd.print((int)currentIndicators.relativeMoistureProxy);
    lcd.print("%");
  }
}

void updateAlerts() {
  if (!fruitConfirmed) {
    setRgbColor(false, false, true);
    buzzerOff();
    return;
  }

  unsigned long currentMillis = millis();

  switch (currentIndicators.state) {
    case STATE_NORMAL:
      setRgbColor(false, true, false);
      buzzerOff();
      break;

    case STATE_ATTENTION:
      setRgbColor(true, true, false);
      buzzerOff();
      break;

    case STATE_CRITICAL:
      setRgbColor(true, false, false);
      if (currentMillis - lastBuzzerToggle >= 250) {
        lastBuzzerToggle = currentMillis;
        buzzerState = !buzzerState;
        if (buzzerState) tone(PIN_BUZZER, BUZZER_FREQ);
        else noTone(PIN_BUZZER);
      }
      break;

    case STATE_FAULT:
      if (currentMillis - lastBuzzerToggle >= 400) {
        lastBuzzerToggle = currentMillis;
        faultLedState = !faultLedState;
        setRgbColor(faultLedState, false, faultLedState);
      }
      buzzerOff();
      break;

    default:
      setRgbColor(false, false, true);
      buzzerOff();
      break;
  }
}

void setRgbColor(bool red, bool green, bool blue) {
  digitalWrite(PIN_LED_RED,   red   ? HIGH : LOW);
  digitalWrite(PIN_LED_GREEN, green ? HIGH : LOW);
  digitalWrite(PIN_LED_BLUE,  blue  ? HIGH : LOW);
}

void printTelemetry() {
  const char* stateStr = "DESCONHECIDO";
  if (currentIndicators.state == STATE_NORMAL) stateStr = "NORMAL (VERDE)";
  else if (currentIndicators.state == STATE_ATTENTION) stateStr = "ATENCAO (AMARELO)";
  else if (currentIndicators.state == STATE_CRITICAL) stateStr = "CRITICO (VERMELHO)";
  else if (currentIndicators.state == STATE_FAULT) stateStr = "FALHA";

  const char* stageStr = "INDEFINIDO";
  if (currentIndicators.stage == STAGE_IMMATURE) stageStr = "IMATURO / VERDE";
  else if (currentIndicators.stage == STAGE_RIPE) stageStr = "MADURO / IDEAL";
  else if (currentIndicators.stage == STAGE_OVERRIPE) stageStr = "MUITO MADURO";
  else if (currentIndicators.stage == STAGE_SPOILED) stageStr = "PASSADO / DETERIORADO";

  Serial.println(F("\n--- [FRESH FRUIT - TELEMETRIA DE BORDA] ---"));
  Serial.print(F("Dispositivo: ")); Serial.print(DEVICE_ID);
  Serial.print(F(" | Fruta: ")); Serial.println(FRUITS[selectedFruitIndex].name);
  Serial.print(F("Estado do Sistema: ")); Serial.print(stateStr);
  Serial.print(F(" | Estagio: ")); Serial.println(stageStr);
  Serial.print(F("Indice de Maturacao: ")); Serial.print(currentIndicators.maturityIndex, 1); Serial.println(F("%"));
  Serial.print(F("Solidos Soluveis (Brix): ")); Serial.print(currentIndicators.solubleSolidsProxy, 2); Serial.println(F(" Bx"));
  Serial.print(F("Proxy de Clorofila: ")); Serial.println(currentIndicators.chlorophyllProxy, 1);
  Serial.print(F("Umidade Estimada: ")); Serial.print(currentIndicators.relativeMoistureProxy, 1); Serial.println(F("%"));
  Serial.print(F("Vida Util Estimada: ")); Serial.print(currentIndicators.estimatedShelfLifeDays, 1); Serial.println(F(" dias"));

  Serial.print(F("Espectro Vis-NIR (18 canais %): ["));
  for (int i = 0; i < NUM_CHANNELS; i++) {
    Serial.print((int)currentIndicators.channels[i]);
    if (i < NUM_CHANNELS - 1) Serial.print(F(", "));
  }
  Serial.println(F("]"));
  Serial.println(F("------------------------------------------"));
}

void sendReading() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[HTTP] Wi-Fi desconectado. Mantendo operacao autonoma local."));
    return;
  }

  HTTPClient http;
  http.begin(BACKEND_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", API_KEY);
  http.setTimeout(1200);

  StaticJsonDocument<1024> doc;
  doc["device_id"] = DEVICE_ID;
  doc["fruit"] = FRUITS[selectedFruitIndex].name;
  doc["maturity_index"] = currentIndicators.maturityIndex;
  doc["chlorophyll_proxy"] = currentIndicators.chlorophyllProxy;
  doc["soluble_solids_proxy"] = currentIndicators.solubleSolidsProxy;
  doc["relative_moisture_proxy"] = currentIndicators.relativeMoistureProxy;
  doc["estimated_shelf_life_days"] = currentIndicators.estimatedShelfLifeDays;

  switch (currentIndicators.stage) {
    case STAGE_IMMATURE: doc["stage"] = "Imaturo / Verde"; break;
    case STAGE_RIPE:     doc["stage"] = "Maduro / Ideal"; break;
    case STAGE_OVERRIPE: doc["stage"] = "Muito Maduro"; break;
    case STAGE_SPOILED:  doc["stage"] = "Passado / Deteriorado"; break;
    default:             doc["stage"] = "Desconhecido"; break;
  }

  switch (currentIndicators.state) {
    case STATE_NORMAL:    doc["state"] = "NORMAL"; break;
    case STATE_ATTENTION: doc["state"] = "ATENCAO"; break;
    case STATE_CRITICAL:  doc["state"] = "CRITICO"; break;
    case STATE_FAULT:     doc["state"] = "FALHA"; break;
    default:              doc["state"] = "IDLE"; break;
  }

  JsonArray channelsArray = doc.createNestedArray("spectral_channels");
  for (int i = 0; i < NUM_CHANNELS; i++) {
    channelsArray.add(currentIndicators.channels[i]);
  }

  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody);

  if (httpResponseCode > 0) {
    Serial.print(F("[HTTP] Sucesso no envio ao Backend! Codigo: "));
    Serial.println(httpResponseCode);
  } else {
    Serial.print(F("[HTTP] Backend inacessivel ou erro de conexao: "));
    Serial.println(http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}
