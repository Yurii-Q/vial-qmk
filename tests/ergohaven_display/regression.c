#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define KB_SETTINGS_LCD_SIZE 35
#define KB_SETTINGS_LCD_OFFSET 0
#define KB_SETTINGS_LED_COLORS_OFFSET 64
#define BACKLIGHT_LEVELS 5
#define MAX(a,b) ((a) > (b) ? (a) : (b))
typedef struct { uint8_t timeout_mins; } kb_settings_led_colors_t;
static uint8_t storage[128];
static unsigned writes;
void eeconfig_read_kb_datablock(void *p, size_t offset, size_t n) { memcpy(p, storage + offset, n); }
void eeconfig_update_kb_datablock(const void *p, size_t offset, size_t n) { memcpy(storage + offset, p, n); ++writes; }
void eh_extra_settings_read(size_t offset, void *p, size_t n) { (void)offset; memcpy(p, storage, n); }
void eh_extra_settings_write(size_t offset, const void *p, size_t n) { (void)offset; memcpy(storage, p, n); ++writes; }
void display_apply_accent_color(uint8_t r, uint8_t g, uint8_t b) { (void)r; (void)g; (void)b; }
void display_apply_background_color(uint8_t r, uint8_t g, uint8_t b) { (void)r; (void)g; (void)b; }
void display_apply_button_style(uint8_t s) { (void)s; }
void display_apply_brightness(void) {}
void display_apply_clock_settings(void) {}
void display_housekeeping_task(void) {}
void display_init_kb(void) {}
void eh_extra_settings_housekeep(void) {}
bool screen_home_is_active(void) { return false; }
uint8_t eh_date_get(uint8_t n) { (void)n; return 100; }
#include "production.c"

static unsigned checks, failures;
#define CHECK(expr) do { ++checks; if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __func__, __LINE__, #expr); ++failures; } } while (0)
static void accent(uint8_t r, uint8_t g, uint8_t b) { set_display_accent_red(r); set_display_accent_green(g); set_display_accent_blue(b); }
static void text(uint8_t r, uint8_t g, uint8_t b) { set_clock_text_red(r); set_clock_text_green(g); set_clock_text_blue(b); }
static void info(uint8_t r, uint8_t g, uint8_t b) { set_clock_info_red(r); set_clock_info_green(g); set_clock_info_blue(b); }
static void modifiers(uint8_t r, uint8_t g, uint8_t b) { set_clock_modifiers_red(r); set_clock_modifiers_green(g); set_clock_modifiers_blue(b); }
#define COLOR(prefix,r,g,b) CHECK(get_##prefix##_red() == (r) && get_##prefix##_green() == (g) && get_##prefix##_blue() == (b))
static void custom(void) {
    kb_settings_lcd_reset();
    text(10,177,139); info(10,177,139); modifiers(10,177,139);
    accent(10,20,30);
    COLOR(clock_text,10,177,139); COLOR(clock_info,10,177,139); COLOR(clock_modifiers,10,177,139);
    kb_settings_lcd_init(); accent(40,50,60);
    COLOR(clock_text,10,177,139); COLOR(clock_info,10,177,139); COLOR(clock_modifiers,10,177,139);

    // Coincidence persists across a reboot, not just a contiguous R/G/B sequence.
    kb_settings_lcd_reset(); text(10,177,139); set_display_accent_red(10);
    kb_settings_lcd_init(); set_display_accent_green(20); set_display_accent_blue(30);
    COLOR(clock_text,10,177,139); COLOR(clock_info,10,20,30); COLOR(clock_modifiers,10,20,30);

    // Explicit writes of the existing accent still mean an independent color.
    kb_settings_lcd_reset(); text(209,177,139); kb_settings_lcd_init(); accent(1,2,3);
    COLOR(clock_text,209,177,139); COLOR(clock_info,1,2,3); COLOR(clock_modifiers,1,2,3);

    // Each element detaches independently, in arbitrary channel order.
    kb_settings_lcd_reset(); info(209,177,139); modifiers(209,177,139);
    set_display_accent_blue(42); set_lcd_brightness(70); kb_settings_lcd_init();
    set_display_accent_red(44); set_display_accent_green(43);
    COLOR(clock_text,44,43,42); COLOR(clock_info,209,177,139); COLOR(clock_modifiers,209,177,139);

    kb_settings_lcd_reset(); accent(7,8,9); kb_settings_lcd_init(); accent(12,13,14);
    COLOR(clock_text,12,13,14); COLOR(clock_info,12,13,14); COLOR(clock_modifiers,12,13,14);
    CHECK(sizeof(display_settings) == 35); CHECK(storage[20] == 0xDD);
    // Each unchanged channel is an explicit detach; repeated writes stay no-op.
    void (*setters[])(uint8_t) = {set_clock_text_red, set_clock_text_green, set_clock_text_blue,
                                set_clock_info_red, set_clock_info_green, set_clock_info_blue,
                                set_clock_modifiers_red, set_clock_modifiers_green, set_clock_modifiers_blue};
    const uint8_t initial[] = {209,177,139};
    for (size_t i = 0; i < sizeof(setters) / sizeof(setters[0]); ++i) {
        kb_settings_lcd_reset(); unsigned before = writes;
        setters[i](initial[i % 3]); CHECK(writes == before + 1);
        setters[i](initial[i % 3]); CHECK(writes == before + 1);
        kb_settings_lcd_init(); accent(1,2,3);
        if (i / 3 == 0) { COLOR(clock_text,209,177,139); } else { COLOR(clock_text,1,2,3); }
        if (i / 3 == 1) { COLOR(clock_info,209,177,139); } else { COLOR(clock_info,1,2,3); }
        if (i / 3 == 2) { COLOR(clock_modifiers,209,177,139); } else { COLOR(clock_modifiers,1,2,3); }
    }
}

