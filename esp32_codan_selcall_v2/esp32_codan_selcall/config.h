#pragma once
#include <Arduino.h>

// ---------------- TFT (ST7735, 128x160) — SPI ----------------
#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    2
#define TFT_MOSI  23   // hardware VSPI MOSI
#define TFT_SCLK  18   // hardware VSPI SCK
#define TFT_BL    15   // backlight, PWM dimmable (optional)

// ---------------- Navigation buttons (active LOW) ----------------
#define BTN_UP     34
#define BTN_DOWN   35
#define BTN_SELECT 32
#define BTN_BACK   33

// ---------------- Audio TX (encoder output, DAC) ----------------
#define AUDIO_TX_DAC_PIN   25   
#define AUDIO_TX_SAMPLE_HZ 20000UL

// ---------------- Audio RX (decoder input, ADC) ----------------
#define AUDIO_RX_ADC_PIN   36   // ADC1_CH0 (VP)
#define AUDIO_RX_SAMPLE_HZ 9600UL

// ---------------- PTT keying output ----------------
#define PTT_PIN 27              
#define PTT_LEAD_MS   300        
#define PTT_TAIL_MS   150        

// ---------------- CODAN / CCIR 493-4 protocol constants ----------------
#define SELCALL_BAUD        100
#define SELCALL_BIT_MS       10          
#define SELCALL_CENTER_HZ  1785.0f       
#define SELCALL_SHIFT_HZ    170.0f
#define SELCALL_SPACE_HZ  (SELCALL_CENTER_HZ - SELCALL_SHIFT_HZ/2.0f)  // bit=0 → 1700 Hz
#define SELCALL_MARK_HZ   (SELCALL_CENTER_HZ + SELCALL_SHIFT_HZ/2.0f)  // bit=1 → 1870 Hz

#define SELCALL_PREAMBLE_DIBITS 100

// ---------------- NVS / storage ----------------
#define NVS_NAMESPACE "codansel"
#define MAX_CONTACTS   20
