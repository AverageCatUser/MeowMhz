// ============================================================================
//  MM (Meow MHz) — CyberMeow sub-GHz handheld                    [v1.0 - release]
//  ESP32-S3 Super Mini + ST7789 135x240 + CC1101
//
//  Two separate SPI buses:
//    Display -> HSPI (its own peripheral)   SCK 12  MOSI 11  CS 10  DC 9  RST 8
//    CC1101  -> FSPI (default, via ELECHOUSE)  SCK 14  SI 15  SO 16  CSN 17  GDO0 18
//
//  Menu: Read  |  Analyze (RSSI freq finder)  |  Transmit  |  Saved (flash library)
//
//  Flash: captures save to LittleFS on the ESP's internal flash and survive
//  power-off. NOTE: reflashing the firmware can wipe saves unless the LittleFS
//  partition is preserved. (An SD card would sidestep that later.)
//
//  Libraries:
//    "Adafruit ST7735 and ST7789 Library"  (+ GFX + BusIO)
//    "SmartRC-CC1101-Driver-Lib"           (ELECHOUSE_CC1101_SRC_DRV)
//
//  Board:  ESP32S3 Dev Module / USB CDC On Boot = Enabled / PSRAM = Disabled
//  Flashing: works with the CC1101 connected. Enter bootloader by replugging.
//
//  Controls (buttons mirror serial):  w=UP s=DOWN e=OK b=BACK f=FN
// ============================================================================

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WebServer.h>

// ---------- display pins (own HSPI bus) ----------
#define TFT_SCK   12
#define TFT_MOSI  11
#define TFT_CS    10
#define TFT_DC     9
#define TFT_RST    8
#define TFT_BL     7

// ---------- CC1101 pins (own FSPI bus, gold pads) ----------
#define CC_SCK    14
#define CC_MOSI   15
#define CC_MISO   16
#define CC_CSN    17
#define CC_GDO0   18

// ---------- buttons ----------
#define BTN_UP     1
#define BTN_DOWN   2
#define BTN_OK     4
#define BTN_BACK   5
#define BTN_FN     6

// ---------- display on a dedicated SPI peripheral ----------
SPIClass tftSPI(HSPI);
Adafruit_ST7789 tft = Adafruit_ST7789(&tftSPI, TFT_CS, TFT_DC, TFT_RST);

#define SCR_W 240
#define SCR_H 135

// ---------- phosphor palette ----------
#define C_BLACK  0x0000
#define C_GRN    0x07E0
#define C_GRN_M  0x0480
#define C_GRN_D  0x0200

// ---------- cat box ----------
#define CAT_X   164
#define CAT_Y    27
#define CAT_W    68
#define CAT_H    64
#define CAT_CX  (CAT_X + CAT_W / 2)
#define CAT_CY  (CAT_Y + CAT_H / 2)
enum { CAT_IDLE, CAT_HAPPY, CAT_SAD };

// ---------- radio config ----------
const float FREQS[] = {
  300.00, 303.87, 310.00, 315.00, 318.00, 330.00, 345.00,
  390.00, 418.00, 433.42, 433.92, 434.42, 868.35, 915.00
};
const int NFREQ = sizeof(FREQS) / sizeof(FREQS[0]);
int  rxFreqIdx = 10;   // 433.92 default
int  txFreqIdx = 10;
bool ccOK      = false;

// ---------- capture buffer ----------
#define CAP_MAX    1024
#define MIN_PULSE    40
#define GAP_END    6000
#define MIN_EDGES    16

volatile uint32_t capBuf[CAP_MAX];
volatile uint16_t capLen     = 0;
volatile uint32_t lastEdgeUs = 0;
volatile bool     listening  = false;

uint32_t shotBuf[CAP_MAX];
uint16_t shotLen  = 0;
uint32_t shotDur  = 0;
int      shotFreq = 10;
bool     haveShot = false;
uint32_t lastCaptureMs = 0;

// ---------- analyzer ----------
#define AN_THRESH -80
int anScan[NFREQ];
int anScanIdx = 0;
int anPeakIdx = -1;
int anPeakRssi = -127;

// ---------- flash storage ----------
#define SIG_MAGIC 0x47534D4D            // 'MMSG'
struct __attribute__((packed)) SigHeader { uint32_t magic; float freqMHz; uint16_t len; };
#define MAX_SIGS 24
bool     fsOK = false;
int      sigCount = 0;
float    sigFreq[MAX_SIGS];
uint16_t sigEdges[MAX_SIGS];
char     sigFile[MAX_SIGS][10];         // e.g. "/s07.bin"
int      sigSel = 0, sigTop = 0;

// ---------- web server (AP mode) ----------
WebServer   server(80);
const char *AP_SSID  = "MeowMHz";
const char *WEB_USER = "MM";
const char *WEB_PASS = "subghz";
String authToken = "";
bool   webOn = false;
File   upFile;
char   upPath[16];
bool   upOK = false;

void IRAM_ATTR onGdo0() {
  if (!listening) return;
  uint32_t now = micros();
  uint32_t d   = now - lastEdgeUs;
  lastEdgeUs   = now;
  if (capLen == 0) { capBuf[capLen++] = 0; return; }
  if (d < MIN_PULSE) return;
  if (capLen < CAP_MAX) capBuf[capLen++] = d;
}

