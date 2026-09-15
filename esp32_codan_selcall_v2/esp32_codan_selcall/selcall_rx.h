#pragma once
#include <Arduino.h>

struct SelcallRxEvent {
  bool valid;
  bool isChanTest;     
  uint16_t callerId;    
  uint16_t calledId;    
};

void selcall_rx_init();
bool selcall_rx_poll(SelcallRxEvent *ev);

enum RxSyncState { RX_IDLE_SEARCHING, RX_IDLE_SYNCED };
RxSyncState selcall_rx_get_status();

void selcall_rx_get_level(uint16_t *minOut, uint16_t *maxOut);
