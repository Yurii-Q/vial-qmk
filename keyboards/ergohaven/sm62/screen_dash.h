// Copyright 2026 Ergohaven
// SPDX-License-Identifier: GPL-2.0-or-later
// Экран «дашборд»: рисует только то, что прислал хост (текст, полосы, прямоугольники).
//
// Кадр собирается из пакетов raw HID (32 байта, report id 0):
//
//   0xC0 BEGIN  [0xC0, версия, target, кол-во_элементов]
//   0xC1 TEXT   [0xC1, target, id, x, y_lo, y_hi, w_lo, w_hi,
//                font|align<<4, color_lo, color_hi, flags, offset, len, utf8...]
//   0xC2 BAR    [0xC2, target, id, x, y_lo, y_hi, w_lo, w_hi, h_lo, h_hi,
//                pos(4 LE), dur(4 LE), fg_lo, fg_hi, bg_lo, bg_hi]
//   0xC3 RECT   [0xC3, target, id, x, y_lo, y_hi, w_lo, w_hi, h_lo, h_hi,
//                color_lo, color_hi, radius]
//   0xC4 COMMIT [0xC4, target] — показать накопленный кадр
//   0xC5 SCREEN зарезервирован
//   0xC6 INVERT [0xC6, target, on] — аппаратная инверсия панели
//
// target: 0 — левая половина, 1 — правая, 2 — обе. Адрес есть в каждом пакете,
// потому что канал общий: половинки разбирают один и тот же поток.
//
// Покадровое обновление атомарное: элементы копятся в модели, на экран
// попадают по COMMIT (вернее, в ближайшем housekeep после него).
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "src/display/eh_display.h"

/* Больше 24 элементов на кадр не поддерживаем: это уже больше, чем влезает
 * в 220x264 читаемым шрифтом, а каждый элемент — отдельный LVGL-объект. */
#define SM62_DASH_MAX_ELEMENTS 24

/* Безопасная область: скругления экрана режут ~12 px слева и ~8 px сверху/снизу.
 * Координаты хоста считаются от 0,0 внутри этой области, смещение прибавляет
 * прошивка (см. SM62_DASH_PAD_* в screen_dash.c). */
#define SM62_DASH_PAD_LEFT 12
#define SM62_DASH_PAD_TOP 8
/* Строка целиком: длинное название встречи приходит несколькими пакетами,
 * в каждом до 18 байт полезных данных. */
#define SM62_DASH_TEXT_BYTES 96

typedef enum {
    SM62_DASH_BEGIN = 0xC0,
    SM62_DASH_TEXT = 0xC1,
    SM62_DASH_BAR = 0xC2,
    SM62_DASH_RECT = 0xC3,
    SM62_DASH_COMMIT = 0xC4,
    SM62_DASH_SCREEN = 0xC5,
    SM62_DASH_INVERT = 0xC6,
} sm62_dash_type;

/* target: 0 — левая половина, 1 — правая, 2 — обе */
void sm62_dash_begin(uint8_t version, uint8_t target, uint8_t count);
/* offset — смещение куска в байтах от начала строки; 0 начинает новую строку */
void sm62_dash_text(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint8_t font, uint16_t color, uint8_t align, uint8_t flags, uint8_t offset, const uint8_t *data, uint8_t length);
/* pos/dur — заполнение полосы; dur == 0 даёт пустую полосу */
void sm62_dash_bar(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t pos, uint32_t duration, uint16_t color, uint16_t background);
void sm62_dash_rect(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color, uint8_t radius);
void sm62_dash_commit(void);
/* Только складывает запрос в флаг: к панели отсюда не прикасаемся */
void sm62_dash_invert(bool on);
/* Применяет накопленный запрос инверсии, вызывается из housekeep */
void sm62_dash_apply_invert(void);
/* Снять инверсию при уходе с экрана: она живёт в панели, а не в кадре */
void sm62_dash_reset_invert(void);

/* Хост присылал хотя бы один кадр — по этому признаку включается экран */
bool sm62_dash_has_frame(void);

extern const eh_screen_t eh_screen_dash;