// ---------- screens ----------
enum Screen   { SCR_MENU, SCR_READ, SCR_ANALYZE, SCR_XMIT, SCR_SAVED, SCR_WEB };
enum ReadPhase{ RD_IDLE, RD_LISTEN, RD_DONE };
Screen    screen  = SCR_MENU;
ReadPhase rdPhase = RD_IDLE;
#define MENU_N 5
int  menuSel  = 0;             // 0 READ, 1 ANALYZE, 2 TRANSMIT, 3 SAVED
bool needFull = true;

// ============================================================================
//  input
// ============================================================================
struct Btn { uint8_t pin; bool stable; bool lastRaw; uint32_t tChange; };
Btn btns[] = {
  { BTN_UP, true, true, 0 }, { BTN_DOWN, true, true, 0 },
  { BTN_OK, true, true, 0 }, { BTN_BACK, true, true, 0 },
  { BTN_FN, true, true, 0 },
};
const char BTN_CMD[] = { 'U', 'D', 'O', 'B', 'F' };
const int  NBTN = sizeof(btns) / sizeof(btns[0]);

char pollButtons() {
  char cmd = 0;
  uint32_t now = millis();
  for (int i = 0; i < NBTN; i++) {
    bool raw = digitalRead(btns[i].pin);
    if (raw != btns[i].lastRaw) { btns[i].lastRaw = raw; btns[i].tChange = now; }
    if (now - btns[i].tChange > 25 && raw != btns[i].stable) {
      btns[i].stable = raw;
      if (raw == LOW) cmd = BTN_CMD[i];
    }
  }
  return cmd;
}

char pollSerial() {
  while (Serial.available()) {
    char c = tolower(Serial.read());
    switch (c) {
      case 'w': return 'U';
      case 's': return 'D';
      case 'e': return 'O';
      case 'b': return 'B';
      case 'f': return 'F';
      default: break;
    }
  }
  return 0;
}

char pollInput() {
  char c = pollButtons();
  if (!c) c = pollSerial();
  return c;
}

// ============================================================================
//  radio helpers  (CC1101 owns the default SPI bus)
// ============================================================================
void radioConfigOOK() {
  ELECHOUSE_cc1101.setCCMode(0);
  ELECHOUSE_cc1101.setModulation(2);   // ASK/OOK
}
void radioRx(int fi) {
  detachInterrupt(digitalPinToInterrupt(CC_GDO0));
  listening = false;
  radioConfigOOK();
  ELECHOUSE_cc1101.setMHZ(FREQS[fi]);
  ELECHOUSE_cc1101.SetRx();
  pinMode(CC_GDO0, INPUT);
}
void startListen() {
  capLen = 0; lastEdgeUs = micros(); listening = true;
  attachInterrupt(digitalPinToInterrupt(CC_GDO0), onGdo0, CHANGE);
}
void stopListen() {
  listening = false;
  detachInterrupt(digitalPinToInterrupt(CC_GDO0));
}
void transmitShot(int reps) {
  stopListen();
  radioConfigOOK();
  ELECHOUSE_cc1101.setMHZ(FREQS[shotFreq]);
  ELECHOUSE_cc1101.SetTx();
  pinMode(CC_GDO0, OUTPUT);
  for (int r = 0; r < reps; r++) {
    bool level = HIGH;               // flip to LOW if a target won't trigger
    for (uint16_t i = 1; i < shotLen; i++) {
      digitalWrite(CC_GDO0, level);
      uint32_t us = shotBuf[i];
      while (us > 16000) { delayMicroseconds(16000); us -= 16000; }
      delayMicroseconds(us);
      level = !level;
    }
    digitalWrite(CC_GDO0, LOW);
    delay(12);
  }
  radioRx(rxFreqIdx);
}

// ============================================================================
//  flash storage (LittleFS)
// ============================================================================
bool saveCapture() {
  if (!fsOK || !haveShot || shotLen < 2) return false;
  char path[16]; int slot = -1;
  for (int i = 0; i < MAX_SIGS; i++) {
    snprintf(path, sizeof(path), "/s%02d.bin", i);
    if (!LittleFS.exists(path)) { slot = i; break; }
  }
  if (slot < 0) return false;                 // library full
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  SigHeader h; h.magic = SIG_MAGIC; h.freqMHz = FREQS[shotFreq]; h.len = shotLen;
  f.write((uint8_t*)&h, sizeof(h));
  f.write((uint8_t*)shotBuf, (size_t)shotLen * sizeof(uint32_t));
  f.close();
  return true;
}

void scanSaved() {
  sigCount = 0;
  if (!fsOK) return;
  for (int i = 0; i < MAX_SIGS && sigCount < MAX_SIGS; i++) {
    char path[16]; snprintf(path, sizeof(path), "/s%02d.bin", i);
    if (!LittleFS.exists(path)) continue;
    File f = LittleFS.open(path, "r");
    if (!f) continue;
    SigHeader h;
    if (f.read((uint8_t*)&h, sizeof(h)) == (int)sizeof(h) && h.magic == SIG_MAGIC) {
      strncpy(sigFile[sigCount], path, sizeof(sigFile[sigCount]) - 1);
      sigFile[sigCount][sizeof(sigFile[sigCount]) - 1] = 0;
      sigFreq[sigCount]  = h.freqMHz;
      sigEdges[sigCount] = h.len;
      sigCount++;
    }
    f.close();
  }
}

