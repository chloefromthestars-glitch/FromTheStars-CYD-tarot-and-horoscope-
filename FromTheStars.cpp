// FromTheStars - daily horoscope and tarot, for the Cheap Yellow Display.
//
// Tap DRAW 3 CARDS to pull three cards from the 78-card deck (22 major arcana
// and 56 minor arcana), each reversed about half the time. Read each card in
// its position (past, present, future), then the summary.
//
// The first time, pick your sign. DAILY HOROSCOPE shows today's reading for
// it. For that, tap WI-FI, pick your network and type the password with the
// on-screen keyboard. The board remembers the network and your sign.
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_random.h>
#include <time.h>
#include "config.h"

// The CYD's screen, same settings as LutroFX (ILI9341, mirrored, inverted).
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341_2 panel;
  lgfx::Bus_SPI bus;
  lgfx::Light_PWM light;
  lgfx::Touch_XPT2046 touchCtl;

 public:
  LGFX() {
    {
      auto c = bus.config();
      c.spi_host = HSPI_HOST; c.spi_mode = 0;
      c.freq_write = 40000000; c.freq_read = 16000000;
      c.spi_3wire = false; c.use_lock = true; c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk = 14; c.pin_mosi = 13; c.pin_miso = 12; c.pin_dc = 2;
      bus.config(c);
      panel.setBus(&bus);
    }
    {
      auto c = panel.config();
      c.pin_cs = 15; c.pin_rst = -1; c.pin_busy = -1;
      c.panel_width = 240; c.panel_height = 320;
      c.offset_x = 0; c.offset_y = 0; c.offset_rotation = 2;
      c.readable = true; c.invert = true;
      c.rgb_order = false; c.dlen_16bit = false; c.bus_shared = false;
      panel.config(c);
    }
    {
      auto c = light.config();
      c.pin_bl = 21; c.invert = false; c.freq = 12000; c.pwm_channel = 7;
      light.config(c);
      panel.setLight(&light);
    }
    {
      auto c = touchCtl.config();
      c.x_min = 300; c.x_max = 3900; c.y_min = 3700; c.y_max = 200;
      c.pin_int = -1; c.bus_shared = false; c.spi_host = -1;
      c.pin_sclk = 25; c.pin_mosi = 32; c.pin_miso = 39; c.pin_cs = 33;
      c.freq = 1000000;
      touchCtl.config(c);
      panel.setTouch(&touchCtl);
    }
    setPanel(&panel);
  }
};

static LGFX lcd;

// ── the deck ─────────────────────────────────────────────────────────────
static const char* MAJOR_NAME[22] = {
  "The Fool", "The Magician", "The High Priestess", "The Empress", "The Emperor",
  "The Hierophant", "The Lovers", "The Chariot", "Strength", "The Hermit",
  "Wheel of Fortune", "Justice", "The Hanged Man", "Death", "Temperance",
  "The Devil", "The Tower", "The Star", "The Moon", "The Sun", "Judgement", "The World"};

static const char* MAJOR_TEXT[22] = {
  "New beginnings, leaps of faith and a fresh start with an open heart.",
  "Skill, willpower, and having what you need to make things happen.",
  "Intuition and hidden knowledge. Listen to what you already sense.",
  "Abundance, nurturing, creativity and growth in comfort.",
  "Structure, authority, discipline and a stable foundation.",
  "Tradition, guidance, and learning from a teacher or an established path.",
  "Connection and choice. Your values line up in love or partnership.",
  "Drive and determination. Triumph through steady willpower.",
  "Courage, patience and gentle inner strength.",
  "Solitude and reflection. Searching for inner truth.",
  "Cycles and turning points. Change you can't fully control.",
  "Fairness, truth, and cause and effect. Honest decisions.",
  "A pause and a change of view. Letting go to see differently.",
  "An ending that makes room for change. Not literal death.",
  "Balance and moderation. Patience and blending opposites.",
  "Attachment and temptation. Feeling trapped by a habit.",
  "Sudden upheaval and revelation. Something old breaks down.",
  "Hope and healing. Renewed faith after hard times.",
  "Uncertainty and dreams. Things below the surface are not yet clear.",
  "Joy, success, clarity and warmth. Things go well.",
  "Awakening and reckoning. Answering a calling.",
  "Completion and wholeness. A cycle is fulfilled."};

