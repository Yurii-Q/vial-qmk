# Macropad display-settings regressions

Run from the repository root with Python 3 and a native C compiler:

```sh
python3 tests/ergohaven_display/run.py all
python3 tests/ergohaven_display/run.py all --source-ref d0e571904ea3321c9938e1cb2ad8ef1e04aae4ec
python3 tests/ergohaven_app_combo.py
```

The display runner compiles the actual `rev3.c` with only hardware includes replaced. EEPROM and flash storage, display calls and timers are stubbed; this is not physical-device validation. The Combo runner includes the keyboard pre-process hook, HID layout implementation and full QMK Combo engine; downstream action handling and keymap fallback are mocked.

## Clock-color persistence

Settings retain their 35-byte layout and `0xDD` marker. Bits 3–5 of the obsolete `0xC7` marker byte (byte 8) persist independent clock text, info and modifier colors. Explicit writes detach an element even when its RGB value equals the accent. Reset restores accent-following defaults.

Existing DD records with unequal clock/accent colors infer independence on load. If an old record contains equal colors, the original user's intent cannot be recovered; it retains accent-following behavior. Current DD records are not palette-normalized. Supported older formats converge through the same stock-palette migration while preserving nondefault custom colors and unrelated settings.

Downgrading to older firmware can erase the new independence flags when it writes settings. Pre-DD firmware is not guaranteed to recognize flagged records. Exact legacy C7 validation remains unchanged rather than accepting arbitrary masked markers.

## Physical smoke test

1. Configure Entropy layer 0 controls as 7/8 and their Vial Combo as MO(1); set the same layer 1 controls to A/B. Hold the Combo longer than COMBO_TERM and release both keys. Layer 0 must return, with no stuck Combo.
2. Set independent clock text/info/modifier colors to (10,177,139), starting with accent (209,177,139). Set accent to (10,20,30). Custom colors must remain (10,177,139), including after reboot.
3. Upgrade a device with supported old stock display settings without resetting storage. Verify the new (209,177,139) stock palette; repeat with nondefault custom colors and verify they survive.
