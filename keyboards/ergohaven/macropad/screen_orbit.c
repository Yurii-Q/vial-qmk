// Copyright 2026 Ergohaven
// SPDX-License-Identifier: GPL-2.0-or-later

#include <lvgl.h>

#include "ergohaven.h"
#include "src/display/eh_display.h"

LV_IMG_DECLARE(sm62_layer_04);
LV_IMG_DECLARE(anim_00); LV_IMG_DECLARE(anim_01); LV_IMG_DECLARE(anim_02); LV_IMG_DECLARE(anim_03);
LV_IMG_DECLARE(anim_04); LV_IMG_DECLARE(anim_05); LV_IMG_DECLARE(anim_06); LV_IMG_DECLARE(anim_07);
LV_IMG_DECLARE(anim_08); LV_IMG_DECLARE(anim_09); LV_IMG_DECLARE(anim_10); LV_IMG_DECLARE(anim_11);
LV_IMG_DECLARE(anim_12); LV_IMG_DECLARE(anim_13); LV_IMG_DECLARE(anim_14); LV_IMG_DECLARE(anim_15);
LV_IMG_DECLARE(flame_00); LV_IMG_DECLARE(flame_01); LV_IMG_DECLARE(flame_02); LV_IMG_DECLARE(flame_03);
LV_IMG_DECLARE(flame_04); LV_IMG_DECLARE(flame_05); LV_IMG_DECLARE(flame_06); LV_IMG_DECLARE(flame_07);
LV_IMG_DECLARE(flame_08); LV_IMG_DECLARE(flame_09); LV_IMG_DECLARE(flame_10); LV_IMG_DECLARE(flame_11);
LV_IMG_DECLARE(flame_12); LV_IMG_DECLARE(flame_13); LV_IMG_DECLARE(flame_14); LV_IMG_DECLARE(flame_15);

#define ORBIT_WIDTH 240
#define ORBIT_HEIGHT 224
#define ORBIT_SOURCE_WIDTH 240
#define ORBIT_SOURCE_HEIGHT 224
#define ORBIT_COLORS 16
#define ORBIT_FRAME_MS 50U
#define ORBIT_BG 0x070705

static const lv_img_dsc_t *const ship_frames[] = {
    &anim_00, &anim_01, &anim_02, &anim_03, &anim_04, &anim_05, &anim_06, &anim_07,
    &anim_08, &anim_09, &anim_10, &anim_11, &anim_12, &anim_13, &anim_14, &anim_15,
};
static const lv_img_dsc_t *const flame_frames[] = {
    &flame_00, &flame_01, &flame_02, &flame_03, &flame_04, &flame_05, &flame_06, &flame_07,
    &flame_08, &flame_09, &flame_10, &flame_11, &flame_12, &flame_13, &flame_14, &flame_15,
};

static lv_obj_t *screen_orbit;
static lv_obj_t *fallback_image;
static lv_obj_t *animation_image;
static lv_img_dsc_t frame_buffer;
static uint32_t animation_timer;
static int32_t animation_index;
static int8_t frame_index;
static bool frame_drawn;
static bool frame_in_flight;
static bool animation_ready;
static bool animation_started;

#ifdef KEYBOARD_ergohaven_sm62
#include "src/draw/sw/lv_draw_sw.h"
#include "src/draw/sw/lv_draw_sw_blend.h"
/* SM62 only: immutable prepared scene, decoded into LVGL's requested span.
 * No per-open allocation or cursor mutation: repeated/out-of-order reads agree.
 * Owner housekeep prepares only after the preceding refresh completes. */
