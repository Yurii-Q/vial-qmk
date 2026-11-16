// Copyright 2026 Ergohaven
// SPDX-License-Identifier: GPL-2.0-or-later
// Универсальный экран: хост присылает элементы (текст, полоса, прямоугольник),
// прошивка только раскладывает их по координатам. Никакой логики приложения здесь нет.
//
// Поток данных:
//   1. hid.c разбирает пакеты 0xC0..0xC6 и складывает их в модель (elements[]);
//   2. housekeep замечает dash_dirty и один раз перерисовывает изменённое;
//   3. LVGL сам решает, какие пиксели перерисовать, и отдаёт их в SPI.
//
// Почему отрисовка не в обработчике HID: LVGL не реентерабелен по отношению к
// идущему флашу. Вызов из USB-контекста попадает внутрь передачи по SPI и
// вешает клавиатуру (проверено). Заодно это удешевляет кадр: несколько пакетов
// подряд дают одну перерисовку, а не по одной на пакет.

#include "keyboards/ergohaven/sm62/render_alias.h"
#include "src/display/eh_display.h"
#include <lvgl.h>
#include <string.h>
#include "screen_dash.h"

LV_FONT_DECLARE(sm62_font_zen_medium_12);
LV_FONT_DECLARE(sm62_font_zen_medium_14);
LV_FONT_DECLARE(sm62_font_zen_medium_16);
LV_FONT_DECLARE(sm62_font_zen_medium_20);
LV_FONT_DECLARE(sm62_font_zen_regular_24);
LV_FONT_DECLARE(sm62_font_zen_medium_28);

/* Один элемент кадра. Модель полная: сюда попадает всё, что прислал хост,
 * а LVGL-объект создаётся лениво и только когда элемент реально виден.
 * rendered_type хранит тип, под который построен объект: label нельзя
 * превратить в контейнер полосы, поэтому при смене типа объект пересоздаётся. */
typedef struct {
    uint8_t  type; // 0 — пусто, 1 — текст, 2 — полоса, 3 — прямоугольник
    uint16_t x, y, w, h;
    uint8_t  font, align, flags, radius;
    uint16_t color, background;
    uint32_t position, duration; // заполнение полосы: position из duration
    char     text[SM62_DASH_TEXT_BYTES + 1]; // строка целиком, склеена из кусков
    lv_obj_t *object; // label или подложка полосы/прямоугольника
    lv_obj_t *fill;   // заливка полосы, только у type == 2
    uint8_t   rendered_type; // тип, под который построен object
} dash_element_t;

static dash_element_t elements[SM62_DASH_MAX_ELEMENTS];
static lv_obj_t        *dash_root;   // корневой контейнер экрана, создаётся при первом показе
static bool             dash_dirty;  // модель изменилась, нужен рендер в housekeep
static bool             dash_frame_seen; // хост присылал хотя бы один кадр
static uint8_t          dash_count;  // сколько элементов в текущем кадре

/* Цвета приходят в RGB565, LVGL ждёт компоненты 0..255. */
static lv_color_t dash_color(uint16_t c) {
    uint8_t r = (c >> 11) & 0x1F;
    uint8_t g = (c >> 5) & 0x3F;
    uint8_t b = c & 0x1F;
    return lv_color_make((uint8_t)((r * 255) / 31), (uint8_t)((g * 255) / 63), (uint8_t)((b * 255) / 31));
}

static const lv_font_t *dash_font(uint8_t code) {
    switch (code) {
        case 0:
            return &sm62_font_zen_medium_12;
        case 1:
            return &sm62_font_zen_medium_14;
        case 2:
            return &sm62_font_zen_medium_16;
        case 3:
            return &sm62_font_zen_medium_20;
        case 4:
            return &sm62_font_zen_regular_24;
        case 5:
            return &sm62_font_zen_medium_28;
        default:
            return &sm62_font_zen_medium_12;
    }
}

