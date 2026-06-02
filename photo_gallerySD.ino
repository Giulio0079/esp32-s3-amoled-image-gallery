/*
 * ============================================================
 *  Waveshare ESP32-S3 AMOLED 1.75 — Image Gallery (SD Card)
 * ============================================================
 *
 *  Descrizione:
 *    Galleria di immagini scorrevole che legge file dalla
 *    scheda SD e li visualizza sul display AMOLED integrato.
 *
 *  Hardware:
 *    - Board : Waveshare ESP32-S3 AMOLED 1.75"
 *    - Display: AMOLED integrato (driver interno)
 *    - Storage: MicroSD card
 *
 *  Autore   : [giulio0079]
 *  Data     : Giugno 2026
 *  AI       : Codice generato con l'assistenza di Claude (Anthropic)
 *             https://www.anthropic.com
 *
 *  DISCLAIMER:
 *    Questo codice è fornito "così com'è", senza alcuna garanzia.
 *    L'autore declina ogni responsabilità per danni diretti o
 *    indiretti derivanti dall'uso di questo software.
 *    Usare a proprio rischio.
 *
 *  Licenza  : MIT License
 * ============================================================
 */

/*
 * ============================================================
 *  PHOTO GALLERY – Waveshare ESP32-S3-Touch-AMOLED-1.75  v6
 * ============================================================
 *  PIN da pin_config.h UFFICIALE Waveshare:
 *
 *  Display QSPI:
 *    SDIO0=GPIO4  SDIO1=GPIO5  SDIO2=GPIO6  SDIO3=GPIO7
 *    SCLK=GPIO38  CS=GPIO12  RESET=GPIO39
 *
 *  Touch I2C:
 *    SDA=GPIO15  SCL=GPIO14  INT=GPIO11  RST=GPIO40
 *
 *  SD card (SDMMC):
 *    CLK=GPIO2  CMD=GPIO1  DATA=GPIO3
 *
 *  LIBRERIE:
 *    GFX_Library_for_Arduino  → OFFLINE dal demo package
 *    pin_config.h             → OFFLINE dal demo package
 *    SensorLib          v0.3.1
 *    XPowersLib         v0.2.6
 *    JPEGDEC            (online, Bodmer)
 *    PNGdec             (online, Bodmer)
 *    SD_MMC             (built-in ESP32 core)
 *
 *  BOARD: ESP32S3 Dev Module
 *    USB CDC On Boot : Enabled
 *    Flash           : 16MB
 *    PSRAM           : OPI PSRAM
 * ============================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <SD_MMC.h>
#include "Arduino_GFX_Library.h"
#include "pin_config.h"
#include <TouchDrv.hpp>
#include <XPowersLib.h>
#include <JPEGDEC.h>
#include <PNGdec.h>

#ifndef BLACK
  #define BLACK  0x0000
#endif
#ifndef WHITE
  #define WHITE  0xFFFF
#endif
#ifndef RED
  #define RED    0xF800
#endif
#ifndef GREEN
  #define GREEN  0x07E0
#endif

// ============================================================
//  DISPLAY
// ============================================================
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCLK,
  LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3
);
Arduino_CO5300 *gfx = new Arduino_CO5300(
  bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0
);

// ============================================================
//  PERIFERICHE
// ============================================================
TouchDrvCSTXXX touch;
XPowersAXP2101 pmu;
bool pmuOK   = false;
bool touchOK = false;

JPEGDEC jpeg;
PNG     png;

// ============================================================
//  GALLERIA
// ============================================================
#define MAX_FILES 200
char  filePaths[MAX_FILES][128];
int   fileCount    = 0;
int   currentIndex = 0;

uint8_t *pngBuf     = nullptr;
size_t   pngBufSize = 0;

// Swipe
struct { bool on; int16_t x0, xN; uint32_t t0; }
  sw = {false, 0, 0, 0};
#define SWIPE_MIN_PX  60
#define SWIPE_MAX_MS  600

// ============================================================
//  FORWARD DECLARATIONS
// ============================================================
void scanDir(const char *dir);
void showImage(int idx);
void showJpeg(const char *path);
void showPng(const char *path);
void showBattery();
void showError(const char *msg);
int  cbJpeg(JPEGDRAW *p);
int  cbPng(PNGDRAW *p);

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Photo Gallery v6 ===");

  Wire.begin(IIC_SDA, IIC_SCL);

  // AXP2101
  if (pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    pmuOK = true;
    Serial.println("[PMU] OK");
    pmu.enableBattDetection();
    pmu.enableVbusVoltageMeasure();
    pmu.enableBattVoltageMeasure();
    pmu.enableSystemVoltageMeasure();
  } else {
    Serial.println("[PMU] non trovato");
  }

  // Display
  gfx->begin();
  gfx->setBrightness(255);
  gfx->fillScreen(BLACK);
  Serial.println("[GFX] OK");

  // Touch
  touch.setPins(TP_RESET, TP_INT);
  if (touch.begin(Wire, 0x5A, IIC_SDA, IIC_SCL)) {
    touchOK = true;
    Serial.println("[Touch] OK (0x5A)");
  } else if (touch.begin(Wire, 0x15, IIC_SDA, IIC_SCL)) {
    touchOK = true;
    Serial.println("[Touch] OK (0x15)");
  } else {
    Serial.println("[Touch] non trovato");
  }

  // SD card SDMMC 1-bit
  SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("[SD] Mount fallito");
    showError("SD non trovata!\nInserisci scheda SD\ncon foto .jpg/.png");
    return;
  }
  Serial.printf("[SD] OK – %llu MB\n", SD_MMC.totalBytes() / (1024*1024));

  // DEBUG: stampa tutti i file nella root della SD
  Serial.println("[SD] Contenuto root:");
  File root = SD_MMC.open("/");
  File dbg = root.openNextFile();
  while (dbg) {
    Serial.printf("  %s %s\n", dbg.isDirectory() ? "[DIR]" : "[FILE]", dbg.name());
    dbg = root.openNextFile();
  }
  root.close();

  // Buffer PNG
  pngBufSize = 512 * 1024;
  pngBuf = (uint8_t*)ps_malloc(pngBufSize);
  if (!pngBuf) {
    pngBufSize = 64 * 1024;
    pngBuf = (uint8_t*)malloc(pngBufSize);
    Serial.println("[PNG] uso DRAM 64KB");
  }

  // Scansione – prova prima "/" poi "/sdcard"
  scanDir("/");
  if (fileCount == 0) scanDir("/sdcard");

  Serial.printf("[Gallery] %d immagini trovate\n", fileCount);

  if (fileCount == 0) {
    showError("Nessuna immagine trovata.\nCopia .jpg/.png\nnella SD.");
    return;
  }

  showImage(0);
}

// ============================================================
//  LOOP – swipe con timeout inattività
//  getPoint() aggiorna le coordinate solo quando il chip
//  rileva un nuovo evento. Usiamo un timeout di 200ms senza
//  nuovi eventi per capire che il dito si è alzato.
// ============================================================
#define TOUCH_IDLE_MS 200   // ms senza eventi = dito alzato

static int16_t  last_x     = -1;
static uint32_t last_event = 0;

void loop() {
  if (!touchOK) { delay(10); return; }

  int16_t tx = 0, ty = 0;
  uint8_t n = touch.getPoint(&tx, &ty, 1);

  if (n > 0) {
    last_event = millis();

    if (!sw.on) {
      // primo tocco
      sw.on = true;
      sw.x0 = tx;
      sw.xN = tx;
      sw.t0 = millis();
      Serial.printf("[Down] x=%d\n", tx);
    } else {
      // aggiorna solo se la X è cambiata di almeno 5px
      if (abs(tx - sw.xN) > 5) {
        sw.xN = tx;
        Serial.printf("[Move] x=%d\n", tx);
      }
    }
    last_x = tx;
  }

  // Timeout: se sono passati TOUCH_IDLE_MS senza eventi → dito alzato
  if (sw.on && (millis() - last_event) > TOUCH_IDLE_MS) {
    sw.on = false;
    int32_t  dx = sw.xN - sw.x0;
    uint32_t dt = (last_event + TOUCH_IDLE_MS) - sw.t0;
    Serial.printf("[Up] x0=%d xN=%d dx=%d dt=%dms\n", sw.x0, sw.xN, dx, dt);

    if (abs(dx) > SWIPE_MIN_PX && dt < SWIPE_MAX_MS) {
      currentIndex = (dx < 0)
        ? (currentIndex + 1) % fileCount
        : (currentIndex - 1 + fileCount) % fileCount;
      showImage(currentIndex);
    } else if (abs(dx) < 15) {
      showBattery();
    }
    last_x = -1;
  }

  delay(10);
}

// ============================================================
//  SCAN SD – trova tutti i JPG/PNG ricorsivamente
// ============================================================
void scanDir(const char *dir) {
  File root = SD_MMC.open(dir);
  if (!root || !root.isDirectory()) return;

  File f = root.openNextFile();
  while (f && fileCount < MAX_FILES) {
    if (f.isDirectory()) {
      // evita cartelle di sistema
      const char *dn = f.name();
      if (dn[0] != '.') {
        char sub[128];
        if (strcmp(dir, "/") == 0)
          snprintf(sub, sizeof(sub), "/%s", dn);
        else
          snprintf(sub, sizeof(sub), "%s/%s", dir, dn);
        scanDir(sub);
      }
    } else {
      const char *n = f.name();
      int l = strlen(n);
      auto ei = [&](const char *e) {
        int le = strlen(e);
        if (l < le) return false;
        for (int i = 0; i < le; i++)
          if (tolower((uint8_t)n[l-le+i]) != tolower((uint8_t)e[i])) return false;
        return true;
      };
      if (ei(".jpg") || ei(".jpeg") || ei(".png")) {
        // se dir è "/" il path è solo "/nome", altrimenti "/dir/nome"
        if (strcmp(dir, "/") == 0)
          snprintf(filePaths[fileCount], 128, "/%s", n);
        else
          snprintf(filePaths[fileCount], 128, "%s/%s", dir, n);
        Serial.printf("[SD] trovato: %s\n", filePaths[fileCount]);
        fileCount++;
      }
    }
    f = root.openNextFile();
  }
  root.close();
}

// ============================================================
//  SHOW IMAGE
// ============================================================
void showImage(int idx) {
  if (idx < 0 || idx >= fileCount) return;
  currentIndex = idx;
  const char *p = filePaths[idx];

  gfx->fillScreen(BLACK);
  gfx->setTextColor(0x4208);
  gfx->setTextSize(1);
  gfx->setCursor(8, 8);
  gfx->printf("%d/%d", idx+1, fileCount);

  int l = strlen(p);
  char e3[4] = {
    (char)tolower((uint8_t)p[l-3]),
    (char)tolower((uint8_t)p[l-2]),
    (char)tolower((uint8_t)p[l-1]), 0
  };

  Serial.printf("[Show] %s\n", p);

  if (!strcmp(e3, "jpg") || !strcmp(e3, "peg")) showJpeg(p);
  else if (!strcmp(e3, "png"))                  showPng(p);
}

// ============================================================
//  JPEG
// ============================================================
static Arduino_GFX *_g;
static int _jx, _jy;

int cbJpeg(JPEGDRAW *p) {
  _g->draw16bitRGBBitmap(p->x+_jx, p->y+_jy, p->pPixels, p->iWidth, p->iHeight);
  return 1;
}

void showJpeg(const char *path) {
  File f = SD_MMC.open(path);
  if (!f) { Serial.printf("[JPEG] apri fallito: %s\n", path); return; }
  size_t sz = f.size();
  Serial.printf("[JPEG] dimensione: %u bytes\n", sz);
  uint8_t *b = (uint8_t*)ps_malloc(sz);
  if (!b) b = (uint8_t*)malloc(sz);
  if (!b) { Serial.println("[JPEG] memoria esaurita"); f.close(); return; }
  f.read(b, sz);
  f.close();
  if (jpeg.openRAM(b, sz, cbJpeg)) {
    int iw = jpeg.getWidth(), ih = jpeg.getHeight();
    Serial.printf("[JPEG] %dx%d\n", iw, ih);
    _g  = gfx;
    _jx = max(0, (LCD_WIDTH  - iw) / 2);
    _jy = max(0, (LCD_HEIGHT - ih) / 2);
    int sc = 0;  // 0 = full resolution, nessuna riduzione
    jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
    // sc=0 sempre: nessuna riduzione qualità
    jpeg.decode(_jx, _jy, 0);
    jpeg.close();
  } else {
    Serial.println("[JPEG] decode fallito");
  }
  free(b);
}

// ============================================================
//  PNG
// ============================================================
static int _px, _py;

int cbPng(PNGDRAW *p) {
  uint16_t line[LCD_WIDTH];
  png.getLineAsRGB565(p, line, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
  _g->draw16bitRGBBitmap(_px, _py + p->y, line, p->iWidth, 1);
  return 1;
}

void showPng(const char *path) {
  File f = SD_MMC.open(path);
  if (!f) { Serial.printf("[PNG] apri fallito: %s\n", path); return; }
  size_t sz = f.size();
  Serial.printf("[PNG] dimensione: %u bytes\n", sz);
  if (sz > pngBufSize) { f.close(); Serial.println("[PNG] file troppo grande"); return; }
  f.read(pngBuf, sz);
  f.close();
  _g = gfx;
  if (png.openRAM(pngBuf, sz, cbPng) == PNG_SUCCESS) {
    _px = max(0, (LCD_WIDTH  - png.getWidth())  / 2);
    _py = max(0, (LCD_HEIGHT - png.getHeight()) / 2);
    png.decode(nullptr, 0);
    png.close();
  } else {
    Serial.println("[PNG] decode fallito");
  }
}

// ============================================================
//  BATTERY OVERLAY
// ============================================================
void showBattery() {
  gfx->fillRoundRect(70, 150, 326, 166, 18, 0x2104);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(100, 168);
  gfx->print("Batteria");
  if (pmuOK) {
    int  pct = pmu.getBatteryPercent();
    int  mv  = pmu.getBattVoltage();
    bool chg = pmu.isCharging();
    bool vb  = pmu.isVbusIn();
    gfx->setTextSize(3);
    gfx->setTextColor(pct > 20 ? GREEN : RED);
    gfx->setCursor(100, 200);
    gfx->printf("%d%%", pct);
    gfx->setTextSize(1);
    gfx->setTextColor(WHITE);
    gfx->setCursor(100, 248); gfx->printf("Tensione : %d mV", mv);
    gfx->setCursor(100, 264); gfx->printf("Ricarica : %s", chg ? "SI" : "No");
    gfx->setCursor(100, 280); gfx->printf("USB      : %s", vb  ? "SI" : "No");
  } else {
    gfx->setTextSize(1);
    gfx->setTextColor(0xFDA0);
    gfx->setCursor(100, 210);
    gfx->print("PMU non disponibile");
  }
  delay(3000);
  showImage(currentIndex);
}

// ============================================================
//  ERROR SCREEN
// ============================================================
void showError(const char *msg) {
  gfx->fillScreen(BLACK);
  gfx->setTextColor(0xFDA0);
  gfx->setTextSize(2);
  gfx->setCursor(20, 150);
  gfx->println("ERRORE");
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(20, 190);
  gfx->println(msg);
}