static int orbit_stars[20][3];
static int8_t orbit_prepared_frame = -1;
static const uint32_t orbit_star_rgb[] = {
    0xFFFFFF, 0xFF9223, 0xFFF1EB, 0xE9ECFF,
    0xBFD3FF, 0xFEF9FF, 0xFF7800, 0xBACFFF,
};
typedef struct {
    const lv_img_dsc_t *asset;
    uint16_t row_byte[ORBIT_HEIGHT];
    uint8_t row_skip[ORBIT_HEIGHT];
    uint16_t colors[ORBIT_COLORS];
    uint8_t alpha[ORBIT_COLORS];
} orbit_rle_t;
static orbit_rle_t orbit_ship, orbit_flame;
_Static_assert(sizeof(orbit_ship) + sizeof(orbit_flame) + sizeof(orbit_stars) + sizeof(orbit_prepared_frame) <= 1700,
               "Orbit prepared scene must remain memory bounded");

static bool orbit_rle_prepare(orbit_rle_t *r, const lv_img_dsc_t *asset) {
    r->asset = NULL;
    if (!asset || !asset->data || asset->data_size < 52 || asset->data_size > UINT16_MAX) return false;
    const uint8_t *d = asset->data;
    unsigned x0=d[48], y0=d[49], width=d[50], height=d[51];
    if (!width || !height || x0+width>ORBIT_WIDTH || y0+height>ORBIT_HEIGHT) return false;
    unsigned total=0;
    for (unsigned i=52;i<asset->data_size;i++) total+=d[i]>>4;
    if (total!=width*height) return false;
    for (unsigned i=0;i<ORBIT_COLORS;i++) {
        r->colors[i]=lv_color_make(d[i*3],d[i*3+1],d[i*3+2]).full;
        r->alpha[i]=d[i*3+1];
    }
    unsigned byte=52,skip=0;
    for (unsigned y=0;y<ORBIT_HEIGHT;y++) {
        r->row_byte[y]=asset->data_size;
        r->row_skip[y]=0;
        if (y<y0) continue;
        r->row_byte[y]=byte;
        r->row_skip[y]=skip;
        unsigned left=width;
        while (left && byte<asset->data_size) {
            unsigned available=(d[byte]>>4)-skip;
            unsigned n=available<left?available:left;
            left-=n; skip+=n;
            if (skip==(unsigned)(d[byte]>>4)) {byte++;skip=0;}
        }
    }
    r->asset=asset;
    return true;
}

static void orbit_rle_span(const orbit_rle_t *r, bool flame, int x, int y, int len, lv_color_t *out) {
    if (!r->asset) return;
    const uint8_t *d=r->asset->data;
    unsigned byte=r->row_byte[y],skip=r->row_skip[y];
    int u=0, width=d[50], x0=d[48];
    while (u<width && byte<r->asset->data_size) {
        unsigned index=d[byte]&15,run=(d[byte]>>4)-skip;
        int end=u+(int)run;
        if (end>width) end=width;
        if (index) for (int p=u;p<end;p++) {
            int dest=x0+p-x;
            if (dest>=0 && dest<len) {
                if (flame) out[dest]=lv_color_mix(lv_color_white(),out[dest],r->alpha[index]);
                else out[dest].full=r->colors[index];
            }
        }
        u=end; byte++; skip=0;
    }
}
static lv_res_t orbit_decoder_info(lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header) {
    (void)decoder;
    if (src!=&frame_buffer || !header) return LV_RES_INV;
    *header=frame_buffer.header;
    header->cf=LV_IMG_CF_TRUE_COLOR;
    return LV_RES_OK;
}
static lv_res_t orbit_decoder_open(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc) {
    if (!dsc || orbit_decoder_info(decoder,dsc->src,&dsc->header)!=LV_RES_OK) return LV_RES_INV;
    dsc->img_data=NULL;
    return LV_RES_OK;
}
static void orbit_star_pixel(lv_color_t *out,int x,int y,int len,int px,int py,lv_color_t c) {
    if (py==y && px>=x && px<x+len) out[px-x]=c;
}
static lv_res_t orbit_decoder_read(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc,
                                  lv_coord_t x,lv_coord_t y,lv_coord_t len,uint8_t *buf) {
    (void)decoder;
    if (!dsc || dsc->src!=&frame_buffer || !buf || x<0 || y<0 || y>=ORBIT_HEIGHT || len<=0 || x>ORBIT_WIDTH-len) return LV_RES_INV;
    lv_color_t *out=(lv_color_t *)buf;
    for (int p=0;p<len;p++) out[p]=lv_color_hex(ORBIT_BG);
    for (unsigned i=0;i<ARRAY_SIZE(orbit_stars);i++) {
        int sx=orbit_stars[i][0],sy=orbit_stars[i][1],speed=orbit_stars[i][2];
        lv_color_t c=lv_color_hex(orbit_star_rgb[i%ARRAY_SIZE(orbit_star_rgb)]);
        orbit_star_pixel(out,x,y,len,sx,sy,c);
        if (speed>=7 && sx>0 && sx<ORBIT_WIDTH-2 && sy>0 && sy<ORBIT_HEIGHT-2) {
            orbit_star_pixel(out,x,y,len,sx,sy-1,c);
            orbit_star_pixel(out,x,y,len,sx-1,sy,c);
            orbit_star_pixel(out,x,y,len,sx+1,sy,c);
            orbit_star_pixel(out,x,y,len,sx,sy+1,c);
        } else if (speed>=5 && sx<ORBIT_WIDTH-1 && sy<ORBIT_HEIGHT-1) {
            orbit_star_pixel(out,x,y,len,sx+1,sy,c);
            orbit_star_pixel(out,x,y,len,sx,sy+1,c);
            orbit_star_pixel(out,x,y,len,sx+1,sy+1,c);
        }
    }
    if (orbit_prepared_frame>=0) {
        orbit_rle_span(&orbit_ship,false,x,y,len,out);
        orbit_rle_span(&orbit_flame,true,x,y,len,out);
    }
    return LV_RES_OK;
}
/* Opaque software-buffer tile path. Unsupported image/style/backend conditions
 * retain the line decoder through the ordinary lv_img class event handler. */
