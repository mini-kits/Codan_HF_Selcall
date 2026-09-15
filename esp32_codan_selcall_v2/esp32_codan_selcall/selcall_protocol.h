#pragma once
#include <Arduino.h>

#define SEL_RTN 100   
#define SEL_SEL 120   
#define SEL_ID  123   
#define SEL_ARQ 117   
#define SEL_PDX 125   
#define SEL_PH0 104
#define SEL_PH1 105
#define SEL_PH2 106
#define SEL_PH3 107
#define SEL_PH4 108
#define SEL_PH5 109
#define SEL_PH6 110
#define SEL_PH7 111
#define SEL_EOS 127

#define SELCALL_MSG_MAX_WORDS 18

uint16_t selcall_encode_word(uint8_t value7);
bool selcall_decode_word(uint16_t word10, uint8_t *value7);
void selcall_build_phasing(uint8_t *dst);
uint8_t selcall_build_call(uint16_t srcId, uint16_t dstId, uint8_t *dst);
uint8_t selcall_build_chantest(uint16_t srcId, uint16_t dstId, uint8_t *dst);