static const char* SUIT_NAME[4] = {"Wands", "Cups", "Swords", "Pentacles"};
static const char* SUIT_THEME[4] = {
  "passion, drive and new ideas", "feelings, love and intuition",
  "thought, truth and conflict", "work, money, health and home"};
static const char* RANK_NAME[14] = {
  "Ace", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine", "Ten",
  "Page", "Knight", "Queen", "King"};
static const char* RANK_TEXT[14] = {
  "A fresh start in", "A choice or balance in", "Growth and early results in",
  "Stability, or a pause, in", "Struggle or loss in", "Help and a return to balance in",
  "A test or a need to reassess", "Steady effort and momentum in",
  "Near completion, and a need for care in", "An ending or culmination in",
  "A message or a lesson about", "Pursuit and action in",
  "Mature, caring mastery of", "Authority and skill over"};

struct Pick { int idx; bool rev; };
static Pick picks[3];

static String cardName, cardText;
static void cardInfo(int idx) {
  if (idx < 22) { cardName = MAJOR_NAME[idx]; cardText = MAJOR_TEXT[idx]; return; }
  int m = idx - 22, s = m / 14, r = m % 14;
  cardName = String(RANK_NAME[r]) + " of " + SUIT_NAME[s];
  cardText = String(RANK_TEXT[r]) + " " + SUIT_THEME[s] + ".";
}

static void pickCards() {
  for (int i = 0; i < 3; i++) {
    int idx;
    bool dup;
    do {
      idx = esp_random() % 78;
      dup = false;
      for (int j = 0; j < i; j++) if (picks[j].idx == idx) dup = true;
    } while (dup);
    picks[i] = {idx, (esp_random() & 1) != 0};
  }
}

static String summary() {
  int majors = 0, suit[4] = {0, 0, 0, 0}, rev = 0;
  for (auto& p : picks) {
    if (p.idx < 22) majors++;
    else suit[(p.idx - 22) / 14]++;
    if (p.rev) rev++;
  }
  String s;
  if (majors >= 2) s += "Big forces are at work, so this reading is about turning points. ";
  else if (majors == 0) s += "This reading is about everyday matters, and you have a lot of say in them. ";
  else s += "One big theme stands out. ";
  int top = 0;
  for (int i = 1; i < 4; i++) if (suit[i] > suit[top]) top = i;
  if (suit[top] >= 2) s += String("Much of it is about ") + SUIT_THEME[top] + ". ";
  if (rev >= 2) s += "Several cards are reversed, so go slowly and look at what's being held back. ";
  else if (rev == 0) s += "All three cards are upright, so the energy is flowing freely. ";
  cardInfo(picks[1].idx);
  s += "The center card, " + cardName + ", is where your attention matters most right now.";
  return s;
}

// ── daily horoscope ──────────────────────────────────────────────────────
static const char* SIGN_NAME[12] = {
  "Aries", "Taurus", "Gemini", "Cancer", "Leo", "Virgo",
  "Libra", "Scorpio", "Sagittarius", "Capricorn", "Aquarius", "Pisces"};

static const char* DAILY_TEXT[8] = {
  "Slow down and check the details before you commit. A small pause today saves a bigger fix later.",
  "Someone may need your help more than they say. A short message would mean a lot to them.",
  "A creative idea is ready to start. Don't wait for it to be perfect before you begin.",
  "Money and plans look clearer when you write them down. Keep one simple list today.",
  "Trust your first instinct on a decision, then test it with one practical step.",
  "You may feel pulled two ways. Choose the path you'd regret not trying.",
  "Rest counts as progress today. Recharge, then come back to the work with fresh eyes.",
  "A conversation you've been avoiding could clear the air. Start it gently."};

// ── saved settings (WiFi, sign) ──────────────────────────────────────────
static String wifiSsid, wifiPass;
static int sign = -1;                 // 0..11, or -1 until you pick one
static bool wifiOn = false;

static void loadSettings() {
  Preferences p;
  p.begin("tarot", true);
  wifiSsid = p.getString("ssid", "");
  wifiPass = p.getString("pass", "");
  sign = p.getInt("sign", -1);
  p.end();
}