static void orbit_tile_pixel(lv_draw_ctx_t *ctx, const lv_area_t *a, int x, int y, lv_color_t c) {
    if (x>=a->x1 && x<=a->x2 && y>=a->y1 && y<=a->y2)
        ((lv_color_t *)ctx->buf)[(y-ctx->buf_area->y1)*lv_area_get_width(ctx->buf_area)+x-ctx->buf_area->x1]=c;
}
static bool orbit_draw_tile(lv_obj_t *obj, lv_draw_ctx_t *ctx) {
#if LV_COLOR_DEPTH == 16
    lv_disp_t *disp=_lv_refr_get_disp_refreshing();
    lv_img_t *img=(lv_img_t *)obj;
    lv_draw_img_dsc_t d;
    lv_draw_img_dsc_init(&d);
    lv_obj_init_draw_img_dsc(obj,LV_PART_MAIN,&d);
    if (!disp || disp->driver->set_px_cb || disp->driver->screen_transp ||
        !ctx || !ctx->buf || !ctx->buf_area || !ctx->clip_area ||
        ctx->draw_img_decoded!=lv_draw_sw_img_decoded ||
        ((lv_draw_sw_ctx_t *)ctx)->blend!=lv_draw_sw_blend_basic ||
        img->src!=&frame_buffer || img->angle || img->zoom!=LV_IMG_ZOOM_NONE ||
        img->offset.x || img->offset.y ||
        lv_obj_get_style_transform_angle(obj,LV_PART_MAIN) ||
        lv_obj_get_style_transform_zoom(obj,LV_PART_MAIN)!=LV_IMG_ZOOM_NONE ||
        lv_obj_get_style_transform_width(obj,LV_PART_MAIN) || lv_obj_get_style_transform_height(obj,LV_PART_MAIN) ||
        lv_obj_get_style_bg_img_src(obj,LV_PART_MAIN) ||
        d.opa!=LV_OPA_COVER || d.recolor_opa!=LV_OPA_TRANSP || d.blend_mode!=LV_BLEND_MODE_NORMAL ||
        lv_obj_get_width(obj)!=ORBIT_WIDTH || lv_obj_get_height(obj)!=ORBIT_HEIGHT ||
        lv_obj_get_style_pad_left(obj,LV_PART_MAIN) || lv_obj_get_style_pad_right(obj,LV_PART_MAIN) ||
        lv_obj_get_style_pad_top(obj,LV_PART_MAIN) || lv_obj_get_style_pad_bottom(obj,LV_PART_MAIN) ||
        lv_obj_get_style_bg_opa(obj,LV_PART_MAIN)!=LV_OPA_TRANSP ||
        lv_obj_get_style_border_width(obj,LV_PART_MAIN) || lv_obj_get_style_shadow_width(obj,LV_PART_MAIN) ||
        lv_obj_get_style_outline_width(obj,LV_PART_MAIN) || lv_obj_get_style_radius(obj,LV_PART_MAIN) ||
        lv_draw_mask_is_any(NULL) || lv_area_get_width(ctx->buf_area)<=0 || lv_area_get_height(ctx->buf_area)<=0)
        return false;
    lv_area_t a;
    if (!_lv_area_intersect(&a,&obj->coords,ctx->clip_area) || !_lv_area_intersect(&a,&a,ctx->buf_area)) return true;
    if (ctx->wait_for_finish) ctx->wait_for_finish(ctx);
    int stride=lv_area_get_width(ctx->buf_area),len=lv_area_get_width(&a);
    lv_color_t bg=lv_color_hex(ORBIT_BG);
    for (int y=a.y1;y<=a.y2;y++) {
        lv_color_t *out=(lv_color_t *)ctx->buf+(y-ctx->buf_area->y1)*stride+a.x1-ctx->buf_area->x1;
        for (int x=0;x<len;x++) out[x]=bg;
    }
    for (unsigned i=0;i<ARRAY_SIZE(orbit_stars);i++) {
        int sx=orbit_stars[i][0],sy=orbit_stars[i][1],speed=orbit_stars[i][2];
        lv_color_t c=lv_color_hex(orbit_star_rgb[i%ARRAY_SIZE(orbit_star_rgb)]);
        int x=sx+obj->coords.x1,y=sy+obj->coords.y1;
        orbit_tile_pixel(ctx,&a,x,y,c);
        if (speed>=7 && sx>0 && sx<ORBIT_WIDTH-2 && sy>0 && sy<ORBIT_HEIGHT-2) {
            orbit_tile_pixel(ctx,&a,x,y-1,c);orbit_tile_pixel(ctx,&a,x-1,y,c);
            orbit_tile_pixel(ctx,&a,x+1,y,c);orbit_tile_pixel(ctx,&a,x,y+1,c);
        } else if (speed>=5 && sx<ORBIT_WIDTH-1 && sy<ORBIT_HEIGHT-1) {
            orbit_tile_pixel(ctx,&a,x+1,y,c);orbit_tile_pixel(ctx,&a,x,y+1,c);orbit_tile_pixel(ctx,&a,x+1,y+1,c);
        }
    }
    if (orbit_prepared_frame>=0) for (int y=a.y1;y<=a.y2;y++) {
        lv_color_t *out=(lv_color_t *)ctx->buf+(y-ctx->buf_area->y1)*stride+a.x1-ctx->buf_area->x1;
        orbit_rle_span(&orbit_ship,false,a.x1-obj->coords.x1,y-obj->coords.y1,len,out);
        orbit_rle_span(&orbit_flame,true,a.x1-obj->coords.x1,y-obj->coords.y1,len,out);
    }
    return true;
#else
    (void)obj;(void)ctx;
    return false;
#endif
}
static void orbit_image_event(const lv_obj_class_t *class_p, lv_event_t *e);
static const lv_obj_class_t orbit_image_class={
    .base_class=&lv_img_class,
    .instance_size=sizeof(lv_img_t),
    .event_cb=orbit_image_event,
};
static void orbit_image_event(const lv_obj_class_t *class_p, lv_event_t *e) {
    (void)class_p;
    if (lv_event_get_code(e)==LV_EVENT_DRAW_MAIN &&
        orbit_draw_tile(lv_event_get_target(e),lv_event_get_draw_ctx(e))) return;
    lv_obj_event_base(&orbit_image_class,e);
}

