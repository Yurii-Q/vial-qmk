// Copyright 2026 Ergohaven
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#define SM62_DUALCORE_RENDER 1
#define SM62_STAGE_TRACE 1
/* 960 RGB565 pixels/1920 bytes per full-width strip. CPU render and native
 * service costs remain additional, explicitly unqualified physical budgets. */
#define QP_LVGL_BUFFER_ROWS 28
/* Yield only after a complete LVGL part; ordinary owner loop handles HID. */
#define LV_USE_COOPERATIVE_REFR 1
#define QP_LVGL_SLICE_MS 4
/* One bus/DMA acquisition per complete part; retain operation CS edges. */
#define QP_LVGL_SPI_TRANSACTION
/* Core housekeeping_task owns the sole user dispatch, after the kb hook. */
#define SM62_SINGLE_USER_HOUSEKEEPING
#define SM62_RAW_HID_FRAME_GATE

#define BOOTMAGIC_LITE_ROW          1
#define BOOTMAGIC_LITE_COLUMN       5
#define BOOTMAGIC_LITE_ROW_RIGHT    6
#define BOOTMAGIC_LITE_COLUMN_RIGHT 5

#define SERIAL_USART_FULL_DUPLEX
#define SERIAL_USART_TX_PIN GP0
#define SERIAL_USART_RX_PIN GP1
#define SPLIT_HAND_PIN GP29
#define SELECT_SOFT_SERIAL_SPEED 1
#define EH_APP_LAYOUT_ENABLE
#define EH_SM62_SPLIT_RUNTIME_ENABLE
#define RPC_M2S_BUFFER_SIZE 40
#define SPLIT_TRANSACTION_IDS_USER RPC_SYNC_HID, RPC_SM62_DIAG, RPC_SM62_SETTINGS, RPC_SM62_APP_LAYOUT
#define SPLIT_MODS_ENABLE
#define SPLIT_ACTIVITY_ENABLE
#define SPLIT_LAYER_STATE_ENABLE
#define SPLIT_LED_STATE_ENABLE
#define SPLIT_WATCHDOG_ENABLE
#define SPLIT_MAX_CONNECTION_ERRORS 10 // tolerate transient display/flash stalls before declaring the slave absent
#define SM62_DEFER_HID_RELAY

#define SPI_DRIVER SPID0
#define SPI_SCK_PIN GP2
#define SPI_MOSI_PIN GP3
#define SPI_MISO_PIN GP3
#define LCD_RST_PIN GP4
#define LCD_DC_PIN GP5
#define LCD_CS_PIN GP6
#define BACKLIGHT_PWM_DRIVER PWMD3
#define BACKLIGHT_PWM_CHANNEL RP2040_PWM_CHANNEL_B
#define QUANTUM_PAINTER_LVGL_USE_CUSTOM_CONF
#define QUANTUM_PAINTER_DISPLAY_TIMEOUT 0
#define SM62_DISPLAY_SYNC_DELAY_MS 2500U
#define QUANTUM_PAINTER_TASK_THROTTLE 8
#define QP_ROTATION QP_ROTATION_0

#define TAP_CODE_DELAY 1
#define EH_HAS_DISPLAY
#define EH_SHORT_PRODUCT_NAME "SM62"
// This candidate retains the exact legacy EEPROM format across rebuilds.
#define SM62_PRESERVE_LEGACY_DEFAULT
#define VIAL_TAP_DANCE_ENTRIES 32
#define VIAL_COMBO_ENTRIES 32
#define VIAL_KEY_OVERRIDE_ENTRIES 32

// Keep all historical addresses/macro bytes; extra slots use reserved flash.
#define SM62_VIAL_LEGACY_EXTENSION
