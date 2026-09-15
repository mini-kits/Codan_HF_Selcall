#include "display_menu.h"
#include "config.h"
#include "selcall_tx.h"
#include "selcall_rx.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Preferences.h>

static Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
static Preferences prefs;

struct Contact { char name[12]; uint16_t id; };
static Contact contacts[MAX_CONTACTS];
static uint8_t contactCount = 0;
static uint16_t myId = 1000;

static bool levelMeterLayoutDrawn = false;

static void loadContacts() {
  prefs.begin(NVS_NAMESPACE, true);
  contactCount = prefs.getUChar("cnt", 0);
  if (contactCount > MAX_CONTACTS) contactCount = MAX_CONTACTS;
  if (contactCount > 0) {
    prefs.getBytes("contacts", contacts, contactCount * sizeof(Contact));
  }
  myId = prefs.getUShort("myid", 1000);
  prefs.end();

  if (contactCount == 0) {
    strcpy(contacts[0].name, "BASE");  contacts[0].id = 1000;
    strcpy(contacts[1].name, "MOBILE1"); contacts[1].id = 1234;
    contactCount = 2;
  }
}

static void saveContacts() {
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putUChar("cnt", contactCount);
  prefs.putBytes("contacts", contacts, contactCount * sizeof(Contact));
  prefs.end();
}

static void saveMyId() {
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putUShort("myid", myId);
  prefs.end();
}

enum Btn { B_NONE, B_UP, B_DOWN, B_SELECT, B_BACK };
static uint32_t lastBtnMs = 0;

static Btn readButton() {
  if (millis() - lastBtnMs < 150) return B_NONE; 
  Btn b = B_NONE;
  if (digitalRead(BTN_UP) == LOW) b = B_UP;
  else if (digitalRead(BTN_DOWN) == LOW) b = B_DOWN;
  else if (digitalRead(BTN_SELECT) == LOW) b = B_SELECT;
  else if (digitalRead(BTN_BACK) == LOW) b = B_BACK;
  if (b != B_NONE) lastBtnMs = millis();
  return b;
}

enum UiState {
  ST_MAIN, ST_CONTACTS, ST_CONFIRM, ST_SENDING,
  ST_MYID_EDIT, ST_ADD_CONTACT, ST_ABOUT, ST_RX_ALERT, ST_LEVEL_METER
};
static UiState state = ST_MAIN;

static const char *mainItems[] = { "Call Contact", "Add Contact", "My ID", "RX Level Meter", "About" };
#define MAIN_ITEM_COUNT 5
static int8_t mainSel = 0;
static int8_t contactSel = 0;
static bool confirmChanTest = false; 
static int8_t editDigits[4] = {1, 0, 0, 0};
static uint8_t editPos = 0;

static SelcallRxEvent lastRx;

#define COL_BG     ST77XX_BLACK
#define COL_FG     ST77XX_WHITE
#define COL_HI     ST77XX_YELLOW
#define COL_OK     ST77XX_GREEN
#define COL_WARN   ST77XX_RED

static RxSyncState lastDrawnRxStatus = (RxSyncState)-1; 
#define RX_DOT_CX 118
#define RX_DOT_CY 8
#define RX_DOT_R  4

static void drawRxIndicator() {
  RxSyncState st = selcall_rx_get_status();
  if (st == lastDrawnRxStatus) return; 
  lastDrawnRxStatus = st;

  tft.fillRect(RX_DOT_CX - RX_DOT_R - 3, RX_DOT_CY - RX_DOT_R - 3,
               (RX_DOT_R + 3) * 2, (RX_DOT_R + 3) * 2, ST77XX_BLUE);

  if (st == RX_IDLE_SYNCED) {
    tft.fillCircle(RX_DOT_CX, RX_DOT_CY, RX_DOT_R, ST77XX_GREEN);
  } else {
    tft.drawCircle(RX_DOT_CX, RX_DOT_CY, RX_DOT_R, ST77XX_WHITE);
  }
}

