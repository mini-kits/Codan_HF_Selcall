#include "selcall_rx.h"
#include "config.h"
#include "selcall_protocol.h"
#include <driver/adc.h>

// ---------------- ADC sampling ring buffer ----------------
#define RING_SIZE 1024           
#define RING_MASK (RING_SIZE - 1)
static uint16_t ring[RING_SIZE];
static volatile uint32_t writeIndex = 0;   

static hw_timer_t *rxTimer = nullptr;
static portMUX_TYPE rxMux = portMUX_INITIALIZER_UNLOCKED;

// Global tracking variables for telemetry diagnostics
static float diagAvgMark = 0.0f;
static float diagAvgSpace = 0.0f;
static uint32_t diagBitsCount = 0;
static uint32_t diagWordsCount = 0;
static uint32_t diagParityErrorsCount = 0;

void IRAM_ATTR onRxTimer() {
  portENTER_CRITICAL_ISR(&rxMux);
  int v = adc1_get_raw(ADC1_CHANNEL_0);    
  ring[writeIndex & RING_MASK] = (uint16_t)v;
  writeIndex++;
  portEXIT_CRITICAL_ISR(&rxMux);
}

void selcall_rx_init() {
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11); 

  rxTimer = timerBegin(1, 80, true);        
  timerAttachInterrupt(rxTimer, &onRxTimer, true);
  timerAlarmWrite(rxTimer, 1000000UL / AUDIO_RX_SAMPLE_HZ, true); // Actual: 9615.38 Hz
  timerAlarmEnable(rxTimer);
}

static float goertzelMag(uint32_t startSample, int N, float targetHz) {
  const float ACTUAL_RX_SAMPLE_HZ = 9615.3846f;
  float k = 0.5f + ((float)N * targetHz) / ACTUAL_RX_SAMPLE_HZ;
  float w = (2.0f * PI / (float)N) * (float)((int)k);
  float coeff = 2.0f * cosf(w);
  float q0, q1 = 0.0f, q2 = 0.0f;

  for (int i = 0; i < N; i++) {
    // Center raw 12-bit ADC sample around 0 and attenuate by 100 for telemetry scaling
    float sample = ((float)ring[(startSample + i) & RING_MASK] - 2048.0f) / 100.0f;
    q0 = coeff * q1 - q2 + sample;
    q2 = q1;
    q1 = q0;
  }
  return (q1 * q1) + (q2 * q2) - coeff * q1 * q2;
}

#define RX_NUM_PHASES 4
static const float samplesPerBitFloat = 9615.3846f / (float)SELCALL_BAUD; // ~96.1538 samples
static const float phaseStepFloat    = samplesPerBitFloat / (float)RX_NUM_PHASES; 

#define MAX_PARITY_FAILS  6

struct PhaseSlicer {
  float    nextBitBoundaryFloat; 
  uint16_t wordShiftReg;
  uint8_t  wordBitCount;
  uint8_t  parityFailStreak;
  uint8_t  wordHistory[16];      // Corrected: Full history array restored
  uint8_t  wordHistoryLen;
  bool     framed;
  uint8_t  msgWords[SELCALL_MSG_MAX_WORDS];
  uint8_t  msgLen;
};

static PhaseSlicer phases[RX_NUM_PHASES];
static bool phasesInited = false;

static void initPhasesIfNeeded() {
  if (phasesInited) return;
  for (int p = 0; p < RX_NUM_PHASES; p++) {
    memset(&phases[p], 0, sizeof(PhaseSlicer));
    phases[p].nextBitBoundaryFloat = (float)p * phaseStepFloat;
  }
  phasesInited = true;
}

static void resetFrame(PhaseSlicer *ps) {
  ps->wordHistoryLen = 0;
  ps->framed = false;
  ps->msgLen = 0;
  ps->wordShiftReg = 0;
  ps->wordBitCount = 0;
  ps->parityFailStreak = 0;
}

static void pushHistory(PhaseSlicer *ps, uint8_t v) {
  if (ps->wordHistoryLen < 16) {
    ps->wordHistory[ps->wordHistoryLen++] = v;
  } else {
    memmove(ps->wordHistory, ps->wordHistory + 1, 15);
    ps->wordHistory[15] = v;
  }
}

static bool historyMatchesPhasing(PhaseSlicer *ps) {
  if (ps->wordHistoryLen < 12) return false;
  uint8_t expect[12];            // Corrected: Restored 12-byte tracking array
  selcall_build_phasing(expect);
  const uint8_t *tail = ps->wordHistory + (ps->wordHistoryLen - 12);
  for (int i = 0; i < 12; i++) {
    if (tail[i] != expect[i]) return false;
  }
  return true;
}

