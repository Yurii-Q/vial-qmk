#include "eh_display.h"
#include <lvgl.h>

#include "ergohaven.h"
#include "../eh_settings.h"
#include "lvgl_helpers.h"
#include "hid.h"
#include "eh_symbols.h"
#ifdef EH_STANDBY_BACKGROUND_ENABLE
#    include "eh_background.h"
#endif

LV_FONT_DECLARE(eh_font_montserrat_20);
LV_FONT_DECLARE(eh_font_montserrat_28);
#ifdef EH_CLOCK_FONT_CHOICES_ENABLE
LV_FONT_DECLARE(eh_font_clock_ubuntu_sans_28);
LV_FONT_DECLARE(eh_font_clock_ubuntu_sans_40);
LV_FONT_DECLARE(eh_font_clock_ubuntu_sans_48);
LV_FONT_DECLARE(eh_font_clock_ubuntu_mono_28);
LV_FONT_DECLARE(eh_font_clock_ubuntu_mono_40);
LV_FONT_DECLARE(eh_font_clock_ubuntu_mono_48);
LV_FONT_DECLARE(eh_font_clock_liberation_mono_28);
LV_FONT_DECLARE(eh_font_clock_liberation_mono_40);
LV_FONT_DECLARE(eh_font_clock_liberation_mono_48);
LV_FONT_DECLARE(eh_font_clock_dejavu_sans_28);
LV_FONT_DECLARE(eh_font_clock_dejavu_sans_40);
LV_FONT_DECLARE(eh_font_clock_dejavu_sans_48);
LV_FONT_DECLARE(eh_font_clock_dejavu_serif_28);
LV_FONT_DECLARE(eh_font_clock_dejavu_serif_40);
LV_FONT_DECLARE(eh_font_clock_dejavu_serif_48);
LV_FONT_DECLARE(eh_font_clock_dejavu_mono_28);
LV_FONT_DECLARE(eh_font_clock_dejavu_mono_40);
LV_FONT_DECLARE(eh_font_clock_dejavu_mono_48);
LV_FONT_DECLARE(eh_font_clock_liberation_sans_28);
LV_FONT_DECLARE(eh_font_clock_liberation_sans_40);
LV_FONT_DECLARE(eh_font_clock_liberation_sans_48);
LV_FONT_DECLARE(eh_font_clock_liberation_serif_28);
LV_FONT_DECLARE(eh_font_clock_liberation_serif_40);
LV_FONT_DECLARE(eh_font_clock_liberation_serif_48);
LV_FONT_DECLARE(eh_font_clock_liberation_narrow_28);
LV_FONT_DECLARE(eh_font_clock_liberation_narrow_40);
LV_FONT_DECLARE(eh_font_clock_liberation_narrow_48);
#endif

static lv_obj_t *screen_home;
#ifdef EH_STANDBY_BACKGROUND_ENABLE
static lv_obj_t *standby_background;
static lv_obj_t *standby_background_dim;
#endif
static lv_obj_t *label_product;
static lv_obj_t *label_time;
static lv_obj_t *label_time_colon;
static lv_obj_t *label_time_minutes;
static lv_obj_t *label_layer_icon;
static lv_obj_t *label_layer;
static lv_obj_t *label_mac;
static lv_obj_t *label_layout;
static lv_obj_t *label_shift;
static lv_obj_t *label_ctrl;
static lv_obj_t *label_alt;
static lv_obj_t *label_gui;
static lv_obj_t *label_num;
static lv_obj_t *label_caps;
static lv_obj_t *label_scroll;
static lv_obj_t *label_hid_media_artist;
static lv_obj_t *label_hid_media_title;
static lv_obj_t *screen_home_mods;
static lv_obj_t *screen_home_media;
static lv_obj_t *clock_custom;
static bool      modifiers_context_visible = true;
static uint8_t   clock_hours;
static uint8_t   clock_minutes;
static uint8_t   flip_previous_digits[4];
static uint8_t   flip_changed_mask;
static int32_t   flip_animation_progress = 100;
static bool      clock_time_initialized;
static bool      clock_colon_visible = true;
#ifdef EH_STANDBY_BACKGROUND_ENABLE
static lv_img_dsc_t standby_background_frames[EH_BACKGROUND_MAX_FRAMES];
static uint32_t standby_background_generation = UINT32_MAX;
static uint32_t standby_background_frame_time;
static uint64_t standby_background_frame_elapsed;
static uint16_t standby_background_speed_remainder;
static uint8_t standby_background_frame;
#endif

#define FLIP_ANIMATION_MS 360

static lv_opa_t opacity_from_percent(uint8_t opacity) {
    return (lv_opa_t)((uint16_t)MIN(opacity, 100) * LV_OPA_COVER / 100);
}

#ifdef EH_STANDBY_BACKGROUND_ENABLE
static void refresh_standby_background(void) {
    uint32_t current_generation = eh_background_generation();
    if (standby_background_generation == current_generation) return;
    standby_background_generation = current_generation;
    standby_background_frame = 0;
    standby_background_frame_time = timer_read32();
    standby_background_frame_elapsed = 0;
    standby_background_speed_remainder = 0;

    uint8_t frame_count = eh_background_frame_count();
    bool animation = eh_background_kind() == EH_BACKGROUND_KIND_ANIMATION;
    uint32_t frame_size = animation ? EH_BACKGROUND_ANIMATION_FRAME_SIZE : EH_BACKGROUND_IMAGE_FRAME_SIZE;
    for (uint8_t frame = 0; frame < frame_count; frame++) {
        standby_background_frames[frame] = (lv_img_dsc_t){
            .header.always_zero = 0,
            .header.w = EH_BACKGROUND_WIDTH,
            .header.h = EH_BACKGROUND_HEIGHT,
            .data_size = frame_size,
            .header.cf = animation ? LV_IMG_CF_RAW_ALPHA : LV_IMG_CF_INDEXED_8BIT,
            .data = eh_background_frame_data(frame),
        };
    }
    if (frame_count > 0) lv_img_set_src(standby_background, &standby_background_frames[0]);
    toggle_hidden(standby_background, frame_count > 0);
    toggle_hidden(standby_background_dim, frame_count > 0 && get_clock_background_dim() > 0);
}