static lv_obj_t *make_box(lv_obj_t *parent, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color, uint8_t radius) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, w, h);
    lv_obj_set_style_radius(box, radius, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_bg_color(box, dash_color(color), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    return box;
}

/* Битая UTF-8 вешает LVGL (он уходит в бесконечный разбор последовательности).
 * Хост режет строку по границам символов, но полагаться на это нельзя:
 * потерянный по дороге пакет даёт дырку в середине символа. Заменяем всё
 * невалидное на '?', чтобы экран остался живым. */
static void dash_sanitize_utf8(char *text) {
    uint8_t *p = (uint8_t *)text;
    uint8_t *out = (uint8_t *)text;
    while (*p) {
        uint8_t c = *p;
        int len = 0;
        if (c < 0x80) len = 1;
        else if ((c & 0xE0) == 0xC0 && c >= 0xC2) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        if (len == 0) { *out++ = '?'; p++; continue; }
        bool ok = true;
        for (int i = 1; i < len; ++i) {
            if ((p[i] & 0xC0) != 0x80) { ok = false; break; }
        }
        if (!ok) { *out++ = '?'; p++; continue; }
        for (int i = 0; i < len; ++i) *out++ = p[i];
        p += len;
    }
    *out = 0;
}

/* align: 0 влево, 1 центр, 2 вправо (старшие 4 бита байта font|align<<4).
 * flags, бит 0: 1 — перенос по словам, 0 — обрезка по ширине. */
static lv_obj_t *make_label(lv_obj_t *parent, uint16_t x, uint16_t y, uint16_t w, const lv_font_t *font, uint16_t color, uint8_t align, uint8_t flags) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, w);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, dash_color(color), 0);
    lv_obj_set_style_text_align(label, align == 0 ? LV_TEXT_ALIGN_LEFT : (align == 1 ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_RIGHT), 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    lv_label_set_long_mode(label, (flags & 0x01) ? LV_LABEL_LONG_WRAP : LV_LABEL_LONG_CLIP);
    lv_label_set_text(label, "");
    return label;
}

/* Перерисовать один элемент. Объекты не пересоздаются без нужды: кадр
 * приходит часто, а создание/удаление LVGL-объектов — самая дорогая часть. */
