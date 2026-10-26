// Copyright 2026 Ergohaven
// SPDX-License-Identifier: GPL-2.0-or-later
#include "sm62_settings.h"
#include "sm62_side.h"
#include "eeconfig.h"
#include "timer.h"
#include "transactions.h"
#include <string.h>

typedef struct {
    uint8_t marker;
    uint8_t version;
    sm62_side_settings_t side[2];
    uint8_t date_enabled[2];
    uint16_t keymap_hold_ms[2];
} sm62_saved_settings_t;

_Static_assert(sizeof(sm62_saved_settings_t) == KB_SETTINGS_SM62_SIZE, "SM62 settings layout changed");
static sm62_saved_settings_t saved;
/* Reserved LCD block precedes v5 SM62 bytes: no offsets/version changes. */
static uint8_t lcd_idle[3] = {0xA6, 1, 10};
_Static_assert(KB_SETTINGS_LCD_SIZE == sizeof(lcd_idle), "LCD idle layout changed");
static bool timeout_valid(uint8_t minutes) {
    return minutes == 0 || minutes == 1 || minutes == 2 || minutes == 5 ||
           minutes == 10 || minutes == 15 || minutes == 30 || minutes == 60;
}
static bool lcd_idle_valid(const uint8_t *value) {
    return value[0] == 0xA6 && value[1] == 1 && timeout_valid(value[2]);
}
uint8_t get_lcd_timeout_mins(void) { return lcd_idle[2]; }
uint32_t get_lcd_timeout_ms(void) { return (uint32_t)lcd_idle[2] * 60000U; }
static bool pending_sync;
static uint32_t last_sync_attempt;
static uint32_t sync_requested_at;
static bool delay_sync_after_host_set;
static bool pending_persist;
static uint32_t persist_requested_at;
static uint32_t local_mode_changed_at;
static bool local_mode_transition_pending;
static bool local_mode_readback_seen;
static uint32_t local_mode_readback_at;

static void schedule_persist(void) {
    pending_persist = true;
    persist_requested_at = timer_read32();
}

void set_lcd_timeout_mins(uint8_t minutes) {
    if (!timeout_valid(minutes) || lcd_idle[2] == minutes) return;
    lcd_idle[2] = minutes;
    schedule_persist();
    pending_sync = true;
    sync_requested_at = timer_read32();
    delay_sync_after_host_set = true;
}

static void defaults(void) {
    lcd_idle[0] = 0xA6; lcd_idle[1] = 1; lcd_idle[2] = 10;
    saved.marker = 0x62;
    saved.version = 5;
    /* Factory startup: left clock, right continuous ship; both show the
     * local layout while keys are pressed. The idle timeout is irrelevant
     * in ship mode, so keep the right side at zero to make that explicit. */
    saved.side[0] = (sm62_side_settings_t){.mode = 0, .ship_enabled = 0,
                                         .ship_timeout_s = 20, .keymap_enabled = 1,
                                         .cyrillic_enabled = 1};
    saved.side[1] = (sm62_side_settings_t){.mode = 2, .ship_enabled = 1,
                                         .ship_timeout_s = 0, .keymap_enabled = 1,
                                         .cyrillic_enabled = 1};
    saved.date_enabled[0] = 1;
    saved.date_enabled[1] = 1;
    saved.keymap_hold_ms[0] = 350;
    saved.keymap_hold_ms[1] = 350;
}

static bool valid(const sm62_saved_settings_t *value) {
    if (value->marker != 0x62 || (value->version < 1 || value->version > 5)) return false;
    for (uint8_t side = 0; side < 2; ++side) {
        const sm62_side_settings_t *s = &value->side[side];
        if (s->mode > 3 || s->ship_enabled > 1 || s->ship_timeout_s > 240 ||
            s->keymap_enabled > 1 || s->cyrillic_enabled > 1) return false;
    }
    if (value->version >= 2 && (value->date_enabled[0] > (value->version >= 5 ? 15 : value->version == 4 ? 3 : 1) || value->date_enabled[1] > (value->version >= 5 ? 15 : value->version == 4 ? 3 : 1))) return false;
    if (value->version >= 3 && (value->keymap_hold_ms[0] > 10000 || value->keymap_hold_ms[1] > 10000)) return false;
    return true;
}

static void persist(void) {
    eeconfig_update_kb_datablock(&saved, KB_SETTINGS_SM62_OFFSET, sizeof(saved));
    eeconfig_update_kb_datablock(lcd_idle, KB_SETTINGS_LCD_OFFSET, sizeof(lcd_idle));
}

void kb_settings_extra_reset(void) {
    defaults();
    persist();
    pending_sync = true;
}

