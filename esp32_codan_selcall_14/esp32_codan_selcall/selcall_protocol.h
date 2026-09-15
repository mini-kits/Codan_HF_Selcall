#pragma once
/*
 * selcall_protocol.h
 *
 * CCIR 493-4 ("CODAN Selcall") word/message framing.
 *
 * Each transmitted word = 7 data bits (0-127) + 3 parity bits.
 * Parity bits = a (non-linear/Gray-coded) representation of the COUNT of
 * zero-bits in the 7-bit value, taken from a fixed lookup table.
 * The full 10-bit word is sent LSB-first.
 *
 * This framing is ported faithfully from the openly published reference
 * modulator (M. Jessop / VK5QI, "alltheFSKs/CCIR493-4.py", itself based on
 * the QITX project and reverse-engineering of commercial CODAN traffic).
 * It is believed accurate for interoperating with CODAN HF radios, but
 * exact bit-for-bit compliance has not been independently re-verified
 * against the ITU text — test on the bench before relying on it.
 */

#include <Arduino.h>

// --- Special (non-digit) selcall words ---
#define SEL_RTN 100   // Routine call marker
#define SEL_SEL 120   // Selective call type
#define SEL_ID  123   // Individual/automatic service (CODAN "channel test")
#define SEL_ARQ 117   // Acknowledge-request / end-of-address marker
#define SEL_PDX 125   // Phasing "DX" filler word
#define SEL_PH0 104
#define SEL_PH1 105
#define SEL_PH2 106
#define SEL_PH3 107
#define SEL_PH4 108
#define SEL_PH5 109
#define SEL_PH6 110
#define SEL_PH7 111
#define SEL_EOS 127

// Max words in one built message (phasing handled separately by the caller)
#define SELCALL_MSG_MAX_WORDS 18

// Encode a 7-bit value (0-127) into the full 10-bit line word (bits 0..9).
uint16_t selcall_encode_word(uint8_t value7);

// Decode a received 10-bit line word back to its 7-bit value.
// Returns true and fills *value7 if the parity check passes.
bool selcall_decode_word(uint16_t word10, uint8_t *value7);

// Build the 12-word phasing sequence into dst[]. dst must hold >=12 entries.
void selcall_build_phasing(uint8_t *dst);

// Build a normal "ring the radio" selective call from srcId -> dstId
// (both 4-digit, 0000-9999). Returns number of words written into dst[]
// (dst must hold >= SELCALL_MSG_MAX_WORDS entries).
uint8_t selcall_build_call(uint16_t srcId, uint16_t dstId, uint8_t *dst);

// Build a CODAN "channel test" request (radio replies with a tone burst).
uint8_t selcall_build_chantest(uint16_t srcId, uint16_t dstId, uint8_t *dst);
