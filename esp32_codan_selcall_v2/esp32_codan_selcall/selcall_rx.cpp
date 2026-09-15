#include "selcall_rx.h"
#include "config.h"
#include "selcall_protocol.h"
#include <driver/adc.h>

#define RING_SIZE 1024           
#define RING_MASK (RING_SIZE - 1)
static uint16_t ring[RING_SIZE];
static volatile uint32_t writeIndex = 0;   

static hw_timer_t *rxTimer = nullptr;

void IRAM_ATTR onRxTimer() {
  ring[writeIndex & RING_MASK] = (uint16_t)adc1_get_raw(ADC1_CHANNEL_0);
  writeIndex++;
}

void selcall_rx_init() {
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);

  rxTimer = timerBegin(1, 80, true);        
  timerAttachInterrupt(rxTimer, &onRxTimer, true);
  timerAlarmWrite(rxTimer, 1000000UL / AUDIO_RX_SAMPLE_HZ, true);
  timerAlarmEnable(rxTimer);
}

static float goertzelMag(uint32_t startSample, int N, float targetHz) {
  float k = 0.5f + (N * targetHz) / (float)AUDIO_RX_SAMPLE_HZ;
  float w = (2.0f * PI / N) * (float)((int)k);
  float coeff = 2.0f * cosf(w);
  float q0, q1 = 0, q2 = 0;

  for (int i = 0; i < N; i++) {
    float sample = ((float)ring[(startSample + i) & RING_MASK] - 2048.0f) / 2048.0f;
    q0 = coeff * q1 - q2 + sample;
    q2 = q1;
    q1 = q0;
  }
  return (q1 * q1) + (q2 * q2) - coeff * q1 * q2;
}

static const uint32_t samplesPerBit = AUDIO_RX_SAMPLE_HZ / SELCALL_BAUD;      
static const int GOERTZEL_WIN_LEN   = (samplesPerBit * 60) / 100;              
static const uint32_t GOERTZEL_WIN_OFFSET = (samplesPerBit - GOERTZEL_WIN_LEN) / 2;

#define MAX_PARITY_FAILS 6

struct DpllDecoder {
  uint32_t nextBitBoundary;
  uint16_t wordShiftReg;
  uint8_t  wordBitCount;
  uint8_t  parityFailStreak;
  uint8_t  wordHistory[16];   // FIXED: Re-established as array
  uint8_t  wordHistoryLen;
  bool     framed;
  uint8_t  msgWords[SELCALL_MSG_MAX_WORDS];
  uint8_t  msgLen;
  uint8_t  lastBitState;
  uint32_t lastTransitionIndex;
};

static DpllDecoder rxState;
static bool dpllInited = false;

static void resetDecoder() {
  rxState.wordHistoryLen = 0;
  rxState.framed = false;
  rxState.msgLen = 0;
  rxState.wordShiftReg = 0;
  rxState.wordBitCount = 0;
  rxState.parityFailStreak = 0;
}

static void initDpllIfNeeded() {
  if (dpllInited) return;
  memset(&rxState, 0, sizeof(DpllDecoder));
  rxState.nextBitBoundary = writeIndex;
  rxState.lastBitState = 2; 
  dpllInited = true;
}

static void pushHistory(uint8_t v) {
  if (rxState.wordHistoryLen < 16) {
    rxState.wordHistory[rxState.wordHistoryLen++] = v;
  } else {
    memmove(rxState.wordHistory, rxState.wordHistory + 1, 15);
    rxState.wordHistory[15] = v; // FIXED: Safely write to array tail
  }
}

static bool historyMatchesPhasing() {
  if (rxState.wordHistoryLen < 12) return false;
  uint8_t expect[12];
  selcall_build_phasing(expect);
  const uint8_t *tail = rxState.wordHistory + (rxState.wordHistoryLen - 12);
  for (int i = 0; i < 12; i++) {
    if (tail[i] != expect[i]) return false;
  }
  return true;
}

