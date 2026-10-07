#!/usr/bin/env python3
"""Check every shipped v2/v3 control has a native icon or a one-line digit."""
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
screen = (root / 'keyboards/ergohaven/macropad/screen_layout.c').read_text()
cases = set(re.findall(r'case\s+([A-Z_0-9]+|C\(KC_[A-Z]\))\s*:', screen.split('screen_layout_builtin_key_icon', 1)[1].split('static void screen_layout_set_key_content', 1)[0]))
alias = {
    'KC_MUTE': 'KC_AUDIO_MUTE', 'KC_INS': 'KC_INSERT', 'KC_DEL': 'KC_DELETE',
    'KC_PSCR': 'KC_PRINT_SCREEN',
    'KC_BTN1': 'KC_MS_BTN1', 'KC_BTN2': 'KC_MS_BTN2', 'KC_BTN3': 'KC_MS_BTN3',
    'KC_MS_U': 'KC_MS_UP', 'KC_MS_D': 'KC_MS_DOWN',
    'KC_MS_L': 'KC_MS_LEFT', 'KC_MS_R': 'KC_MS_RIGHT',
    'KC_BRID': 'KC_BRIGHTNESS_DOWN', 'KC_BRIU': 'KC_BRIGHTNESS_UP',
    'KC_CPNL': 'KC_CONTROL_PANEL', 'KC_MYCM': 'KC_MY_COMPUTER',
    'KC_WSCH': 'KC_WWW_SEARCH', 'KC_MPRV': 'KC_MEDIA_PREV_TRACK',
    'KC_MPLY': 'KC_MEDIA_PLAY_PAUSE', 'KC_MNXT': 'KC_MEDIA_NEXT_TRACK',
    'KC_CALC': 'KC_CALCULATOR', 'KC_VOLD': 'KC_AUDIO_VOL_DOWN',
    'KC_VOLU': 'KC_AUDIO_VOL_UP', 'KC_PGDN': 'KC_PAGE_DOWN',
    'KC_PGUP': 'KC_PAGE_UP', 'KC_WH_D': 'KC_MS_WH_DOWN',
    'KC_WH_U': 'KC_MS_WH_UP',
}
for rev in ('v2', 'v3'):
    source = (root / f'keyboards/ergohaven/macropad/keymaps/{rev}/keymap.c').read_text()
    layer_source = source.split('const uint16_t PROGMEM keymaps', 1)[1].split('#ifdef ENCODER_MAP_ENABLE', 1)[0]
    controls = re.findall(r'C\(KC_[A-Z]\)|\b(?:KC_[A-Z0-9_]+|PREVWRD|NEXTWRD|LAYER_PREV|LAYER_NEXT)\b', layer_source)
    encoder = source.split('const uint16_t PROGMEM encoder_map', 1)[1].split('};', 1)[0]
    controls += re.findall(r'\bKC_[A-Z0-9_]+\b', encoder)
    unknown = []
    for control in controls:
        if control in ('LAYER_PREV', 'LAYER_NEXT') or re.fullmatch(r'KC_[0-9]', control):
            continue
        if alias.get(control, control) not in cases:
            unknown.append(control)
    assert not unknown, (rev, unknown)
    print(rev, len(controls), 'controls checked; all non-digit controls have native icons')