bool loadSig(int idx) {
  if (idx < 0 || idx >= sigCount) return false;
  File f = LittleFS.open(sigFile[idx], "r");
  if (!f) return false;
  SigHeader h;
  if (f.read((uint8_t*)&h, sizeof(h)) != (int)sizeof(h) || h.magic != SIG_MAGIC) {
    f.close(); return false;
  }
  uint16_t n = h.len; if (n > CAP_MAX) n = CAP_MAX;
  f.read((uint8_t*)shotBuf, (size_t)n * sizeof(uint32_t));
  f.close();
  shotLen = n;
  shotDur = 0; for (uint16_t i = 0; i < n; i++) shotDur += shotBuf[i];
  int best = 0; float bd = 1e9;               // snap to nearest tunable freq
  for (int i = 0; i < NFREQ; i++) { float d = fabs(FREQS[i] - h.freqMHz); if (d < bd) { bd = d; best = i; } }
  shotFreq = best; txFreqIdx = best;
  haveShot = true;
  return true;
}

bool deleteSig(int idx) {
  if (idx < 0 || idx >= sigCount || !fsOK) return false;
  return LittleFS.remove(sigFile[idx]);
}

// ============================================================================
//  web server (AP mode, plain-text UI)
// ============================================================================
bool validSigName(const String &f) {                 // must be exactly sNN.bin
  if (f.length() != 7) return false;
  if (f[0] != 's' || !isdigit(f[1]) || !isdigit(f[2])) return false;
  return f.endsWith(".bin");
}
bool isAuthed() {
  if (server.hasHeader("Cookie")) {
    String c = server.header("Cookie");
    if (authToken.length() && c.indexOf("mmauth=" + authToken) >= 0) return true;
  }
  return false;
}
void goLogin() { server.sendHeader("Location", "/login"); server.send(303); }

void sendLogin() {
  server.send(200, "text/html",
    "<html><head><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<style>body{background:#000;color:#0f0;font-family:monospace;margin:16px}"
    "input{background:#000;color:#0f0;border:1px solid #0f0;padding:4px;margin:3px 0}"
    "</style></head><body><h2>MEOW MHz</h2>"
    "<form method=POST action=/login>user<br><input name=u><br>"
    "pass<br><input name=p type=password><br><br>"
    "<input type=submit value=login></form></body></html>");
}
void handleRoot() {
  if (!isAuthed()) { goLogin(); return; }
  scanSaved();
  String h = "<html><head><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<style>body{background:#000;color:#0f0;font-family:monospace;margin:16px}"
    "a{color:#0f0}input{background:#000;color:#0f0;border:1px solid #0f0}"
    "hr{border-color:#030}</style></head><body><h2>MEOW MHz</h2>";
  if (server.hasArg("sent")) h += "<p>[sent]</p>";
  if (server.hasArg("up"))   h += "<p>[imported]</p>";
  h += "<p>saved: " + String(sigCount) + "/" + String(MAX_SIGS) + "</p><hr>";
  if (sigCount == 0) h += "<p>none saved</p>";
  for (int i = 0; i < sigCount; i++) {
    String fn = String(sigFile[i]).substring(1);      // drop leading '/'
    h += "<p><b>" + fn + "</b> - " + String(sigFreq[i], 2) + " MHz - "
       + String(sigEdges[i]) + " edges<br>"
       + "<a href='/send?f=" + fn + "'>[send]</a> &nbsp; "
       + "<a href='/dl?f="   + fn + "'>[download]</a></p>";
  }
  h += "<hr><p>import a .bin:</p>"
       "<form method=POST action=/up enctype='multipart/form-data'>"
       "<input type=file name=f> <input type=submit value=import></form>"
       "<hr><p><a href='/logout'>[logout]</a></p></body></html>";
  server.send(200, "text/html", h);
}
void handleLoginPost() {
  if (server.arg("u") == WEB_USER && server.arg("p") == WEB_PASS) {
    authToken = String((uint32_t)micros(), HEX) + String((long)random(0xFFFFFF), HEX);
    server.sendHeader("Set-Cookie", "mmauth=" + authToken + "; Path=/");
    server.sendHeader("Location", "/"); server.send(303);
  } else goLogin();
}
void handleLogout() { authToken = ""; goLogin(); }