static void parseMessage(SelcallRxEvent *ev) {
  uint8_t type = rxState.msgWords[0];
  uint8_t addrB1 = rxState.msgWords[2], addrB2 = rxState.msgWords[4];
  uint8_t addrA1 = rxState.msgWords[8], addrA2 = rxState.msgWords[10];
  ev->valid = true;
  ev->isChanTest = (type == SEL_ID);
  ev->calledId = (uint16_t)addrB1 * 100 + addrB2;
  ev->callerId = (uint16_t)addrA1 * 100 + addrA2;
}

bool selcall_rx_poll(SelcallRxEvent *ev) {
  ev->valid = false;
  initDpllIfNeeded();

  if (writeIndex > rxState.nextBitBoundary + (samplesPerBit * 4)) {
    rxState.nextBitBoundary = writeIndex - samplesPerBit;
  }

  while (writeIndex >= rxState.nextBitBoundary + samplesPerBit) {
    uint32_t winStart = rxState.nextBitBoundary + GOERTZEL_WIN_OFFSET;
    float mMark  = goertzelMag(winStart, GOERTZEL_WIN_LEN, SELCALL_MARK_HZ);
    float mSpace = goertzelMag(winStart, GOERTZEL_WIN_LEN, SELCALL_SPACE_HZ);
    uint8_t bit = (mMark > mSpace) ? 1 : 0;

    if (rxState.lastBitState != 2 && bit != rxState.lastBitState) {
      uint32_t calculatedTransition = rxState.nextBitBoundary;
      
      if (rxState.lastTransitionIndex != 0) {
        uint32_t actualIntervalSamples = calculatedTransition - rxState.lastTransitionIndex;
        uint32_t bitRemainder = actualIntervalSamples % samplesPerBit;

        if (bitRemainder != 0) {
          if (bitRemainder < (samplesPerBit / 2)) {
            rxState.nextBitBoundary -= 1; 
          } else {
            rxState.nextBitBoundary += 1;
          }
        }
      }
      rxState.lastTransitionIndex = calculatedTransition;
    }
    rxState.lastBitState = bit;

    rxState.wordShiftReg |= ((uint16_t)bit << rxState.wordBitCount);
    rxState.wordBitCount++;
    rxState.nextBitBoundary += samplesPerBit;

    if (rxState.wordBitCount == 10) {
      uint8_t value7;
      bool ok = selcall_decode_word(rxState.wordShiftReg, &value7);
      rxState.wordShiftReg = 0;
      rxState.wordBitCount = 0;

      if (ok) {
        rxState.parityFailStreak = 0;
        pushHistory(value7);
        if (!rxState.framed) {
          if (historyMatchesPhasing()) {
            rxState.framed = true;
            rxState.msgLen = 0;
          }
        } else {
          if (rxState.msgLen < SELCALL_MSG_MAX_WORDS) {
            rxState.msgWords[rxState.msgLen++] = value7;
          }
          if (rxState.msgLen >= SELCALL_MSG_MAX_WORDS) {
            parseMessage(ev);
            resetDecoder();
            return ev->valid;
          }
        }
      } else {
        rxState.parityFailStreak++;
        if (rxState.parityFailStreak >= MAX_PARITY_FAILS) {
          resetDecoder();
        }
      }
    }
  }
  return false;
}

RxSyncState selcall_rx_get_status() {
  initDpllIfNeeded();
  return rxState.framed ? RX_IDLE_SYNCED : RX_IDLE_SEARCHING;
}

void selcall_rx_get_level(uint16_t *minOut, uint16_t *maxOut) {
  const uint32_t WIN = 960;
  uint32_t end = writeIndex;
  uint32_t start = (end >= WIN) ? (end - WIN) : 0;
  uint16_t mn = 4095, mx = 0;
  for (uint32_t i = start; i < end; i++) {
    uint16_t v = ring[i & RING_MASK];
    if (v < mn) mn = v;
    if (v > mx) mx = v;
  }
  if (end == start) { mn = 2048; mx = 2048; }
  *minOut = mn;
  *maxOut = mx;
}
