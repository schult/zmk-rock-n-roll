#include <device.h>
#include <drivers/behavior.h>
#include <logging/log.h>
#include <sys/dlist.h>
#include <kernel.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/hid.h>
#include <zmk/matrix.h>
#include <zmk/keymap.h>


#define DT_DRV_COMPAT zmk_rocknroll

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define LAYER_CHILD_LEN(node) 1 +
#define ZMK_ROCKNROLL_NODE DT_DRV_INST(0)
#define ZMK_KEY_POSITIONS_LEN DT_PROP_LEN(ZMK_ROCKNROLL_NODE, key_positions)
#define ZMK_LAYERS_LEN (DT_INST_FOREACH_CHILD(0, LAYER_CHILD_LEN) 0)
#define CHOOSE2(n) ((n*n+n)/2)
#define ZMK_BINDINGS_LEN (CHOOSE2(ZMK_KEY_POSITIONS_LEN) * 4)

#define BINDING_WITH_COMMA(idx, drv_inst) ZMK_KEYMAP_EXTRACT_BINDING(idx, drv_inst),
#define TRANSFORMED_LAYER(node)                                                                    \
    {UTIL_LISTIFY(DT_PROP_LEN(node, bindings), BINDING_WITH_COMMA, node)},

static int32_t zmk_key_positions[] = DT_PROP(ZMK_ROCKNROLL_NODE, key_positions);
static struct zmk_behavior_binding zmk_bindings[ZMK_LAYERS_LEN][ZMK_BINDINGS_LEN] = {
    DT_INST_FOREACH_CHILD(0, TRANSFORMED_LAYER) };

static struct {
    int key1;
    int key2;
    int num_pressed;
} state = {
    .key1 = -1,
    .key2 = -1,
    .num_pressed = 0,
};

static int permutation_id(int32_t key1, int32_t key2) {
    // Items are numbered starting at 0; -1 is treated as NONE.
    // Permutation order follows this pattern:
    // 0, NONE  <section 0>
    // 1, NONE  <section 1>
    // 1, 0
    // 2, NONE  <section 2>
    // 2, 0
    // 2, 1
    // 3, NONE  <section 3>
    // 3, 0
    // 3, 1
    // 3, 2

    int min = (key1 < key2) ? key1 : key2;
    int max = (key1 > key2) ? key1 : key2;
    int section = (max * max + max) / 2;
    return section + min + 1;
}

enum Action {
    ROLL,
    ROCK
};

static int behavior_index(int32_t key1, int32_t key2, enum Action action ) {
    int pid = permutation_id(key1, key2);
    int action_offset = (action * 2) + ((key1 < key2) ? 1 : 0);
    return (pid * 4) + action_offset;
}

static int rocknroll_listener(const zmk_event_t *ev) {
    struct zmk_position_state_changed *data = as_zmk_position_state_changed(ev);
    if (data == NULL) {
        return 0;
    }

    int key_idx = 0;
    for(; key_idx < ZMK_KEY_POSITIONS_LEN; ++key_idx) {
        if (zmk_key_positions[key_idx] == data->position) {
            break;
        }
    }
    if (key_idx == ZMK_KEY_POSITIONS_LEN) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (data->state) { // keydown
        state.num_pressed += 1;
        if (state.num_pressed == 1) {
            state.key1 = key_idx;
        } else if (state.num_pressed == 2 && state.key1 >= 0) {
            state.key2 = key_idx;
        } else {
            state.key1 = -1;
            state.key2 = -1;
        }
    } else { // keyup
        state.num_pressed -= 1;
        uint8_t layer = zmk_keymap_highest_layer_active();
        if (state.key1 == key_idx) { // Roll or Single Key
            int b = behavior_index(state.key1, state.key2, ROLL);
            if (b < ZMK_BINDINGS_LEN) {
                struct zmk_behavior_binding_event event = {
                    .position = data->position,
                    .timestamp = data->timestamp,
                };
                behavior_keymap_binding_pressed(&zmk_bindings[layer][b], event);
                behavior_keymap_binding_released(&zmk_bindings[layer][b], event);
            }
        } else if (state.key2 == key_idx) { // Rock
            int b = behavior_index(state.key1, state.key2, ROCK);
            if (b < ZMK_BINDINGS_LEN) {
                struct zmk_behavior_binding_event event = {
                    .position = data->position,
                    .timestamp = data->timestamp,
                };
                behavior_keymap_binding_pressed(&zmk_bindings[layer][b], event);
                behavior_keymap_binding_released(&zmk_bindings[layer][b], event);
            }
        }

        state.key1 = -1;
        state.key2 = -1;
    }

    return ZMK_EV_EVENT_HANDLED;
}

ZMK_LISTENER(rocknroll, rocknroll_listener);
ZMK_SUBSCRIPTION(rocknroll, zmk_position_state_changed);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