void handleDownload() {
  if (!isAuthed()) { goLogin(); return; }
  String f = server.arg("f");
  if (!validSigName(f)) { server.send(400, "text/plain", "bad name"); return; }
  String path = "/" + f;
  if (!LittleFS.exists(path)) { server.send(404, "text/plain", "not found"); return; }
  File file = LittleFS.open(path, "r");
  server.sendHeader("Content-Disposition", "attachment; filename=" + f);
  server.streamFile(file, "application/octet-stream");
  file.close();
}
void handleSend() {
  if (!isAuthed()) { goLogin(); return; }
  String f = server.arg("f");
  if (!validSigName(f)) { server.send(400, "text/plain", "bad name"); return; }
  scanSaved();
  int idx = -1;
  for (int i = 0; i < sigCount; i++) if (String(sigFile[i]) == "/" + f) { idx = i; break; }
  if (idx >= 0 && loadSig(idx)) transmitShot(5);
  server.sendHeader("Location", "/?sent=1"); server.send(303);
}
void handleUploadDone() {
  if (!isAuthed()) { goLogin(); return; }
  server.sendHeader("Location", upOK ? "/?up=1" : "/"); server.send(303);
}
void handleUpload() {                                  // streamed multipart upload
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    upOK = false;
    if (upFile) upFile.close();
    if (!isAuthed() || !fsOK) return;         // don't let un-logged-in clients write
    int slot = -1;
    for (int i = 0; i < MAX_SIGS; i++) {
      snprintf(upPath, sizeof(upPath), "/s%02d.bin", i);
      if (!LittleFS.exists(upPath)) { slot = i; break; }
    }
    if (slot >= 0) upFile = LittleFS.open(upPath, "w");
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (upFile) upFile.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (upFile) {
      upFile.close();
      File f = LittleFS.open(upPath, "r");             // validate header, else discard
      SigHeader hd; upOK = false;
      if (f) {
        if (f.read((uint8_t*)&hd, sizeof(hd)) == (int)sizeof(hd) && hd.magic == SIG_MAGIC) upOK = true;
        f.close();
      }
      if (!upOK) LittleFS.remove(upPath);
    }
  }
}
void enterWeb() {
  stopListen();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);                                 // open network
  static bool routesSet = false;
  if (!routesSet) {                                     // register once, not per entry
    const char *hdrs[] = { "Cookie" };
    server.collectHeaders(hdrs, 1);
    server.on("/",       HTTP_GET,  handleRoot);
    server.on("/login",  HTTP_GET,  [](){ sendLogin(); });
    server.on("/login",  HTTP_POST, handleLoginPost);
    server.on("/logout", HTTP_GET,  handleLogout);
    server.on("/dl",     HTTP_GET,  handleDownload);
    server.on("/send",   HTTP_GET,  handleSend);
    server.on("/up",     HTTP_POST, handleUploadDone, handleUpload);
    server.onNotFound([](){ goLogin(); });
    routesSet = true;
  }
  server.begin();
  authToken = ""; webOn = true;
  screen = SCR_WEB; needFull = true; redraw();
}
void stopWeb() {
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  webOn = false; authToken = "";
  radioRx(rxFreqIdx);
}

// ============================================================================
//  the cat
// ============================================================================
void drawCat(int cx, int cy, uint8_t mood, bool blink, int tail) {
  uint16_t g = C_GRN;
  tft.fillTriangle(cx-12, cy-14, cx-3, cy-17, cx-9, cy-27, g);
  tft.fillTriangle(cx+12, cy-14, cx+3, cy-17, cx+9, cy-27, g);
  tft.drawCircle(cx, cy-6, 13, g);
  tft.drawLine(cx-11, cy+5, cx-15, cy+28, g);
  tft.drawLine(cx+11, cy+5, cx+15, cy+28, g);
  tft.drawFastHLine(cx-15, cy+28, 31, g);
  tft.fillCircle(cx-8, cy+28, 2, g);
  tft.fillCircle(cx+8, cy+28, 2, g);
  tft.drawLine(cx+14, cy+27, cx+21, cy+16, g);
  tft.drawLine(cx+21, cy+16, cx+20+tail, cy+4, g);
  if (blink) {
    tft.drawFastHLine(cx-8, cy-7, 5, g);
    tft.drawFastHLine(cx+3, cy-7, 5, g);
  } else if (mood == CAT_HAPPY) {
    tft.drawLine(cx-8, cy-6, cx-5, cy-9, g); tft.drawLine(cx-5, cy-9, cx-2, cy-6, g);
    tft.drawLine(cx+2, cy-6, cx+5, cy-9, g); tft.drawLine(cx+5, cy-9, cx+8, cy-6, g);
  } else if (mood == CAT_SAD) {
    tft.drawLine(cx-8, cy-9, cx-3, cy-6, g);
    tft.drawLine(cx+3, cy-6, cx+8, cy-9, g);
  } else {
    tft.fillCircle(cx-5, cy-7, 2, g);
    tft.fillCircle(cx+5, cy-7, 2, g);
  }
  tft.fillTriangle(cx-2, cy-3, cx+2, cy-3, cx, cy-1, g);
  tft.drawLine(cx, cy-1, cx-2, cy+1, g);
  tft.drawLine(cx, cy-1, cx+2, cy+1, g);
  tft.drawLine(cx-4, cy-2, cx-15, cy-4, g);
  tft.drawLine(cx-4, cy,   cx-15, cy+1, g);
  tft.drawLine(cx+4, cy-2, cx+15, cy-4, g);
  tft.drawLine(cx+4, cy,   cx+15, cy+1, g);
  tft.setTextSize(1); tft.setTextColor(g);
  if      (mood == CAT_HAPPY) { tft.setCursor(cx+14, cy-27); tft.print("!"); }
  else if (mood == CAT_SAD)   { tft.setCursor(cx+14, cy-27); tft.print("z"); }
}

uint8_t catMood() {
  if (!ccOK) return CAT_SAD;
  if (haveShot && millis() - lastCaptureMs < 5000) return CAT_HAPPY;
  return CAT_IDLE;
}
void animateCat() {
  tft.fillRect(CAT_X + 1, CAT_Y + 1, CAT_W - 2, CAT_H - 2, C_BLACK);
  bool blink = (millis() % 3200) < 140;
  int  tail  = (int)(sin(millis() / 350.0) * 4.0);
  drawCat(CAT_CX, CAT_CY, catMood(), blink, tail);
}

