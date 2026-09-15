#pragma once
/*
 * config.h — pin map and global constants
 * ESP32 CODAN (CCIR 493-4) Selcall Encoder/Decoder
 */

#include <Arduino.h>

// ---------------- TFT (ST7735, 128x160) — SPI ----------------
#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    2
#define TFT_MOSI  23   // hardware VSPI MOSI
#define TFT_SCLK  18   // hardware VSPI SCK
#define TFT_BL    15   // backlight, PWM dimmable (optional)

// ---------------- Navigation buttons (active LOW) ----------------
// GPIO34/35 are input-only and have NO internal pull-ups — external 10k
// to 3V3 required on those two. GPIO32/33 DO have internal pull-up
// hardware and display_menu.cpp enables it in software (INPUT_PULLUP),
// so an external resistor on those two is optional, not required.
#define BTN_UP     34
#define BTN_DOWN   35
#define BTN_SELECT 32
#define BTN_BACK   33

// ---------------- Audio TX (encoder output, DAC) ----------------
// ESP32 has two 8-bit DACs on fixed pins: GPIO25 (DAC1) and GPIO26 (DAC2).
#define AUDIO_TX_DAC_PIN   25   // DAC1 -> RC filter -> isolation xfmr -> radio DATA/LINE IN
#define AUDIO_TX_SAMPLE_HZ 20000UL

// ---------------- Audio RX (decoder input, ADC) ----------------
// Use an ADC1 pin (ADC2 conflicts with Wi-Fi; unused here but keep on ADC1 to be safe).
#define AUDIO_RX_ADC_PIN   36   // ADC1_CH0 (VP) — after bias/attenuator network
#define AUDIO_RX_SAMPLE_HZ 9600UL

// ---------------- PTT keying output ----------------
#define PTT_PIN 27              // drives keying transistor, see README
#define PTT_LEAD_MS   300        // key up this long before audio starts
#define PTT_TAIL_MS   150        // hold PTT this long after last tone

// ---------------- CODAN / CCIR 493-4 protocol constants ----------------
#define SELCALL_BAUD        100
#define SELCALL_BIT_MS       10          // 1000/BAUD
#define SELCALL_CENTER_HZ  1785.0f       // measured from a reference recording off
                                          // a real IC-7610 TX (voicetx5.wav): stable
                                          // (non-transition) bit windows measured
                                          // space=1700.00Hz, mark=1870.01Hz, std
                                          // <0.1Hz — center = (space+mark)/2 = 1785Hz.
                                          // Both the generic ITU-R M.493 maritime
                                          // value (1700Hz) and the earlier 1763Hz
                                          // figure differ from this measurement.
#define SELCALL_SHIFT_HZ    170.0f
#define SELCALL_SPACE_HZ  (SELCALL_CENTER_HZ - SELCALL_SHIFT_HZ/2.0f)  // bit=0 → 1700 Hz
#define SELCALL_MARK_HZ   (SELCALL_CENTER_HZ + SELCALL_SHIFT_HZ/2.0f)  // bit=1 → 1870 Hz

// Preamble length: SELCALL_PREAMBLE_DIBITS iterations of buildPreamble()
// in selcall_tx.cpp each emit one 0-bit then one 1-bit (2 bits/iteration).
// 100 dibits = 200 bits = 2.0s @ 100 baud — matches the ~2.0s alternating
// preamble measured in the reference IC-7610 recording (see README §1.1).
// This is shorter than the full CCIR 493-4 spec's ~6s preamble (which
// would be SELCALL_PREAMBLE_DIBITS=300); RX doesn't need it either way
// (see selcall_rx.cpp) since it finds the phasing pattern directly rather
// than acquiring lock off the preamble — this is purely to match what a
// real Codan-style transmission on this system actually sends.
#define SELCALL_PREAMBLE_DIBITS 100

// ---------------- NVS / storage ----------------
#define NVS_NAMESPACE "codansel"
#define MAX_CONTACTS   20
