# Ergohaven / Vial native regressions

From the repository root, with Python 3 and native GCC (no Python packages,
ARM toolchain, hardware or submodules required):

```sh
python3 tests/ergohaven/run_native.py /absolute/path/to/scratch-output
python3 util/ci_vial_verify_uid.py
```

All generated C, mock headers, executables and `results.txt` go to the explicit
scratch directory. The runner uses `-std=c11 -Wall -Wextra -Werror` and UBSan,
without warning exemptions. These are standalone native hardware-mock tests,
not QMK's GoogleTest firmware-target runner (`test.mk` is intentionally absent).

The runner includes the actual flash translation units. For Vial command/task,
LCD/reset and home-indicator paths, it extracts complete current function bodies
and supplies only peripheral/dependency mocks. Static assertions check that the
matrix/reset/shared-consumer hooks still invoke those owners. Assertions describe
correct behavior, unlike the initial PR review's defect-reproduction probes.

Coverage:

- Exact 3000ms continuous physical unlock hold, delayed first poll, release and
  restart, timer wrap, host cadences from every millisecond to no polling;
  insecure build remains insecure by explicit configuration only
- All seven shared-home indicators, oneshot mods, caps-word, Mac label selection,
  media/activity visibility and no redundant unchanged-state redraws; macropad
  rev2/rev3 explicitly omit these widgets
- Active-screen brightness after volume load, including zero standby/main
  brightness and zero authoritative volume; inactive-volume guard
- Journal validation/debounce/reboot/rollover, legacy erased-field defaults,
  interrupted saves; reset immediately persists all date/standby defaults and
  preserves every byte outside the journal (including uploaded assets and EEPROM)
- Both 2MiB/20-frame and 4MiB/56-frame background mappings, all supported frame
  counts, seven complete CRC uploads, 32/64-byte reports, 16-bit sequence wrap,
  and speed/EEPROM sentinels
- Pictogram single-slot edits/deletion, first/header sector, straddling record,
  distant slot and final slot. Power fails before, halfway through and after
  every erase/program operation; failures also interrupt rollback and reboot
  again. Every unrelated slot and every byte outside the pictogram reservation
  must survive. Full collection upload/CRC rejection, CLEAR cancellation, fresh
  slot initialization and v3 legacy read/reboot without destructive migration

The flash mock enforces erase/page alignment, chip bounds and NOR 1-to-0 writes.
Torn writes model a half-completed operation, not all possible analog failure
patterns. No test claims ARM linking/size, USB driver integration, real LVGL
rendering, physical scan/encoder cadence, watchdog timing or flash endurance.