// ============================================================================
//  UI primitives
// ============================================================================
void chrome() {
  tft.fillScreen(C_BLACK);
  tft.drawRect(0, 0, SCR_W, SCR_H, C_GRN_M);
  tft.setTextColor(C_GRN); tft.setTextSize(2);
  tft.setCursor(6, 5); tft.print("CYBERMEOW");
  tft.drawFastHLine(4, 24, SCR_W - 8, C_GRN_M);
  tft.setTextSize(1);
  tft.setCursor(SCR_W - 46, 6);
  tft.print(FREQS[screen == SCR_XMIT ? txFreqIdx : rxFreqIdx], 2);
}

void hintLine(const char *s) {
  tft.fillRect(4, SCR_H - 12, SCR_W - 8, 10, C_BLACK);
  tft.setTextSize(1); tft.setTextColor(C_GRN_D);
  tft.setCursor(6, SCR_H - 11); tft.print(s);
}

void toast(const char *s, uint16_t col) {         // one-line message on the hint row
  tft.fillRect(4, SCR_H - 12, SCR_W - 8, 10, C_BLACK);
  tft.setTextSize(1); tft.setTextColor(col);
  tft.setCursor(6, SCR_H - 11); tft.print(s);
}

void drawWave(uint32_t *buf, uint16_t len, int x, int y, int w, int h) {
  tft.fillRect(x + 1, y + 1, w - 2, h - 2, C_BLACK);
  tft.drawRect(x, y, w, h, C_GRN_D);
  int ix = x + 2, iy = y + 2, iw = w - 4, ih = h - 4;
  if (len < 2) { tft.setCursor(x + w/2 - 4, y + h/2 - 3);
                 tft.setTextColor(C_GRN_D); tft.print("--"); return; }
  uint32_t total = 0;
  for (uint16_t i = 1; i < len; i++) total += buf[i];
  if (total == 0) return;
  float scale = (float)iw / (float)total;
  bool level = true; int cx = ix, prevY = iy;
  for (uint16_t i = 1; i < len; i++) {
    int seg = (int)(buf[i] * scale + 0.5f);
    if (seg < 1) seg = 1;
    int ly = level ? iy : iy + ih - 1;
    if (cx + seg > ix + iw) seg = ix + iw - cx;
    if (seg <= 0) break;
    tft.drawFastHLine(cx, ly, seg, C_GRN);
    tft.drawFastVLine(cx, min(prevY, ly), abs(prevY - ly) + 1, C_GRN);
    prevY = ly; cx += seg; level = !level;
  }
}

void drawRssi(int rssi) {
  int x = 8, y = 44, w = SCR_W - 16, h = 14;
  tft.drawRect(x, y, w, h, C_GRN_M);
  tft.fillRect(x + 1, y + 1, w - 2, h - 2, C_BLACK);
  int pct = rssi + 110; if (pct < 0) pct = 0; if (pct > 90) pct = 90;
  int fill = (int)((w - 2) * (pct / 90.0f));
  tft.fillRect(x + 1, y + 1, fill, h - 2, C_GRN);
  tft.fillRect(SCR_W - 62, y + h + 3, 54, 10, C_BLACK);
  tft.setTextSize(1); tft.setTextColor(C_GRN);
  tft.setCursor(SCR_W - 62, y + h + 3); tft.print(rssi); tft.print(" dBm");
}

// ============================================================================
//  screens
// ============================================================================
void drawMenu() {
  chrome();
  const char *items[MENU_N] = { "READ", "ANALYZE", "TRANSMIT", "SAVED", "WEB" };
  for (int i = 0; i < MENU_N; i++) {
    int y = 27 + i * 20;
    if (i == menuSel) { tft.fillRect(8, y, 148, 18, C_GRN); tft.setTextColor(C_BLACK); }
    else              { tft.drawRect(8, y, 148, 18, C_GRN_M); tft.setTextColor(C_GRN); }
    tft.setTextSize(2); tft.setCursor(14, y + 2); tft.print(items[i]);
  }
  tft.drawRect(CAT_X, CAT_Y, CAT_W, CAT_H, C_GRN_M);
  animateCat();
  tft.setTextSize(1);
  tft.setTextColor(ccOK ? C_GRN : C_GRN_D);
  tft.setCursor(CAT_X + 8, CAT_Y + CAT_H + 4); tft.print(ccOK ? "RF OK" : "RF --");
}

void webClients() {
  tft.fillRect(8, 114, SCR_W - 16, 10, C_BLACK);
  tft.setTextSize(1); tft.setTextColor(C_GRN_M);
  tft.setCursor(8, 114); tft.print("clients: "); tft.print(WiFi.softAPgetStationNum());
}
void drawWeb() {
  chrome();
  tft.setTextColor(C_GRN); tft.setTextSize(2);
  tft.setCursor(6, 28); tft.print("WEB ON");
  tft.setTextSize(1); tft.setTextColor(C_GRN);
  tft.setCursor(8, 52); tft.print("wifi:  "); tft.print(AP_SSID);
  tft.setCursor(8, 66); tft.print("go to  http://"); tft.print(WiFi.softAPIP());
  tft.setCursor(8, 80); tft.print("login: "); tft.print(WEB_USER); tft.print(" / "); tft.print(WEB_PASS);
  tft.setTextColor(C_GRN_D);
  tft.setCursor(8, 98); tft.print("send / export / import saved");
  webClients();
  hintLine("b = stop wifi");
}

