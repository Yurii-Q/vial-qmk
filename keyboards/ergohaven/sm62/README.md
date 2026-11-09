# Space Mission 62 — production PCB, 4 MiB bring-up

Local QMK/Vial target for the confirmed split SM62 production PCB with 4 MiB flash. Derived from the TEST23 prototype checkpoint (original port from `8797193afd69689b4130b2525118760103531763` onto upstream `05ec665ecf879b3eee4362d966980365bbd34663`). This bring-up changes physical matrix GPIOs and flash geometry only. It is not a firmware target for the old 2 MiB Pico prototype.

The normal `ergohaven/sm62:v1` target retains the shared application-layout engine (`EH_APP_LAYOUT_ENABLE`) with version-8/62-control geometry, split publication and display integration. Build normally without an enabling override. The existing three-profile cache remains in this bring-up; up-to-100-layout persistent storage is a separate unfinished successor. The reported static Ship symptom is not repaired by this GPIO/flash port. Native tests and MCU/UF2 verification do not certify physical hardware acceptance.

## Hardware and build

- RP2040 on both halves; 5×7 matrix per half, `COL2ROW`; 62 physical keys.
- Rows: GP28, GP19, GP18, GP17, GP16. Columns: GP21, GP22, GP23, GP24, GP25, GP26, GP27. Physical/matrix mapping is authoritative in `keyboard.json`; all 62 logical key coordinates and 16 layers are unchanged.
- Split UART full duplex: TX GP0, RX GP1. Handedness GP29, high=left, low=right.
- ST7789 240×280: SCK GP2, MOSI GP3, RST GP4, DC GP5, CS GP6, backlight GP7; rotation 0.
- No RGB or encoders. `DEBOUNCE=10`, `asym_eager_defer_pk`.
- One common UF2 targets both halves. QMK reads `SPLIT_HAND_PIN` (GP29): high=LEFT, low=RIGHT, independently of the USB master. Build once with `make -j4 ergohaven/sm62:v1 SKIP_GIT=yes`; do not define `SM62_FIRMWARE_SIDE`. This builds but does not flash.
- Physical flash is explicitly 4194304 bytes. EEPROM backing uses `[0x3fc000,0x400000)`, extension `[0x3fa000,0x3fc000)`, and the existing 32 KiB cache `[0x3f2000,0x3fa000)`. Prototype settings are not migrated or preserved by this new map. Logical settings formats/indices remain unchanged. Code keeps the conservative `0x1f2000` ceiling; the intervening upper-flash space is not yet a 100-layout store.

## Accepted display and persistent settings

Each half retains its independent mode (Clock, Player, Ship, Keymap), idle-ship enable/seconds, layout-on-keypress enable/hold, Cyrillic key legends, date, RU/EN visibility, stars and persistent screens-off. Defaults are LEFT Clock and RIGHT Ship; factory keymap hold is **300 ms**. Valid saved current-version values remain unchanged, including custom hold and idle values. The packed SM62 EEPROM block remains **18 bytes, version 5**, after the existing shared LCD block. QSID **230–249** and shared brightness/timeout **318–319** retain their wire meanings; sync uses deferred split RPC.

- Persistent off gates normal wake and panel power, repeatedly suppresses the LCD backlight without deleting saved brightness, and keeps HID/settings/mode-transition leases serviced. Re-enable restores the current view without restarting permanent Ship.
- Boot rendering waits for split/role stabilization; painter initialization retries reuse the device handle. All views live beneath a persistent LVGL root. Frame completion releases pending mode/animation state.
- Keypress focus is the accepted physical-mask-aware 3×3 grid with compact service legends, Cyrillic glyphs, rounded internal junctions and accepted fill/colors. Held keys stay filled. Temporary release fade is `max(1, local_hold_ms/400) * 96 ms` using the accepted 200 ms reference; permanent Keymap always uses 96 ms. Additional uncolored focus hold is 70 ms. Expired off-viewport flags cannot revive when settings increase.
- MAIN momentary NAV is `MO(1)` at matrix **[4][0]**. Saved legacy factory maps migrate only after existing PCB/thumb/legend migrations and only when the **entire** base map matches factory defaults with the one recognized old TG binding. Custom base maps and every upper layer remain intact.
- Unsynchronized clock retains the original Unbounded `SM:62`, centered by its actual glyph bounds. Every valid live time keeps the original hour/colon/minute coordinates. Date and language are independently optional.
- RU/EN requires an actual layout report; time/media activity cannot authenticate it. Layout lease is 15 seconds; explicit layout value 255 clears it. Deferred relay never replays an already delivered layout on mode changes and drops expired pending layouts.
- Player keeps the accepted fonts, geometry and fade masks. Long titles move seamlessly at 18 px/s using repeated text; identical snapshots and forced re-entry preserve phase. Title and artist independently retain prior nonempty metadata for **1500 ms** during transient empty snapshots without extending repeated-empty deadlines. Generic HID inactivity bypasses retention; its accepted lease is 61 seconds.
- Clock, player and focus share the accepted starfield; Ship uses the original Orbit framebuffer/cadence and data assets.

## TEST28: future complete-application space reservation only

Based on accepted TEST27 PCB 4 MiB source. `layout_reserve.h` names the unused
flash-offset interval `[0x200000,0x2ca000)` (827392 bytes): **100 complete
application records plus one copy-on-write spare**, 8192 bytes each. A current
maximum FULL application payload (4560 bytes: 16 × 62 key bindings, visuals,
names and encoder stacks), a reserved 256-byte header budget and a separate
256-byte commit-page budget fit in each record. These budgets are not a new
wire format. Default remains outside this future store in native EEPROM.

This is **not functional 100-layout storage**: runtime/advertised cache capacity
stays three logical slots/four physical records, with unchanged addresses and
formats. No storage parser, new protocol, dummy profiles, RAM arrays, boot
initialization, erase, migration or zero-fill is added. Existing cache,
extension and EEPROM writers remain outside the reservation; TEST27 settings
are retained. The earlier prototype-map migration warning above does not mean
TEST28 migrates accepted TEST27 settings.

The production cache backend compiles the geometry assertions with the actual
4 MiB flag. SM62 rules share the unchanged conservative code limit `0x1f2000`
between C and the linker. SM62-only `layout_reserve.ld` rejects a code limit
crossing the reservation and exposes absolute XIP markers
`__sm62_layout_reserve_start=0x10200000` and
`__sm62_layout_reserve_end=0x102ca000`; it allocates no section or flash bytes.
No global linker script is modified.

Focused native check (hardware/flash/IRQ/rendezvous functions are mocked, real
production backend is compiled with `-Wall -Wextra -Werror`):
`python tests/ergohaven/run_sm62_layout_reserve.py /path/to/approved/scratch`.
MCU build, final ELF/UF2 verification and physical acceptance are separate gates.
The split flash/reset freeze is deferred and not changed by this reservation.

## Stable integration boundaries

The current upstream's other keyboards, runtime app layouts, startup/background/pictogram transfers, display colors/date controls, USB wake hooks and configurable LVGL buffer divisor are retained. SM62-specific date/layout/time parsing, settings storage, painter startup/power, frame diagnostics and task throttling are selected only for this target; unrelated devices retain their upstream behavior. Firmware version definitions, Vial UID and existing settings protocol remain unchanged.

Host C/LVGL tests and UF2/ELF verification are not physical hardware validation. No build command here flashes a device.