static void saveWifi(const String& ssid, const String& pass) {
  Preferences p;
  p.begin("tarot", false);
  p.putString("ssid", ssid);
  p.putString("pass", pass);
  p.end();
  wifiSsid = ssid;
  wifiPass = pass;
}

static void saveSign(int s) {
  Preferences p;
  p.begin("tarot", false);
  p.putInt("sign", s);
  p.end();
  sign = s;
}

// ── WiFi ─────────────────────────────────────────────────────────────────
// Joins a network and waits up to timeoutMs. Sets the clock by internet time.
static bool connectWifi(const String& ssid, const String& pass, uint32_t timeoutMs = 15000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) break;
    if (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL) break;
    delay(200);
  }
  wifiOn = WiFi.status() == WL_CONNECTED;
  if (wifiOn) configTime(UTC_OFFSET_HOURS * 3600, 0, "pool.ntp.org");
  return wifiOn;
}

struct Net { String ssid; bool lock; int rssi; };
static Net nets[20];
static int netCount = 0;
static int netPage = 0;
static const int NET_ROWS = 5;

// Looks for networks nearby (a few seconds), strongest first, no repeats.
static void scanNets() {
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  netCount = 0;
  for (int i = 0; i < n && netCount < 20; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;
    bool dup = false;
    for (int j = 0; j < netCount; j++) if (nets[j].ssid == s) dup = true;
    if (!dup) nets[netCount++] = {s, WiFi.encryptionType(i) != WIFI_AUTH_OPEN, WiFi.RSSI(i)};
  }
  WiFi.scanDelete();
  for (int i = 1; i < netCount; i++) {          // insertion sort, strongest first
    Net t = nets[i];
    int j = i - 1;
    while (j >= 0 && nets[j].rssi < t.rssi) { nets[j + 1] = nets[j]; j--; }
    nets[j + 1] = t;
  }
  netPage = 0;
}

// ── drawing helpers ──────────────────────────────────────────────────────
struct Btn { int x, y, w, h; const char* label; bool on; };
static bool dirty = true;

static void drawBtn(Btn b) {
  lcd.fillRoundRect(b.x, b.y, b.w, b.h, 6, b.on ? TFT_GOLD : TFT_DARKGREY);
  lcd.setTextColor(b.on ? TFT_BLACK : TFT_WHITE, b.on ? TFT_GOLD : TFT_DARKGREY);
  lcd.setTextDatum(lgfx::middle_center);
  lcd.setFont(&fonts::Font2);
  lcd.drawString(b.label, b.x + b.w / 2, b.y + b.h / 2);
}

static bool hit(Btn b, int x, int y) {
  return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}

// Little sparkles: a four-point star with dots on the diagonals
static void sparkle(int x, int y, int r, uint16_t c) {
  lcd.drawLine(x - r, y, x + r, y, c);
  lcd.drawLine(x, y - r, x, y + r, c);
  int d = r / 2 + 1;
  lcd.drawPixel(x - d, y - d, c); lcd.drawPixel(x + d, y - d, c);
  lcd.drawPixel(x - d, y + d, c); lcd.drawPixel(x + d, y + d, c);
}

// A crescent moon
static void moon(int x, int y, int r) {
  lcd.fillCircle(x, y, r, TFT_YELLOW);
  lcd.fillCircle(x + r / 2 + 2, y - r / 3, r - 1, TFT_BLACK);
}

// Stars around the edges, kept clear of the text and buttons
static void starfield() {
  static const int16_t S[][3] = {
    {12, 20, 3}, {306, 22, 2}, {22, 118, 2}, {300, 124, 3},
    {14, 212, 2}, {306, 210, 3}, {30, 60, 1}, {292, 66, 1}, {160, 232, 1}};
  for (auto& s : S) sparkle(s[0], s[1], s[2], (s[2] == 3) ? TFT_GOLD : TFT_SKYBLUE);
}