static void animate_standby_background(void) {
    uint32_t now = timer_read32();
    uint32_t real_elapsed = TIMER_DIFF_32(now, standby_background_frame_time);
    standby_background_frame_time = now;
    if (eh_background_animation_paused()) return;
    uint8_t frame_count = eh_background_frame_count();
    if (frame_count < 2) return;

    uint64_t scaled = (uint64_t)real_elapsed * eh_background_speed_percent() + standby_background_speed_remainder;
    standby_background_frame_elapsed += scaled / 100u;
    standby_background_speed_remainder = (uint16_t)(scaled % 100u);

    uint32_t cycle_duration = 0;
    for (uint8_t frame = 0; frame < frame_count; frame++) {
        cycle_duration += MAX(1u, (uint32_t)eh_background_frame_delay(frame));
    }
    if (cycle_duration > 0 && standby_background_frame_elapsed >= cycle_duration) {
        standby_background_frame_elapsed %= cycle_duration;
    }

    bool advanced = false;
    for (uint8_t skipped = 0; skipped < frame_count; skipped++) {
        uint32_t delay = MAX(1u, (uint32_t)eh_background_frame_delay(standby_background_frame));
        if (standby_background_frame_elapsed < delay) break;
        standby_background_frame_elapsed -= delay;
        standby_background_frame = (standby_background_frame + 1) % frame_count;
        advanced = true;
    }
    if (advanced) lv_img_set_src(standby_background, &standby_background_frames[standby_background_frame]);
}
#endif

enum {
    CLOCK_STYLE_MODERN,
    CLOCK_STYLE_SEGMENT_STRAIGHT,
    CLOCK_STYLE_SEGMENT_ITALIC,
    CLOCK_STYLE_TUBE,
    CLOCK_STYLE_FLIP,
    CLOCK_STYLE_DOT_MATRIX,
    CLOCK_STYLE_NEON,
};

static void apply_clock_element_visibility(void) {
    bool clock_visible = get_clock_visible();
    toggle_hidden(label_time, clock_visible);
    toggle_hidden(label_time_colon, clock_visible && clock_colon_visible);
    toggle_hidden(label_time_minutes, clock_visible);
    toggle_hidden(clock_custom, false);

    bool info_visible = get_clock_info_visible();
    toggle_hidden(label_layer_icon, info_visible);
    toggle_hidden(label_layer, info_visible);
    toggle_hidden(label_mac, info_visible && split_get_mac());
    toggle_hidden(label_layout, info_visible);

    toggle_hidden(screen_home_mods, get_clock_modifiers_visible() && modifiers_context_visible);
}

static const lv_font_t *clock_font(void) {
#ifdef EH_CLOCK_FONT_CHOICES_ENABLE
    static const lv_font_t *const fonts[10][3] = {
        {&lv_font_montserrat_28, &lv_font_montserrat_40, &lv_font_montserrat_48},
        {&eh_font_clock_ubuntu_sans_28, &eh_font_clock_ubuntu_sans_40, &eh_font_clock_ubuntu_sans_48},
        {&eh_font_clock_ubuntu_mono_28, &eh_font_clock_ubuntu_mono_40, &eh_font_clock_ubuntu_mono_48},
        {&eh_font_clock_liberation_mono_28, &eh_font_clock_liberation_mono_40, &eh_font_clock_liberation_mono_48},
        {&eh_font_clock_dejavu_sans_28, &eh_font_clock_dejavu_sans_40, &eh_font_clock_dejavu_sans_48},
        {&eh_font_clock_dejavu_serif_28, &eh_font_clock_dejavu_serif_40, &eh_font_clock_dejavu_serif_48},
        {&eh_font_clock_dejavu_mono_28, &eh_font_clock_dejavu_mono_40, &eh_font_clock_dejavu_mono_48},
        {&eh_font_clock_liberation_sans_28, &eh_font_clock_liberation_sans_40, &eh_font_clock_liberation_sans_48},
        {&eh_font_clock_liberation_serif_28, &eh_font_clock_liberation_serif_40, &eh_font_clock_liberation_serif_48},
        {&eh_font_clock_liberation_narrow_28, &eh_font_clock_liberation_narrow_40, &eh_font_clock_liberation_narrow_48},
    };
    return fonts[MIN(get_clock_style(), 9)][MIN(get_clock_size(), 2)];
#else
    static const lv_font_t *const fonts[] = {&lv_font_montserrat_28, &lv_font_montserrat_40, &lv_font_montserrat_48};
    return fonts[MIN(get_clock_size(), 2)];
#endif
}

static lv_coord_t clock_height(void) {
    static const lv_coord_t heights[] = {32, 46, 60};
    return heights[MIN(get_clock_size(), 2)];
}

static lv_coord_t clock_aligned_x(const lv_area_t *area, lv_coord_t width) {
    switch (get_clock_alignment()) {
        case 0:
            return area->x1 + 8;
        case 2:
            return area->x2 - width - 8;
        default:
            return area->x1 + (lv_area_get_width(area) - width) / 2;
    }
}

static lv_coord_t clock_text_width(const char *text, const lv_font_t *font) {
    return lv_txt_get_width(text, strlen(text), font, 0, LV_TEXT_FLAG_NONE);
}