// Seed actual historical layouts: bytes beyond each format are uninitialized.
static void legacy(uint8_t marker, uint8_t customized, bool beige_clock) {
    kb_settings_lcd_reset();
    macropad_display_settings_t fixture = display_settings;
    fixture.red = customized == 1 ? 17 : 200;
    fixture.green = customized == 1 ? 28 : 178;
    fixture.blue = customized == 1 ? 39 : 146;
    fixture.style = 3; fixture.brightness = 67;
    fixture.legacy_magic = 0xC7;
    fixture.clock_text_red = customized ? 51 : (beige_clock ? 200 : 255);
    fixture.clock_text_green = customized ? 62 : (beige_clock ? 178 : 255);
    fixture.clock_text_blue = customized ? 73 : (beige_clock ? 146 : 255);
    fixture.clock_info_red = customized ? 84 : (beige_clock ? 200 : 255);
    fixture.clock_info_green = customized ? 95 : (beige_clock ? 178 : 255);
    fixture.clock_info_blue = customized ? 106 : (beige_clock ? 146 : 255);
    fixture.clock_modifiers_red = customized ? 117 : (beige_clock ? 200 : 255);
    fixture.clock_modifiers_green = customized ? 128 : (beige_clock ? 178 : 255);
    fixture.clock_modifiers_blue = customized ? 149 : (beige_clock ? 146 : 255);
    fixture.magic = marker;
    size_t length = marker == 0xDC ? 35 : marker == 0xDB ? 26 :
                    marker == 0xDA ? 25 : marker == 0xD9 ? 22 : marker == 0xD8 ? 21 : marker == 0xC7 ? 9 : 4;
    if (length == 4) fixture.style = marker;
    memset(storage, 0xFF, sizeof(storage)); memcpy(storage, &fixture, length);
    unsigned before = writes;
    kb_settings_lcd_init();
    CHECK(writes == before + 1); CHECK(storage[20] == 0xDD);
    CHECK(get_display_button_style() == (length == 4 ? (marker == 0xD3 ? 0 : 3) : 3));
    CHECK(get_lcd_brightness() == (length == 4 ? 100 : 67));
    if (customized) {
        if (customized == 1) { COLOR(display_accent,17,28,39); } else { COLOR(display_accent,209,177,139); }
        if (length >= 21) { COLOR(clock_text,51,62,73); }
        else if (customized == 1) { COLOR(clock_text,17,28,39); } else { COLOR(clock_text,209,177,139); }
        if (length >= 25) { COLOR(clock_info,84,95,106); }
        else if (customized == 1) { COLOR(clock_info,17,28,39); } else { COLOR(clock_info,209,177,139); }
        if (length == 35) { COLOR(clock_modifiers,117,128,149); }
        else if (length >= 25) { COLOR(clock_modifiers,84,95,106); }
        else if (customized == 1) { COLOR(clock_modifiers,17,28,39); } else { COLOR(clock_modifiers,209,177,139); }
    } else {
        COLOR(display_accent,209,177,139); COLOR(clock_text,209,177,139);
        COLOR(clock_info,209,177,139); COLOR(clock_modifiers,209,177,139);
    }
    uint8_t saved[35]; memcpy(saved, storage, sizeof(saved));
    before = writes; kb_settings_lcd_init(); CHECK(writes == before); CHECK(!memcmp(saved, storage, sizeof(saved)));
    accent(1,2,3);
    if (!customized || length < 21) { COLOR(clock_text,1,2,3); } else { COLOR(clock_text,51,62,73); }
}
static void migration(void) {
    const uint8_t markers[] = {0xDC,0xDB,0xDA,0xD9,0xD8,0xC7,0xD3,0xE3,0xA3};
    for (size_t i = 0; i < sizeof(markers); ++i) {
        legacy(markers[i], false, false); legacy(markers[i], false, true); legacy(markers[i], true, false);
        legacy(markers[i], 2, false);
    }
    // Current DD records are not palette migrations, even with old stock colors.
    kb_settings_lcd_reset(); accent(200,178,146); text(255,255,255);
    info(200,178,146); modifiers(200,178,146); kb_settings_lcd_init();
    COLOR(display_accent,200,178,146); COLOR(clock_text,255,255,255);
    COLOR(clock_info,200,178,146); COLOR(clock_modifiers,200,178,146);
    accent(1,2,3); COLOR(clock_info,200,178,146); COLOR(clock_modifiers,200,178,146);
    // Existing DD without flags retains nonmatching custom colors without recoloring.
    kb_settings_lcd_reset(); text(10,177,139); info(20,30,40); modifiers(50,60,70);
    storage[8] = 0xC7; unsigned before = writes;
    kb_settings_lcd_init(); CHECK(writes == before); accent(10,20,30);
    kb_settings_lcd_init(); accent(40,50,60);
    COLOR(clock_text,10,177,139); COLOR(clock_info,20,30,40); COLOR(clock_modifiers,50,60,70);
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "all";
    if (!strcmp(mode,"custom") || !strcmp(mode,"all")) custom();
    if (!strcmp(mode,"migration") || !strcmp(mode,"all")) migration();
    printf("%u checks, %u failures\n", checks, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