// Draw text word by word, wrapped to a width. Returns the next free y.
static int wrapped(String s, int x, int y, int w, uint16_t color) {
  lcd.setTextColor(color, TFT_BLACK);
  lcd.setTextDatum(lgfx::top_left);
  lcd.setFont(&fonts::Font2);
  String line;
  int start = 0, cy = y;
  while (start <= (int)s.length()) {
    int sp = s.indexOf(' ', start);
    if (sp < 0) sp = s.length();
    String word = s.substring(start, sp);
    String test = line.length() ? line + " " + word : word;
    if (line.length() && lcd.textWidth(test.c_str()) > w) {
      lcd.drawString(line.c_str(), x, cy);
      cy += 18;
      line = word;
    } else {
      line = test;
    }
    start = sp + 1;
  }
  if (line.length()) { lcd.drawString(line.c_str(), x, cy); cy += 18; }
  return cy;
}

static void header(const char* t) {
  lcd.fillScreen(TFT_BLACK);
  starfield();
  lcd.setTextColor(TFT_GOLD, TFT_BLACK);
  lcd.setTextDatum(lgfx::top_center);
  lcd.setFont(&fonts::Font2);
  lcd.drawString(t, 160, 8);
}

static void message(const char* a, const char* b) {
  lcd.fillScreen(TFT_BLACK);
  starfield();
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.setTextDatum(lgfx::middle_center);
  lcd.setFont(&fonts::Font2);
  lcd.drawString(a, 160, 105);
  lcd.drawString(b, 160, 130);
}

// ── touch setup ──────────────────────────────────────────────────────────
static void calibrate(Preferences* p) {
  uint16_t cal[8];
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.setTextDatum(lgfx::middle_center);
  lcd.setFont(&fonts::Font2);
  lcd.drawString("TOUCH SETUP", 160, 90);
  lcd.drawString("Tap each arrow corner as it lights up", 160, 115);
  lcd.calibrateTouch(cal, TFT_YELLOW, TFT_BLACK, 24);
  p->putBytes("cal", cal, sizeof cal);
  lcd.setTouchCalibrate(cal);
  lcd.fillScreen(TFT_BLACK);
}

// ── screens ──────────────────────────────────────────────────────────────
enum Screen { HOME, READING, SIGNS, DAILY, NOWIFI, WIFI_LIST, PASSWORD_KEYS, WIFI_RESULT };
static Screen screen = HOME;
static int page = 0;   // 0..2 = cards, 3 = summary

static const Btn BTN_DRAW  = {60, 70, 200, 34, "DRAW 3 CARDS", false};
static const Btn BTN_DAILY = {60, 112, 200, 34, "DAILY HOROSCOPE", false};
static Btn BTN_SIGN = {10, 158, 150, 30, "", false};
static Btn BTN_WIFI = {170, 158, 140, 30, "WI-FI", false};
static const Btn BTN_BACK = {110, 196, 100, 32, "BACK", false};
static const Btn BTN_PREV = {6, 200, 92, 32, "BACK", false};
static const Btn BTN_NEXT = {222, 200, 92, 32, "NEXT", true};

static char signLabel[24];
static String kbText;           // what's typed on the keyboard
static int kbMode = 0;          // 0 lowercase, 1 UPPERCASE, 2 symbols
static String pendingSsid;      // network the password is for
static String resultTitle, resultLine, resultBtn;
static Screen resultNext = HOME;
static bool signSetup = false;  // true while picking your sign for the first time

static void drawHome() {
  lcd.fillScreen(TFT_BLACK);
  starfield();
  moon(292, 30, 11);
  lcd.setTextColor(TFT_GOLD, TFT_BLACK);
  lcd.setTextDatum(lgfx::top_center);
  lcd.setFont(&fonts::Font4);
  lcd.drawString("FromTheStars", 160, 12);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(TFT_PINK, TFT_BLACK);
  lcd.drawString("~ daily horoscope and tarot ~", 160, 44);
  drawBtn(BTN_DRAW);
  drawBtn(BTN_DAILY);
  snprintf(signLabel, sizeof signLabel, "%s",
           sign >= 0 ? (String("* ") + SIGN_NAME[sign]).c_str() : "PICK A SIGN");
  BTN_SIGN.label = signLabel;
  drawBtn(BTN_SIGN);
  drawBtn(BTN_WIFI);
  lcd.setTextDatum(lgfx::top_center);
  lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  String status = wifiOn ? "wifi: " + wifiSsid
                         : (wifiSsid.length() ? "wifi: not connected" : "wifi: not set up yet");
  lcd.drawString(status.c_str(), 160, 204);
}

