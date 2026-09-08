#include "display.h"
#include "qp.h"
#include "src/eh_ruen.h"
#include "src/eh_settings.h"
#include "hid.h"
#include "ergohaven.h"
#include "src/display/eh_symbols.h"
#include "src/display/eh_display.h"

extern const eh_screen_t eh_screen_layout;
void screen_layout_process_keyevent(uint8_t row, uint8_t col, bool pressed);
void screen_layout_process_encoder_event(uint8_t index, bool clockwise);
void screen_layout_apply_accent_color(void);
void screen_layout_apply_background_color(void);
void screen_layout_apply_button_style(uint8_t style);
bool screen_layout_has_active_key_animation(void);
void screen_volume_process_encoder_event(bool increase);
void screen_volume_apply_accent_color(void);
void screen_home_apply_clock_settings(void);
void last_matrix_activity_trigger(void);
uint32_t last_matrix_activity_time(void);

static uint32_t screen_timer             = 0;
static uint32_t last_user_activity_timer = 0;
static uint32_t encoder_volume_timer     = 0;
static bool     encoder_volume_recent    = false;
static uint32_t handled_matrix_activity  = 0;

#define ENCODER_VOLUME_SYNC_WINDOW_MS 3000

typedef enum {
    SCREEN_OFF = -1,
    SCREEN_SPLASH,
    SCREEN_LAYOUT,
    SCREEN_VOLUME,
    SCREEN_HOME,
} screen_t;

static screen_t screen_state        = SCREEN_OFF;
static screen_t change_screen_state = SCREEN_OFF;

eh_screen_t current_screen;

bool display_should_wake_on_usb_resume(void) {
    return screen_state != SCREEN_OFF;
}

static void apply_screen_state(void) {
    if (change_screen_state == screen_state) return;

    screen_timer = timer_read32();
    screen_state = change_screen_state;
    switch (screen_state) {
        case SCREEN_SPLASH:
            current_screen = eh_screen_splash;
            display_turn_on();
            break;
        case SCREEN_HOME:
            current_screen = eh_screen_home;
            display_turn_on();
            break;
        case SCREEN_LAYOUT:
            current_screen = eh_screen_layout;
            display_turn_on();
            break;
        case SCREEN_VOLUME:
            current_screen = eh_screen_volume;
            display_turn_on();
            break;
        case SCREEN_OFF:
            display_turn_off();
            return;
    }
    current_screen.load();
}

void display_process_matrix_press(void) {
    if (!is_display_enabled()) return;
    last_matrix_activity_trigger();
    last_user_activity_timer = timer_read32();
    change_screen_state      = SCREEN_LAYOUT;
    screen_timer             = last_user_activity_timer;
    apply_screen_state();
}

void display_process_keyevent(uint8_t row, uint8_t col, bool pressed) {
    if (pressed) {
        // Redundant fallback for synthetic/special key events which do not
        // produce a rising edge in the physical matrix scanner.
        display_process_matrix_press();
    }
    screen_layout_process_keyevent(row, col, pressed);
}

static int8_t volume_direction_for_keycode(uint16_t keycode) {
    switch (keycode) {
        case KC_AUDIO_VOL_UP:
        case KC_KB_VOLUME_UP:
            return 1;
        case KC_AUDIO_VOL_DOWN:
        case KC_KB_VOLUME_DOWN:
            return -1;
        default:
            return 0;
    }
}

void display_process_encoder_event(uint8_t index, bool clockwise, uint16_t keycode) {
    if (index != 0) return;
    last_user_activity_timer = timer_read32();

    int8_t volume_direction = volume_direction_for_keycode(keycode);
    if (volume_direction == 0) return;

    if (screen_state == SCREEN_LAYOUT && change_screen_state == SCREEN_LAYOUT) {
        screen_layout_process_encoder_event(index, clockwise);
        return;
    }

    screen_volume_process_encoder_event(volume_direction > 0);
    change_screen_state   = SCREEN_VOLUME;
    screen_timer          = timer_read32();
    encoder_volume_timer  = screen_timer;
    encoder_volume_recent = true;
    apply_screen_state();
}

void display_accent_color_changed_kb(void) {
    last_user_activity_timer = timer_read32();
    screen_layout_apply_accent_color();
    screen_volume_apply_accent_color();
}

void display_background_color_changed_kb(void) {
    last_user_activity_timer = timer_read32();
    screen_layout_apply_background_color();
}

void display_button_style_changed_kb(uint8_t style) {
    last_user_activity_timer = timer_read32();
    screen_layout_apply_button_style(style);
}

void display_clock_settings_changed_kb(void) {
    last_user_activity_timer = timer_read32();
    screen_home_apply_clock_settings();
}

