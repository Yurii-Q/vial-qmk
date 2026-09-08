#pragma once

#include <stdbool.h>
#include <stdint.h>

#define EH_PICTOGRAM_WIDTH 32
#define EH_PICTOGRAM_HEIGHT 32
#define EH_PICTOGRAM_BYTES (EH_PICTOGRAM_WIDTH * EH_PICTOGRAM_HEIGHT / 8)
#define EH_PICTOGRAM_MACRO_SLOTS 256
#define EH_PICTOGRAM_TAP_DANCE_SLOTS 256

#ifdef EH_PICTOGRAM_ENABLE
void eh_pictograms_init(void);
const uint8_t *eh_pictogram_for_keycode(uint16_t keycode);
uint32_t eh_pictogram_color_for_keycode(uint16_t keycode);
uint32_t eh_pictograms_generation(void);
bool eh_pictograms_process_hid(uint8_t *data, uint8_t length);
#else
static inline void eh_pictograms_init(void) {}
static inline const uint8_t *eh_pictogram_for_keycode(uint16_t keycode) {
    (void)keycode;
    return (const uint8_t *)0;
}
static inline uint32_t eh_pictogram_color_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}
static inline uint32_t eh_pictograms_generation(void) {
    return 0;
}
#endif