static void drawCardPage() {
  header(page == 0 ? "PAST" : page == 1 ? "PRESENT" : "FUTURE");
  cardInfo(picks[page].idx);
  String name = cardName, text = cardText;
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.setTextDatum(lgfx::top_center);
  lcd.setFont(&fonts::Font2);
  lcd.drawString(name.c_str(), 160, 36);
  int y = 60;
  if (picks[page].rev) {
    y = wrapped("Reversed: the energy is blocked or turned inward.", 12, y, 296, TFT_ORANGE);
    y += 6;
  }
  wrapped(text, 12, y, 296, TFT_LIGHTGREY);
  lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
  lcd.setTextDatum(lgfx::top_center);
  lcd.drawString((String(page + 1) + " OF 3").c_str(), 160, 186);
  if (page > 0) drawBtn(BTN_PREV);
  drawBtn(BTN_NEXT);
}

static void drawSummary() {
  header("SUMMARY");
  lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  lcd.setTextDatum(lgfx::top_left);
  lcd.setFont(&fonts::Font2);
  int y = 30;
  const char* pos[3] = {"Past: ", "Present: ", "Future: "};
  for (int i = 0; i < 3; i++) {
    cardInfo(picks[i].idx);
    String name = cardName;
    lcd.setTextColor(TFT_GOLD, TFT_BLACK);
    lcd.drawString(pos[i], 10, y);
    int lx = 10 + lcd.textWidth(pos[i]);
    lcd.setTextColor(picks[i].rev ? TFT_ORANGE : TFT_WHITE, TFT_BLACK);
    lcd.drawString((name + (picks[i].rev ? " (rev)" : "")).c_str(), lx, y);
    y += 20;
  }
  wrapped(summary(), 10, y + 6, 300, TFT_LIGHTGREY);
  drawBtn({BTN_PREV.x, BTN_PREV.y, BTN_PREV.w, BTN_PREV.h, "MENU", false});
  drawBtn({BTN_NEXT.x, BTN_NEXT.y, BTN_NEXT.w, BTN_NEXT.h, "AGAIN", true});
}

// Sign picker, used for the first pick and from the home screen
static void drawSigns() {
  header(signSetup ? "PICK YOUR SIGN" : "CHOOSE YOUR SIGN");
  for (int i = 0; i < 12; i++) {
    int col = i % 4, row = i / 4;
    Btn b = {10 + col * 78, 34 + row * 48, 70, 40, SIGN_NAME[i], sign == i};
    drawBtn(b);
  }
  if (signSetup) {
    lcd.setTextColor(TFT_PINK, TFT_BLACK);
    lcd.setTextDatum(lgfx::top_center);
    lcd.drawString("Your sign shows your daily reading", 160, 186);
  } else {
    drawBtn(BTN_BACK);
  }
}

static void drawDaily() {
  String title = String("* ") + SIGN_NAME[sign] + " *";
  header(title.c_str());
  struct tm t;
  if (getLocalTime(&t, 2000)) {
    char d[24];
    strftime(d, sizeof d, "%A, %B %d", &t);
    lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    lcd.setTextDatum(lgfx::top_center);
    lcd.setFont(&fonts::Font2);
    lcd.drawString(d, 160, 30);
    int idx = (t.tm_yday * 7 + sign * 3) % 8;
    wrapped(DAILY_TEXT[idx], 14, 60, 292, TFT_WHITE);
  } else {
    wrapped("The clock isn't set yet. Try again in a moment.", 14, 60, 292, TFT_ORANGE);
  }
  drawBtn(BTN_BACK);
}

static void drawNoWifi() {
  header("WI-FI NEEDED");
  wrapped("The daily reading needs WiFi for today's date. Tap SET UP WI-FI to pick your network. Card readings work without it.",
          14, 40, 292, TFT_LIGHTGREY);
  drawBtn({70, 170, 180, 32, "SET UP WI-FI", true});
  drawBtn(BTN_BACK);
}

