#include <ESP8266WiFi.h>
#include <time.h>
#include <coredecls.h> 
#include <ArduinoOTA.h>

const char* ssid = "default314";
const char* password = "rev0lut10n";

// --- ПІНИ ---
#define PIN_DIN 0  // GPIO 0
#define PIN_CLK 2  // GPIO 2
#define PIN_CS  3  // GPIO 3 (RX)
#define PIN_PWR 1  // GPIO 1 (TX) - Вхід детектора напруги

#define MY_TZ "EET-2EEST,M3.5.0/3,M10.5.0/4"
#define NTP_SERVER "ua.pool.ntp.org"

// Інтервал оновлення часу: 1 година
#define NTP_INTERVAL 3600000 

// --- Глобальні змінні ---
byte videoBuffer[4][8]; 
unsigned long lastSyncMillis = 0; 
bool hasSyncedAtLeastOnce = false;

// Шрифт 5x7
const byte FONT_5x7[10][5] = {
  {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 0
  {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
  {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
  {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
  {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
  {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
  {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
  {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
  {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
  {0x06, 0x49, 0x49, 0x29, 0x1E}  // 9
};
const byte FONT_5x8[10][5] = {
  {0x7E, 0x81, 0x81, 0x81, 0x7E}, // 0 (Овал, порожні кути)
  {0x00, 0x04, 0xFF, 0x00, 0x00}, // 1 (Чиста одиниця без зайвих нижніх рисок)
  {0xC2, 0xA1, 0x91, 0x89, 0x86}, // 2
  {0x42, 0x81, 0x89, 0x89, 0x76}, // 3
  {0x18, 0x14, 0x12, 0xFF, 0x10}, // 4 (Класична четвірка)
  {0x4F, 0x89, 0x89, 0x89, 0x71}, // 5
  {0x7E, 0x89, 0x89, 0x89, 0x70}, // 6
  {0x01, 0xe1, 0x11, 0x09, 0x07}, // 7 (Рівна діагональ до самого низу)
  {0x76, 0x89, 0x89, 0x89, 0x76}, // 8
  {0x4E, 0x91, 0x91, 0x91, 0x7E}  // 9
};

// CALLBACK часу
void time_is_set(bool from_sntp) {
  if (from_sntp) {
    lastSyncMillis = millis();
    hasSyncedAtLeastOnce = true;
  }
}

// Налаштування інтервалу NTP
extern "C" uint32_t sntp_update_delay_MS_rfc_not_less_than_15000 () {
    return NTP_INTERVAL; 
}

void setup() {
  // 1. Налаштування пінів дисплея
  pinMode(PIN_DIN, OUTPUT);
  pinMode(PIN_CLK, OUTPUT);
  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_DIN, HIGH);
  digitalWrite(PIN_CLK, HIGH);

  // 2. Налаштування піна детекції напруги (TX)
  // УВАГА: Це вимикає Serial Monitor!
  pinMode(PIN_PWR, INPUT); 

  initMax7219();
  settimeofday_cb(time_is_set);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  configTime(0, 0, NTP_SERVER); 
  setenv("TZ", MY_TZ, 1);
  tzset();

  ArduinoOTA.setHostname("ESP01-Clock-PowerSense");
  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 200) {
    lastUpdate = millis();

    time_t now = time(nullptr);
    struct tm* p_tm = localtime(&now);

    if (p_tm->tm_year + 1900 > 2000) {
      updateClockBuffer(p_tm->tm_hour, p_tm->tm_min, p_tm->tm_sec);
    } else {
      showLoading();
    }
    refreshDisplay();
  }
}

// ==========================================
// ЛОГІКА МАЛЮВАННЯ
// ==========================================
void updateClockBuffer(int h, int m, int s) {
  clearBuffer();

  int h_tens = h / 10;
  int h_ones = h % 10;
  int m_tens = m / 10;
  int m_ones = m % 10;

  // --- ЦИФРИ (Координати ті самі) ---
  if (h_tens > 0) drawDigitAt(3, h_tens); 
  drawDigitAt(9, h_ones);
  drawDigitAt(18, m_tens);
  drawDigitAt(24, m_ones);

  // --- ДВОКРАПКА ---
  if (s % 2 == 0) {
     drawPixel(15, 2, 1);
     drawPixel(15, 5, 1);
  }

  // --- ІНДИКАТОРИ СТАТУСУ (ПРАВИЙ КРАЙ X=31) ---

  // 1. ІНДИКАТОР БАТАРЕЇ (31, 0 - Верхній правий)
  // Логіка: Якщо на TX (GPIO1) низький рівень (немає 220В) -> СВІТИМО
  if (digitalRead(PIN_PWR) == LOW) {
    drawPixel(31, 0, 1); 
  }

  // 2. ІНДИКАТОР ЗАСТАРІЛОГО ЧАСУ (31, 7 - Нижній правий)
  // Логіка: Якщо давно не було оновлення -> СВІТИМО
  bool isDataFresh = false;

  if (hasSyncedAtLeastOnce) {
    unsigned long timeSinceSync = millis() - lastSyncMillis;
    // Поріг: Інтервал (1 год) + 5 хвилин допуску
    unsigned long staleThreshold = NTP_INTERVAL + 300000; 
    
    if (timeSinceSync < staleThreshold) {
      isDataFresh = true;
    }
  }

  // Якщо дані НЕ свіжі (або ніколи не синхронізувались) -> вмикаємо крапку
  if (!isDataFresh) {
    drawPixel(31, 7, 1);
  }
}

// --- Стандартні функції ---
/*
void drawDigitAt(int x, int num) {
  for (int col = 0; col < 5; col++) {
    byte columnByte = FONT_5x7[num][col];
    for (int bit = 0; bit < 7; bit++) {
      if (columnByte & (1 << bit)) {
        drawPixel(x + col, bit + 1, 1); 
      }
    }
  }
}
*/
void drawDigitAt(int x, int num) {
  for (int col = 0; col < 5; col++) {
    byte columnByte = FONT_5x8[num][col];
    for (int bit = 0; bit < 8; bit++) { // Читаємо всі 8 біт маски
      if (columnByte & (1 << bit)) {
        drawPixel(x + col, bit, 1);     // Малюємо без зміщення +1
      }
    }
  }
}
void drawPixel(int x, int y, int state) {
  if (x < 0 || x > 31 || y < 0 || y > 7) return;
  int matrixIdx = 3 - (x / 8); 
  int bitInRow  = x % 8;       
  if (state) videoBuffer[matrixIdx][y] |= (1 << (7 - bitInRow));
  else videoBuffer[matrixIdx][y] &= ~(1 << (7 - bitInRow));
}

void clearBuffer() {
  for(int m=0; m<4; m++) for(int r=0; r<8; r++) videoBuffer[m][r] = 0;
}

void refreshDisplay() {
  for (int row = 0; row < 8; row++) {
    digitalWrite(PIN_CS, LOW);
    for (int m = 3; m >= 0; m--) { 
      shiftOut(PIN_DIN, PIN_CLK, MSBFIRST, row + 1);
      shiftOut(PIN_DIN, PIN_CLK, MSBFIRST, videoBuffer[m][row]);
    }
    digitalWrite(PIN_CS, HIGH);
  }
}

void initMax7219() {
  sendCommandAll(0x0F, 0x00); 
  sendCommandAll(0x0C, 0x01); 
  sendCommandAll(0x0B, 0x07); 
  sendCommandAll(0x09, 0x00); 
  sendCommandAll(0x0A, 0x01); 
  clearBuffer();
  refreshDisplay();
}

void sendCommandAll(byte address, byte data) {
  digitalWrite(PIN_CS, LOW);
  for(int i=0; i<4; i++) {
    shiftOut(PIN_DIN, PIN_CLK, MSBFIRST, address);
    shiftOut(PIN_DIN, PIN_CLK, MSBFIRST, data);
  }
  digitalWrite(PIN_CS, HIGH);
}

void showLoading() {
  static int x = 0;
  clearBuffer();
  drawPixel(x, 4, 1);
  x++; if(x>31) x=0;
}