void drawRead(bool full) {
  if (full) { chrome();
    tft.setTextColor(C_GRN); tft.setTextSize(2);
    tft.setCursor(6, 28); tft.print("READ"); }
  tft.fillRect(70, 28, SCR_W - 76, 14, C_BLACK);
  tft.setTextSize(1); tft.setTextColor(C_GRN);
  tft.setCursor(74, 30);
  if      (rdPhase == RD_IDLE)   tft.print("armed");
  else if (rdPhase == RD_LISTEN) tft.print("listening...");
  else                           tft.print("CAPTURED");

  if (rdPhase == RD_DONE) {
    drawWave(shotBuf, shotLen, 8, 46, SCR_W - 16, 46);
    tft.fillRect(8, 96, SCR_W - 16, 12, C_BLACK);
    tft.setTextColor(C_GRN); tft.setCursor(8, 98);
    tft.print("edges "); tft.print(shotLen);
    tft.print("  "); tft.print(shotDur / 1000.0f, 1); tft.print(" ms");
    tft.setCursor(8, 110); tft.setTextColor(C_GRN_M); tft.print("=^.^= caught it!");
    hintLine("e re-listen  up/dn freq  f save  b back");
  } else {
    hintLine("e listen   up/dn freq   b back");
  }
}

void drawAnalyze(bool full) {
  if (full) {
    chrome();
    tft.setTextColor(C_GRN); tft.setTextSize(2);
    tft.setCursor(6, 28); tft.print("ANALYZE");
    tft.setTextSize(1); tft.setTextColor(C_GRN_D);
    tft.setCursor(130, 34); tft.print("hold remote");
    hintLine("e use freq   f reset   b back");
  }
  int by = 48, bh = 42, x0 = 8, area = SCR_W - 16;
  tft.fillRect(x0, by, area, bh + 1, C_BLACK);
  int bw = area / NFREQ;
  for (int i = 0; i < NFREQ; i++) {
    int pct = anScan[i] + 110; if (pct < 0) pct = 0; if (pct > 90) pct = 90;
    int hgt = (int)(bh * (pct / 90.0f));
    int bx = x0 + i * bw;
    tft.fillRect(bx, by + bh - hgt, bw - 1, hgt, (i == anPeakIdx) ? C_GRN : C_GRN_M);
  }
  tft.fillRect(8, 98, SCR_W - 16, 18, C_BLACK);
  if (anPeakIdx >= 0) {
    tft.setTextSize(2); tft.setTextColor(C_GRN);
    tft.setCursor(8, 99); tft.print(FREQS[anPeakIdx], 2);
    tft.setTextSize(1); tft.setCursor(120, 99); tft.print("MHz");
    tft.setCursor(120, 108); tft.print(anPeakRssi); tft.print(" dBm");
  } else {
    tft.setTextSize(1); tft.setTextColor(C_GRN_D);
    tft.setCursor(8, 104); tft.print("no signal - hold your remote down");
  }
}

void drawSaved(bool full) {
  if (full) {
    chrome();
    tft.setTextColor(C_GRN); tft.setTextSize(2);
    tft.setCursor(6, 28); tft.print("SAVED");
    tft.setTextSize(1); tft.setTextColor(C_GRN_M);
    tft.setCursor(SCR_W - 52, 30); tft.print(sigCount); tft.print("/"); tft.print(MAX_SIGS);
    hintLine("e replay   f delete   b back");
  }
  tft.fillRect(6, 44, SCR_W - 12, 74, C_BLACK);
  if (sigCount == 0) {
    tft.setTextSize(1); tft.setTextColor(C_GRN_D);
    tft.setCursor(18, 72); tft.print("no saved signals");
    tft.setCursor(18, 84); tft.print("catch one in READ, press FN to save");
    return;
  }
  const int rows = 5, ry = 46, rh = 14;
  for (int i = 0; i < rows; i++) {
    int idx = sigTop + i;
    if (idx >= sigCount) break;
    int y = ry + i * rh;
    if (idx == sigSel) { tft.fillRect(6, y - 1, SCR_W - 12, rh, C_GRN); tft.setTextColor(C_BLACK); }
    else                 tft.setTextColor(C_GRN);
    tft.setTextSize(1); tft.setCursor(10, y + 2);
    tft.print(idx + 1); tft.print("   ");
    tft.print(sigFreq[idx], 2); tft.print(" MHz   ");
    tft.print(sigEdges[idx]); tft.print(" edges");
  }
  tft.setTextColor(C_GRN_D); tft.setTextSize(1);
  if (sigTop > 0)                 { tft.setCursor(SCR_W - 14, 46);  tft.print("^"); }
  if (sigTop + rows < sigCount)   { tft.setCursor(SCR_W - 14, 104); tft.print("v"); }
}

