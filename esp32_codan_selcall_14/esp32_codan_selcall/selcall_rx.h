#pragma once
#include <Arduino.h>

struct SelcallRxEvent {
  bool valid;
  bool isChanTest;     
  uint16_t callerId;    
  uint16_t calledId;    
};

// Diagnostic structure to safely bridge real-time DSP data to the Serial Monitor
struct SelcallDiagnostics {
  float avgMarkMag;       // Rolling average magnitude of Mark tones (1870Hz)
  float avgSpaceMag;      // Rolling average magnitude of Space tones (1700Hz)
  uint32_t totalBitsProcessed; // Monotonic count of bits evaluated
  uint32_t totalWordsDecoded;  // Count of valid 10-bit words that passed parity
  uint32_t totalParityErrors;  // Count of corrupted words rejected by parity
  bool isAnyPhaseFramed;  // True if a thread is actively capture-locked onto phasing data
};

void selcall_rx_init();
bool selcall_rx_poll(SelcallRxEvent *ev);
enum RxSyncState { RX_IDLE_SEARCHING, RX_IDLE_SYNCED };
RxSyncState selcall_rx_get_status();
void selcall_rx_get_level(uint16_t *minOut, uint16_t *maxOut);

// Fetches the latest diagnostic data safely without introducing timing jitter
void selcall_rx_get_diagnostics(SelcallDiagnostics *diagOut);