static void position_clock_labels(void) {
    const lv_font_t *font = clock_font();
    lv_coord_t widest_digit = 0;
    for (char digit = '0'; digit <= '9'; digit++) {
        char text[2] = {digit, '\0'};
        widest_digit = MAX(widest_digit, clock_text_width(text, font));
    }
    lv_coord_t pair_width  = widest_digit * 2;
    lv_coord_t colon_width = clock_text_width(":", font);
    lv_coord_t total_width = pair_width * 2 + colon_width;
    lv_area_t area = {.x1 = 10, .y1 = 20, .x2 = 229, .y2 = 99};
    lv_coord_t x = clock_aligned_x(&area, total_width);
    lv_coord_t y = 36 + (48 - font->line_height) / 2;
    lv_coord_t height = font->line_height + 4;

    lv_obj_set_pos(label_time, x, y);
    lv_obj_set_size(label_time, pair_width, height);
    lv_obj_set_pos(label_time_colon, x + pair_width, y);
    lv_obj_set_size(label_time_colon, colon_width, height);
    lv_obj_set_pos(label_time_minutes, x + pair_width + colon_width, y);
    lv_obj_set_size(label_time_minutes, pair_width, height);
}

static void update_clock_label(void) {
    if (label_time == NULL || label_time_colon == NULL || label_time_minutes == NULL) return;
    lv_label_set_text_fmt(label_time, "%02d", clock_hours);
    lv_label_set_text(label_time_colon, ":");
    lv_label_set_text_fmt(label_time_minutes, "%02d", clock_minutes);
    toggle_hidden(label_time_colon, get_clock_visible() && clock_colon_visible);
}

static void update_clock_colon(lv_timer_t *timer) {
    (void)timer;
    bool visible = get_clock_colon_blink() ? !clock_colon_visible : true;
    if (visible == clock_colon_visible) return;
    clock_colon_visible = visible;
    update_clock_label();
    if (clock_custom != NULL) lv_obj_invalidate(clock_custom);
}

static void draw_clock_dot(lv_draw_ctx_t *draw_ctx, lv_coord_t x, lv_coord_t y, lv_coord_t radius, lv_color_t color, lv_opa_t opa) {
    lv_area_t area = {.x1 = x - radius, .y1 = y - radius, .x2 = x + radius, .y2 = y + radius};
    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init(&dot);
    dot.bg_color = color;
    dot.bg_opa   = opa;
    dot.radius   = LV_RADIUS_CIRCLE;
    dot.border_width = 0;
    lv_draw_rect(draw_ctx, &dot, &area);
}

static lv_coord_t segment_skew(bool italic, lv_coord_t height, lv_coord_t y) {
    return italic ? (height - y) / 7 : 0;
}