void drawXmit(bool full) {
  if (full) { chrome();
    tft.setTextColor(C_GRN); tft.setTextSize(2);
    tft.setCursor(6, 28); tft.print("TRANSMIT"); }
  tft.fillRect(8, 46, SCR_W - 16, 62, C_BLACK);
  tft.setTextSize(1);
  if (!haveShot) {
    tft.setTextColor(C_GRN_D);
    tft.setCursor(20, 60); tft.print("no capture");
    tft.setCursor(20, 74); tft.print("READ or SAVED first");
    hintLine("b back");
    return;
  }
  drawWave(shotBuf, shotLen, 8, 46, SCR_W - 16, 40);
  tft.setTextColor(C_GRN);
  tft.setCursor(8, 90);
  tft.print("freq "); tft.print(FREQS[txFreqIdx], 2);
  tft.print("  edges "); tft.print(shotLen);
  hintLine("e transmit x5   up/dn freq   b back");
}

void redraw() {
  switch (screen) {
    case SCR_MENU:    drawMenu(); break;
    case SCR_READ:    drawRead(needFull); break;
    case SCR_ANALYZE: drawAnalyze(needFull); break;
    case SCR_SAVED:   drawSaved(needFull); break;
    case SCR_WEB:     drawWeb(); break;
    case SCR_XMIT:    drawXmit(needFull); break;
  }
  needFull = false;
}

void enterAnalyze() {
  stopListen();
  screen = SCR_ANALYZE;
  anPeakIdx = -1; anPeakRssi = -127; anScanIdx = 0;
  for (int i = 0; i < NFREQ; i++) anScan[i] = -110;
  radioConfigOOK();
  needFull = true; redraw();
}

void enterSaved() {
  stopListen();
  scanSaved();
  sigSel = 0; sigTop = 0;
  screen = SCR_SAVED;
  needFull = true; redraw();
}

// ============================================================================
//  setup / loop
// ============================================================================
void setup() {
  Serial.begin(115200);

  pinMode(BTN_UP, INPUT_PULLUP);   pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);   pinMode(BTN_BACK, INPUT_PULLUP);
  pinMode(BTN_FN, INPUT_PULLUP);

  fsOK = LittleFS.begin(true);     // mount flash; format on first boot if needed

  // display on its own HSPI bus
  pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH);
  tftSPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.init(135, 240);
  tft.setSPISpeed(20000000);
  tft.setRotation(3);
  tft.fillScreen(C_BLACK);

  // CC1101 on the default (FSPI) bus, gold pads
  ELECHOUSE_cc1101.setSpiPin(CC_SCK, CC_MISO, CC_MOSI, CC_CSN);
  ELECHOUSE_cc1101.setGDO0(CC_GDO0);
  ELECHOUSE_cc1101.Init();
  ccOK = ELECHOUSE_cc1101.getCC1101();
  ELECHOUSE_cc1101.setPA(10);
  radioRx(rxFreqIdx);

  // splash
  tft.setTextColor(C_GRN); tft.setTextSize(3);
  tft.setCursor(48, 18); tft.print("MEOW MHz");
  tft.setTextSize(1); tft.setCursor(196, 34); tft.print("v1.0");
  drawCat(120, 82, ccOK ? CAT_HAPPY : CAT_SAD, false, 3);
  tft.setTextSize(1);
  tft.setTextColor(ccOK ? C_GRN : C_GRN_D);
  tft.setCursor(90, 118); tft.print(ccOK ? "CC1101: OK" : "CC1101: FAIL");
  if (!fsOK) { tft.setTextColor(C_GRN_D); tft.setCursor(70, 128); tft.print("flash mount failed"); }
  delay(1200);

  Serial.println("\nMM ready.  w/s=move e=ok b=back f=fn");
  needFull = true;
  redraw();
}

