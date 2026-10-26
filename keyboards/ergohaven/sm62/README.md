# Space Mission 62 — prototype target

Local QMK/Vial target for the split SM62 prototype, ported from committed source `8797193afd69689b4130b2525118760103531763` onto upstream `05ec665ecf879b3eee4362d966980365bbd34663` with scoped shared integration. The existing GPIOs below are PROTOTYPE ONLY; actual PCB pin reassignment is deferred until a separate hardware instruction. No compatibility with that different PCB matrix is claimed.

The normal `ergohaven/sm62:v1` prototype target enables the shared application-layout engine (`EH_APP_LAYOUT_ENABLE`) with version-8/62-control geometry, split main-loop publication and effective-keymap display integration. Build it normally without an `EXTRAFLAGS` enabling override. This is a prototype-only integration: native source-backed tests and MCU/UF2 verification do not certify physical hardware or host lifecycle acceptance, and actual-PCB GPIO reassignment remains deferred.

## Hardware and build

- RP2040 on both halves; 5×7 matrix per half, `COL2ROW`; 62 physical keys.
- Rows: GP28, GP19, GP18, GP17, GP16. Columns: GP21, GP22, GP15, GP24, GP20, GP26, GP27. Physical/matrix mapping is authoritative in `keyboard.json`.
- Split UART full duplex: TX GP0, RX GP1. Handedness GP29, high=left, low=right.
- ST7789 240×280: SCK GP2, MOSI GP3, RST GP4, DC GP5, CS GP6, backlight GP7; rotation 0.
- No RGB or encoders. `DEBOUNCE=10`, `asym_eager_defer_pk`.
- One common UF2 is flashed to both halves. QMK reads `SPLIT_HAND_PIN` (GP29): high=LEFT, low=RIGHT, independently of the USB master. Build once with `make -j8 ergohaven/sm62:v1 SKIP_GIT=yes` from a clean scratch source copy; do not define `SM62_FIRMWARE_SIDE`. This builds but does not flash. The EEPROM layout and per-half wire/settings indices remain unchanged.

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

## Stable integration boundaries

The current upstream's other keyboards, runtime app layouts, startup/background/pictogram transfers, display colors/date controls, USB wake hooks and configurable LVGL buffer divisor are retained. SM62-specific date/layout/time parsing, settings storage, painter startup/power, frame diagnostics and task throttling are selected only for this target; unrelated devices retain their upstream behavior. Firmware version definitions, Vial UID and existing settings protocol remain unchanged.

Host C/LVGL tests and UF2/ELF verification are not physical hardware validation. No build command here flashes a device.