static void drawWifiList() {
  header("WI-FI");
  if (netCount == 0) {
    wrapped("No networks found yet. Tap RESCAN.", 14, 50, 292, TFT_LIGHTGREY);
  }
  for (int i = 0; i < NET_ROWS; i++) {
    int n = netPage * NET_ROWS + i;
    if (n >= netCount) break;
    const Net& net = nets[n];
    String label = net.ssid.substring(0, 24);
    String bars = net.rssi > -60 ? "***" : net.rssi > -75 ? "**" : "*";
    String line = label + (net.lock ? "  lock" : "") + "  " + bars;
    lcd.fillRoundRect(10, 30 + i * 34, 300, 28, 6, TFT_DARKGREY);
    lcd.setTextColor(TFT_WHITE, TFT_DARKGREY);
    lcd.setTextDatum(lgfx::middle_left);
    lcd.setFont(&fonts::Font2);
    lcd.drawString(line.c_str(), 20, 30 + i * 34 + 14);
  }
  drawBtn({6, 200, 74, 30, "BACK", false});
  drawBtn({84, 200, 74, 30, "RESCAN", false});
  drawBtn({162, 200, 74, 30, "MORE", netCount > (netPage + 1) * NET_ROWS});
  if (wifiSsid.length()) drawBtn({240, 200, 74, 30, "FORGET", false});
}

static void drawWifiPass() {
  String title = "PASSWORD FOR " + pendingSsid.substring(0, 14);
  header(title.c_str());
  lcd.fillRoundRect(4, 26, 312, 22, 4, TFT_BLACK);
  lcd.drawRoundRect(4, 26, 312, 22, 4, TFT_GOLD);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.setTextDatum(lgfx::middle_left);
  lcd.setFont(&fonts::Font2);
  String shown = kbText;
  if (shown.length() > 44) shown = shown.substring(shown.length() - 44);
  lcd.drawString((shown + "_").c_str(), 10, 37);
  drawBtn({250, 2, 66, 20, "CANCEL", false});
  // keyboard: rows at y=58 with 36px pitch
  for (int r = 0; r < 5; r++) {
    if (r < 4) {
      String keys = "";
      if (r == 0) keys = "1234567890";
      else if (kbMode == 2) keys = r == 1 ? "!@#$%^&*()" : r == 2 ? "-_=+[]{};:" : ".,?/\\|'`~";
      else {
        keys = r == 1 ? "qwertyuiop" : r == 2 ? "asdfghjkl" : "zxcvbnm";
        if (kbMode == 1) keys.toUpperCase();
      }
      if (r == 3) keys += "<";
      int n = keys.length(), pitch = 320 / n;
      for (int i = 0; i < n; i++) {
        String k = keys.substring(i, i + 1);
        Btn b = {i * pitch + 1, 58 + r * 36, pitch - 2, 32, nullptr, false};
        lcd.fillRoundRect(b.x, b.y, b.w, b.h, 4, TFT_DARKGREY);
        lcd.setTextColor(TFT_WHITE, TFT_DARKGREY);
        lcd.setTextDatum(lgfx::middle_center);
        lcd.setFont(&fonts::Font2);
        lcd.drawString(k == "<" ? "DEL" : k.c_str(), b.x + b.w / 2, b.y + b.h / 2);
      }
    } else {
      struct Key { int x, w; const char* label; bool on; };
      Key row[4] = {
        {0, 64, kbMode == 2 ? "abc" : "SHIFT", kbMode == 1},
        {64, 64, kbMode == 2 ? "abc" : "#+=", kbMode == 2},
        {128, 128, "SPACE", false},
        {256, 64, "OK", true}};
      for (auto& k : row) {
        lcd.fillRoundRect(k.x + 1, 58 + 4 * 36, k.w - 2, 32, 4, k.on ? TFT_GOLD : TFT_DARKGREY);
        lcd.setTextColor(k.on ? TFT_BLACK : TFT_WHITE, k.on ? TFT_GOLD : TFT_DARKGREY);
        lcd.setTextDatum(lgfx::middle_center);
        lcd.setFont(&fonts::Font2);
        lcd.drawString(k.label, k.x + k.w / 2, 58 + 4 * 36 + 16);
      }
    }
  }
}

static void drawResult() {
  header(resultTitle.c_str());
  wrapped(resultLine, 14, 60, 292, TFT_LIGHTGREY);
  drawBtn({100, 180, 120, 34, resultBtn.c_str(), true});
}

