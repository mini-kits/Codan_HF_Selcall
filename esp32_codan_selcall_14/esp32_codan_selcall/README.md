# ESP32 CODAN Selcall (CCIR 493-4) Encoder / Decoder

A menu-driven Selcall box for an HF amateur transceiver: pick a contact on a
128x160 TFT, press select, and it keys the rig and sends a CODAN-compatible
Selcall burst. It also listens continuously on receive audio and pops up an
alert with the caller's ID when a valid Selcall is decoded.

**Read this whole file before wiring anything to your radio.** The audio
interface section in particular has values you must adjust for your specific
transceiver.

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

This firmware's **encoder is a faithful port** of the openly published
reference implementation (M. Jessop/VK5QI's `CCIR493-4.py`, itself derived
from the QITX project and analysis of real CODAN traffic), so a correctly
wired TX chain should produce a burst that rings a real CODAN (or
CCIR‑493‑4‑compatible Barrett, etc.) radio.

### 1.1 Validated against a real IC-7610 TX recording

The values above (baud rate, shift, word/parity format, phasing pattern,
message layout) were checked against a real reference recording of this
system's standard Selcall as transmitted by an Icom IC-7610. Method:
Hilbert-transform instantaneous frequency to find the tone pair, a
Goertzel bit-slicer at 100 baud to recover the bitstream, then decoding
that bitstream with this project's own parity/phasing/message logic.
Results: all 30 decoded words passed the parity check (0 failures), the
first 12 matched the phasing pattern exactly word-for-word, and the
18-word message decoded cleanly with its redundant address copies
self-consistent. The one value that didn't match what was configured at
the time was the center frequency — restricting the frequency estimate to
bit windows well clear of any transition (std <0.1Hz) gave space=1700.00Hz,
mark=1870.01Hz, i.e. center=1785Hz, shift=170Hz — which is what's now in
`config.h`. The recording also contained a genuine ~2.0s (200-bit)
alternating preamble; TX now sends the same length
(`SELCALL_PREAMBLE_DIBITS` = 100 in `config.h`, since `selcall_tx.cpp`'s
preamble builder emits 2 bits per unit of that constant).

**Preamble is sent, but RX doesn't rely on it — parallel-phase bit
clocks instead:** the full CCIR 493-4 spec's preamble runs ~6s; this build
sends a shorter ~2.0s version to match the real reference recording above,
purely so a genuine Codan-style receiver on this system (which may expect
some lead-in for squelch/AGC settling) sees something familiar. RX doesn't
use it for bit-clock acquisition at all — `selcall_rx.cpp` just treats the
preamble's alternating bits as words that fail the parity check and skips
over them exactly like it would any other leading noise, the same way it
already handled the recording above before this build sent any preamble
of its own.

