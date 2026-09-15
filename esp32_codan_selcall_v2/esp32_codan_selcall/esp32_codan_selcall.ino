/*
 * ESP32 CODAN Selcall (CCIR 493-4) Encoder / Decoder
 * TFT: 1.8" 128x160 ST7735, menu-selectable contacts for TX,
 * line-level audio in/out for connection to an HF amateur transceiver.
 */

#include "config.h"
#include "selcall_tx.h"
#include "selcall_rx.h"
#include "display_menu.h"

void setup() {
  Serial.begin(115200);
  selcall_tx_init();
  selcall_rx_init();
  ui_init();
  Serial.println("System Initialised. DPLL Sync Active.");
}

void loop() {
  ui_task();
}
