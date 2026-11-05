MCU = RP2040
BOARD = GENERIC_RP_RP2040
BOOTLOADER = rp2040
SPLIT_KEYBOARD = yes
SERIAL_DRIVER = vendor
# The full-screen Orbit SPI transfer pauses matrix scanning. Accept a press
# on its first observed scan; debounce the release independently per key.
DEBOUNCE_TYPE = asym_eager_defer_pk

NKRO_ENABLE = yes
BOOTMAGIC_ENABLE = yes
MOUSEKEY_ENABLE = yes
EXTRAKEY_ENABLE = yes
TAP_DANCE_ENABLE = yes
COMBO_ENABLE = yes
KEY_OVERRIDE_ENABLE = yes
DYNAMIC_MACRO_ENABLE = yes
CAPS_WORD_ENABLE = yes
REPEAT_KEY_ENABLE = yes
AUTO_SHIFT_ENABLE = yes
RAW_ENABLE = yes
LTO_ENABLE = no
RGBLIGHT_ENABLE = no
RGB_MATRIX_ENABLE = no
ENCODER_ENABLE = no
BACKLIGHT_ENABLE = yes
BACKLIGHT_DRIVER = pwm

QUANTUM_PAINTER_ENABLE = yes
QUANTUM_PAINTER_DRIVERS += st7789_spi
QUANTUM_PAINTER_LVGL_INTEGRATION = yes
UNICODE_COMMON = yes
UNICODE_ENABLE = yes

SRC += keyboards/ergohaven/ergohaven_main.c
SRC += keyboards/ergohaven/src/eh_ruen.c
SRC += keyboards/ergohaven/hid.c
SRC += keyboards/ergohaven/src/eh_pointing.c
SRC += keyboards/ergohaven/src/eh_settings.c
SRC += keyboards/ergohaven/src/display/eh_display.c
SRC += keyboards/ergohaven/src/display/lvgl_helpers.c
SRC += keyboards/ergohaven/src/display/eh_keycode_str.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_iosevka_28.c
SRC += keyboards/ergohaven/sm30/anim.c
SRC += keyboards/ergohaven/macropad/screen_orbit.c
SRC += keyboards/ergohaven/macropad/sm62_starfield.c
SRC += keyboards/ergohaven/macropad/sm62_layer_04.c
SRC += keyboards/ergohaven/macropad/sm62_font_unbounded_16.c
SRC += keyboards/ergohaven/macropad/sm62_font_unbounded_48.c
SRC += keyboards/ergohaven/src/display/fonts/eh_font_iosevka_18.c
SRC += keyboards/ergohaven/macropad/sm62_font_zen_medium_16.c
SRC += keyboards/ergohaven/macropad/sm62_font_zen_medium_17.c
SRC += keyboards/ergohaven/macropad/sm62_font_zen_medium_20.c
SRC += keyboards/ergohaven/macropad/sm62_font_zen_medium_28.c
SRC += keyboards/ergohaven/macropad/sm62_font_zen_regular_20.c
SRC += sm62_settings.c
SRC += application_display.c
SRC += display_modes.c
SRC += display_input.c
SRC += screen_clock.c
SRC += screen_player.c
SRC += screen_keymap.c

SRC += keyboards/ergohaven/macropad/sm62_font_zen_regular_24.c
SRC += keyboards/ergohaven/macropad/sm62_font_zen_bold_24.c
SRC += player_fonts.c
SRC += sm62_font_cjk_24.c

SRC += vial_extension.c
SRC += vial_extension_flash.c
SRC += flash_gate.c
SRC += flash_guard.c
SRC += render_mailbox.c
SRC += render_exchange.c
SRC += render_owner.c
SRC += dualcore_trace.c
# Dedicated extension sectors precede the unchanged 16KiB QMK backing.
LDFLAGS += -Wl,--defsym=FLASH_LEN=0x1fa000