static void drawHeader(const char *title) {
  tft.fillRect(0, 0, 128, 16, ST77XX_BLUE);
  tft.setTextColor(COL_FG);
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.print(title);
  lastDrawnRxStatus = (RxSyncState)-1;
  drawRxIndicator();
}

static void drawMain() {
  tft.fillScreen(COL_BG);
  drawHeader("CODAN Selcall");
  tft.setTextSize(1);
  for (int i = 0; i < MAIN_ITEM_COUNT; i++) {
    tft.setCursor(6, 24 + i * 14);
    tft.setTextColor(i == mainSel ? COL_HI : COL_FG);
    tft.print(i == mainSel ? "> " : "  ");
    tft.print(mainItems[i]);
  }
  tft.setTextColor(COL_FG);
  tft.setCursor(4, 148);
  tft.printf("My ID: %04u", myId);
}

static void drawContacts() {
  tft.fillScreen(COL_BG);
  drawHeader("Select Contact");
  for (int i = 0; i < contactCount; i++) {
    tft.setCursor(4, 20 + i * 14);
    tft.setTextColor(i == contactSel ? COL_HI : COL_FG);
    tft.print(i == contactSel ? "> " : "  ");
    tft.print(contacts[i].name);
    tft.setCursor(90, 20 + i * 14);
    tft.printf("%04u", contacts[i].id);
  }
}

static void drawConfirm() {
  tft.fillScreen(COL_BG);
  drawHeader("Confirm");
  tft.setTextColor(COL_FG);
  tft.setCursor(6, 30);
  tft.print(contacts[contactSel].name);
  tft.setCursor(6, 44);
  tft.printf("ID: %04u", contacts[contactSel].id);
  tft.setCursor(6, 66);
  tft.print("Mode (UP/DOWN):");
  tft.setCursor(6, 80);
  tft.setTextColor(confirmChanTest ? COL_HI : COL_OK);
  tft.print(confirmChanTest ? "> Channel Test" : "> Selective Call");
  tft.setTextColor(COL_FG);
  tft.setCursor(6, 110);
  tft.print("SELECT=send BACK=cancel");
}

static void drawSending() {
  tft.fillScreen(COL_BG);
  drawHeader("Transmitting...");
  tft.setTextColor(COL_WARN);
  tft.setCursor(10, 60);
  tft.setTextSize(2);
  tft.print("ON AIR");
  tft.setTextSize(1);
}

static void drawEditId(const char *title) {
  tft.fillScreen(COL_BG);
  drawHeader(title);
  tft.setTextSize(3);
  for (int i = 0; i < 4; i++) {
    tft.setTextColor(i == editPos ? COL_HI : COL_FG);
    tft.setCursor(14 + i * 24, 60);
    tft.print(editDigits[i]);
  }
  tft.setTextSize(1);
  tft.setTextColor(COL_FG);
  tft.setCursor(4, 120);
  tft.print("UP/DN=digit SEL=next");
  tft.setCursor(4, 132);
  tft.print("(hold past 4th to save)");
}

static void drawAbout() {
  tft.fillScreen(COL_BG);
  drawHeader("About");
  tft.setTextColor(COL_FG);
  tft.setCursor(4, 24);
  tft.println("ESP32 CODAN Selcall");
  tft.setCursor(4, 38);
  tft.println("CCIR 493-4 encoder/");
  tft.setCursor(4, 50);
  tft.println("decoder for HF amateur");
  tft.setCursor(4, 62);
  tft.println("transceivers.");
  tft.setCursor(4, 84);
  tft.println("BACK to return");
}