static void parseMessage(PhaseSlicer *ps, SelcallRxEvent *ev) {
  // Corrected: Restored explicit indexing positions for the protocol message fields
  uint8_t type   = ps->msgWords[0];
  uint8_t addrB1 = ps->msgWords[2], addrB2 = ps->msgWords[4];
  uint8_t addrA1 = ps->msgWords[8], addrA2 = ps->msgWords[10];
  
  ev->valid = true;
  ev->isChanTest = (type == SEL_ID);
  ev->calledId = (uint16_t)addrB1 * 100 + addrB2;
  ev->callerId = (uint16_t)addrA1 * 100 + addrA2;
}

bool selcall_rx_poll(SelcallRxEvent *ev) {
  ev->valid = false;
  initPhasesIfNeeded();

  portENTER_CRITICAL(&rxMux);
  uint32_t snapshotWriteIndex = writeIndex;
  portEXIT_CRITICAL(&rxMux);

  const int N_samples = 96; 

  for (int p = 0; p < RX_NUM_PHASES; p++) {
    PhaseSlicer *ps = &phases[p];

    while (snapshotWriteIndex >= (uint32_t)ps->nextBitBoundaryFloat + N_samples) {
      uint32_t winStart = (uint32_t)ps->nextBitBoundaryFloat;
      
      float mMark  = goertzelMag(winStart, N_samples, SELCALL_MARK_HZ);
      float mSpace = goertzelMag(winStart, N_samples, SELCALL_SPACE_HZ);
      uint8_t bit = (mMark > mSpace) ? 1 : 0;

      if (p == 0) {
        diagAvgMark = (0.95f * diagAvgMark) + (0.05f * mMark);
        diagAvgSpace = (0.95f * diagAvgSpace) + (0.05f * mSpace);
        diagBitsCount++;
      }

      ps->wordShiftReg |= ((uint16_t)bit << ps->wordBitCount);
      ps->wordBitCount++;
      ps->nextBitBoundaryFloat += samplesPerBitFloat;

      if (ps->wordBitCount == 10) {
        uint8_t value7;
        bool ok = selcall_decode_word(ps->wordShiftReg, &value7);
        ps->wordShiftReg = 0;
        ps->wordBitCount = 0;

        if (ok) {
          diagWordsCount++;
          ps->parityFailStreak = 0;
          pushHistory(ps, value7);
          if (!ps->framed) {
            if (historyMatchesPhasing(ps)) {
              portENTER_CRITICAL(&rxMux);
              ps->framed = true;
              portEXIT_CRITICAL(&rxMux);
              ps->msgLen = 0;
            }
          } else {
            if (ps->msgLen < SELCALL_MSG_MAX_WORDS) {
              ps->msgWords[ps->msgLen++] = value7;
            }
            if (ps->msgLen >= SELCALL_MSG_MAX_WORDS) {
              parseMessage(ps, ev);
              portENTER_CRITICAL(&rxMux);
              resetFrame(ps);
              portEXIT_CRITICAL(&rxMux);
              return ev->valid; 
            }
          }
        } else {
          diagParityErrorsCount++;
          ps->parityFailStreak++;
          if (ps->parityFailStreak >= MAX_PARITY_FAILS) {
            portENTER_CRITICAL(&rxMux);
            resetFrame(ps);
            portEXIT_CRITICAL(&rxMux);
          }
        }
      }
    }
  }
  return false;
}

RxSyncState selcall_rx_get_status() {
  initPhasesIfNeeded();
  portENTER_CRITICAL(&rxMux);
  for (int p = 0; p < RX_NUM_PHASES; p++) {
    if (phases[p].framed) {
      portEXIT_CRITICAL(&rxMux);
      return RX_IDLE_SYNCED;
    }
  }
  portEXIT_CRITICAL(&rxMux);
  return RX_IDLE_SEARCHING;
}

void selcall_rx_get_level(uint16_t *minOut, uint16_t *maxOut) {
  const uint32_t WIN = 960; 
  portENTER_CRITICAL(&rxMux);
  uint32_t end = writeIndex;
  
  uint32_t start = (end >= WIN) ? (end - WIN) : 0;
  uint16_t mn = 4095, mx = 0;
  for (uint32_t i = start; i < end; i++) {
    uint16_t v = ring[i & RING_MASK];
    if (v < mn) mn = v;
    if (v > mx) mx = v;
  }
  portEXIT_CRITICAL(&rxMux);
  
  if (end == start) { mn = 2048; mx = 2048; } 
  *minOut = mn;
  *maxOut = mx;
}

void selcall_rx_get_diagnostics(SelcallDiagnostics *diagOut) {
  portENTER_CRITICAL(&rxMux);
  diagOut->avgMarkMag = diagAvgMark;
  diagOut->avgSpaceMag = diagAvgSpace;
  diagOut->totalBitsProcessed = diagBitsCount;
  diagOut->totalWordsDecoded = diagWordsCount;
  diagOut->totalParityErrors = diagParityErrorsCount;
  
  bool framedStatus = false;
  for (int p = 0; p < RX_NUM_PHASES; p++) {
    if (phases[p].framed) framedStatus = true;
  }
  diagOut->isAnyPhaseFramed = framedStatus;
  portEXIT_CRITICAL(&rxMux);
}