void loop() {
  char c = pollInput();

  // menu cat animation
  static uint32_t catT = 0;
  if (screen == SCR_MENU && millis() - catT > 140) { catT = millis(); animateCat(); }

  // web server
  if (screen == SCR_WEB) {
    server.handleClient();
    static uint32_t webT = 0;
    if (millis() - webT > 1000) { webT = millis(); webClients(); }
  }

  // live RSSI while armed in Read
  static uint32_t rssiT = 0;
  if (screen == SCR_READ && rdPhase == RD_IDLE && millis() - rssiT > 200) {
    rssiT = millis();
    drawRssi(ELECHOUSE_cc1101.getRssi());
  }

  // analyzer: hop one frequency per loop, redraw after a full sweep
  if (screen == SCR_ANALYZE) {
    ELECHOUSE_cc1101.setMHZ(FREQS[anScanIdx]);
    ELECHOUSE_cc1101.SetRx();
    delayMicroseconds(2500);                 // let RSSI settle after the hop
    anScan[anScanIdx] = ELECHOUSE_cc1101.getRssi();
    anScanIdx++;
    if (anScanIdx >= NFREQ) {
      anScanIdx = 0;
      int mi = 0;
      for (int i = 1; i < NFREQ; i++) if (anScan[i] > anScan[mi]) mi = i;
      if (anScan[mi] > AN_THRESH) { anPeakIdx = mi; anPeakRssi = anScan[mi]; }
      drawAnalyze(false);
    }
  }

  // Read capture state machine
  if (screen == SCR_READ && rdPhase == RD_LISTEN) {
    if (listening && capLen > 0 && capLen < MIN_EDGES &&
        (micros() - lastEdgeUs) > GAP_END) {
      listening = false; capLen = 0; lastEdgeUs = micros(); listening = true;
    }
    if ((capLen >= MIN_EDGES && (micros() - lastEdgeUs) > GAP_END) ||
         capLen >= CAP_MAX) {
      stopListen();
      shotLen = capLen; shotDur = 0;
      for (uint16_t i = 0; i < capLen; i++) { shotBuf[i] = capBuf[i]; shotDur += capBuf[i]; }
      shotFreq = rxFreqIdx; txFreqIdx = rxFreqIdx; haveShot = true;
      lastCaptureMs = millis();
      rdPhase = RD_DONE;
      Serial.print("captured "); Serial.print(shotLen);
      Serial.print(" edges @ "); Serial.println(FREQS[shotFreq]);
      drawRead(false);
    }
  }

  if (!c) return;

  switch (screen) {
    case SCR_MENU:
      if      (c == 'U') { menuSel = (menuSel + MENU_N - 1) % MENU_N; redraw(); }
      else if (c == 'D') { menuSel = (menuSel + 1) % MENU_N; redraw(); }
      else if (c == 'O') {
        if      (menuSel == 0) { screen = SCR_READ; rdPhase = RD_IDLE; radioRx(rxFreqIdx);
                                 needFull = true; redraw(); }
        else if (menuSel == 1) { enterAnalyze(); }
        else if (menuSel == 2) { screen = SCR_XMIT; txFreqIdx = shotFreq;
                                 needFull = true; redraw(); }
        else if (menuSel == 3) { enterSaved(); }
        else                   { enterWeb(); }
      }
      break;

    case SCR_READ:
      if (c == 'B') {
        stopListen(); screen = SCR_MENU; needFull = true; redraw();
      } else if (c == 'F') {                          // save capture to flash
        if (rdPhase == RD_DONE) {
          if (saveCapture()) toast("saved to flash!", C_GRN);
          else               toast("save failed (full or no flash)", C_GRN_D);
        }
      } else if (c == 'U' || c == 'D') {
        rxFreqIdx += (c == 'U') ? 1 : -1;
        if (rxFreqIdx < 0) rxFreqIdx = NFREQ - 1;
        if (rxFreqIdx >= NFREQ) rxFreqIdx = 0;
        radioRx(rxFreqIdx);
        rdPhase = RD_IDLE;
        needFull = true; redraw();
      } else if (c == 'O') {
        if (rdPhase == RD_LISTEN) { stopListen(); rdPhase = RD_IDLE; }
        else {
          rdPhase = RD_LISTEN; startListen();
          tft.fillRect(8, 44, SCR_W - 16, 74, C_BLACK);
        }
        drawRead(false);
      }
      break;

    case SCR_ANALYZE:
      if (c == 'B') { radioRx(rxFreqIdx); screen = SCR_MENU; needFull = true; redraw(); }
      else if (c == 'F') { anPeakIdx = -1; anPeakRssi = -127; drawAnalyze(false); }
      else if (c == 'O' && anPeakIdx >= 0) {
        rxFreqIdx = anPeakIdx; txFreqIdx = anPeakIdx;
        tft.fillRect(SCR_W - 48, 5, 44, 9, C_BLACK);          // refresh header freq
        tft.setTextColor(C_GRN); tft.setTextSize(1);
        tft.setCursor(SCR_W - 46, 6); tft.print(FREQS[rxFreqIdx], 2);
        toast("set as Read/TX freq!", C_GRN);
      }
      break;

    case SCR_WEB:
      if (c == 'B') { stopWeb(); screen = SCR_MENU; needFull = true; redraw(); }
      break;

    case SCR_SAVED:
      if (c == 'B') { screen = SCR_MENU; needFull = true; redraw(); }
      else if (sigCount > 0) {
        if (c == 'U') {
          if (sigSel > 0) sigSel--;
          if (sigSel < sigTop) sigTop = sigSel;
          drawSaved(false);
        } else if (c == 'D') {
          if (sigSel < sigCount - 1) sigSel++;
          if (sigSel >= sigTop + 5) sigTop = sigSel - 4;
          drawSaved(false);
        } else if (c == 'O') {                        // load + replay
          if (loadSig(sigSel)) {
            toast("TX...", C_GRN);
            transmitShot(5);
            toast("sent - e replay  f delete  b back", C_GRN);
          } else toast("load failed", C_GRN_D);
        } else if (c == 'F') {                         // delete
          if (deleteSig(sigSel)) {
            scanSaved();
            if (sigSel >= sigCount) sigSel = (sigCount > 0) ? sigCount - 1 : 0;
            if (sigTop > sigSel)    sigTop = sigSel;
            needFull = true; redraw();
          }
        }
      }
      break;

    case SCR_XMIT:
      if (c == 'B') { screen = SCR_MENU; needFull = true; redraw(); }
      else if ((c == 'U' || c == 'D') && haveShot) {
        txFreqIdx += (c == 'U') ? 1 : -1;
        if (txFreqIdx < 0) txFreqIdx = NFREQ - 1;
        if (txFreqIdx >= NFREQ) txFreqIdx = 0;
        shotFreq = txFreqIdx;
        drawXmit(false);
      } else if (c == 'O' && haveShot) {
        toast("TX...", C_GRN);
        transmitShot(5);
        Serial.println("sent x5");
        drawXmit(false); toast("SENT   e again   b back", C_GRN);
      }
      break;
  }
}
