#include "selcall_tx.h"
#include "config.h"
#include "selcall_protocol.h"

// ---------------- 256-entry sine lookup table (8-bit, centered at 127) ----------------
static uint8_t sineLUT[256];

// ---------------- Bit-stream buffer for one full TX burst ----------------
// Preamble (600 bits) + phasing (12 words * 10 bits = 120) + message (<=18*10=180)
#define TX_MAX_BITS (SELCALL_PREAMBLE_DIBITS * 2 + 12 * 10 + SELCALL_MSG_MAX_WORDS * 10)
static uint8_t txBits[TX_MAX_BITS];
static volatile uint32_t txBitCount = 0;

// ---------------- DDS state (touched only from ISR + start/stop from main) ----------------
static hw_timer_t *txTimer = nullptr;
static volatile uint32_t phaseAcc = 0;
static volatile uint32_t freqWordSpace, freqWordMark;
static volatile uint32_t freqWord;
static volatile uint32_t sampleInBit = 0;
static volatile uint32_t samplesPerBit;
static volatile uint32_t bitIndex = 0;
static volatile bool txDone = true;

static uint32_t hzToFreqWord(float hz) {
  return (uint32_t)((hz * 4294967296.0f) / (float)AUDIO_TX_SAMPLE_HZ);
}

void IRAM_ATTR onTxTimer() {
  if (txDone) return;

  // Output current sample, advance phase.
  dacWrite(AUDIO_TX_DAC_PIN, sineLUT[phaseAcc >> 24]);
  phaseAcc += freqWord;

  // Bit-boundary handling.
  sampleInBit++;
  if (sampleInBit >= samplesPerBit) {
    sampleInBit = 0;
    bitIndex++;
    if (bitIndex >= txBitCount) {
      txDone = true;
      return;
    }
    freqWord = txBits[bitIndex] ? freqWordMark : freqWordSpace;
  }
}

void selcall_tx_init() {
  for (int i = 0; i < 256; i++) {
    sineLUT[i] = (uint8_t)(127.5f + 127.5f * sinf(2.0f * PI * i / 256.0f));
  }
  freqWordSpace = hzToFreqWord(SELCALL_SPACE_HZ);
  freqWordMark  = hzToFreqWord(SELCALL_MARK_HZ);
  samplesPerBit = AUDIO_TX_SAMPLE_HZ / SELCALL_BAUD;

  pinMode(PTT_PIN, OUTPUT);
  digitalWrite(PTT_PIN, LOW);   // PTT idle (not keyed) — see README for polarity
  dacWrite(AUDIO_TX_DAC_PIN, 128); // idle mid-scale (no DC on the line)

  txTimer = timerBegin(0, 80, true);           // 80 MHz / 80 = 1 MHz tick
  timerAttachInterrupt(txTimer, &onTxTimer, true);
  timerAlarmWrite(txTimer, 1000000UL / AUDIO_TX_SAMPLE_HZ, true);
  // Timer left disabled until a burst is queued.
}

// Push one 10-bit line word (LSB-first) onto the bit-stream buffer.
static void pushWord(uint8_t value7) {
  uint16_t w = selcall_encode_word(value7);
  for (int i = 0; i < 10; i++) {
    if (txBitCount < TX_MAX_BITS) {
      txBits[txBitCount++] = (w >> i) & 1;
    }
  }
}

static void buildPreamble() {
  // Continuous alternating 0/1 — this is what makes the long "warble" that
  // lets a scanning receiver find and lock onto the signal.
  for (int i = 0; i < SELCALL_PREAMBLE_DIBITS; i++) {
    if (txBitCount < TX_MAX_BITS) txBits[txBitCount++] = 0;
    if (txBitCount < TX_MAX_BITS) txBits[txBitCount++] = 1;
  }
}

static void sendBitstream() {
  bitIndex = 0;
  sampleInBit = 0;
  phaseAcc = 0;
  freqWord = txBits[0] ? freqWordMark : freqWordSpace;
  txDone = false;

  timerAlarmEnable(txTimer);

  while (!txDone) {
    delay(1);
  }

  timerAlarmDisable(txTimer);
  dacWrite(AUDIO_TX_DAC_PIN, 128); // back to idle / no DC
}

static void transmitBurst(uint8_t *msgWords, uint8_t msgLen) {
  txBitCount = 0;
  buildPreamble();

  uint8_t phasing[12];
  selcall_build_phasing(phasing);
  for (int i = 0; i < 12; i++) pushWord(phasing[i]);

  for (int i = 0; i < msgLen; i++) pushWord(msgWords[i]);

  // Key the transmitter, let it settle, then send tones, then hang PTT
  // briefly so the last tone isn't clipped by rig audio/PTT delay.
  digitalWrite(PTT_PIN, HIGH);
  delay(PTT_LEAD_MS);

  sendBitstream();

  delay(PTT_TAIL_MS);
  digitalWrite(PTT_PIN, LOW);
}

void selcall_tx_send_call(uint16_t srcId, uint16_t dstId) {
  uint8_t msg[SELCALL_MSG_MAX_WORDS];
  uint8_t len = selcall_build_call(srcId, dstId, msg);
  transmitBurst(msg, len);
}

void selcall_tx_send_chantest(uint16_t srcId, uint16_t dstId) {
  uint8_t msg[SELCALL_MSG_MAX_WORDS];
  uint8_t len = selcall_build_chantest(srcId, dstId, msg);
  transmitBurst(msg, len);
}