static void orbit_prepare_assets(void) {
    orbit_prepared_frame=frame_index;
    if (frame_index>=0) {
        bool ship_ok=orbit_rle_prepare(&orbit_ship,ship_frames[frame_index]);
        bool flame_ok=orbit_rle_prepare(&orbit_flame,flame_frames[frame_index]);
        LV_ASSERT_MSG(ship_ok && flame_ok,"Invalid Orbit RLE asset");
    }
}
#endif

#ifndef KEYBOARD_ergohaven_sm62
static void clear_frame(lv_img_dsc_t *out) {
    lv_color_t background = lv_color_hex(ORBIT_BG);
    lv_color_t *pixels = (lv_color_t *)out->data;
    for (uint32_t i = 0; i < ORBIT_WIDTH * ORBIT_HEIGHT; ++i) {
        pixels[i] = background;
    }
}

#endif

static void draw_stars(lv_img_dsc_t *out) {
    static bool stars_initialized;
    static int stars[20][3];
    #ifndef KEYBOARD_ergohaven_sm62
    static const uint32_t rgb[] = {
        0xFFFFFF, 0xFF9223, 0xFFF1EB, 0xE9ECFF,
        0xBFD3FF, 0xFEF9FF, 0xFF7800, 0xBACFFF,
    };
    #endif
    if (!stars_initialized) {
        for (uint8_t i = 0; i < ARRAY_SIZE(stars); ++i) {
            stars[i][0] = rand() % ORBIT_WIDTH;
            stars[i][1] = rand() % ORBIT_HEIGHT;
            stars[i][2] = rand() % 8 + 3;
        }
        stars_initialized = true;
    }

#ifdef KEYBOARD_ergohaven_sm62
    (void)out;
    memcpy(orbit_stars, stars, sizeof(orbit_stars));
    orbit_prepare_assets();
#else
    clear_frame(out);
    uint16_t *pixels = (uint16_t *)out->data;
#endif
    for (uint8_t i = 0; i < ARRAY_SIZE(stars); ++i) {
        int x = stars[i][0];
#ifndef KEYBOARD_ergohaven_sm62
        int y = stars[i][1];
#endif
        int speed = stars[i][2];

#ifndef KEYBOARD_ergohaven_sm62
        uint32_t c = rgb[i % ARRAY_SIZE(rgb)];
        uint16_t color = lv_color_make((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF).full;

        if (speed >= 7 && x > 0 && x < ORBIT_WIDTH - 2 && y > 0 && y < ORBIT_HEIGHT - 2) {
            pixels[(y - 1) * ORBIT_WIDTH + x] = color;
            pixels[y * ORBIT_WIDTH + x - 1] = color;
            pixels[y * ORBIT_WIDTH + x] = color;
            pixels[y * ORBIT_WIDTH + x + 1] = color;
            pixels[(y + 1) * ORBIT_WIDTH + x] = color;
        } else if (speed >= 5 && x < ORBIT_WIDTH - 1 && y < ORBIT_HEIGHT - 1) {
            pixels[y * ORBIT_WIDTH + x] = color;
            pixels[y * ORBIT_WIDTH + x + 1] = color;
            pixels[(y + 1) * ORBIT_WIDTH + x] = color;
            pixels[(y + 1) * ORBIT_WIDTH + x + 1] = color;
        } else {
            pixels[y * ORBIT_WIDTH + x] = color;
        }

#endif
        x -= speed;
        if (x < 0) {
            stars[i][0] = ORBIT_WIDTH - 1;
            stars[i][1] = rand() % ORBIT_HEIGHT;
            stars[i][2] = rand() % 8 + 3;
        } else {
            stars[i][0] = x;
        }
    }
}

#ifndef KEYBOARD_ergohaven_sm62
static void draw_ship(const lv_img_dsc_t *input, lv_img_dsc_t *output) {
    uint16_t palette[ORBIT_COLORS];
    for (uint8_t i = 0; i < ORBIT_COLORS; ++i) {
        palette[i] = lv_color_make(input->data[i * 3], input->data[i * 3 + 1], input->data[i * 3 + 2]).full;
    }
    int x0 = input->data[ORBIT_COLORS * 3];
    int y0 = input->data[ORBIT_COLORS * 3 + 1];
    int width = input->data[ORBIT_COLORS * 3 + 2];
    int u = 0;
    uint16_t *pixels = (uint16_t *)output->data + y0 * output->header.w + x0;
    for (uint32_t i = ORBIT_COLORS * 3 + 4; i < input->data_size; ++i) {
        uint8_t packed = input->data[i];
        uint8_t index = packed & 0x0F;
        int run = packed >> 4;
        for (; run > 0; --run, ++u) {
            if (u >= width) {
                u -= width;
                pixels += output->header.w;
            }
            if (index) pixels[u] = palette[index];
        }
    }
}

static void draw_flame(const lv_img_dsc_t *input, lv_img_dsc_t *output) {
    uint8_t alpha[ORBIT_COLORS];
    for (uint8_t i = 0; i < ORBIT_COLORS; ++i) alpha[i] = input->data[i * 3 + 1];
    int x0 = input->data[ORBIT_COLORS * 3];
    int y0 = input->data[ORBIT_COLORS * 3 + 1];
    int width = input->data[ORBIT_COLORS * 3 + 2];
    int u = 0;
    lv_color_t *pixels = (lv_color_t *)output->data + y0 * output->header.w + x0;
    for (uint32_t i = ORBIT_COLORS * 3 + 4; i < input->data_size; ++i) {
        uint8_t packed = input->data[i];
        uint8_t index = packed & 0x0F;
        int run = packed >> 4;
        for (; run > 0; --run, ++u) {
            if (u >= width) {
                u -= width;
                pixels += output->header.w;
            }
            if (index) pixels[u] = lv_color_mix(lv_color_white(), pixels[u], alpha[index]);
        }
    }
}

#endif

static void screen_orbit_init(void) {
    screen_orbit = display_view_create();
    lv_obj_set_style_pad_all(screen_orbit, 0, 0);
    lv_obj_set_style_border_width(screen_orbit, 0, 0);
    lv_obj_set_style_bg_color(screen_orbit, lv_color_hex(ORBIT_BG), 0);
    lv_obj_set_style_bg_opa(screen_orbit, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen_orbit, LV_OBJ_FLAG_SCROLLABLE);

    fallback_image = lv_img_create(screen_orbit);
    lv_img_set_src(fallback_image, &sm62_layer_04);
    lv_obj_set_pos(fallback_image, 0, 0);

#ifdef KEYBOARD_ergohaven_sm62
    /* LVGL allocates only its ordinary requested-line buffer, never a scene. */
    frame_buffer.header.w = ORBIT_WIDTH;
    frame_buffer.header.h = ORBIT_HEIGHT;
    frame_buffer.header.cf = LV_IMG_CF_USER_ENCODED_0;
    /* LVGL rejects variable descriptors with NULL data before dispatch. */
    frame_buffer.data = (const uint8_t *)&frame_buffer;
    frame_buffer.data_size = 1;
    lv_img_decoder_t *decoder = lv_img_decoder_create();
    LV_ASSERT_MALLOC(decoder);
    lv_img_decoder_set_info_cb(decoder, orbit_decoder_info);
    lv_img_decoder_set_open_cb(decoder, orbit_decoder_open);
    lv_img_decoder_set_read_line_cb(decoder, orbit_decoder_read);
#else
    uint16_t *pixels = lv_mem_alloc(ORBIT_WIDTH * ORBIT_HEIGHT * sizeof(uint16_t));
    if (!pixels) return;
    frame_buffer.header.always_zero = 0;
    frame_buffer.header.w = ORBIT_WIDTH;
    frame_buffer.header.h = ORBIT_HEIGHT;
    frame_buffer.header.cf = LV_IMG_CF_TRUE_COLOR;
    frame_buffer.data_size = ORBIT_WIDTH * ORBIT_HEIGHT * sizeof(uint16_t);
    frame_buffer.data = (uint8_t *)pixels;
    clear_frame(&frame_buffer);
#endif

#ifdef KEYBOARD_ergohaven_sm62
    animation_image = lv_obj_class_create_obj(&orbit_image_class, screen_orbit);
    lv_obj_class_init_obj(animation_image);
#else
    animation_image = lv_img_create(screen_orbit);
#endif
    lv_img_set_src(animation_image, &frame_buffer);
    lv_obj_set_pos(animation_image, 0, 28);
    /* The supplied layer-4 bitmap contains a one-pixel top seam. It is only a
     * fallback for allocation failure; hide it when the live buffer exists. */
    lv_obj_add_flag(fallback_image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *top_mask = lv_obj_create(screen_orbit);
    lv_obj_set_pos(top_mask, 0, 0);
    lv_obj_set_size(top_mask, ORBIT_WIDTH, 3);
    lv_obj_set_style_radius(top_mask, 0, 0);
    lv_obj_set_style_pad_all(top_mask, 0, 0);
    lv_obj_set_style_border_width(top_mask, 0, 0);
    lv_obj_set_style_bg_color(top_mask, lv_color_hex(ORBIT_BG), 0);
    lv_obj_set_style_bg_opa(top_mask, LV_OPA_COVER, 0);
    lv_obj_clear_flag(top_mask, LV_OBJ_FLAG_SCROLLABLE);
    animation_ready = true;
}

void sm62_orbit_restart(void) { animation_started = false; }

static void screen_orbit_load(void) {
    /* Idle entries replay the arrival; a permanent ship keeps its sequence
     * when the temporary keymap view is dismissed. */
    if (!animation_started) {
        animation_index = 0;
        frame_index = -1;
        srand(timer_read32());
        /* Replace the previous ship frame before exposing this view again. */
        if (animation_ready) draw_stars(&frame_buffer);
        frame_drawn = animation_ready;
        animation_started = true;
    }
    animation_timer = timer_read32();
    frame_in_flight = false;
    display_show_view(screen_orbit);
}

static void screen_orbit_housekeep(void) {
    if (!animation_ready || frame_in_flight) return;
    if (!frame_drawn) {
        draw_stars(&frame_buffer);
#ifndef KEYBOARD_ergohaven_sm62
        if (frame_index >= 0) {
            draw_ship(ship_frames[frame_index], &frame_buffer);
            draw_flame(flame_frames[frame_index], &frame_buffer);
        }
#endif
        frame_drawn = true;
        return;
    }
    if (timer_elapsed32(animation_timer) < ORBIT_FRAME_MS) return;

    display_diag_mark(0x610);
    /* The descriptor and backing storage never change. Resetting the source
     * on every frame needlessly rebuilds image state and adds another full
     * invalidation; one explicit invalidate is sufficient. */
    lv_obj_invalidate(animation_image);
    frame_in_flight = true;
    display_diag_mark(0x611);
    animation_timer = timer_read32();
    animation_index++;

    static const uint8_t table[] = {
        9,15,9,15,9,15,9,15,9,15, 9,15,9,15,9,15,9,15,9,15,
        9,15,9,15,9,15,9,15,9,15, 9,15,9,15,9,15,9,15,9,15,
        10,11,12, 13,14,13,14,13,14,13,14,13,14,
        13,14,13,14,13,14,13,14,13,14, 13,14,13,14,13,14,13,14,13,14,
        12,11,10,
    };
    if (animation_index >= 14) frame_index = table[(animation_index - 14) % ARRAY_SIZE(table)];
    else if (animation_index > 5) frame_index = animation_index - 5;
    else frame_index = -1;
    frame_drawn = false;
}

void sm62_orbit_frame_completed(void) { frame_in_flight = false; }

const eh_screen_t eh_screen_orbit = {
    .init = screen_orbit_init,
    .load = screen_orbit_load,
    .housekeep = screen_orbit_housekeep,
};