static void dash_render_element(uint8_t index) {
    dash_element_t *element = &elements[index];
    if (!dash_root || !element->type) return;

    /* Тип элемента мог смениться (например, текст -> полоса). Нельзя
     * переиспользовать label как контейнер: у него нет fill-объекта, и он
     * не умеет держать дочерние объекты так, как нужно полосе. */
    if (element->object && element->rendered_type != element->type) {
        if (element->fill) {
            lv_obj_del(element->fill);
            element->fill = NULL;
        }
        lv_obj_del(element->object);
        element->object = NULL;
    }
    element->rendered_type = element->type;

    if (element->type == 1) {
        if (!element->object) {
            element->object = make_label(dash_root, element->x + SM62_DASH_PAD_LEFT, element->y + SM62_DASH_PAD_TOP, element->w, dash_font(element->font), element->color, element->align, element->flags);
        }
        lv_obj_set_pos(element->object, element->x + SM62_DASH_PAD_LEFT, element->y + SM62_DASH_PAD_TOP);
        lv_obj_set_width(element->object, element->w);
        lv_obj_set_style_text_font(element->object, dash_font(element->font), 0);
        lv_obj_set_style_text_color(element->object, dash_color(element->color), 0);
        lv_obj_set_style_text_align(element->object, element->align == 0 ? LV_TEXT_ALIGN_LEFT : (element->align == 1 ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_RIGHT), 0);
        lv_label_set_long_mode(element->object, (element->flags & 0x01) ? LV_LABEL_LONG_WRAP : LV_LABEL_LONG_CLIP);
        /* Не трогаем label, если текст не изменился: lv_label_set_text()
         * инвалидирует объект и пересобирает строку в куче даже при том же
         * содержимом, а кадры приходят регулярно. */
        dash_sanitize_utf8(element->text);
        const char *current = lv_label_get_text(element->object);
        if (!current || strcmp(current, element->text) != 0) lv_label_set_text(element->object, element->text);
        return;
    }

    if (element->type == 2) {
        if (!element->object) {
            element->object = make_box(dash_root, element->x + SM62_DASH_PAD_LEFT, element->y + SM62_DASH_PAD_TOP, element->w, element->h, element->background, element->radius);
            element->fill   = element->object ? make_box(element->object, 0, 0, 0, element->h, element->color, element->radius) : NULL;
        }
        if (!element->object) return;
        if (!element->fill) {
            element->fill = make_box(element->object, 0, 0, 0, element->h, element->color, element->radius);
            if (!element->fill) return;
        }
        lv_obj_set_pos(element->object, element->x + SM62_DASH_PAD_LEFT, element->y + SM62_DASH_PAD_TOP);
        lv_obj_set_size(element->object, element->w, element->h);
        lv_obj_set_style_bg_color(element->object, dash_color(element->background), 0);
        /* Заполнение = position/duration, с зажимом сверху: хост может
         * прислать position больше duration (округления на его стороне),
         * и полоса не должна вылезать за подложку. */
        uint32_t width = 0;
        if (element->duration) {
            uint32_t ratio = element->position >= element->duration ? element->duration : element->position;
            width          = (uint32_t)element->w * ratio / element->duration;
        }
        if (lv_obj_get_width(element->fill) != (int32_t)width) lv_obj_set_size(element->fill, width, element->h);
        lv_obj_set_style_bg_color(element->fill, dash_color(element->color), 0);
        return;
    }

    if (element->type == 3) {
        if (!element->object) {
            element->object = make_box(dash_root, element->x + SM62_DASH_PAD_LEFT, element->y + SM62_DASH_PAD_TOP, element->w, element->h, element->color, element->radius);
            /* Прямоугольник — это подложка (фон футера, разделители). LVGL рисует
             * объекты в порядке создания: только что созданный оказался бы поверх
             * текста, который создан раньше. Уводим его в самый низ. */
            if (element->object) lv_obj_move_background(element->object);
        }
        lv_obj_set_pos(element->object, element->x + SM62_DASH_PAD_LEFT, element->y + SM62_DASH_PAD_TOP);
        lv_obj_set_size(element->object, element->w, element->h);
        lv_obj_set_style_bg_color(element->object, dash_color(element->color), 0);
        lv_obj_set_style_radius(element->object, element->radius, 0);
    }
}