static void draw_segment_digit(lv_draw_ctx_t *draw_ctx, uint8_t digit, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height,
                               lv_coord_t line_width, bool italic, bool rounded, lv_color_t color, lv_opa_t opa, bool draw_all) {
    static const uint8_t masks[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    uint8_t mask = draw_all ? 0x7F : masks[digit % 10];
    lv_coord_t pad = MAX(1, line_width / 2);
    lv_coord_t mid = height / 2;
    lv_point_t segments[7][2] = {
        {{x + pad + segment_skew(italic, height, 0), y}, {x + width - pad + segment_skew(italic, height, 0), y}},
        {{x + width + segment_skew(italic, height, pad), y + pad}, {x + width + segment_skew(italic, height, mid - pad), y + mid - pad}},
        {{x + width + segment_skew(italic, height, mid + pad), y + mid + pad}, {x + width + segment_skew(italic, height, height - pad), y + height - pad}},
        {{x + pad + segment_skew(italic, height, height), y + height}, {x + width - pad + segment_skew(italic, height, height), y + height}},
        {{x + segment_skew(italic, height, mid + pad), y + mid + pad}, {x + segment_skew(italic, height, height - pad), y + height - pad}},
        {{x + segment_skew(italic, height, pad), y + pad}, {x + segment_skew(italic, height, mid - pad), y + mid - pad}},
        {{x + pad + segment_skew(italic, height, mid), y + mid}, {x + width - pad + segment_skew(italic, height, mid), y + mid}},
    };
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color       = color;
    line.opa         = opa;
    line.width       = line_width;
    line.round_start = rounded;
    line.round_end   = rounded;
    for (uint8_t segment = 0; segment < 7; segment++) {
        if (mask & (1U << segment)) lv_draw_line(draw_ctx, &line, &segments[segment][0], &segments[segment][1]);
    }
}

static void draw_hex_segment(lv_draw_ctx_t *draw_ctx, lv_point_t start, lv_point_t end, lv_coord_t thickness, bool horizontal,
                             lv_color_t color) {
    lv_coord_t half  = MAX(1, thickness / 2);
    lv_coord_t bevel = half;
    lv_point_t points[6];

    if (horizontal) {
        points[0] = (lv_point_t){.x = start.x + bevel, .y = start.y - half};
        points[1] = (lv_point_t){.x = end.x - bevel, .y = end.y - half};
        points[2] = end;
        points[3] = (lv_point_t){.x = end.x - bevel, .y = end.y + half};
        points[4] = (lv_point_t){.x = start.x + bevel, .y = start.y + half};
        points[5] = start;
    } else {
        points[0] = start;
        points[1] = (lv_point_t){.x = start.x + half, .y = start.y + bevel};
        points[2] = (lv_point_t){.x = end.x + half, .y = end.y - bevel};
        points[3] = end;
        points[4] = (lv_point_t){.x = end.x - half, .y = end.y - bevel};
        points[5] = (lv_point_t){.x = start.x - half, .y = start.y + bevel};
    }

    lv_draw_rect_dsc_t fill;
    lv_draw_rect_dsc_init(&fill);
    fill.bg_color     = color;
    fill.bg_opa       = LV_OPA_COVER;
    fill.border_width = 0;
    lv_draw_polygon(draw_ctx, &fill, points, ARRAY_SIZE(points));
}

static void draw_hex_segment_digit(lv_draw_ctx_t *draw_ctx, uint8_t digit, lv_coord_t x, lv_coord_t y, lv_coord_t width,
                                   lv_coord_t height, bool italic, lv_color_t color) {
    static const uint8_t masks[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    uint8_t mask = masks[digit % 10];
    lv_coord_t thickness = MAX(3, height / 9);
    if ((thickness & 1) == 0) thickness++;

    lv_coord_t half     = thickness / 2;
    lv_coord_t joint    = half + 1;
    lv_coord_t mid      = height / 2;
    lv_coord_t left     = x + half;
    lv_coord_t right    = x + width - half;
    lv_point_t segments[7][2] = {
        {{left + joint + segment_skew(italic, height, 0), y},
         {right - joint + segment_skew(italic, height, 0), y}},
        {{right + segment_skew(italic, height, joint), y + joint},
         {right + segment_skew(italic, height, mid - joint), y + mid - joint}},
        {{right + segment_skew(italic, height, mid + joint), y + mid + joint},
         {right + segment_skew(italic, height, height - joint), y + height - joint}},
        {{left + joint + segment_skew(italic, height, height), y + height},
         {right - joint + segment_skew(italic, height, height), y + height}},
        {{left + segment_skew(italic, height, mid + joint), y + mid + joint},
         {left + segment_skew(italic, height, height - joint), y + height - joint}},
        {{left + segment_skew(italic, height, joint), y + joint},
         {left + segment_skew(italic, height, mid - joint), y + mid - joint}},
        {{left + joint + segment_skew(italic, height, mid), y + mid},
         {right - joint + segment_skew(italic, height, mid), y + mid}},
    };

    for (uint8_t segment = 0; segment < 7; segment++) {
        if (mask & (1U << segment)) {
            bool horizontal = segment == 0 || segment == 3 || segment == 6;
            draw_hex_segment(draw_ctx, segments[segment][0], segments[segment][1], thickness, horizontal, color);
        }
    }
}

static void draw_dot_matrix_digit(lv_draw_ctx_t *draw_ctx, uint8_t digit, lv_coord_t x, lv_coord_t y, lv_coord_t dot, lv_color_t color) {
    static const uint8_t rows[10][7] = {
        {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
        {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E},
        {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E},
        {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
        {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E},
    };
    lv_coord_t step = dot * 2 + 1;
    for (uint8_t row = 0; row < 7; row++) {
        for (uint8_t col = 0; col < 5; col++) {
            if (rows[digit % 10][row] & (1U << (4 - col))) {
                draw_clock_dot(draw_ctx, x + col * step + dot, y + row * step + dot, dot, color, LV_OPA_COVER);
            }
        }
    }
}

static void draw_flip_digit(lv_draw_ctx_t *draw_ctx, uint8_t digit, const lv_area_t *area, const lv_font_t *font, lv_color_t text_color,
                            lv_color_t background_color) {
    lv_draw_rect_dsc_t card;
    lv_draw_rect_dsc_init(&card);
    card.bg_color     = lv_color_mix(text_color, background_color, 22);
    card.bg_opa       = LV_OPA_COVER;
    card.radius       = 5;
    card.border_color = lv_color_mix(text_color, background_color, 90);
    card.border_opa   = LV_OPA_50;
    card.border_width = 1;
    lv_draw_rect(draw_ctx, &card, area);

    lv_draw_line_dsc_t split;
    lv_draw_line_dsc_init(&split);
    split.color = background_color;
    split.opa   = LV_OPA_70;
    split.width = 1;
    lv_point_t split_points[2] = {
        {.x = area->x1 + 2, .y = (area->y1 + area->y2) / 2},
        {.x = area->x2 - 2, .y = (area->y1 + area->y2) / 2},
    };
    lv_draw_line(draw_ctx, &split, &split_points[0], &split_points[1]);

    char text[2] = {(char)('0' + digit), '\0'};
    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init(&label);
    label.color = text_color;
    label.font  = font;
    label.align = LV_TEXT_ALIGN_CENTER;
    lv_area_t text_area = *area;
    text_area.y1 += (lv_area_get_height(area) - font->line_height) / 2;
    lv_draw_label(draw_ctx, &label, &text_area, text, NULL);
}

static void draw_flip_digit_clipped(lv_draw_ctx_t *draw_ctx, uint8_t digit, const lv_area_t *area, const lv_area_t *clip,
                                    const lv_font_t *font, lv_color_t text_color, lv_color_t background_color) {
    lv_area_t clipped;
    if (!_lv_area_intersect(&clipped, draw_ctx->clip_area, clip)) return;

    const lv_area_t *saved_clip = draw_ctx->clip_area;
    draw_ctx->clip_area         = &clipped;
    draw_flip_digit(draw_ctx, digit, area, font, text_color, background_color);
    draw_ctx->clip_area = saved_clip;
}

static void draw_animated_flip_digit(lv_draw_ctx_t *draw_ctx, uint8_t previous_digit, uint8_t digit, const lv_area_t *area,
                                     const lv_font_t *font, lv_color_t text_color, lv_color_t background_color) {
    int32_t progress = MIN(flip_animation_progress, 100);
    lv_coord_t middle = (area->y1 + area->y2) / 2;
    lv_coord_t half_height = middle - area->y1;
    lv_area_t top = {.x1 = area->x1, .y1 = area->y1, .x2 = area->x2, .y2 = middle};

    draw_flip_digit(draw_ctx, previous_digit, area, font, text_color, background_color);
    draw_flip_digit_clipped(draw_ctx, digit, area, &top, font, text_color, background_color);

    if (progress < 50) {
        lv_area_t folding_top = top;
        folding_top.y1 += half_height * progress / 50;
        draw_flip_digit_clipped(draw_ctx, previous_digit, area, &folding_top, font, text_color, background_color);
    } else {
        lv_area_t folding_bottom = {.x1 = area->x1,
                                    .y1 = middle,
                                    .x2 = area->x2,
                                    .y2 = middle + half_height * (progress - 50) / 50};
        draw_flip_digit_clipped(draw_ctx, digit, area, &folding_bottom, font, text_color, background_color);
    }

    lv_draw_rect_dsc_t hinge;
    lv_draw_rect_dsc_init(&hinge);
    hinge.bg_color = background_color;
    hinge.bg_opa   = (lv_opa_t)(LV_OPA_20 + (50 - LV_ABS(50 - progress)) * 2);
    hinge.border_width = 0;
    lv_area_t hinge_area = {.x1 = area->x1 + 2, .y1 = middle - 1, .x2 = area->x2 - 2, .y2 = middle + 1};
    lv_draw_rect(draw_ctx, &hinge, &hinge_area);
}

static void set_flip_animation_progress(void *object, int32_t value) {
    flip_animation_progress = value;
    lv_obj_invalidate(object);
}

static void finish_flip_animation(lv_anim_t *animation) {
    (void)animation;
    flip_animation_progress = 100;
    flip_changed_mask       = 0;
    lv_obj_invalidate(clock_custom);
    lv_obj_invalidate(label_time);
    lv_obj_invalidate(label_time_colon);
    lv_obj_invalidate(label_time_minutes);
}

static void start_flip_animation(uint8_t previous_hours, uint8_t previous_minutes, uint8_t hours, uint8_t minutes) {
    uint8_t previous[4] = {previous_hours / 10, previous_hours % 10, previous_minutes / 10, previous_minutes % 10};
    uint8_t current[4]  = {hours / 10, hours % 10, minutes / 10, minutes % 10};
    flip_changed_mask   = 0;
    for (uint8_t index = 0; index < ARRAY_SIZE(current); index++) {
        flip_previous_digits[index] = previous[index];
        if (previous[index] != current[index]) flip_changed_mask |= (1U << index);
    }
    if (flip_changed_mask == 0) return;

    lv_anim_del(clock_custom, set_flip_animation_progress);
    flip_animation_progress = 0;
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, clock_custom);
    lv_anim_set_exec_cb(&animation, set_flip_animation_progress);
    lv_anim_set_values(&animation, 0, 100);
    lv_anim_set_time(&animation, FLIP_ANIMATION_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    lv_anim_set_ready_cb(&animation, finish_flip_animation);
    lv_anim_start(&animation);
}

static void draw_tube_digit(lv_draw_ctx_t *draw_ctx, uint8_t digit, const lv_area_t *area, const lv_font_t *font, lv_color_t text_color,
                            lv_color_t background_color) {
    lv_draw_rect_dsc_t tube;
    lv_draw_rect_dsc_init(&tube);
    tube.bg_color     = lv_color_mix(text_color, background_color, 12);
    tube.bg_opa       = LV_OPA_30;
    tube.radius       = LV_RADIUS_CIRCLE;
    tube.border_color = text_color;
    tube.border_opa   = LV_OPA_40;
    tube.border_width = 1;
    lv_draw_rect(draw_ctx, &tube, area);

    char text[2] = {(char)('0' + digit), '\0'};
    lv_draw_label_dsc_t glow;
    lv_draw_label_dsc_init(&glow);
    glow.color = text_color;
    glow.opa   = LV_OPA_30;
    glow.font  = font;
    glow.align = LV_TEXT_ALIGN_CENTER;
    lv_area_t glow_area = *area;
    glow_area.x1 -= 1;
    glow_area.x2 += 1;
    glow_area.y1 += (lv_area_get_height(area) - font->line_height) / 2;
    lv_draw_label(draw_ctx, &glow, &glow_area, text, NULL);

    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init(&label);
    label.color = text_color;
    label.font  = font;
    label.align = LV_TEXT_ALIGN_CENTER;
    lv_area_t text_area = *area;
    text_area.y1 += (lv_area_get_height(area) - font->line_height) / 2;
    lv_draw_label(draw_ctx, &label, &text_area, text, NULL);
}

static void draw_clock_event(lv_event_t *event) {
    uint8_t style = get_clock_style();
    if (style == CLOCK_STYLE_MODERN) return;

    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(event);
    lv_area_t area;
    lv_obj_get_coords(clock_custom, &area);
    lv_color_t color = lv_color_make(get_clock_text_red(), get_clock_text_green(), get_clock_text_blue());
    lv_color_t background = lv_color_make(get_clock_background_red(), get_clock_background_green(), get_clock_background_blue());
    uint8_t digits[4] = {clock_hours / 10, clock_hours % 10, clock_minutes / 10, clock_minutes % 10};
    lv_coord_t height = clock_height();
    lv_coord_t y = area.y1 + (lv_area_get_height(&area) - height) / 2;
    lv_coord_t gap = MAX(4, height / 9);

    if (style == CLOCK_STYLE_DOT_MATRIX) {
        lv_coord_t dot = MAX(1, (height - 6) / 15);
        lv_coord_t digit_width = dot * 10 + 4;
        lv_coord_t colon_width = dot * 4;
        lv_coord_t total_width = digit_width * 4 + gap * 4 + colon_width;
        lv_coord_t x = clock_aligned_x(&area, total_width);
        for (uint8_t index = 0; index < 4; index++) {
            draw_dot_matrix_digit(draw_ctx, digits[index], x, y, dot, color);
            x += digit_width + gap;
            if (index == 1) {
                if (clock_colon_visible) {
                    draw_clock_dot(draw_ctx, x + colon_width / 2, y + height / 3, dot, color, LV_OPA_COVER);
                    draw_clock_dot(draw_ctx, x + colon_width / 2, y + height * 2 / 3, dot, color, LV_OPA_COVER);
                }
                x += colon_width + gap;
            }
        }
        return;
    }

    lv_coord_t digit_width = height * 5 / 9;
    lv_coord_t digit_advance = digit_width;
    if (style == CLOCK_STYLE_SEGMENT_ITALIC) digit_advance += segment_skew(true, height, 0);
    if (style == CLOCK_STYLE_NEON) gap += MAX(4, height / 10);
    lv_coord_t colon_width = MAX(5, height / 5);
    lv_coord_t colon_advance = colon_width;
    if (style == CLOCK_STYLE_SEGMENT_ITALIC) colon_advance += segment_skew(true, height, 0);
    lv_coord_t total_width = digit_advance * 4 + gap * 4 + colon_advance;
    lv_coord_t x = clock_aligned_x(&area, total_width);
    const lv_font_t *font = clock_font();
    for (uint8_t index = 0; index < 4; index++) {
        if (style == CLOCK_STYLE_FLIP) {
            lv_area_t card = {.x1 = x, .y1 = y, .x2 = x + digit_width, .y2 = y + height};
            if ((flip_changed_mask & (1U << index)) && flip_animation_progress < 100) {
                draw_animated_flip_digit(draw_ctx, flip_previous_digits[index], digits[index], &card, font, color, background);
            } else {
                draw_flip_digit(draw_ctx, digits[index], &card, font, color, background);
            }
        } else if (style == CLOCK_STYLE_TUBE) {
            lv_area_t tube = {.x1 = x, .y1 = y, .x2 = x + digit_width, .y2 = y + height};
            draw_tube_digit(draw_ctx, digits[index], &tube, font, color, background);
        } else if (style == CLOCK_STYLE_NEON) {
            draw_segment_digit(draw_ctx, digits[index], x, y, digit_width, height, MAX(5, height / 7), false, true, color, LV_OPA_20, false);
            draw_segment_digit(draw_ctx, digits[index], x, y, digit_width, height, MAX(2, height / 18), false, true, color, LV_OPA_COVER, false);
        } else {
            draw_hex_segment_digit(draw_ctx, digits[index], x, y, digit_width, height, style == CLOCK_STYLE_SEGMENT_ITALIC, color);
        }
        x += digit_advance + gap;
        if (index == 1) {
            if (clock_colon_visible) {
                lv_coord_t radius = MAX(1, height / 18);
                bool italic = style == CLOCK_STYLE_SEGMENT_ITALIC;
                lv_coord_t upper_y = height / 3;
                lv_coord_t lower_y = height * 2 / 3;
                draw_clock_dot(draw_ctx, x + colon_width / 2 + segment_skew(italic, height, upper_y), y + upper_y, radius, color,
                               LV_OPA_COVER);
                draw_clock_dot(draw_ctx, x + colon_width / 2 + segment_skew(italic, height, lower_y), y + lower_y, radius, color,
                               LV_OPA_COVER);
            }
            x += colon_advance + gap;
        }
    }
}

void screen_home_apply_clock_settings(void) {
    if (screen_home == NULL) return;
    lv_color_t text_color = lv_color_make(get_clock_text_red(), get_clock_text_green(), get_clock_text_blue());
    lv_color_t background = lv_color_make(get_clock_background_red(), get_clock_background_green(), get_clock_background_blue());
    lv_color_t info_color = lv_color_make(get_clock_info_red(), get_clock_info_green(), get_clock_info_blue());
    lv_color_t modifiers_color = lv_color_make(get_clock_modifiers_red(), get_clock_modifiers_green(), get_clock_modifiers_blue());
    lv_obj_set_style_bg_color(screen_home, background, 0);
#ifdef EH_STANDBY_BACKGROUND_ENABLE
    lv_obj_set_style_bg_opa(standby_background_dim, (lv_opa_t)((uint16_t)get_clock_background_dim() * LV_OPA_COVER / 100), 0);
    toggle_hidden(standby_background_dim, eh_background_is_valid() && get_clock_background_dim() > 0);
#endif
    lv_obj_t *time_labels[] = {label_time, label_time_colon, label_time_minutes};
    for (uint8_t index = 0; index < ARRAY_SIZE(time_labels); index++) {
        lv_obj_set_style_text_color(time_labels[index], text_color, 0);
        lv_obj_set_style_text_font(time_labels[index], clock_font(), LV_PART_MAIN);
        lv_obj_set_style_opa(time_labels[index], opacity_from_percent(get_clock_opacity()), 0);
    }
    lv_obj_set_style_opa(clock_custom, opacity_from_percent(get_clock_opacity()), 0);
    lv_obj_t *info_labels[] = {label_layer_icon, label_layer, label_mac, label_layout};
    for (uint8_t index = 0; index < ARRAY_SIZE(info_labels); index++) {
        lv_obj_set_style_text_color(info_labels[index], info_color, 0);
        lv_obj_set_style_text_opa(info_labels[index], opacity_from_percent(get_clock_info_opacity()), 0);
    }
    lv_obj_set_style_text_color(label_product, info_color, 0);
    lv_obj_set_style_text_color(label_hid_media_title, info_color, 0);
    lv_obj_set_style_text_color(label_hid_media_artist, info_color, 0);
    lv_obj_t *status_labels[] = {label_shift, label_ctrl, label_alt, label_gui, label_num, label_caps, label_scroll};
    for (uint8_t index = 0; index < ARRAY_SIZE(status_labels); index++) {
        lv_obj_set_style_text_color(status_labels[index], modifiers_color, 0);
        lv_obj_set_style_bg_color(status_labels[index], modifiers_color, LV_STATE_PRESSED);
        lv_obj_set_style_text_color(status_labels[index], background, LV_STATE_PRESSED);
        lv_obj_set_style_opa(status_labels[index], opacity_from_percent(get_clock_modifiers_opacity()), 0);
    }
    lv_obj_set_style_text_align(label_time, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_align(label_time_colon, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(label_time_minutes, LV_TEXT_ALIGN_LEFT, 0);
    position_clock_labels();
    clock_colon_visible = true;
    update_clock_label();
    toggle_hidden(label_product, false);
    apply_clock_element_visibility();
    lv_obj_invalidate(clock_custom);
}

void screen_home_init(void) {
    screen_home = lv_obj_create(NULL);
    lv_obj_add_style(screen_home, &style_screen, 0);

#ifdef EH_STANDBY_BACKGROUND_ENABLE
    eh_background_init();
    standby_background = lv_img_create(screen_home);
    lv_obj_set_pos(standby_background, 0, 0);
    lv_obj_add_flag(standby_background, LV_OBJ_FLAG_HIDDEN);
    standby_background_dim = lv_obj_create(screen_home);
    lv_obj_set_pos(standby_background_dim, 0, 0);
    lv_obj_set_size(standby_background_dim, EH_BACKGROUND_WIDTH, EH_BACKGROUND_HEIGHT);
    lv_obj_set_style_bg_color(standby_background_dim, lv_color_black(), 0);
    lv_obj_set_style_border_width(standby_background_dim, 0, 0);
    lv_obj_set_style_pad_all(standby_background_dim, 0, 0);
    lv_obj_clear_flag(standby_background_dim, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(standby_background_dim, LV_OBJ_FLAG_HIDDEN);
    refresh_standby_background();
#endif

    label_product = lv_label_create(screen_home);
    lv_obj_set_style_text_font(label_product, &lv_font_montserrat_40, LV_PART_MAIN);
    lv_obj_set_style_text_align(label_product, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label_product, 0, 40);
    lv_obj_set_size(label_product, 240, 50);
    lv_label_set_text(label_product, EH_SHORT_PRODUCT_NAME);

    label_time = lv_label_create(screen_home);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_align(label_time, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(label_time, "00");
    lv_obj_add_flag(label_time, LV_OBJ_FLAG_HIDDEN);

    label_time_colon = lv_label_create(screen_home);
    lv_obj_set_style_text_font(label_time_colon, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_align(label_time_colon, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(label_time_colon, ":");
    lv_obj_add_flag(label_time_colon, LV_OBJ_FLAG_HIDDEN);

    label_time_minutes = lv_label_create(screen_home);
    lv_obj_set_style_text_font(label_time_minutes, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_align(label_time_minutes, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(label_time_minutes, "00");
    lv_obj_add_flag(label_time_minutes, LV_OBJ_FLAG_HIDDEN);

    clock_custom = lv_obj_create(screen_home);
    lv_obj_set_pos(clock_custom, 0, 20);
    lv_obj_set_size(clock_custom, 240, 80);
    lv_obj_set_style_bg_opa(clock_custom, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(clock_custom, 0, 0);
    lv_obj_set_style_pad_all(clock_custom, 0, 0);
    lv_obj_clear_flag(clock_custom, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(clock_custom, draw_clock_event, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_flag(clock_custom, LV_OBJ_FLAG_HIDDEN);
    lv_timer_create(update_clock_colon, 500, NULL);

    label_layer_icon = lv_label_create(screen_home);
    lv_label_set_text(label_layer_icon, EH_SYMBOL_LAYER);
    lv_obj_set_pos(label_layer_icon, 0, 100);
    lv_obj_set_size(label_layer_icon, 26, 28);
    lv_obj_set_style_text_align(label_layer_icon, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(label_layer_icon, &eh_font_montserrat_20, LV_PART_MAIN);

    label_layer = lv_label_create(screen_home);
    lv_label_set_text(label_layer, "");
    lv_obj_set_pos(label_layer, 30, 100);
    lv_obj_set_size(label_layer, 115, 28);
    lv_label_set_long_mode(label_layer, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(label_layer, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(label_layer, &eh_font_montserrat_20, LV_PART_MAIN);

    label_mac = lv_label_create(screen_home);
    lv_label_set_text(label_mac, EH_SYMBOL_MAC);
    lv_obj_set_pos(label_mac, 145, 100);
    lv_obj_set_size(label_mac, 30, 20);
    lv_obj_set_style_text_align(label_mac, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_mac, &eh_font_montserrat_20, LV_PART_MAIN);
    toggle_hidden(label_mac, false);

    label_layout = lv_label_create(screen_home);
    lv_label_set_text(label_layout, "");
    lv_obj_set_pos(label_layout, 175, 100);
    lv_obj_set_size(label_layout, 65, 20);
    lv_obj_set_style_text_align(label_layout, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(label_layout, &eh_font_montserrat_20, LV_PART_MAIN);

    screen_home_mods = lv_obj_create(screen_home);
    lv_obj_add_style(screen_home_mods, &style_container, 0);
    use_flex_row(screen_home_mods);
    lv_obj_set_pos(screen_home_mods, 0, 130);
    lv_obj_set_size(screen_home_mods, 240, 100);

    label_gui   = create_button(screen_home_mods, "GUI", &style_button, &style_button_active);
    label_alt   = create_button(screen_home_mods, "ALT", &style_button, &style_button_active);
    label_ctrl  = create_button(screen_home_mods, "CTL", &style_button, &style_button_active);
    label_shift = create_button(screen_home_mods, "SFT", &style_button, &style_button_active);

    label_num    = create_button(screen_home_mods, "NUM", &style_button, &style_button_active);
    label_caps   = create_button(screen_home_mods, "CAPS", &style_button, &style_button_active);
    label_scroll = create_button(screen_home_mods, "SCRL", &style_button, &style_button_active);

    screen_home_media = lv_obj_create(screen_home);
    lv_obj_add_style(screen_home_media, &style_container, 0);
    toggle_hidden(screen_home_media, false);
    use_flex_row(screen_home_media);
    lv_obj_set_pos(screen_home_media, 0, 130);
    lv_obj_set_size(screen_home_media, 240, 130);

    label_hid_media_title = lv_label_create(screen_home_media);
    lv_label_set_text(label_hid_media_title, "");
    lv_label_set_long_mode(label_hid_media_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_hid_media_title, lv_pct(95));
    lv_obj_set_style_text_align(label_hid_media_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_hid_media_title, &eh_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_pad_top(label_hid_media_title, 0, 0);
    lv_obj_set_style_pad_bottom(label_hid_media_title, 0, 0);

    label_hid_media_artist = lv_label_create(screen_home_media);
    lv_label_set_text(label_hid_media_artist, "");
    lv_label_set_long_mode(label_hid_media_artist, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_hid_media_artist, lv_pct(95));
    lv_obj_set_style_text_align(label_hid_media_artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_hid_media_artist, &eh_font_montserrat_20, LV_PART_MAIN);

    lv_obj_set_style_pad_top(label_hid_media_artist, 10, 0);
    lv_obj_set_style_text_color(label_hid_media_artist, accent_color_blue, 0);

    screen_home_apply_clock_settings();
}

void screen_home_load(void) {
    lv_label_set_text(label_layer, layer_name(get_current_layer()));
    lv_scr_load(screen_home);
}

void screen_home_housekeep(void) {
    static uint8_t prev_layer      = 255;
    static uint8_t prev_lang       = -1;
    static led_t   prev_led_state  = {.raw = 0};
    static uint8_t prev_mods       = 255;
    static bool    prev_show_mods  = false;
    static bool    prev_hid_active = false;
    static bool    prev_mac        = false;

#ifdef EH_STANDBY_BACKGROUND_ENABLE
    refresh_standby_background();
    animate_standby_background();
#endif

    uint8_t cur_layer = get_current_layer();
    uint8_t cur_lang  = split_get_lang();
    led_t   led_state = host_keyboard_led_state();
    led_state.caps_lock |= split_get_caps_word();
    uint8_t mods = get_mods() | get_oneshot_mods();
    bool    mac  = split_get_mac();

    static uint32_t mods_timer = 0;
    if (mods != 0 || prev_led_state.raw != led_state.raw) {
        mods_timer = timer_read32();
    }

    bool show_mods  = timer_elapsed32(mods_timer) < EH_DISPLAY_TIMEOUT_ACTIVITY;
    bool hid_active = is_hid_active();

    uint32_t activity_elapsed = last_input_activity_elapsed();
    if (activity_elapsed > __UINT32_MAX__ - 1000) // possible overflow on split
        activity_elapsed = 0;
    bool typing = activity_elapsed < 500; // prevent display updates when typing

    if (prev_hid_active != hid_active || (prev_show_mods != show_mods && hid_active)) {
        if (typing) return;
        if (!hid_active) {
            clock_hours            = 0;
            clock_minutes          = 0;
            clock_time_initialized = false;
            update_clock_label();
            lv_obj_invalidate(clock_custom);
        }
        toggle_hidden(label_product, false);
        modifiers_context_visible = !hid_active || show_mods;
        apply_clock_element_visibility();
        toggle_hidden(screen_home_media, hid_active && !show_mods);
        prev_show_mods  = show_mods;
        prev_hid_active = hid_active;
        return;
    }

    if (prev_layer != cur_layer || layer_name_updated) {
        lv_label_set_text(label_layer, layer_name(cur_layer));
        prev_layer         = cur_layer;
        layer_name_updated = false;
        return;
    }

    if (prev_lang != cur_lang) {
        lv_label_set_text(label_layout, get_layout_label(cur_lang));
        prev_lang = cur_lang;
        return;
    }

    if (mac != prev_mac) {
        toggle_hidden(label_mac, get_clock_info_visible() && mac);
        if (mac) {
            lv_label_set_text(label_gui, "CMD");
            lv_label_set_text(label_alt, "OPT");
        } else {
            lv_label_set_text(label_gui, "GUI");
            lv_label_set_text(label_alt, "ALT");
        }
        prev_mac = mac;
        return;
    }

    if (led_state.raw != prev_led_state.raw) {
        toggle_state(label_caps, LV_STATE_PRESSED, led_state.caps_lock);
        toggle_state(label_num, LV_STATE_PRESSED, led_state.num_lock);
        toggle_state(label_scroll, LV_STATE_PRESSED, led_state.scroll_lock);
        prev_led_state.raw = led_state.raw;
        return;
    }

    if (mods != prev_mods) {
        toggle_state(label_shift, LV_STATE_PRESSED, mods & MOD_MASK_SHIFT);
        toggle_state(label_ctrl, LV_STATE_PRESSED, mods & MOD_MASK_CTRL);
        toggle_state(label_alt, LV_STATE_PRESSED, mods & MOD_MASK_ALT);
        toggle_state(label_gui, LV_STATE_PRESSED, mods & MOD_MASK_GUI);
        prev_mods = mods;
        return;
    }

    hid_data_t *hid = get_hid_data();
    if (hid->hid_changed) {
        if (typing) return;
        if (hid->time_changed) {
            uint8_t previous_hours   = clock_hours;
            uint8_t previous_minutes = clock_minutes;
            clock_hours   = hid->hours;
            clock_minutes = hid->minutes;
            update_clock_label();
            if (clock_time_initialized && get_clock_style() == CLOCK_STYLE_FLIP) {
                start_flip_animation(previous_hours, previous_minutes, hid->hours, hid->minutes);
            }
            clock_time_initialized = true;
            lv_obj_invalidate(clock_custom);
            hid->time_changed = false;
            return;
        }
        if (hid->media_title_changed) {
            lv_label_set_text(label_hid_media_title, hid->media_title);
            hid->media_title_changed = false;
            return;
        }
        if (hid->media_artist_changed) {
            lv_label_set_text(label_hid_media_artist, hid->media_artist);
            hid->media_artist_changed = false;
            return;
        }
        hid->hid_changed = false;
        return;
    }
}

const eh_screen_t eh_screen_home = {
    .init      = screen_home_init,
    .load      = screen_home_load,
    .housekeep = screen_home_housekeep,
};
