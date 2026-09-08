#include "eh_display.h"
#include <lvgl.h>

#include "ergohaven.h"
#include "lvgl_helpers.h"
#include "hid.h"

static lv_obj_t *screen_volume;
static lv_obj_t *arc_volume;
static lv_obj_t *label_volume_arc;

#define VOLUME_ANIMATION_MS 140
#define VOLUME_ENCODER_STEP 5

static int16_t  displayed_volume = -1;
static int16_t  preview_volume   = -1;
static uint8_t  previous_volume  = UINT8_MAX;
static uint32_t encoder_timer    = 0;

void screen_volume_apply_accent_color(void) {
    if (arc_volume == NULL) return;

    lv_obj_set_style_arc_color(arc_volume, accent_color_blue, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(arc_volume, accent_color_blue, LV_PART_KNOB);
}

static void set_displayed_volume(void *obj, int32_t value) {
    displayed_volume = value;
    lv_arc_set_value(obj, value);
    lv_label_set_text_fmt(label_volume_arc, "%02ld", value);
}

static void animate_volume_to(uint8_t target) {
    int32_t start = displayed_volume < 0 ? target : displayed_volume;
    lv_anim_del(arc_volume, set_displayed_volume);

    if (start == target) {
        set_displayed_volume(arc_volume, target);
        return;
    }

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, arc_volume);
    lv_anim_set_exec_cb(&animation, set_displayed_volume);
    lv_anim_set_time(&animation, VOLUME_ANIMATION_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_values(&animation, start, target);
    lv_anim_start(&animation);
}

void screen_volume_process_encoder_event(bool increase) {
    hid_data_t *hid = get_hid_data();
    int16_t base = preview_volume;
    if (base < 0 || timer_elapsed32(encoder_timer) > EH_DISPLAY_TIMEOUT_VOLUME_SCREEN) {
        base = hid->volume;
    }

    int16_t target = base + (increase ? VOLUME_ENCODER_STEP : -VOLUME_ENCODER_STEP);
    target = MAX(0, MIN(100, target));
    previous_volume = hid->volume;
    preview_volume  = target;
    encoder_timer   = timer_read32();
    animate_volume_to(target);
}

void screen_volume_init(void) {
    screen_volume = lv_obj_create(NULL);
    lv_obj_add_style(screen_volume, &style_screen, 0);

    arc_volume = lv_arc_create(screen_volume);
    lv_obj_set_size(arc_volume, 200, 200);
    lv_arc_set_range(arc_volume, 0, 100);
    lv_obj_center(arc_volume);
    screen_volume_apply_accent_color();

    label_volume_arc = lv_label_create(screen_volume);
    lv_label_set_text(label_volume_arc, "00");
    lv_obj_set_style_text_font(label_volume_arc, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_center(label_volume_arc);

    lv_obj_t *volume_text_label = lv_label_create(screen_volume);
    lv_label_set_text(volume_text_label, "Volume");
    lv_obj_align(volume_text_label, LV_ALIGN_BOTTOM_MID, 0, -50);
}

void screen_volume_load(void) {
    if (displayed_volume < 0) {
        uint8_t volume = get_hid_data()->volume;
        preview_volume = volume;
        set_displayed_volume(arc_volume, volume);
    }
    lv_scr_load(screen_volume);
}

void screen_volume_housekeep(void) {
    hid_data_t *hid = get_hid_data();
    if (hid->volume != previous_volume) {
        previous_volume = hid->volume;
        preview_volume  = hid->volume;
        animate_volume_to(hid->volume);
    }
    hid->volume_changed = false;
}

const eh_screen_t eh_screen_volume = {
    .init      = screen_volume_init,
    .load      = screen_volume_load,
    .housekeep = screen_volume_housekeep,
};
