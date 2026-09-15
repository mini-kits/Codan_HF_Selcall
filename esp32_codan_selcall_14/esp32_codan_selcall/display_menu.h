#pragma once
/*
 * display_menu.h — ST7735 TFT menu UI: contact list, send/channel-test,
 * "my ID" setting, and incoming-call alert popup. Contacts and own ID are
 * persisted in NVS (Preferences).
 */

#include <Arduino.h>

void ui_init();
void ui_task();   // call every loop() iteration; handles buttons + drawing
