/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT zmk_input_processor_scroll_scale
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#error "scroll-scale requires a central-side devicetree instance"
#endif

#include <stdint.h>
#include <drivers/input_processor.h>

struct scroll_scale_config {
    uint16_t wheel_code;
    uint16_t hwheel_code;
    uint16_t touch_code;
    int32_t scale_percent;
};

struct scroll_scale_data {
    int32_t wheel_remainder;
    int32_t hwheel_remainder;
};

static int scale_input(const struct device *dev, struct input_event *event,
                       uint32_t param1, uint32_t param2,
                       struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);
    const struct scroll_scale_config *cfg = dev->config;
    struct scroll_scale_data *data = dev->data;

    if (event->type == INPUT_EV_KEY && event->code == cfg->touch_code) {
        /* A new gesture must not inherit a previous gesture's partial wheel step. */
        data->wheel_remainder = 0;
        data->hwheel_remainder = 0;
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    int32_t *remainder;
    if (event->code == cfg->wheel_code) {
        remainder = &data->wheel_remainder;
    } else if (event->code == cfg->hwheel_code) {
        remainder = &data->hwheel_remainder;
    } else {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    int64_t total = (int64_t)event->value * cfg->scale_percent + *remainder;
    event->value = (int32_t)(total / 100);
    *remainder = (int32_t)(total % 100);
    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api api = {.handle_event = scale_input};

#define CREATE_INSTANCE(n)                                                                        \
    BUILD_ASSERT(DT_INST_PROP(n, scale_percent) > 0 && DT_INST_PROP(n, scale_percent) <= 100,      \
                 "scale-percent must be between 1 and 100");                                   \
    BUILD_ASSERT(DT_INST_PROP(n, wheel_code) != DT_INST_PROP(n, hwheel_code),                      \
                 "wheel and hwheel codes must differ");                                         \
    static struct scroll_scale_data data_##n;                                                     \
    static const struct scroll_scale_config config_##n = {                                        \
        .wheel_code = DT_INST_PROP(n, wheel_code),                                                \
        .hwheel_code = DT_INST_PROP(n, hwheel_code),                                              \
        .touch_code = DT_INST_PROP(n, touch_code),                                                \
        .scale_percent = DT_INST_PROP(n, scale_percent),                                          \
    };                                                                                            \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &data_##n, &config_##n, POST_KERNEL,                     \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &api);

DT_INST_FOREACH_STATUS_OKAY(CREATE_INSTANCE)
#endif
