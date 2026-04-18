# UV Sterilizer — PIC18F45K22 Embedded Controller

A microcontroller-driven UV sterilization unit built around the PIC18F45K22. The system sequences through four stages — idle, preparation, UV exposure, and ventilation — with real-time countdown feedback on a multiplexed 7-segment display and stage annunciation on a character LCD. A normally-closed door interlock aborts the active cycle on breach.

> **Academic context.** Coursework project for **CPEN 231 — Microprocessors**, Faculty of Engineering, **University of Balamand**, Fall 2025–2026.
>
> **Team:** Georgy Al Mazraani · Fatima Youssef · Yahya Al Youssef

## Features

- Four-stage finite state machine: Idle → Preparation (5 s) → UV Sterilization (20 s) → Ventilation (10 s)
- Dual-display output — 2-digit common-cathode 7-segment for countdown, 16×2 character LCD for stage text
- Door-interlock safety: UV lamp de-energized instantly on door open, cycle aborted, abort message posted to LCD
- Two-level dirtiness selector — extends UV and ventilation durations by 10 s when the elevated setting is latched
- Fan driven through a 2N2222 BJT with a 1N4007 flyback diode for coil protection
- Single-timer architecture — Timer0 handles both countdown arithmetic and display multiplexing via a software time-divider

## Hardware

### Bill of Materials

| Component | Qty | Role |
|-----------|-----|------|
| PIC18F45K22 | 1 | Main controller |
| 2-digit 7-segment display (CC, multiplexed) | 1 | Countdown readout |
| 16×2 character LCD (4-bit mode) | 1 | Stage annunciation |
| UV LED | 1 | Sterilization source |
| 12 V fan | 1 | Post-cycle ventilation |
| 2N2222 NPN transistor | 1 | Fan current driver |
| 1N4007 diode | 1 | Flyback protection |
| NOT gate (7404 or equivalent) | 2 | Active-low to active-high conversion on digit enables |
| Tactile push-button | 3 | Start + dirtiness level select |
| Door sensor (NC contact) | 1 | Safety interlock |
| 220 Ω resistor | 1 | UV LED current limit |
| 1 kΩ resistor | 2 | 2N2222 base drive, input bias |

### Pin Map

**Inputs — PORTB with internal pull-ups**

| Pin | Function |
|-----|----------|
| RB0 | Start push-button |
| RB1 | Door sensor (NC — logic 1 = door open) |
| RB2 | Dirtiness selector, normal level |
| RB3 | Dirtiness selector, elevated level |

**Outputs**

| Pin | Function |
|-----|----------|
| RD0–RD7 | 7-segment drive (segments a–g, dp) |
| RC0 | UV lamp control line |
| RC1 | Fan control line (through 2N2222) |
| RC2 | 7-segment digit 1 enable |
| RC3 | 7-segment digit 2 enable |
| RA0–RA7 | LCD control and data, 4-bit mode |

## Operation

**Idle.** UV lamp and fan de-energized. The LCD shows `Welcome / Press button`, and the 7-segment reads `00`. The controller polls RB2 and RB3 to latch the dirtiness level, then blocks on RB0 until a press-release cycle completes.

**Preparation.** LCD updates to `Stage 1: / Preparing`. A five-second countdown runs on the 7-segment, giving the operator time to seat the door. All actuators remain off during this window.

**UV Sterilization.** LCD shows `Stage 2: / UV Sterilization`. RC0 drives the UV lamp high. The countdown is set to `20 + time_add` seconds. Throughout the stage the controller polls RB1 on every loop iteration — a door-open event immediately clears RC0, zeros the countdown, posts `Abort! / Door is opened`, and drops the system back to idle after a blocking delay.

**Ventilation.** LCD shows `Stage 3: / Ventilation`. RC1 drives the fan high for `10 + time_add` seconds to evacuate residual ozone and heat. The controller returns to idle on timeout.

## LCD Messaging

The 16×2 character LCD operates in 4-bit mode through PORTA and serves as the primary status channel. Every stage transition triggers `Update_Display(stage)`, which clears both rows with `DispBlanks()` and writes a fresh pair of ROM strings to `Ln1Ch0` and `Ln2Ch0`. The seven-segment display carries the numeric countdown, while the LCD carries the semantic state — the two readouts together let the operator confirm both *what* the system is doing and *how much time remains* at a glance.

