# ASkompu multi-board application shell

Wikissä https://github.com/Mikky100/ASkompu/wiki kerrottu toiminta suomeksi AI:lle selittämiseksi ja käyttäjien/kehittäjien avuksi. 

Current release: **0.1.0 (prototype)**. This is the first versioned ASkompu
release for hardware and in-car evaluation. It is not yet a production-ready
or safety-certified navigation instrument; see [CHANGELOG.md](CHANGELOG.md) for
the implemented scope and known limitations.

This branch contains a hardware-independent ASkompu application shell and two
main-computer display ports: the 320x170 LilyGO T-Display S3 and an ESP32-S3
N16R8 with a 480x320 ILI9488 SPI display. The normative product and interaction
specification is the [ASkompu GitHub Wiki](https://github.com/Mikky100/ASkompu/wiki).

## Competition runtime

TIME, SPEED and MITTIS calculation, start waiting, normal points, immediate
point undo, JAT stages, finish, scoring, AT, additional orders and road breaks
run in RAM. JAT supports `MANNED_JAT`, `EMIT_JAT_OFFSET`, `EMIT_MLA` and
`EMIT_ULA`, including their start-time proposals and physical stage-distance
zero points. At JAT the completed delta is stored for scoring while the active
competition clock resets to zero and remains stopped until the accepted next
start time. MITTIS calibration proposals use integer arithmetic and a changed
factor affects only future pulses.

At the start of every MITTIS segment, Trip 1 resets. Its displayed value stays
at zero for ten seconds while the internal Trip 1 and competition calculations
continue accumulating normally; after the hold the current accumulated value
is shown.

Events are written in order to a hardware-independent RAM repository. The log
contains point/undo, AT/cancel, JAT, finish, start-time, MITTIS, override,
reverse and trip-reset records. JAT and finish records carry their `StageResult`;
totals use saturating arithmetic. Runtime events and in-progress competition
state are intentionally not restored after power loss yet, and rapidly
changing state is not written to NVS.

## Startup and software clock

Every boot starts in an unskippable time-entry view. Time is entered as `H:MM`:
Up/Down changes the active field, Right advances from hours to minutes and
accepts from minutes, and Left returns from minutes to hours. Hours wrap within
`0...23` and minutes within `0...59`, so an invalid value cannot be accepted.

Acceptance sets the clock to exactly `H:MM:00`; elapsed time before acceptance
is ignored. Time is never read from or written to Preferences/NVS, so it must be
entered again after every restart. The current clock is software-only and is
not battery-backed. `core::Clock` separates the application from the clock
implementation; the current `SoftwareClock` uses an injected monotonic
`TimeSource`, and a later RTC adapter can implement the same interface without
changing the core, DisplayModel, or navigation semantics. Unsigned elapsed-time
tracking handles Arduino `millis()` rollover when the clock is serviced by the
normal non-blocking loop.

## Non-competition basic view

After time acceptance the basic view shows:

- Trip 1 and `HH:MM:SS` in two large fixed top-row regions, using the same
  visual size and selected text color; Trip 2 remains available to the domain,
  diagnostics and reset input but is hidden from the basic view;
- while a competition is active, current segment, right-aligned delta, and next
  segment in fixed left/centre/right regions; point zero is formatted as `L`
  and the finish as `M`; the segment label is `JAT`, `MAALI`, or `MITTIS` as
  applicable; while waiting for a start, the segment that will begin is shown
  in the right-hand next-segment region;
- optional rounded debug speed with `km/h` in its own bottom region.
- on a running SPEED segment, a short Left toggles manual distance
  subtraction when the point-undo window is not active. While enabled, forward
  speed pulses reduce both trips and the competition distance/ideal time using
  the same signed-distance path as reverse. A large red `MIINUSTUS` banner is
  shown, and the mode clears at a point transition or when leaving the basic
  view.
- while a stage is running, Right opens a signed time adjustment in the
  next-segment region. Up/Down changes it in 10-second steps, Right accepts and
  Left cancels. An accepted adjustment changes the cumulative ideal time,
  delta and eventual stage score, and is recorded in the event log.

It contains no `MENU` or `DEV` text, menu hint, GPIO numbers, button states,
total pulse count, pulse age, or calibration data. Debug speed defaults off and
appears only when enabled through `JARJESTELMA > DEBUG > NOPEUS`.

The wiki defines Up/Down, not Right, as the way to open the main menu from the
drive/basic view. Down opens at the first item and Up at the last item. The main
menu shows three larger rows at a time and wraps; submenus clamp at their ends.
Left returns, Right opens or accepts,
and a long Left abandons the current non-startup UI operation and returns to the
basic view.

Button edges are captured by interrupts with 20 ms debounce. General buttons
use a 150 ms accepted-press guard. Right uses a stricter 350 ms guard and its
normal navigation action is dispatched only after a complete stable release,
so one noisy physical press cannot advance two input fields. A dispatched UI
event requests an immediate redraw instead of waiting for the periodic display
refresh. After the finish time and total points are shown, Left or Right returns
to the normal basic view; the completed competition and its results remain
available through the results menu.

## Menu tree

The current wiki's top-level skeleton is represented. `AJOMAARAYS` is a single
workflow; unavailable unrelated items remain visible but dimmed.

| Main group | Items | Status |
|---|---|---|
| Ajomääräys | Unified creation, browsing, editing, and confirmed replacement | Implemented |
| Kello | Direct clock editing | Implemented |
| Kerroin | Direct mm/pulse calibration editing | Implemented; a long-repeat step changes the value by 10 instead of 1 |
| Pisteet ja tapahtumat | Jaksojen pisteet, Tapahtumat | Implemented from the current RAM event/result data; stage browsing also shows the total |
| Näyttöasetukset | Kirkkaus, Tekstin väri, Näyttöselitteet | Persistent 10...100% PWM brightness, white/red/green text color, and label visibility are implemented |
| Tripit | Nollaa Trip 1, Nollaa Trip 2, Ulkoinen trip | Both resets work; external display source needs its hardware/protocol adapter |
| Järjestelmä | Diagnostiikka, Debug, Painikeasetukset, Muut asetukset | Diagnostics and persistent Debug speed work; button/system settings are not implemented |

Calibration editing uses a draft value. Left cancels without changing the
active value or requesting a write. Right accepts; an unchanged value requests
no NVS write, and one changed accepted value requests one controlled write.
Time editing uses the startup editor semantics, but cancel preserves the old
clock and acceptance resets seconds to `00` without touching NVS.

Trip resets have no confirmation. In the LilyGO test profile GPIO11 resets only
Trip 2; Trip 1 and foot reset remain core operations but have no physical GPIO.
GPIO14 is exclusively the Point button. Neither reset changes total pulse
count, competition distance, or speed calculation.

## Diagnostics

`Järjestelmä > Diagnostiikka` is an explicitly developmental view. It shows the
debounced Left/Up/Down/Right/Point/AT/Trip2 states, latest Point and AT events,
the filtered reverse state,
total/Trip 1/Trip 2 pulse counts, both trip distances, mm/pulse calibration,
speed, last-pulse age and zero-timeout state, current UI state, software-clock
state/time, and elapsed time since clock acceptance. Left closes it. Opening or
viewing diagnostics does not reset trips, change settings, stop the clock, or
stop pulse processing.

## Route-order creation, editing, and storage

`AJOMAARAYS` is now one top-level workflow. With no stored order it first asks
for `EI EMIT` (the default) or `EMIT`, locks that choice, asks for the
competition start time, and then enters ordered TIME, SPEED, or MITTIS segments.
After one MITTIS has been entered, later segment-type choices contain only TIME
and SPEED. Each accepted TIME or SPEED value continues with `SEURAAVA`, `JAT`,
or `MAALI`. TIME accepts 1...3599
seconds and SPEED a two-digit 1...99 km/h value. MITTIS accepts 1000...9999
metres and then a TIME value for that same interval; SPEED is not available as
the MITTIS interval's time rule. A finish is mandatory. Segment browsing starts
with the competition start time and shows both distance and time for MITTIS.
Segment
browsing labels the start point as `L` and the finish point as `M`, for example
`L-1` and `3-M`.

With an existing order, opening `AJOMAARAYS` defaults to `UUSI AJOMAARAYS`,
including in `IDLE`. Up/Down toggles to `MUOKKAA AJOMAARAYS`, Right confirms
the selected workflow, and Left returns to the menu without changing
the order or competition state. Editing then browses segments with Up/Down,
Right edits the selected segment, and Left returns or abandons the in-progress
edit. Replacement starts a separate draft from the competition-type selection;
the old long-Right replacement prompt is not used. Competition type is not part
of ordinary editing. The UI term `EI EMIT` maps to the unchanged domain enum
`CompetitionType::NON_EMIT`.

The domain model and validator are in `src/domain/RouteOrder.*`. The editor,
binary codec, and UI-independent storage port are in `src/route/`. The ESP32
adapter stores a checksum-protected blob in alternating Preferences slots. It
writes and verifies the inactive payload before advancing its generation, so a
failed write leaves the previous valid generation loadable. Unsupported schema
versions, corrupt payloads, invalid values, invalid point order, incompatible
JAT types, and missing/early finishes are rejected before replacement.

The active order and the editor draft are distinct objects. Creation or editing
does not mutate the active order; only a validated and successfully persisted
draft becomes current. The codec and validator have no UI, Bluetooth, Arduino,
or Preferences dependency and can therefore be reused by a future Android
import adapter.

## Architecture

- `src/core/` owns the UI state machine, menu selection/scrolling, trips,
  accepted pulses, speed, clock-facing semantics, calibration draft, save
  requests, diagnostics, and semantic `DisplayModel`. It has no Arduino, GPIO,
  TFT, Preferences, or display-resolution dependency.
- `src/core/Clock.*` defines `TimeSource`/`Clock` and the hardware-independent
  rollover-safe software clock.
- `src/domain/` contains calculation, saturation, trip, speed and calibration
  primitives plus scoring, event/result models and the RAM event repository.
- `src/input/` adapts active-low buttons, the reverse level, and
  interrupt-driven pulses. Button CHANGE interrupts timestamp edges into small
  per-button queues; debounce, short/long-press interpretation and application
  calls stay outside the ISR. `ButtonInterpreter` and the 20 ms
  continuous-level `StableSignalFilter` are native-testable.
- `src/ports/ArduinoClock.h` is the `millis()`/`micros()` adapter.
- `include/board/` selects one GPIO/display profile from the PlatformIO build
  definition. Missing or conflicting profile definitions stop compilation.
- `src/settings/` is the only Preferences/NVS adapter. It stores calibration,
  text color, backlight brightness, display-label visibility, competition
  parameters and the versionable Debug bitmask, never wall-clock time.
- `src/ui/DisplayPort.h` is the display boundary. `DisplayView` owns TFT_eSPI,
  controller initialization, backlight and sprite rendering, while
  `DisplayLayout.*` provides native-testable 320x170 and 480x320 geometry.
  The ILI9488 sprite requests PSRAM and handles allocation failure without
  dereferencing a null buffer. The 480x320 profile uses cropped sprite pushes:
  unchanged periodic frames produce no SPI transfer, basic-view fields update
  independently, and an unchanged menu page moves only its narrow selection
  marker. The external ILI9488 SPI clock is 40 MHz.
  LilyGO normally requests basic/competition-view updates every 100 ms. The
  ILI9488 profile requests them every 50 ms to make pulse-quantized Trip 1 motion
  more even, while still transferring only changed cropped regions and avoiding
  a full PSRAM-sprite clear for an ordinary trip-only update. In competition
  mode the current segment, delta, next segment, and debug row are invalidated
  independently; AT's internal travelled-distance updates do not trigger a
  transfer because that value is not drawn in the basic view.
- `src/main.cpp` wires the adapters and keeps pulse, button, speed, clock, save,
  and display work non-blocking.

## Build and native tests

Run deterministic hardware-independent tests:

```powershell
platformio test -e native
```

Compile the hardware-independent core as a standalone smoke build:

```powershell
platformio run -e native
```

Build firmware without uploading it:

```powershell
platformio run -e lilygo-main
platformio run -e esp32s3-ili9488-main
```

`lilygo-t-display-s3` remains as a compatibility alias for `lilygo-main`.

The native tests use a controlled time source and no wall clock, sleeps,
display, NVS, or board. They cover clock validation/entry/acceptance, restart,
minute/hour/day transitions, multi-day running and rollover; basic DisplayModel
contents; menu opening, wrapping, clamping, scrolling, disabled items and
return paths; clock editing; calibration cancel/accept/failure/write requests;
diagnostics; independent trip resets; Point/AT timing and cancellation;
signed forward/reverse distance; MITTIS calibration; every JAT type and start
proposal; scoring; runtime overrides; result/event browsing; reverse-level
filtering; pulses in every relevant UI state; motion math, saturation, debounce,
speed timeout, and the generator cycle.

## Pulse generator test cycle

With `1000 mm/pulse`, the ESP32-C3 generator's 120-second cycle is:

| Stage | Pulse period | Pulses | Expected speed | Distance |
|---|---:|---:|---:|---:|
| 30 s pulses | 100000 us | 300 | 36 km/h | 300 m |
| 10 s pause | - | 0 | 0 km/h after timeout | 0 m |
| 30 s pulses | 60000 us | 500 | 60 km/h | 500 m |
| 10 s pause | - | 0 | 0 km/h after timeout | 0 m |
| 30 s pulses | 30000 us | 1000 | 120 km/h | 1000 m |
| 10 s pause | - | 0 | 0 km/h after timeout | 0 m |

One cycle produces 1800 accepted pulses and 1800 metres in both unreset trips.
Pauses do not increase distance and speed returns to zero one second after the
last pulse.

## Calibration and NVS

Calibration is stored in millimetres per pulse. The default is `1000`, the step
is `1`, and the validated technical range is `1...100000`. A change affects
only future pulses. The existing Preferences namespace and keys remain
`askompu`, `schemaVersion`, and `mmPerPulseFixed`. Missing, incompatible, or
out-of-range storage falls back safely to `1000 mm/pulse`.

The hardware pulse interrupt rejects edges that would imply more than
200 km/h. Its minimum accepted pulse interval is recalculated from the active
millimetres-per-pulse calibration both at startup and after an accepted
calibration change.

The same settings repository validates `lateFactor` (default 1), `earlyFactor`
(default 3), `jatResultSeconds` (0...20, default 5) and
`atDisplayDistanceM` (1...20, default 10). Writes compare stored values and
only update changed accepted fields.

Debug display selection is stored as the `debugDisplay` `uint16_t` bitmask.
The first known bit is speed. Unknown bits are removed on load, an unchanged
accepted value causes no write, and a failed write leaves the old active value
in use.

## Board and display profiles

`lilygo-main` retains the verified LilyGO ST7789 8-bit parallel display,
320x170 logical landscape resolution, display GPIOs and all existing functional
GPIOs. Its main-computer inputs are Left 1, Up 2, Down 3, Right 10, Trip 2 reset
11, AT 12, reverse 13, Point 14 and speed pulse 16. This profile has no physical
Trip 1 or foot-reset input.

`esp32s3-ili9488-main` uses an ESP32-S3 N16R8 manifest with 16 MB QD flash and
8 MB OPI PSRAM. Its active-low pull-up inputs are:

| Function | GPIO |
|---|---:|
| Left / Up / Down / Right | 4 / 5 / 6 / 7 |
| Active-low light switch | 8 |
| Point / AT | 15 / 16 |
| Internal trip reset | 17 |
| External trip resets 1 / 2 | 42 / 39 |
| Speed pulse / active-low reverse | 40 / 41 |
| External trip TX / RX | 47 / 48 (reserved only) |

The ILI9488 cable bundle, starting from the display module's VCC pin, is:

| Wire | TFT signal | ESP32-S3 connection |
|---|---|---:|
| orange | VCC | 5V |
| yellow | GND | GND |
| green | CS | GPIO14 |
| blue | RESET | GPIO13 |
| violet | DC/RS | GPIO12 |
| grey | SDI/MOSI | GPIO11 |
| white | SCK | GPIO10 |
| black | LED | GPIO9 |

The display SDO/MISO pin is not connected. TFT_eSPI uses GPIO0 internally as
an unconnected dummy MISO to avoid its ESP32-S3 `MISO=-1`/MOSI alias; no wire
is added to GPIO0. With the ESP32-S3 board antenna at
the top and USB-C connectors at the bottom, the left-edge order is GPIO9 LED,
GPIO10 SCK, GPIO11 MOSI, GPIO12 DC/RS, GPIO13 RESET, GPIO14 CS, 5V VCC and GND.
Only the VCC and GND wires cross relative to the display module pin order.

The four adjacent replacement inputs are GPIO39 external reset 2,
GPIO40 speed pulse, GPIO41 active-low reverse and GPIO42 external reset 1. Both external
reset inputs are active LOW with internal pull-ups and reset the trip selected
by `TRIPIT > ULK.NOLLAUS`. The internal GPIO17 reset button uses the separately
selected `SIS.NOLLAUS` target.
The GPIO40 speed source is also active LOW: firmware counts its fast falling
edge so the external pull-up/RC network's slower rising edge cannot create
timing jitter.
GPIO8 is the active-low light-switch input. Closing it to GND enables GPIO9
PWM for both the display backlight and the keyboard-LED transistor described
by the prototype wiring; opening it turns both light loads off while the ESP32,
clock, pulses and competition calculation continue running. The input uses a
20 ms stability filter in addition to the external pull-up/RC circuit.
GPIO1, GPIO2, GPIO18, GPIO21 and GPIO38 are deliberately left free by this
profile. GPIO19 and GPIO20 are reserved for native USB D-/D+;
GPIO35-37 remain unavailable because of the N16R8 Octal PSRAM.

The touch pins (`T_IRQ`, `T_DO`, `T_DIN`, `T_CS`, `T_CLK`) and all SD-card
pins are intentionally unused. The controller is selected entirely by the
PlatformIO profile; a later verified ST7796S module can therefore change driver
flags without modifying core or layout code.

GPIO19 and GPIO20 remain available for native USB D-/D+. Hardware USB CDC/JTAG
and USB CDC on boot are enabled, while USB MSC and USB DFU remain disabled.
The left-hand native USB connector can therefore be used for programming and
the firmware's `Serial` diagnostics. The right-hand CH343P USB-UART connector
remains available for esptool uploads through UART0 on GPIO43 TX and GPIO44 RX,
but normal firmware diagnostics are directed to native USB CDC.

## Physical ILI9488 verification checklist

1. With both USB-C cables disconnected, continuity-check the wire colors,
   GPIO9-14 order, crossed VCC/GND pair and unconnected SDO/MISO.
2. Confirm that the left-hand native USB-C port enumerates with GPIO19/20 free,
   and verify that uploading and serial diagnostics work. The right-hand
   CH343P USB-UART port remains an alternative.
3. Confirm the module really contains an ILI9488 and verify 480x320 landscape
   orientation, RGB order, inversion and full-screen
   refresh without tearing or a persistent blank screen.
4. Verify that grounding GPIO8 enables GPIO9 PWM and opening GPIO8 disables
   the display backlight and keyboard LEDs without stopping the clock or pulse
   calculation.
5. Exercise every top-row value and `-1234`, `0`, `+1234` delta values; confirm
   fixed positions, no clipping and no overlap at both short and long values.
6. Verify TIME, SPEED, MITTIS, JAT, AT, finish, result, event, menu and editing
   views, including a missing next segment.
7. Open `JARJESTELMA > DEBUG > NOPEUS`, change it with Up/Down, accept with
   Right, reboot, and confirm persistence and
   that the bottom debug region never covers competition values.
8. Using the shared GND, exercise GPIO40 speed pulses, GPIO41 reverse, and both
   GPIO39/GPIO42 external reset inputs. Verify both external and GPIO17 internal
   reset target selections for Trip 1 and Trip 2.
9. Confirm GPIO1, GPIO2, GPIO18, GPIO21 and GPIO38 remain unused, while the left
   native USB port enumerates without disturbing the inputs.
10. Run long enough to detect SPI integrity, PSRAM allocation, thermal or power
   issues. Touch, SD and the reserved external-trip UART are outside this test.

## Confirmed LilyGO wiring and missing hardware

| Function | GPIO |
|---|---:|
| Left | 1 |
| Up | 2 |
| Down | 3 |
| Right | 10 |
| Trip 2 reset | 11 |
| AT | 12 |
| Reverse level, active LOW | 13 |
| Point, LilyGO board button | 14 |
| Speed/distance pulse | 16 |

All buttons use active LOW with internal pull-ups. Reverse is a continuous
active-LOW input with a 20 ms stability filter; its stable value is sampled
before each pulse batch. Point long-press starts at 1200 ms without repeat, and
its release cannot also create a short point. Trip 1 reset and foot reset are
not physically connected in this profile. RTC hardware, external trip display
and persistent crash-safe runtime/event storage remain unavailable. Trip 1 is
reset automatically at every JAT and again at the accepted MLA/ULA physical
start point. Trip 2 is never reset by JAT automation.

## Physical verification checklist

1. On every power-up, confirm the device remains in H:MM entry until Right is
   pressed on the minute field; accept `0:00`, `7:05`, `12:30`, and `23:59`.
2. Confirm acceptance starts at exactly `H:MM:00`, the clock advances, and a
   power cycle asks again rather than restoring time from NVS.
3. Verify all three persistent menu font sizes and the Trip 1, Trip 2 and
   side-by-side Trip 1+2 display modes. Confirm that the clock remains at the
   bottom right and no stray small Trip 1 marker appears.
4. Confirm Down opens the first wiki menu item, Up opens the last, the main menu
   wraps, long lists scroll, disabled rows cannot be activated, and Left returns.
5. Edit time from the menu: cancel must preserve it; accept must reset seconds
   to `00` and continue from the acceptance instant.
6. Open calibration, test cancel, unchanged accept, changed successful save,
   failed-save indication, and persistence after a power cycle.
7. Press GPIO11 and verify only Trip 2 resets. Confirm GPIO14 never resets or
   pauses Trip 1 accumulation.
8. Open diagnostics and exercise Point, AT, Trip 2, and reverse. Verify live states,
   pulse/trip/calibration/speed/clock data, clean Left exit, and no state reset.
9. Run the full generator cycle: observe 36/60/120 km/h, zero in each pause,
   no pause distance, and exactly 1800 pulses / 1.800 km when trips are unreset.
10. While startup entry, menu, calibration, and diagnostics are visible, verify
    pulses continue accumulating and speed/clock continue updating.
11. Set display brightness through 10...100%, select every text color, toggle
    labels, and confirm that disabling labels removes all footer instructions.
    When enabled, verify Left, Up/Down, and Right instructions appear in that
    physical order. Inspect viewing angle, clipping, sprite refresh, contrast,
    and flicker for the full 120-second run.
12. Verify the two boards share GND and 3.3 V logic only; with separate USB
    supplies, do not connect their 5 V pins.
13. During a pulse run, pull GPIO13 LOW and verify trip and competition distance
    decrease while displayed speed remains positive; release it and verify
    distance increases without losing a pulse batch.
14. Verify a short GPIO14 press advances one point on release and a press longer
    than 1200 ms opens `LISAMAARAYS` / `TIEKATKO` without creating a point.
15. Drive a MITTIS interval, verify the old/new factor prompt, reject once,
    accept once, power-cycle, and confirm only the accepted factor persists.
16. Verify AT shows the press time, a second AT within three seconds cancels it,
    and the overlay disappears only after one second at at most 3.6 km/h plus
    the configured absolute travel distance in either direction.
17. Exercise every JAT type, including result timeout/Right skip, minute editing,
    EMIT offset acceptance with Point, MLA/ULA distance exclusion and a start
    time crossing midnight.
18. Open stage points, total points and event history; verify cancelled events
    are marked and finish keeps the current frozen clock/delta presentation.

Firmware upload and serial-port testing are intentionally outside the automated
workflow.