static void dash_render(void) {
    if (!dash_root) return;
    for (uint8_t i = 0; i < SM62_DASH_MAX_ELEMENTS; ++i) {
        if (i >= dash_count || elements[i].type == 0) {
            if (elements[i].object) lv_obj_add_flag(elements[i].object, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        if (elements[i].object) lv_obj_clear_flag(elements[i].object, LV_OBJ_FLAG_HIDDEN);
        dash_render_element(i);
    }
    dash_dirty = false;
}

/* BEGIN начинает новый кадр: количество элементов задаёт, сколько из них
 * войдёт в кадр, остальные скрываются (сам кадр заменяет предыдущий целиком).
 * target проверен вызывающим — сюда доходит только «наша» половина. */
void sm62_dash_begin(uint8_t version, uint8_t target, uint8_t count) {
    (void)version;
    (void)target;
    dash_count  = count > SM62_DASH_MAX_ELEMENTS ? SM62_DASH_MAX_ELEMENTS : count;
    dash_dirty  = true;
    dash_frame_seen = true;
    /* Кадр заменяет предыдущий целиком: старые элементы скрываются в render() */
    for (uint8_t i = 0; i < SM62_DASH_MAX_ELEMENTS; ++i) {
        if (i >= dash_count) elements[i].type = 0;
    }
}

/* Кусок строки. offset == 0 начинает новую строку, остальные куски
 * дописываются по смещению. Резать по байтам нельзя: хост режет по границам
 * UTF-8, а санитайзер в рендере страхует от потерянных пакетов. */
void sm62_dash_text(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint8_t font, uint16_t color, uint8_t align, uint8_t flags, uint8_t offset, const uint8_t *data, uint8_t length) {
    if (id >= SM62_DASH_MAX_ELEMENTS) return;
    dash_element_t *element = &elements[id];
    if (offset == 0) {
        element->type = 1;
        element->text[0] = '\0';
    }
    if (element->type != 1) return;
    element->x      = x;
    element->y      = y;
    element->w      = width;
    element->font   = font;
    element->color  = color;
    element->align  = align;
    element->flags  = flags;
    if (offset + length <= SM62_DASH_TEXT_BYTES) {
        memcpy(element->text + offset, data, length);
        element->text[offset + length] = '\0';
    }
    dash_dirty = true;
}

/* Полоса: подложка цвета background и заливка цвета color шириной pos/duration.
 * Скругление берём от высоты (height/2), чтобы полоса выглядела капсулой. */
void sm62_dash_bar(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t pos, uint32_t duration, uint16_t color, uint16_t background) {
    if (id >= SM62_DASH_MAX_ELEMENTS) return;
    dash_element_t *element = &elements[id];
    element->type       = 2;
    element->x          = x;
    element->y          = y;
    element->w          = width;
    element->h          = height;
    element->position   = pos;
    element->duration   = duration;
    element->color      = color;
    element->background = background;
    element->radius     = height / 2;
    element->text[0]    = '\0';
    dash_dirty          = true;
}

/* Прямоугольник: подложка, разделитель, акцентная метка. radius приходит
 * от хоста, потому что скругление — часть макета, а не свойство экрана. */
void sm62_dash_rect(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color, uint8_t radius) {
    if (id >= SM62_DASH_MAX_ELEMENTS) return;
    dash_element_t *element = &elements[id];
    element->type   = 3;
    element->x      = x;
    element->y      = y;
    element->w      = width;
    element->h      = height;
    element->color  = color;
    element->radius = radius;
    element->text[0] = '\0';
    dash_dirty      = true;
}

void sm62_dash_commit(void) {
    dash_dirty      = true;
    dash_frame_seen = true;
}

bool sm62_dash_has_frame(void) {
    return dash_frame_seen;
}

static bool invert_pending;
static bool invert_state;

/* Запрос инверсии приходит из обработчика HID. К дисплею отсюда не
 * прикасаемся: команда панели должна идти в одной транзакции с флашем,
 * иначе ловим дедлок с идущей передачей по SPI. Поэтому только флаг,
 * а применяет его housekeep (sm62_dash_apply_invert). */
void sm62_dash_invert(bool on) {
    invert_state   = on;
    invert_pending = true;
}

/* Применяется из display_housekeeping_task, то есть всегда, независимо от
 * активного экрана. Повторный вызов безопасен: без флага ничего не делает. */
void sm62_dash_apply_invert(void) {
    if (!invert_pending) return;
    invert_pending = false;
    display_set_inverted(invert_state);
}

/* Уход с дашборда: гасим инверсию, иначе следующий экран останется в негативе. */
void sm62_dash_reset_invert(void) {
    invert_state   = false;
    invert_pending = false;
    display_set_inverted(false);
}

static void dash_screen_init(void) {
    if (dash_root) return;
    dash_root = display_view_create();
    if (!dash_root) return;
    lv_obj_set_style_pad_all(dash_root, 0, 0);
    lv_obj_set_style_border_width(dash_root, 0, 0);
    lv_obj_set_style_bg_color(dash_root, lv_color_hex(0x070705), 0);
    lv_obj_set_style_bg_opa(dash_root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(dash_root, LV_OBJ_FLAG_SCROLLABLE);
}

static void dash_screen_load(void) {
    dash_screen_init();
    if (dash_root) display_show_view(dash_root);
    dash_dirty = true;
}

/* Штатное место отрисовки: главный цикл, вне обработчика USB и вне флаша.
 * Инверсию здесь не применяем — её применяет display_housekeeping_task для
 * всех экранов сразу, иначе на неактивном дашборде флаг мог бы залипнуть. */
static void dash_screen_housekeep(void) {
    if (!dash_dirty) return;
    dash_screen_init();
    if (dash_root) dash_render();
}

const eh_screen_t eh_screen_dash = {
    .init      = dash_screen_init,
    .load      = dash_screen_load,
    .housekeep = dash_screen_housekeep,
};