void display_init_screens_kb(void) {
    eh_screen_splash.init();
    eh_screen_layout.init();
    eh_screen_home.init();
    eh_screen_volume.init();
    current_screen      = eh_screen_splash;
    change_screen_state = SCREEN_SPLASH;
    screen_state        = SCREEN_SPLASH;
    current_screen.load();
    display_turn_on();
    screen_timer             = timer_read32();
    last_user_activity_timer = screen_timer;
    handled_matrix_activity  = last_matrix_activity_time();
}

void display_housekeeping_task(void) {
    if (!is_display_enabled()) return;

    // QMK updates this timestamp whenever the physical matrix changes. It is
    // independent of keycode processing, so it also catches layer/tap-dance
    // keys and remains a fallback if pre_process_record_kb is bypassed.
    uint32_t matrix_activity = last_matrix_activity_time();
    bool     matrix_changed  = matrix_activity != handled_matrix_activity;
    if (matrix_changed) handled_matrix_activity = matrix_activity;

    // The normal key callback wakes synchronously. Keep the global matrix
    // timestamp as an independent fallback for events consumed before
    // pre_process_record_kb (or any future special matrix event).
    if (matrix_changed) {
        last_user_activity_timer = timer_read32();
        change_screen_state      = SCREEN_LAYOUT;
        screen_timer             = last_user_activity_timer;
        apply_screen_state();
    }

    bool key_animation_active = screen_state == SCREEN_LAYOUT && screen_layout_has_active_key_animation();
    if (key_animation_active) {
        change_screen_state = SCREEN_LAYOUT;
        screen_timer        = timer_read32();
    }

    static uint8_t prev_layer = 0;
    uint8_t        layer      = get_current_layer();
    if (layer != prev_layer) {
        prev_layer               = layer;
        last_user_activity_timer = timer_read32();
        if (screen_state != SCREEN_OFF) change_screen_state = SCREEN_LAYOUT;
    }

    hid_data_t *hid_data             = get_hid_data();
    bool        hid_active           = is_hid_active();
    uint32_t    user_activity_elapsed = timer_elapsed32(last_user_activity_timer);
    // Host status packets are display data, not user input. In particular,
    // periodic media/volume updates must not wake a powered-down panel.
    if (screen_state != SCREEN_OFF && hid_active && hid_data->hid_changed) {
        if (hid_data->volume_changed) {
            if (screen_state == SCREEN_LAYOUT || change_screen_state == SCREEN_LAYOUT) {
                hid_data->volume_changed = false;
                encoder_volume_recent    = false;
            } else if (screen_state == SCREEN_VOLUME || change_screen_state == SCREEN_VOLUME) {
                change_screen_state   = SCREEN_VOLUME;
                screen_timer          = timer_read32();
                encoder_volume_recent = false;
            } else if (encoder_volume_recent && timer_elapsed32(encoder_volume_timer) < ENCODER_VOLUME_SYNC_WINDOW_MS) {
                hid_data->volume_changed = false;
                encoder_volume_recent    = false;
            } else {
                change_screen_state = SCREEN_VOLUME;
                screen_timer        = timer_read32();
            }
        }
        if (!key_animation_active && user_activity_elapsed > EH_DISPLAY_TIMEOUT_ACTIVITY && hid_data->media_artist_changed) {
            change_screen_state = SCREEN_HOME;
        }
        if (!key_animation_active && user_activity_elapsed > EH_DISPLAY_TIMEOUT_ACTIVITY && hid_data->media_title_changed) {
            change_screen_state = SCREEN_HOME;
        }
    }

    if (screen_state == change_screen_state) {
        uint32_t screen_elapsed   = timer_elapsed32(screen_timer);
        uint32_t activity_elapsed = last_input_activity_elapsed();
        uint32_t clock_delay      = get_clock_delay_ms();
        uint32_t display_timeout  = get_lcd_timeout_ms();
        bool     display_expired  = display_timeout > 0 && activity_elapsed > display_timeout && user_activity_elapsed > 500;

        switch (screen_state) {
            case SCREEN_SPLASH:
                if (screen_elapsed > EH_DISPLAY_TIMEOUT_SPLASH_SCREEN) {
                    change_screen_state = SCREEN_LAYOUT;
                }
                break;

            case SCREEN_LAYOUT:
                if (display_expired) {
                    change_screen_state = SCREEN_OFF;
                } else if (clock_delay > 0 && user_activity_elapsed > clock_delay) {
                    change_screen_state = SCREEN_HOME;
                }
                break;

            case SCREEN_HOME:
                if (display_expired && screen_elapsed > 1000) {
                    change_screen_state = SCREEN_OFF;
                }
                break;

            case SCREEN_VOLUME:
                if (screen_elapsed > EH_DISPLAY_TIMEOUT_VOLUME_SCREEN) {
                    change_screen_state = SCREEN_HOME;
                }
                break;

            case SCREEN_OFF:
                break;
        }
    }

    if (change_screen_state != screen_state) {
        apply_screen_state();
        return;
    }

    if (screen_state != SCREEN_OFF) current_screen.housekeep();
}