| Stage code | Line 1 | Line 2 | Meaning |
|-----------|--------|--------|---------|
| `1` | `Welcome` | `Press button` | Idle — waiting for start press |
| `2` | `Stage 1:` | `Preparing` | Five-second pre-UV delay |
| `3` | `Stage 2:` | `UV Sterilization` | Lamp active, door monitored |
| `4` | `Stage 3:` | `Ventilation` | Fan running, cycle completing |
| `5` | `Abort!` | `Door is opened` | Safety interlock tripped during UV |

The abort message is latched on the LCD for the duration of a blocking `Delay10KTCYx(100)` call before the system returns to the idle banner. That pause exists so the operator reads the reason for termination before the display reverts.

Relocating the LCD bus from its default PORTB mapping to PORTA was handled by a modified header, `LCDnew.h` — this frees PORTB for the start button, door sensor, and two level-select inputs without touching the rest of the LCD driver.

## Software Architecture

### Timer0 as a Dual-Purpose Clock

The 4 MHz internal oscillator produces an instruction cycle of 1 µs. Timer0 is configured in 8-bit mode with a 1:32 prescaler and preloaded as `TMR0L = 256 − 250`, producing an overflow every 8 ms. That 8 ms window satisfies two independent requirements at once — it stays well below the 25 ms flicker threshold for multiplexed display refresh, and 8 is an integer divisor of 1000, allowing a software counter to mark one-second ticks on every 125th overflow. The single-timer design avoids the hardware cost of a second peripheral.

### Interrupt Service Routine

The ISR is vectored at `0x0008` through the `#pragma code ISR=0x0008` and `#pragma interrupt ISR` directives. On each entry it clears the Timer0 flag, reloads `TMR0L`, and branches on the `countDown` flag. When counting is active, a modulo-125 software counter decrements the `seconds` variable once per second and converts the result to BCD via `Bin2Bcd()`. The routine then calls `Display()` on every tick to service display multiplexing.

### Display Multiplexing

The two-digit 7-segment display shares PORTD for segment drive. Digit selection is handled by RC2 and RC3 through external inverters — the inverters translate the active-low enable signals into the active-high drive required at the digit commons. A boolean `state` variable toggles inside the ISR, picking one digit per refresh cycle. Both enable lines are cleared before the new segment pattern is written, preventing ghosting between digits.

Segment encodings for digits 0–9 live in the `SSCODES` lookup table, bypassing runtime segment calculation.

### Finite State Machine

The main loop is a nested `while(1)` structure that calls `Idle()`, `WaitDelay()`, `SterProcess()`, and `Ventilation()` in order. The `finish` flag gates the transition from sterilization to ventilation — if the door-interlock aborts the UV stage, `finish` is cleared and the outer loop short-circuits back to idle.

## Dirtiness Level Selector

Two push-buttons on RB2 and RB3 let the operator extend the cycle duration before the start press. The `time_add_method(level)` function maps the latched level to a duration offset:

- Level 0 (RB2): `time_add = 0` → UV = 20 s, ventilation = 10 s
- Level 1 (RB3): `time_add = 10` → UV = 30 s, ventilation = 20 s

The offset is applied at stage entry inside `SterProcess()` and `Ventilation()`. Reading the buttons inside `Idle()` means the selection is latched only between cycles, which blocks mid-run changes.

## Build and Flash

The firmware targets the Microchip C18 compiler. Required headers:

```c
#include <p18cxxx.h>
#include <delays.h>
#include <BCDlib.h>
#include <LCDnew.h>
```

`LCDnew.h` is a derivative of the standard `LCD4lib.h` with the data and control pins relocated from PORTB to PORTA. This frees PORTB for the start, door, and level inputs.

A Proteus 8 simulation is provided — open `Proteus Circuit.pdsprj` and load the compiled HEX into the virtual PIC18F45K22 to exercise the full cycle without hardware.

## Repository Contents

```
├── C Code.c                # Firmware source
├── Proteus Circuit.pdsprj  # Proteus simulation project
├── Report.pdf              # Full design documentation
└── README.md
```

## Future Work

Two indicator LEDs driven from unused PORTA lines would expose the currently-latched dirtiness level to the operator before the start press. A third level, an audible end-of-cycle beeper, and a cycle-count readout on the LCD are straightforward extensions of the existing BCD display path.

## Authors

Developed by **Georgy Al Mazraani**, **Fatima Youssef**, and **Yahya Al Youssef** as part of the CPEN 231 Microprocessors course at the University of Balamand, Faculty of Engineering — Fall 2025–2026.