static void draw() {
  switch (screen) {
    case HOME: drawHome(); break;
    case READING: if (page < 3) drawCardPage(); else drawSummary(); break;
    case SIGNS: drawSigns(); break;
    case DAILY: drawDaily(); break;
    case NOWIFI: drawNoWifi(); break;
    case WIFI_LIST: drawWifiList(); break;
    case PASSWORD_KEYS: drawWifiPass(); break;
    case WIFI_RESULT: drawResult(); break;
  }
}

// ── keyboard input ───────────────────────────────────────────────────────
// Returns the character under the finger on the password keyboard, or 0.
// Special keys: '<' delete, '\n' OK, '^' shift, '#' symbols, ' ' space.
static char keyAt(int x, int y) {
  if (y < 58 || y >= 58 + 5 * 36) return 0;
  int r = (y - 58) / 36;
  if (y - (58 + r * 36) >= 32) return 0;
  if (r == 4) {
    if (x < 64) return '^';
    if (x < 128) return '#';
    if (x < 256) return ' ';
    return '\n';
  }
  String keys = "";
  if (r == 0) keys = "1234567890";
  else if (kbMode == 2) keys = r == 1 ? "!@#$%^&*()" : r == 2 ? "-_=+[]{};:" : ".,?/\\|'`~";
  else {
    keys = r == 1 ? "qwertyuiop" : r == 2 ? "asdfghjkl" : "zxcvbnm";
    if (kbMode == 1) keys.toUpperCase();
  }
  if (r == 3) keys += "<";
  int n = keys.length(), pitch = 320 / n;
  int i = x / pitch;
  if (i >= n) return 0;
  return keys[i];
}

static void connectAndReport(const String& ssid, const String& pass) {
  message("Connecting to", ssid.c_str());
  if (connectWifi(ssid, pass)) {
    saveWifi(ssid, pass);
    resultTitle = "CONNECTED";
    resultLine = "Joined " + ssid + ". The clock is setting itself from the internet.";
    resultBtn = "DONE";
    resultNext = HOME;
  } else {
    resultTitle = "NOT CONNECTED";
    resultLine = "Couldn't join " + ssid + ". Check the password and try again.";
    resultBtn = "TRY AGAIN";
    resultNext = WIFI_LIST;
  }
  screen = WIFI_RESULT;
  dirty = true;
}

