#pragma once
#include <Arduino.h>

void selcall_tx_init();
void selcall_tx_send_call(uint16_t srcId, uint16_t dstId);
void selcall_tx_send_chantest(uint16_t srcId, uint16_t dstId);
