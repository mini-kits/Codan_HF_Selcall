/*
 * ESP32 CODAN Selcall (CCIR 493-4) Encoder / Decoder
 * TFT: 1.8" 128x160 ST7735, menu-selectable contacts for TX,
 * line-level audio in/out for connection to an HF amateur transceiver.
 *
 * See README.md in this folder for wiring, the line-level interface
 * circuit, and calibration notes.
 *
 * Libraries required (Arduino Library Manager):
 *   - Adafruit GFX Library
 *   - Adafruit ST7735 and ST7789 Library
 * (Preferences and driver/adc.h ship with the ESP32 Arduino core.)
 */

/*
 * ESP32 CODAN Selcall (CCIR 493-4) Encoder / Decoder
 * Optimized for Dual-Core execution and race-free timing.
 */

/*
 * ESP32 CODAN Selcall (CCIR 493-4) Encoder / Decoder
 * Optimized for Dual-Core execution and race-free timing.
 */

/*
 * ESP32 CODAN Selcall (CCIR 493-4) Encoder / Decoder
 * Optimized for Dual-Core execution and race-free timing with Diagnostics.
 */

/*
 * ESP32 CODAN Selcall (CCIR 493-4) Encoder / Decoder
 * Optimized for Dual-Core execution, race-free timing, and gated diagnostics.
 */

#include <Arduino.h>
#include "config.h"
#include "selcall_tx.h"
#include "selcall_rx.h"
#include "display_menu.h"

QueueHandle_t rxEventQueue = NULL;
TaskHandle_t dspTaskHandle = NULL; 

// Core 0 Task: UI, Button Handling, and Squenched/Gated Telemetry Logging
void TaskUI(void *pvParameters) {
  (void)pvParameters;
  uint32_t lastLogMs = 0;
  
  for(;;) {
    ui_task();
    
    // Telemetry execution: Evaluated every 1000ms entirely on Core 0
    if (millis() - lastLogMs >= 1000) {
      lastLogMs = millis();
      
      // Fetch current raw ADC levels to compute the signal amplitude
      uint16_t mn, mx;
      selcall_rx_get_level(&mn, &mx);
      uint16_t pp = (mx > mn) ? (mx - mn) : 0;
      float vPP  = pp * 3.3f / 4095.0f;

      // SIGNAL GATE SQUELCH THRESHOLD: Only print if Peak-to-Peak voltage exceeds 0.15V
      // Adjust this value up if ambient station noise still triggers false printing.
      if (vPP > 0.15f) {
        SelcallDiagnostics diag;
        selcall_rx_get_diagnostics(&diag);
        
        unsigned int dspStackWatermark = 0;
        if (dspTaskHandle != NULL) {
          dspStackWatermark = uxTaskGetStackHighWaterMark(dspTaskHandle);
        }

        Serial.println(F("--- [ACTIVE AUDIO RX DIAGNOSTICS] ---"));
        Serial.printf("Input Signal Amplitude: %.2f Vpp\n", vPP);
        Serial.printf("Bits Processed: %u | Valid Words: %u | Parity Faults: %u\n", 
                      diag.totalBitsProcessed, diag.totalWordsDecoded, diag.totalParityErrors);
        Serial.printf("Signal Tracking -> Avg Mark Mag: %.1f | Avg Space Mag: %.1f\n", 
                      diag.avgMarkMag, diag.avgSpaceMag);
        Serial.printf("Framing Status: %s | Core 1 DSP Free Stack: %u bytes\n", 
                      diag.isAnyPhaseFramed ? "LOCKED/SYNCED" : "SEARCHING", dspStackWatermark);
        Serial.println(F("-------------------------------------\n"));
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(16)); 
  }
}

// Core 1 Task: Dedicated DSP decoding engine
void TaskDSP(void *pvParameters) {
  (void)pvParameters;
  SelcallRxEvent ev;
  for(;;) {
    if (selcall_rx_poll(&ev)) {
      xQueueSend(rxEventQueue, &ev, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(2)); 
  }
}

void setup() {
  Serial.begin(115200);
  while(!Serial) { delay(10); } 
  
  Serial.println(F("[SYSTEM START] Initializing multi-core Selcall system..."));
  Serial.println(F("[INFO] Diagnostic monitor is now squelched. Logs will only print when live audio is detected."));
  
  rxEventQueue = xQueueCreate(4, sizeof(SelcallRxEvent));
  
  selcall_tx_init();
  selcall_rx_init();
  ui_init();

  // Pin UI and Telemetry Logger to Core 0
  xTaskCreatePinnedToCore(
    TaskUI, "UITask", 4096, NULL, 1, NULL, 0
  );

  // Pin isolated high-priority DSP engine to Core 1
  xTaskCreatePinnedToCore(
    TaskDSP, "DSPTask", 4096, NULL, 5, &dspTaskHandle, 1
  );
}

void loop() {
  vTaskDelete(NULL); 
}
