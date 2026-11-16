// SPDX-License-Identifier: GPL-2.0-or-later
#include "keyboard.h"

/* Обслуживание ввода во время перерисовки экрана (только SM62).
 *
 * Вызывается из хука qp_lvgl_flush_input_service() между полосами кадра.
 * Задача одна: отдать накопившиеся нажатия до того, как LVGL начнёт
 * передавать следующую полосу. Без этого нажатие и отпускание могут
 * целиком уложиться в блокирующую передачу по SPI, и сканер матрицы
 * не увидит ни одного фронта.
 *
 * Почему именно keyboard_task(), а не что-то своё: это тот же владелец
 * состояния, что и в главном цикле, — протокол, HID, housekeep и painter
 * внутри не вызываются и не могут рекурсивно запустить флаш.
 *
 * sm62_input_initialized — до первого обычного прохода главного цикла хук
 * молчит: инициализационные перерисовки не должны дёргать неготовый ввод.
 * sm62_input_running — защита от рекурсии: keyboard_task() может привести
 * к новой отрисовке, а та — к новому вызову хука. */
static bool sm62_input_initialized;
static bool sm62_input_running;

void protocol_keyboard_task(void) {
    if (sm62_input_running) return;
    sm62_input_running = true;
    keyboard_task();
    sm62_input_running = false;
    sm62_input_initialized = true;
}

void qp_lvgl_flush_input_service(void) {
#if defined(SM62_DUALCORE_RENDER)
    /* Рендер-ядро не имеет права входить в keyboard/TD/USB/split. */
    return;
#else
    if (!sm62_input_initialized || sm62_input_running) return;
    protocol_keyboard_task();
#endif
}