static void drawRxAlert() {
  tft.fillScreen(COL_WARN);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(6, 20);
  tft.print(lastRx.isChanTest ? "CHAN TEST" : "INCOMING");
  tft.setCursor(6, 44);
  tft.print("CALL!");
  tft.setTextSize(1);
  tft.setCursor(6, 80);
  tft.printf("From: %04u", lastRx.callerId);
  tft.setCursor(6, 94);
  tft.printf("To:   %04u", lastRx.calledId);
  tft.setCursor(6, 130);
  tft.print("Any key to dismiss");
}

static void drawLevelMeter() {
  uint16_t mn, mx;
  selcall_rx_get_level(&mn, &mx);
  uint16_t pp = (mx > mn) ? (mx - mn) : 0;
  float vMin = mn * 3.3f / 4095.0f;
  float vMax = mx * 3.3f / 4095.0f;
  float vPP  = pp * 3.3f / 4095.0f;

  if (!levelMeterLayoutDrawn) {
    tft.fillScreen(COL_BG);
    drawHeader("RX Level Meter");
    tft.setTextSize(1);
    tft.setTextColor(COL_FG);
    tft.setCursor(4, 108);
    tft.print("Target: 1.0-1.8Vpp");
    tft.setCursor(4, 120);
    tft.print("centered ~1.65V");
    tft.setCursor(4, 148);
    tft.print("Any key = exit");
    levelMeterLayoutDrawn = true;
  }

  tft.fillRect(0, 20, 128, 40, COL_BG); 
  tft.setTextSize(1);
  tft.setTextColor(COL_FG);
  tft.setCursor(4, 22);
  tft.printf("Min: %4u  %.2fV", mn, vMin);
  tft.setCursor(4, 34);
  tft.printf("Max: %4u  %.2fV", mx, vMax);
  tft.setCursor(4, 46);
  tft.printf("Pk-pk:%4u  %.2fV", pp, vPP);

  tft.fillRect(0, 60, 128, 12, COL_BG);
  tft.setCursor(4, 62);
  if (vPP < 0.5f) {
    tft.setTextColor(COL_WARN);
    tft.print("Too low - raise gain");
  } else if (vPP > 2.2f) {
    tft.setTextColor(COL_WARN);
    tft.print("Too high - risk clip");
  } else if (vPP >= 1.0f && vPP <= 1.8f) {
    tft.setTextColor(COL_OK);
    tft.print("Good level");
  } else {
    tft.setTextColor(COL_HI);
    tft.print("Usable, could improve");
  }

  int barX = 4, barY = 84, barW = 120, barH = 14;
  tft.fillRect(barX + 1, barY + 1, barW - 2, barH - 2, COL_BG);
  tft.drawRect(barX, barY, barW, barH, COL_FG);
  
  int xMin = barX + (int)((mn / 4095.0f) * barW);
  int xMax = barX + (int)((mx / 4095.0f) * barW);
  if (xMax <= xMin) xMax = xMin + 1;
  tft.fillRect(xMin, barY + 1, xMax - xMin, barH - 2, ST77XX_CYAN);
  int xMid = barX + (int)((2048.0f / 4095.0f) * barW);
  tft.drawFastVLine(xMid, barY - 3, barH + 6, COL_HI);
}

static void gotoMain() { 
  levelMeterLayoutDrawn = false; 
  state = ST_MAIN; 
  mainSel = 0; 
  drawMain(); 
}

void ui_init() {
  pinMode(BTN_UP, INPUT);           
  pinMode(BTN_DOWN, INPUT);         
  pinMode(BTN_SELECT, INPUT_PULLUP); 
  pinMode(BTN_BACK, INPUT_PULLUP);   
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);   
  tft.setRotation(0);
  tft.fillScreen(COL_BG);

  loadContacts();
  drawMain();
}