Instead of one free-running RX bit clock (which only decodes correctly if
its phase happens to already match the transmitter's — a matter of luck
each time TX keys up, since there's nothing to synchronize the two), RX
runs **`RX_NUM_PHASES` (4) parallel bit clocks**, evenly spaced across one
bit period, all evaluated against the same buffered audio. Whichever one
lands closest to the true bit edges finds the phasing pattern and decodes
the message; the others just fail their parity checks and get ignored.
With 4 phases spaced a quarter-bit-period apart, the worst-case offset
from the nearest candidate is about 12.5% of a bit period — comfortably
inside what Goertzel tone detection tolerates. This costs more CPU per
poll (`selcall_rx.cpp`'s `RX_NUM_PHASES` constant) but no extra airtime
and no acquisition delay. If it's still missing decodes with good RX
audio level, try raising `RX_NUM_PHASES` (must evenly divide the 96
samples/bit — 4, 6, 8, 12, 16, 24, 32... are all valid) for finer-grained
phase coverage, at the cost of proportionally more Goertzel evaluations
per poll.

---

## 2. Bill of materials

- ESP32 dev board (any WROOM-32 style board)
- 1.8" 128x160 SPI TFT, ST7735 driver ("ST7735 1.8 TFT" — very common eBay/AliExpress module)
- 4x momentary push buttons (or a rotary encoder + button, see §5)
- 2x 10 kΩ resistors (external pull-ups — required for the two input-only
  GPIOs used as buttons; the other two buttons use the ESP32's internal
  pull-ups instead, see §3)
- 2x small 600:600 Ω 1:1 audio isolation transformers (e.g. Bourns/Xicon
  "line matching" transformers, or repurposed telecoms line transformers) —
  **strongly recommended**, see §4
- 2x 10 kΩ linear trim potentiometers (TX level, RX level)
- Assorted resistors/capacitors for the filter/bias networks (§4)
- 1x small-signal NPN transistor (2N3904/BC547) or N-channel MOSFET
  (2N7000) for PTT keying
- 3.5 mm TRS jacks (or whatever connector your rig's accessory/data port
  uses) for AUDIO OUT, AUDIO IN, PTT

---

## 3. Pinout (see `config.h`)

| Function              | ESP32 GPIO |
|------------------------|-----------|
| TFT CS                 | 5         |
| TFT RST                | 4         |
| TFT DC (A0)             | 2         |
| TFT MOSI (SPI)          | 23        |
| TFT SCLK (SPI)          | 18        |
| TFT Backlight           | 15        |
| Button UP               | 34 (+10k pull-up to 3V3, required) |
| Button DOWN             | 35 (+10k pull-up to 3V3, required) |
| Button SELECT           | 32 (internal pull-up used in firmware) |
| Button BACK             | 33 (internal pull-up used in firmware) |
| Audio TX out (DAC1)     | 25        |
| Audio RX in (ADC1_CH0)  | 36 (VP)   |
| PTT keying output       | 27        |

GPIO34/35 are **input-only** and have no internal pull-ups/downs on the
ESP32, so they need the external 10 kΩ resistors to 3V3 shown above.
GPIO32/33 (SELECT/BACK) do have internal pull-up hardware, which
`ui_init()` enables in firmware (`INPUT_PULLUP`) — an external resistor on
those two is optional belt-and-braces, not required. All four buttons pull
their pin to GND when pressed.

---

## 4. Line-level audio interface (the part you asked about)

The point of using "line level" I/O rather than tapping the mic/speaker
directly is that it lets you use your rig's DATA/ACC/PACKET accessory port,
which is unaffected by the front-panel volume/mic-gain controls and (on
most rigs) is designed for exactly this kind of external modem. **Every
radio's accessory-port pinout, impedance, and expected level is different
— check your rig's manual** for the DATA/ACC or PKT connector pinout before
wiring. The circuits below are deliberately generic and adjustable so they
can be trimmed to whatever your particular radio expects.

### 4.1 TX path (ESP32 → radio audio/data input)

```
ESP32 GPIO25 (DAC, 8-bit, 0-3.3V, idles at mid-scale 1.65V)
    |
    +-- R1 1k --+-- C1 10nF --GND      (2-pole ~3 kHz RC low-pass,
    |           |                       removes DAC sample-rate images
    +-- R2 1k --+-- C2 10nF --GND       above the 1.6-1.8 kHz tones)
    |
    C3 1uF (DC block)
    |
    Primary of 600:600 isolation transformer
    |
    Secondary --- 10k trim pot (wired as attenuator) --- TX level out
                                                            |
                                                   3.5mm jack tip -> radio
                                                   DATA/ACC audio-in pin
                                                   jack sleeve -> radio
                                                   accessory-port GND
```

- The isolation transformer breaks the ground connection between the ESP32
  and the radio chassis — important on HF stations to avoid RF getting into
  the ESP32 via the audio cable, and to avoid ground-loop hum/noise.
- The 10 kΩ trim pot lets you set the injected level to match your radio's
  DATA-in sensitivity (commonly tens to a few hundred mV RMS for a full
  "100%" data input — start with the pot near minimum and bring it up
  slowly while watching the rig's ALC/output power, exactly as you would
  when setting up any other digital-mode soundcard interface).
- If your accessory port's audio input is actually the mic line (no
  separate DATA input), keep the level low — a few tens of mV — and check
  whether the port needs mic bias decoupled (the transformer secondary
  handles this cleanly since it has no DC path).

### 4.2 RX path (radio audio/data output → ESP32 ADC)

```
Radio DATA/ACC audio-out (or discriminator/speaker line)
    |
   3.5mm jack tip
    |
    C4 1uF (DC block) -- Primary of 2nd 600:600 isolation transformer
    |
    Secondary
    |
    +-- 10k trim pot (attenuator, sets input level) --+
    |                                                   |
    +-- R3 4.7k --+-- R4 4.7k --+ (bias divider,        |
                   |             center-tap = 1.65V,     |
                  GND         from 3.3V rail)             |
                   |                                       |
                  1.65V bias node ------ C5 1uF ----------+
                   |
                   +-- R5 4.7k --- ESP32 GPIO36 (ADC1_CH0)
                   |
                  C6 10nF --GND   (anti-alias low-pass, ~3 kHz corner
                                    with R5, ahead of the ADC pin)
```

- The ESP32 ADC only reads 0–3.3 V (unipolar); the bias network centers the
  incoming AC audio on 1.65 V so both halves of the waveform are captured.
- Set the RX trim pot so the audio swings comfortably within the ADC range
  without clipping — start attenuated and increase while watching decoded
  bit quality (you can temporarily log raw ADC min/max over Serial while
  tuning).
- If you want better sensitivity/noise performance than a passive
  attenuator+bias network gives you, add a single op-amp (e.g. MCP6002)
  non-inverting buffer/gain stage between the transformer secondary and the
  bias network — the resistor values above assume a fairly "hot" line
  output and are a starting point, not a certified design.

### 4.3 PTT keying

```
ESP32 GPIO27 --- R6 1k --- Base/Gate of Q1 (2N3904 or 2N7000)
                             Collector/Drain -> radio PTT line
                             Emitter/Source  -> GND (shared with radio
                                                 accessory-port ground)
```

Most rigs' accessory-port PTT line is pulled up internally and keyed by
grounding it — the transistor above does exactly that when `GPIO27` goes
HIGH. **Confirm your rig's PTT line polarity and maximum sink current in
its service manual before connecting.** Never wire a GPIO pin directly to
a radio's PTT line — always go through the isolation transistor.

---

## 5. Menu / controls

Four buttons: UP / DOWN / SELECT / BACK.

- **Call Contact** → scroll the stored contact list → SELECT → choose
  *Selective Call* (rings the far radio) or *Channel Test* (asks it to send
  back a test tone) with UP/DOWN → SELECT to transmit.
- **Add Contact** → 4-digit spinner to enter a new contact's Selcall ID
  (stored in flash/NVS; auto-named CT1, CT2, ...).
- **My ID** → 4-digit spinner to set your own station's Selcall address,
  used as the source address in outgoing calls.
- **RX Level Meter** → live RX front-end calibration screen (see §6.1).
  Press any button to return to the main menu.
- **About** → firmware/protocol info screen.
- Incoming decoded calls interrupt whatever screen is showing with a red
  "INCOMING CALL" popup showing the caller's and called ID; press any
  button to dismiss.

Prefer a rotary encoder instead of 4 buttons? Wire its A/B outputs to
`BTN_UP`/`BTN_DOWN` (treat any pulse as up/down) and its push-switch to
`BTN_SELECT`, and keep a separate button for `BTN_BACK` — the debounced
`readButton()` in `display_menu.cpp` doesn't care which kind of switch
produces the edges.

---

## 6. Known limitations / next steps

- TX sends a ~2s preamble but RX doesn't use it — RX instead runs 4
  parallel phase-offset bit clocks (§1) to blindly find bit-phase
  alignment. If decodes still aren't framing with good RX audio level,
  try increasing `RX_NUM_PHASES` in `selcall_rx.cpp` for finer phase
  coverage.
- Only individual selective calls and CODAN channel-test are implemented;
  group calls, paging, and the redundant-field cross-checking that a
  production decoder would use for extra robustness are not.
- The 8-bit DAC + simple RC filter TX chain is adequate for feeding a data
  port but is not hi-fi; if you want a cleaner spectrum, move the DDS
  generator to the ESP32's I2S peripheral driving an external DAC.
- No audible/LED alert on incoming calls yet — the popup is visual only.

### 6.1 RX level meter (front-end calibration mode)

Select **RX Level Meter** from the main menu to open a live calibration
screen, useful for setting the RX trim pot in §4.2 against real numbers
instead of guessing. It shows the raw ADC min/max and peak-to-peak swing
on GPIO36, refreshed a few times a second, plus a rough voltage conversion
and a "too low / good level / too high" verdict against the ~1.0–1.8Vpp
target from §4.2. Press any button to return to the main menu. The voltage
figures are an approximation only — the ESP32 ADC is non-linear near both
rails — so treat them as a tuning aid, not a calibrated measurement.

## 7. References

- CCIR 493-4 word/parity/framing: M. Jessop (VK5QI), *alltheFSKs*,
  `CCIR493-4.py` (open-source reference modulator).
- Protocol background and a from-scratch open decoder write-up: xssfox,
  "CODAN Selcall Part 1" (2024), and the *freeselcall* project.