// ── touch dispatch ───────────────────────────────────────────────────────
static void tap(int x, int y) {
  switch (screen) {
    case HOME:
      if (hit(BTN_DRAW, x, y)) { pickCards(); screen = READING; page = 0; }
      else if (hit(BTN_DAILY, x, y)) {
        if (sign < 0) screen = SIGNS;
        else screen = wifiOn ? DAILY : NOWIFI;
      }
      else if (hit(BTN_SIGN, x, y)) { signSetup = false; screen = SIGNS; }
      else if (hit(BTN_WIFI, x, y)) { scanNets(); screen = WIFI_LIST; }
      else break;
      dirty = true;
      break;
    case READING:
      if (page < 3) {
        if (page > 0 && hit(BTN_PREV, x, y)) page--;
        else if (hit(BTN_NEXT, x, y)) page++;
        else break;
      } else {
        if (hit(BTN_PREV, x, y)) screen = HOME;
        else if (hit(BTN_NEXT, x, y)) { pickCards(); page = 0; }
        else break;
      }
      dirty = true;
      break;
    case SIGNS:
      for (int i = 0; i < 12; i++) {
        Btn b = {10 + (i % 4) * 78, 34 + (i / 4) * 48, 70, 40, SIGN_NAME[i], false};
        if (hit(b, x, y)) {
          saveSign(i);
          screen = HOME;
          dirty = true;
          return;
        }
      }
      if (!signSetup && hit(BTN_BACK, x, y)) { screen = HOME; dirty = true; }
      break;
    case DAILY:
      if (hit(BTN_BACK, x, y)) { screen = HOME; dirty = true; }
      break;
    case NOWIFI:
      if (hit({70, 170, 180, 32, "", false}, x, y)) { scanNets(); screen = WIFI_LIST; dirty = true; }
      else if (hit(BTN_BACK, x, y)) { screen = HOME; dirty = true; }
      break;
    case WIFI_LIST: {
      if (hit({6, 200, 74, 30, "", false}, x, y)) { screen = HOME; dirty = true; return; }
      if (hit({84, 200, 74, 30, "", false}, x, y)) { scanNets(); dirty = true; return; }
      if (hit({162, 200, 74, 30, "", false}, x, y)) {
        if (netCount > (netPage + 1) * NET_ROWS) netPage++; else netPage = 0;
        dirty = true; return;
      }
      if (wifiSsid.length() && hit({240, 200, 74, 30, "", false}, x, y)) {
        saveWifi("", "");
        WiFi.disconnect();
        wifiOn = false;
        resultTitle = "FORGOTTEN";
        resultLine = "The board no longer has a saved network.";
        resultBtn = "DONE";
        resultNext = HOME;
        screen = WIFI_RESULT;
        dirty = true; return;
      }
      for (int i = 0; i < NET_ROWS; i++) {
        int n = netPage * NET_ROWS + i;
        if (n >= netCount) break;
        if (x >= 10 && x < 310 && y >= 30 + i * 34 && y < 30 + i * 34 + 28) {
          pendingSsid = nets[n].ssid;
          if (nets[n].lock) {
            kbText = ""; kbMode = 0;
            screen = PASSWORD_KEYS;
          } else {
            connectAndReport(pendingSsid, "");
          }
          dirty = true; return;
        }
      }
      break;
    }
    case PASSWORD_KEYS: {
      if (hit({250, 2, 66, 20, "", false}, x, y)) { screen = WIFI_LIST; dirty = true; return; }
      if (y < 58 || y >= 58 + 5 * 36) break;
      char k = keyAt(x, y);
      if (!k) break;
      if (k == '\n') { connectAndReport(pendingSsid, kbText); return; }
      if (k == '<') { if (kbText.length()) kbText.remove(kbText.length() - 1); }
      else if (k == '^') { kbMode = (kbMode == 1) ? 0 : (kbMode == 2 ? 0 : 1); }
      else if (k == '#') { kbMode = (kbMode == 2) ? 0 : 2; }
      else if (kbText.length() < 63) kbText += k;
      dirty = true;
      break;
    }
    case WIFI_RESULT:
      if (hit({100, 180, 120, 34, "", false}, x, y)) {
        screen = resultNext;
        dirty = true;
      }
      break;
  }
}

// ── boot ─────────────────────────────────────────────────────────────────
static void splash(const char* hint) {
  lcd.fillScreen(TFT_BLACK);
  starfield();
  moon(292, 30, 11);
  lcd.setTextDatum(lgfx::middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(TFT_GOLD, TFT_BLACK);
  lcd.drawString("FromTheStars", 160, 100);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(TFT_PINK, TFT_BLACK);
  lcd.drawString("~ daily horoscope and tarot ~", 160, 130);
  lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  lcd.drawString(hint, 160, 170);
}

void setup() {
  lcd.init();
  lcd.setRotation(5);                 // landscape, mirrored (the CYD's panel needs it)
  lcd.setBrightness(200);

  Preferences p;
  p.begin("tarot", false);
  uint16_t cal[8];
  bool have = p.getBytes("cal", cal, sizeof cal) == sizeof cal;
  splash("Hold the screen now to redo touch setup");
  delay(1200);
  int32_t rx, ry;
  bool held = lcd.getTouchRaw(&rx, &ry) > 0;
  if (!have || held) calibrate(&p);
  else lcd.setTouchCalibrate(cal);
  p.end();

  loadSettings();
  // join the saved network, or the fallback in config.h if none is saved
  String ssid = wifiSsid.length() ? wifiSsid : String(WIFI_SSID);
  String pass = wifiSsid.length() ? wifiPass : String(WIFI_PASS);
  if (ssid.length()) {
    String m = "Joining " + ssid + "...";
    message("FromTheStars", m.c_str());
    connectWifi(ssid, pass);
  }
  pickCards();
  signSetup = (sign < 0);
  screen = signSetup ? SIGNS : HOME;
  dirty = true;
}

void loop() {
  static bool wasDown = false;
  int x, y;
  bool down = lcd.getTouch(&x, &y) > 0;
  if (down && !wasDown) tap(x, y);
  wasDown = down;
  if (dirty) { draw(); dirty = false; }
  delay(20);
}
