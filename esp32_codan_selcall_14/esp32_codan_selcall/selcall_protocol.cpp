#include "selcall_protocol.h"

// Lookup table: index = count of zero bits (0-7) in the 7-bit value,
// value = the corresponding 3-bit parity field pre-shifted into bits 7-9.
static const uint16_t kParityLUT[8] = {
  0x0000, 0x0200, 0x0100, 0x0300, 0x0080, 0x0280, 0x0180, 0x0380
};

uint16_t selcall_encode_word(uint8_t value7) {
  value7 &= 0x7F;
  uint8_t zeros = 0;
  for (uint8_t i = 0; i < 7; i++) {
    if (!((value7 >> i) & 1)) zeros++;
  }
  return (uint16_t)value7 | kParityLUT[zeros];
}

bool selcall_decode_word(uint16_t word10, uint8_t *value7) {
  uint8_t val = word10 & 0x7F;
  uint16_t parityField = word10 & 0x0380;
  uint8_t zeros = 0;
  for (uint8_t i = 0; i < 7; i++) {
    if (!((val >> i) & 1)) zeros++;
  }
  if (kParityLUT[zeros] != parityField) return false;
  *value7 = val;
  return true;
}

void selcall_build_phasing(uint8_t *dst) {
  // PDX, PH5, PDX, PH4, PDX, PH3, PDX, PH2, PDX, PH1, PDX, PH0  (12 words)
  const uint8_t seq[12] = {
    SEL_PDX, SEL_PH5, SEL_PDX, SEL_PH4, SEL_PDX, SEL_PH3,
    SEL_PDX, SEL_PH2, SEL_PDX, SEL_PH1, SEL_PDX, SEL_PH0
  };
  memcpy(dst, seq, 12);
}

uint8_t selcall_build_call(uint16_t srcId, uint16_t dstId, uint8_t *dst) {
  // NOTE: variables are named addrA1/addrB1/... (not A1/B1/...) because the
  // ESP32 Arduino core's binary.h #defines B1, B2, B3... as binary-literal
  // helper macros (e.g. B1010), which collide with short names like "B1".
  uint8_t addrA1 = (srcId / 100) % 100;
  uint8_t addrA2 = srcId % 100;
  uint8_t addrB1 = (dstId / 100) % 100;
  uint8_t addrB2 = dstId % 100;

  uint8_t msg[SELCALL_MSG_MAX_WORDS] = {
    SEL_SEL, SEL_SEL, addrB1, SEL_SEL, addrB2, SEL_SEL,
    SEL_RTN, addrB1, addrA1, addrB2, addrA2, SEL_RTN,
    SEL_ARQ, addrA1, SEL_ARQ, addrA2, SEL_ARQ, SEL_ARQ
  };
  memcpy(dst, msg, SELCALL_MSG_MAX_WORDS);
  return SELCALL_MSG_MAX_WORDS;
}

uint8_t selcall_build_chantest(uint16_t srcId, uint16_t dstId, uint8_t *dst) {
  uint8_t addrA1 = (srcId / 100) % 100;
  uint8_t addrA2 = srcId % 100;
  uint8_t addrB1 = (dstId / 100) % 100;
  uint8_t addrB2 = dstId % 100;

  uint8_t msg[SELCALL_MSG_MAX_WORDS] = {
    SEL_ID, SEL_ID, addrB1, SEL_ID, addrB2, SEL_ID,
    SEL_RTN, addrB1, addrA1, addrB2, addrA2, SEL_RTN,
    SEL_ARQ, addrA1, SEL_ARQ, addrA2, SEL_ARQ, SEL_ARQ
  };
  memcpy(dst, msg, SELCALL_MSG_MAX_WORDS);
  return SELCALL_MSG_MAX_WORDS;
}
