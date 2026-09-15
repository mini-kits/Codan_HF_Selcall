#pragma once
/*
 * selcall_tx.h — audio FSK generation for CODAN Selcall
 *
 * Uses a hardware timer to drive the ESP32's built-in 8-bit DAC (GPIO25)
 * at a fixed sample rate with a phase-accumulator (DDS) sine generator.
 * The bit stream (preamble + phasing + message words, LSB-first per word)
 * is walked one bit at a time; the DDS frequency word is switched at each
 * 10 ms bit boundary. Because the phase accumulator is never reset, tone
 * switches are phase-continuous (no audible/RF clicks between symbols).
 *
 * Call selcall_tx_send() from the UI/menu code; it blocks until the whole
 * burst (PTT lead-in, preamble, phasing, message, PTT tail) has been sent.
 */

#include <Arduino.h>

void selcall_tx_init();

// Send a full "ring the radio" call from srcId to dstId (both 0000-9999).
void selcall_tx_send_call(uint16_t srcId, uint16_t dstId);

// Send a CODAN channel-test request.
void selcall_tx_send_chantest(uint16_t srcId, uint16_t dstId);
