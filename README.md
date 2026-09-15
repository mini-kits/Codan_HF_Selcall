# ESP32 CODAN Selcall (CCIR 493-4) Encoder / Decoder

A menu-driven Selcall box for an HF amateur transceiver: pick a contact on a
128x160 TFT, press select, and it keys the rig and sends a CODAN-compatible
Selcall burst. It also listens continuously on receive audio and pops up an
alert with the caller's ID when a valid Selcall is decoded.

---

## 1. What protocol this actually implements

"CODAN Selcall" (as used on amateur HF, e.g. the 7045 kHz Selcall calling
channel in Australia/NZ) is CODAN's implementation of **CCIR 493-4**. It is
**not** the simple sequential-tone system (ZVEI/EEA/EIA/etc.) — it's a
2-tone FSK data protocol:

| Parameter        | Value                                   |
|-------------------|------------------------------------------|
| Modulation        | 2-FSK, phase-continuous                  |
| Baud rate          | 100 baud (10 ms/bit)                     |
| Center frequency   | 1785 Hz (measured from a real IC-7610 TX reference recording — see §1.1) |
| Shift              | 170 Hz (tones at 1700 Hz / 1870 Hz)      |
| Word format        | 7 data bits + 3 parity bits (10 bits/word), LSB-first |
| Parity             | Count of zero-bits in the 7-bit value, looked up in a fixed table |
| Preamble           | ~2.0s (200 bits) alternating tone, matching a real IC-7610 TX reference — see §1.1 |
| Framing            | 12-word phasing pattern, then an 18-word message (address sent twice, interleaved with control words, for redundancy) |