void kb_settings_extra_init(void) {
    eeconfig_read_kb_datablock(lcd_idle, KB_SETTINGS_LCD_OFFSET, sizeof(lcd_idle));
    if (!lcd_idle_valid(lcd_idle)) {
        lcd_idle[0] = 0xA6; lcd_idle[1] = 1; lcd_idle[2] = 10;
        eeconfig_update_kb_datablock(lcd_idle, KB_SETTINGS_LCD_OFFSET, sizeof(lcd_idle));
    }
    eeconfig_read_kb_datablock(&saved, KB_SETTINGS_SM62_OFFSET, sizeof(saved));
    if (!valid(&saved)) {
        defaults();
        persist();
    } else if (saved.version < 5) {
        /* Keep existing display settings when extending EEPROM. */
        if (saved.version == 1) {
            saved.date_enabled[0] = 1;
            saved.date_enabled[1] = 1;
        }
        if (saved.version < 3) {
            saved.keymap_hold_ms[0] = 350;
            saved.keymap_hold_ms[1] = 350;
        }
        saved.version = 5;
        persist();
    }
    pending_sync = true;
}

const sm62_side_settings_t *sm62_local_display_settings(void) {
    return &saved.side[sm62_local_side_index()];
}

const sm62_side_settings_t *sm62_remote_display_settings(void) {
    return &saved.side[1U - sm62_local_side_index()];
}

bool sm62_local_date_enabled(void) {
    return (saved.date_enabled[sm62_local_side_index()] & 1U) != 0;
}

// Version4 uses bit1 of the date byte as language-hidden; old EEPROM
// bytes 0/1 therefore preserve date and default to language visible.
bool sm62_local_language_enabled(void) {
    return (saved.date_enabled[sm62_local_side_index()] & 2U) == 0;
}

// Version5: bit2 disables clock/player stars; bit3 disables this panel.
bool sm62_local_stars_enabled(void) { return (saved.date_enabled[sm62_local_side_index()] & 4U) == 0; }
bool sm62_local_screens_off(void) { return (saved.date_enabled[sm62_local_side_index()] & 8U) != 0; }
bool display_user_enabled_kb(void) { return !sm62_local_screens_off(); }

uint16_t sm62_local_keymap_hold_ms(void) {
    return saved.keymap_hold_ms[sm62_local_side_index()];
}

int sm62_setting_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    if (proto->qsid >= 244 && proto->qsid <= 249) {
        if (maxsz < 1) return -1;
        uint8_t bit = 2U << ((proto->qsid - 244) / 2U);
        bool stored = (saved.date_enabled[(proto->qsid - 244) % 2U] & bit) != 0;
        ((uint8_t *)setting)[0] = proto->qsid >= 248 ? stored : !stored;
        return 0;
    }
    if (proto->qsid >= 242 && proto->qsid <= 243) {
        if (maxsz < 2) return -1;
        uint16_t value = saved.keymap_hold_ms[proto->qsid - 242];
        ((uint8_t *)setting)[0] = value & 0xFF;
        ((uint8_t *)setting)[1] = value >> 8;
        return 0;
    }
    if (maxsz < 1 || proto->qsid < 230 || proto->qsid > 241) return -1;
    uint8_t index = proto->qsid - 230;
    ((uint8_t *)setting)[0] = index < 10 ? ((const uint8_t *)saved.side)[index] : (saved.date_enabled[index - 10] & 1U);
    if (local_mode_transition_pending && index == (sm62_local_side_index()) * 5) {
        local_mode_readback_seen = true;
        local_mode_readback_at = timer_read32();
    }
    return 0;
}