void ui_task() {
  SelcallRxEvent ev;
  if (selcall_rx_poll(&ev)) {
    lastRx = ev;
    levelMeterLayoutDrawn = false; 
    state = ST_RX_ALERT;
    drawRxAlert();
  }

  if (state != ST_RX_ALERT) {
    drawRxIndicator();
  }

  if (state == ST_LEVEL_METER) {
    static uint32_t lastMeterDraw = 0;
    if (millis() - lastMeterDraw > 150) {
      lastMeterDraw = millis();
      drawLevelMeter();
    }
  }

  Btn b = readButton();
  if (b == B_NONE) return;

  switch (state) {
    case ST_MAIN:
      if (b == B_UP)   { mainSel = (mainSel + MAIN_ITEM_COUNT - 1) % MAIN_ITEM_COUNT; drawMain(); }
      if (b == B_DOWN) { mainSel = (mainSel + 1) % MAIN_ITEM_COUNT; drawMain(); }
      if (b == B_SELECT) {
        if (mainSel == 0) { state = ST_CONTACTS; contactSel = 0; drawContacts(); }
        else if (mainSel == 1) { state = ST_ADD_CONTACT; editPos = 0; memset(editDigits,0,4); drawEditId("New Contact ID"); }
        else if (mainSel == 2) {
          editDigits[0] = (myId/1000)%10; editDigits[1]=(myId/100)%10;
          editDigits[2] = (myId/10)%10;   editDigits[3]=myId%10;
          editPos = 0; state = ST_MYID_EDIT; drawEditId("Set My ID");
        } else if (mainSel == 3) { 
          levelMeterLayoutDrawn = false; 
          state = ST_LEVEL_METER; 
          drawLevelMeter(); 
        }
        else if (mainSel == 4) { state = ST_ABOUT; drawAbout(); }
      }
      break;

    case ST_CONTACTS:
      if (b == B_UP)   { contactSel = (contactSel + contactCount - 1) % contactCount; drawContacts(); }
      if (b == B_DOWN) { contactSel = (contactSel + 1) % contactCount; drawContacts(); }
      if (b == B_SELECT) { confirmChanTest = false; state = ST_CONFIRM; drawConfirm(); }
      if (b == B_BACK) gotoMain();
      break;

    case ST_CONFIRM:
      if (b == B_UP || b == B_DOWN) { confirmChanTest = !confirmChanTest; drawConfirm(); }
      if (b == B_BACK) { state = ST_CONTACTS; drawContacts(); }
      if (b == B_SELECT) {
        state = ST_SENDING; drawSending();
        uint16_t dst = contacts[contactSel].id;
        if (confirmChanTest) selcall_tx_send_chantest(myId, dst);
        else                 selcall_tx_send_call(myId, dst);
        gotoMain();
      }
      break;

    case ST_ADD_CONTACT:
    case ST_MYID_EDIT:
      if (b == B_UP)   { editDigits[editPos] = (editDigits[editPos] + 1) % 10; drawEditId(state==ST_MYID_EDIT?"Set My ID":"New Contact ID"); }
      if (b == B_DOWN) { editDigits[editPos] = (editDigits[editPos] + 9) % 10; drawEditId(state==ST_MYID_EDIT?"Set My ID":"New Contact ID"); }
      if (b == B_BACK) gotoMain();
      if (b == B_SELECT) {
        if (editPos < 3) {
          editPos++;
          drawEditId(state==ST_MYID_EDIT?"Set My ID":"New Contact ID");
        } else {
          uint16_t val = editDigits[0]*1000 + editDigits[1]*100 + editDigits[2]*10 + editDigits[3];
          if (state == ST_MYID_EDIT) {
            myId = val; saveMyId();
          } else if (contactCount < MAX_CONTACTS) {
            snprintf(contacts[contactCount].name, sizeof(contacts[0].name), "CT%d", contactCount + 1);
            contacts[contactCount].id = val;
            contactCount++;
            saveContacts();
          }
          gotoMain();
        }
      }
      break;

    case ST_ABOUT:
    case ST_RX_ALERT:
    case ST_LEVEL_METER:
      if (b != B_NONE) gotoMain(); 
      break;

    default:
      gotoMain();
      break;
  }
}
