/* SM62 internal split transport. Both halves run the same image. USB wire v8
 * remains unchanged. Bounded RPC frames and a whole-image CRC stage all data
 * before publication; no partial map, name or execution state is visible. */
#define EH_SM62_SPLIT_RUNTIME_IMPLEMENTED 1
#define EH_SM62_SPLIT_DEFAULT_BINDINGS 1
typedef struct {
    uint32_t revision;
    uint16_t keycodes[16][62], default_keycodes[16][62];
    uint8_t visuals[16][62], states[16][62];
    char name[23], layer_names[16][23];
    char defaults[16][LAYER_LABEL_SIZE];
    uint8_t identity[20], identity_seen, active, format;
    uint16_t crc;
} sm62_app_image_t;
static sm62_app_image_t sm62_app_tx, sm62_app_rx;
static char sm62_app_defaults[16][LAYER_LABEL_SIZE];
static uint16_t sm62_app_default_keycodes[16][62];
static uint32_t sm62_app_tx_epoch, sm62_app_rx_epoch, sm62_app_tx_generation;
static uint32_t sm62_app_tx_time, sm62_app_rx_time;
static bool sm62_app_rx_invalid;
static uint8_t sm62_app_rx_pending;
static uint16_t sm62_app_tx_offset, sm62_app_rx_offset;
static bool sm62_app_tx_live, sm62_app_tx_started, sm62_app_rx_seen, sm62_app_rx_published;

const char *hid_sm62_default_layer_name(uint8_t layer) {
    return !is_keyboard_master() && sm62_app_rx_published && layer < 16 ? sm62_app_defaults[layer] : layer_name(layer);
}

uint16_t hid_sm62_default_keycode(uint8_t layer, uint8_t row, uint8_t col) {
    if (!is_keyboard_master() && sm62_app_rx_published && layer < 16) {
        for (uint8_t c = 0; c < 62; ++c) {
            if (sm62_application_positions[c][0] == row && sm62_application_positions[c][1] == col) return sm62_app_default_keycodes[layer][c];
        }
    }
    return dynamic_keymap_get_keycode(layer, row, col);
}