int sm62_setting_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    if (proto->qsid >= 244 && proto->qsid <= 249) {
        if (maxsz < 1 || ((const uint8_t *)setting)[0] > 1) return -1;
        uint8_t bit = 2U << ((proto->qsid - 244) / 2U);
        uint8_t *target = &saved.date_enabled[(proto->qsid - 244) % 2U];
        bool requested = ((const uint8_t *)setting)[0] != 0;
        bool stored = proto->qsid >= 248 ? requested : !requested;
        uint8_t value = (*target & ~bit) | (stored ? bit : 0U);
        if (*target != value) {
            *target = value;
            schedule_persist();
            pending_sync = true;
            sync_requested_at = timer_read32();
            delay_sync_after_host_set = true;
        }
        return 0;
    }
    if (proto->qsid >= 242 && proto->qsid <= 243) {
        if (maxsz < 2) return -1;
        uint16_t value = ((const uint8_t *)setting)[0] | ((uint16_t)((const uint8_t *)setting)[1] << 8);
        if (value > 10000) return -1;
        uint16_t *target = &saved.keymap_hold_ms[proto->qsid - 242];
        if (*target != value) {
            *target = value;
            schedule_persist();
            pending_sync = true;
            sync_requested_at = timer_read32();
            delay_sync_after_host_set = true;
        }
        return 0;
    }
    if (maxsz < 1 || proto->qsid < 230 || proto->qsid > 241) return -1;
    uint8_t index = proto->qsid - 230;
    uint8_t value = ((const uint8_t *)setting)[0];
    if (index < 10 && ((index % 5 == 0 && value > 3) || (index % 5 == 2 && value > 240) ||
        (index % 5 != 0 && index % 5 != 2 && value > 1))) return -1;
    if (index >= 10 && value > 1) return -1;
    uint8_t *target = index < 10 ? &((uint8_t *)saved.side)[index] : &saved.date_enabled[index - 10];
    if (index >= 10) value |= *target & 0xFEU; // date edits retain all independent display flags
    bool changed = *target != value;
    if (changed) {
        if (index < 10 && index % 5 == 0 && index / 5 == sm62_local_side_index()) {
            local_mode_changed_at = timer_read32();
            local_mode_transition_pending = true;
            local_mode_readback_seen = false;
        }
        if (index < 10 && index % 5 == 0) {
            sm62_side_settings_t *side = &saved.side[index / 5];
            /* The factory ship has a zero idle delay. On leaving that mode,
             * do not let its old idle setting immediately hide the new view.
             * Idle ship remains an explicit option the user can re-enable. */
            if (side->mode == 2 && value != 2) {
                side->ship_enabled = 0;
                if (side->ship_timeout_s == 0) side->ship_timeout_s = 20;
            } else if (value == 2) {
                side->ship_enabled = 1;
            }
        }
        *target = value;
        schedule_persist();
        pending_sync = true;
        sync_requested_at = timer_read32();
        delay_sync_after_host_set = true;
    }
    return 0;
}

bool sm62_local_mode_transition_pending(void) {
    if (!local_mode_transition_pending) return false;
    /* Let the USB-side SET ACK and Entropy's verified GET finish before a
     * full-screen LVGL transition can occupy the keyboard main loop. */
    /* Wait for Entropy's verified GET, then leave a short gap for its HID
     * response to leave USB before LVGL starts a full-screen repaint. If a
     * different host never reads back, release the view after a bounded wait. */
    if (local_mode_readback_seen) {
        if (timer_elapsed32(local_mode_readback_at) < 250U) return true;
    } else if (timer_elapsed32(local_mode_changed_at) < 4000U) {
        return true;
    }
    local_mode_transition_pending = false;
    return false;
}

static void sm62_settings_rpc(uint8_t in_len, const void *in, uint8_t out_len, void *out) {
    (void)out_len;
    (void)out;
    if (in_len != sizeof(saved) + sizeof(lcd_idle) || !in) return;
    sm62_saved_settings_t received;
    memcpy(&received, in, sizeof(received));
    const uint8_t *received_idle = (const uint8_t *)in + sizeof(saved);
    if (!valid(&received) || !lcd_idle_valid(received_idle)) return;
    if (memcmp(&saved, &received, sizeof(saved)) == 0 && memcmp(lcd_idle, received_idle, sizeof(lcd_idle)) == 0) return;
    saved = received;
    memcpy(lcd_idle, received_idle, sizeof(lcd_idle));
    schedule_persist();
}

void sm62_settings_register_rpc(void) {
    transaction_register_rpc(RPC_SM62_SETTINGS, sm62_settings_rpc);
}

void sm62_settings_sync_task(void) {
    // Flash-backed EEPROM must not be written from a raw-HID SET or split RPC
    // callback while the transport response is pending. Coalesce edits here.
    // Leave the host's mode-switch readback window clear of flash writes too.
    if (pending_persist && timer_elapsed32(persist_requested_at) >= 4000U) {
        persist();
        pending_persist = false;
    }
    /* Replay heals a slave reboot/lost acknowledgement; unchanged RPCs do not write. */
    if (!is_keyboard_master() || timer_elapsed32(last_sync_attempt) < (pending_sync ? 500U : 5000U)) return;
    /* The USB-side SET is acknowledged before the other half is repainted.
     * Keep the host's immediate verified GET clear of a synchronous split RPC;
     * remote mode changes still arrive promptly after that readback. */
    if (delay_sync_after_host_set &&
        (timer_elapsed32(sync_requested_at) < 1000U || local_mode_transition_pending)) return;
    last_sync_attempt = timer_read32();
    uint8_t packet[sizeof(saved) + sizeof(lcd_idle)];
    memcpy(packet, &saved, sizeof(saved));
    memcpy(packet + sizeof(saved), lcd_idle, sizeof(lcd_idle));
    if (transaction_rpc_send(RPC_SM62_SETTINGS, sizeof(packet), packet)) {
        pending_sync = false;
        delay_sync_after_host_set = false;
    }
}