static uint16_t sm62_app_crc(const void *data, uint16_t size) {
    const uint8_t *p = data;
    uint16_t crc = 0xFFFF;
    while (size--) crc = app_layout_crc16_byte(crc, *p++);
    return crc;
}
static void sm62_app_put32(uint8_t *p, uint32_t n) {
    for (uint8_t i = 0; i < 4; ++i) p[i] = n >> (8 * i);
}
static void sm62_app_capture(void) {
    memset(&sm62_app_tx, 0, sizeof(sm62_app_tx));
    sm62_app_tx.format = 1;
    sm62_app_tx.active = app_layout_session_live();
    sm62_app_tx.revision = app_layout_revision;
    memcpy(sm62_app_tx.keycodes, app_layout_keycodes, sizeof(app_layout_keycodes));
    memcpy(sm62_app_tx.visuals, app_layout_visuals, sizeof(app_layout_visuals));
    memcpy(sm62_app_tx.states, app_layout_execution_states, sizeof(app_layout_execution_states));
    memcpy(sm62_app_tx.name, app_layout_name, sizeof(app_layout_name));
    memcpy(sm62_app_tx.layer_names, app_layout_layer_names, sizeof(app_layout_layer_names));
    memcpy(sm62_app_tx.identity, app_layout_wake_identity, sizeof(app_layout_wake_identity));
    for (uint8_t l = 0; l < 16; ++l) {
        snprintf(sm62_app_tx.defaults[l], LAYER_LABEL_SIZE, "%s", layer_name(l));
        for (uint8_t c = 0; c < 62; ++c) {
            sm62_app_tx.default_keycodes[l][c] = dynamic_keymap_get_keycode(l, sm62_application_positions[c][0], sm62_application_positions[c][1]);
        }
    }
    sm62_app_tx.identity_seen = app_layout_wake_identity_seen;
    sm62_app_tx.crc = sm62_app_crc(&sm62_app_tx, offsetof(sm62_app_image_t, crc));
}
static bool sm62_app_image_valid(void) {
    if (sm62_app_rx.format != 1 || sm62_app_rx.active > 1 || sm62_app_rx.identity_seen > 1 ||
        sm62_app_rx.crc != sm62_app_crc(&sm62_app_rx, offsetof(sm62_app_image_t, crc)) ||
        sm62_app_rx.name[22] != 0) return false;
    for (uint8_t l = 0; l < 16; ++l) {
        if (sm62_app_rx.layer_names[l][22] != 0 || sm62_app_rx.defaults[l][LAYER_LABEL_SIZE - 1] != 0) return false;
        for (uint8_t c = 0; c < 62; ++c) if (sm62_app_rx.states[l][c] > 3) return false;
    }
    return true;
}
static void sm62_app_publish(void) {
    // Called only from the main loop. ChibiOS executes RPC callbacks in its
    // serial worker thread: publishing there races LVGL/read-side accessors.
    // Matrix actions are dispatched by the USB master for BOTH physical halves.
    // The peer's copy is read-only display state, never an independent consumer.
    app_layout_release_held();
    memcpy(app_layout_keycodes, sm62_app_rx.keycodes, sizeof(app_layout_keycodes));
    memcpy(app_layout_visuals, sm62_app_rx.visuals, sizeof(app_layout_visuals));
    memcpy(app_layout_execution_states, sm62_app_rx.states, sizeof(app_layout_execution_states));
    memcpy(app_layout_name, sm62_app_rx.name, sizeof(app_layout_name));
    memcpy(app_layout_layer_names, sm62_app_rx.layer_names, sizeof(app_layout_layer_names));
    memcpy(sm62_app_defaults, sm62_app_rx.defaults, sizeof(sm62_app_defaults));
    memcpy(sm62_app_default_keycodes, sm62_app_rx.default_keycodes, sizeof(sm62_app_default_keycodes));
    sm62_app_rx_published = true;
    app_layout_active = sm62_app_rx.active && timer_elapsed32(sm62_app_rx_time) < EH_APP_LAYOUT_TIMEOUT_MS;
    app_layout_revision = sm62_app_rx.revision;
    app_layout_sync_time = timer_read32();
    app_layout_visual_generation++;
    if (sm62_app_rx.identity_seen) {
        bool changed = !app_layout_wake_identity_seen || memcmp(app_layout_wake_identity, sm62_app_rx.identity, 20) != 0;
        memcpy(app_layout_wake_identity, sm62_app_rx.identity, 20);
        app_layout_wake_identity_seen = true;
        if (changed) hid_app_layout_changed_kb();
    }
}
static void sm62_app_rpc(uint8_t in_len, const void *in, uint8_t out_len, void *out) {
    if (out_len != 5 || !out) return;
    uint8_t *ack = out;
    memset(ack, 0, 5);
    sm62_app_put32(ack + 1, sm62_app_rx_epoch);
    if (is_keyboard_master() || in_len != 40 || !in) return;
    const uint8_t *p = in;
    uint16_t crc = p[38] | ((uint16_t)p[39] << 8);
    if (crc != sm62_app_crc(p, 38)) return;
    uint32_t epoch = app_layout_read_u32(p);
    uint16_t offset = p[4] | ((uint16_t)p[5] << 8);
    uint8_t n = p[6];
    if (!n || n > 31 || offset % 31 || (uint32_t)offset + n > sizeof(sm62_app_rx) ||
        (n != 31 && offset + n != sizeof(sm62_app_rx))) return;
    // Freeze the completed buffer until main-loop publication finishes.
    // A lost final ACK can still be retried without overwriting that buffer.
    if (__atomic_load_n(&sm62_app_rx_pending, __ATOMIC_ACQUIRE)) {
        if (epoch == sm62_app_rx_epoch && offset + n <= sm62_app_rx_offset &&
            memcmp((uint8_t *)&sm62_app_rx + offset, p + 7, n) == 0) ack[0] = 1;
        return;
    }
    if (!sm62_app_rx_seen || (int32_t)(epoch - sm62_app_rx_epoch) > 0) {
        if (offset != 0) return;
        sm62_app_rx_seen = true;
        sm62_app_rx_epoch = epoch;
        sm62_app_rx_offset = 0;
        sm62_app_rx_time = timer_read32();
        sm62_app_rx_invalid = false;
    }
    sm62_app_put32(ack + 1, sm62_app_rx_epoch);
    if (epoch != sm62_app_rx_epoch) return;
    if (sm62_app_rx_offset < sizeof(sm62_app_rx) && timer_elapsed32(sm62_app_rx_time) >= EH_APP_LAYOUT_TIMEOUT_MS) sm62_app_rx_invalid = true;
    if (sm62_app_rx_invalid) return;
    if (offset < sm62_app_rx_offset) {
        // RPC response loss is harmless, including a duplicate final frame.
        ack[0] = offset + n <= sm62_app_rx_offset && memcmp((uint8_t *)&sm62_app_rx + offset, p + 7, n) == 0;
        return;
    }
    if (offset != sm62_app_rx_offset) return;
    memcpy((uint8_t *)&sm62_app_rx + offset, p + 7, n);
    // Expire an idle/incomplete transfer, not a steadily progressing image.
    // Display/SPI work can stretch the complete image beyond one host TTL.
    sm62_app_rx_time = timer_read32();
    sm62_app_rx_offset += n;
    if (sm62_app_rx_offset == sizeof(sm62_app_rx)) {
        if (!sm62_app_image_valid()) {
            sm62_app_rx_invalid = true;
            return;
        }
        __atomic_store_n(&sm62_app_rx_pending, 1, __ATOMIC_RELEASE);
    }
    ack[0] = 1;
}
void hid_app_layout_split_init(void) {
    transaction_register_rpc(RPC_SM62_APP_LAYOUT, sm62_app_rpc);
}
static void sm62_app_split_step(void) {
    if (!is_keyboard_master()) return;
    bool live = app_layout_session_live();
    bool changed = !sm62_app_tx_started || sm62_app_tx_live != live ||
                   (sm62_app_tx_offset == sizeof(sm62_app_tx) && sm62_app_tx_generation != app_layout_visual_generation);
    if (changed || (sm62_app_tx_offset == sizeof(sm62_app_tx) && timer_elapsed32(sm62_app_tx_time) >= 500)) {
        sm62_app_capture();
        sm62_app_tx_generation = app_layout_visual_generation;
        sm62_app_tx_live = live;
        sm62_app_tx_epoch++;
        sm62_app_tx_offset = 0;
        sm62_app_tx_started = true;
    }
    if (sm62_app_tx_offset == sizeof(sm62_app_tx)) return;
    uint8_t packet[40] = {0}, ack[5] = {0};
    sm62_app_put32(packet, sm62_app_tx_epoch);
    packet[4] = sm62_app_tx_offset;
    packet[5] = sm62_app_tx_offset >> 8;
    uint16_t remaining = sizeof(sm62_app_tx) - sm62_app_tx_offset;
    uint8_t n = remaining > 31 ? 31 : remaining;
    packet[6] = n;
    memcpy(packet + 7, (uint8_t *)&sm62_app_tx + sm62_app_tx_offset, n);
    uint16_t crc = sm62_app_crc(packet, 38);
    packet[38] = crc; packet[39] = crc >> 8;
    if (!transaction_rpc_exec(RPC_SM62_APP_LAYOUT, sizeof(packet), packet, sizeof(ack), ack)) return;
    if (ack[0]) {
        sm62_app_tx_offset += n;
        if (sm62_app_tx_offset == sizeof(sm62_app_tx)) sm62_app_tx_time = timer_read32();
    } else {
        // Peer retained a newer epoch across a USB-master reset. Negotiate a
        // strictly newer full image rather than accepting any old BEGIN.
        uint32_t peer_epoch = app_layout_read_u32(ack + 1);
        if ((int32_t)(peer_epoch - sm62_app_tx_epoch) >= 0) sm62_app_tx_epoch = peer_epoch + 1;
        else sm62_app_tx_epoch++;
        sm62_app_tx_offset = 0;
    }
}
void hid_app_layout_split_task(void) {
    if (!is_keyboard_master()) {
        if (__atomic_load_n(&sm62_app_rx_pending, __ATOMIC_ACQUIRE)) {
            sm62_app_publish();
            __atomic_store_n(&sm62_app_rx_pending, 0, __ATOMIC_RELEASE);
        }
        return;
    }
    // Bounded work: at most four 40-byte payloads per loop; service matrix and
    // display work between batches. No full-image heap allocation is required.
    for (uint8_t i = 0; i < 4; ++i) sm62_app_split_step();
}
