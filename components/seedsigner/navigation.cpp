#include "navigation.h"

#define NAV_INDEX_NONE ((size_t)-1)

extern "C" __attribute__((weak)) void seedsigner_lvgl_on_aux_key(const char *key_name) {
    (void)key_name;
}

typedef struct {
    lv_group_t *group;
    lv_obj_t *top_items[2];
    size_t top_count;
    lv_obj_t **body_items;
    size_t body_count;
    nav_zone_t zone;
    nav_body_layout_t body_layout;
    nav_aux_policy_t aux_policy;
} nav_ctx_t;

static bool is_aux_key(uint32_t key, int *idx_out) {
#ifdef LV_KEY_F1
    if (key == LV_KEY_F1) { *idx_out = 1; return true; }
#endif
#ifdef LV_KEY_F2
    if (key == LV_KEY_F2) { *idx_out = 2; return true; }
#endif
#ifdef LV_KEY_F3
    if (key == LV_KEY_F3) { *idx_out = 3; return true; }
#endif
    if (key == (uint32_t)'1') { *idx_out = 1; return true; }
    if (key == (uint32_t)'2') { *idx_out = 2; return true; }
    if (key == (uint32_t)'3') { *idx_out = 3; return true; }
    return false;
}

static nav_aux_action_t action_for_aux(const nav_ctx_t *ctx, int idx) {
    if (!ctx) return NAV_AUX_NOOP;
    if (idx == 1) return ctx->aux_policy.key1;
    if (idx == 2) return ctx->aux_policy.key2;
    if (idx == 3) return ctx->aux_policy.key3;
    return NAV_AUX_NOOP;
}

static void activate_focused(nav_ctx_t *ctx) {
    if (!ctx || !ctx->group) return;
    lv_obj_t *obj = lv_group_get_focused(ctx->group);
    if (!obj) return;
    lv_event_send(obj, LV_EVENT_CLICKED, NULL);
}

static bool focus_top(nav_ctx_t *ctx, size_t idx) {
    if (!ctx || idx >= ctx->top_count) return false;
    lv_group_focus_obj(ctx->top_items[idx]);
    ctx->zone = NAV_ZONE_TOP;
    return true;
}

static bool focus_body(nav_ctx_t *ctx, size_t idx) {
    if (!ctx || idx >= ctx->body_count) return false;
    lv_group_focus_obj(ctx->body_items[idx]);
    ctx->zone = NAV_ZONE_BODY;
    return true;
}

static int focused_index_in(nav_ctx_t *ctx, lv_obj_t **arr, size_t count) {
    if (!ctx || !ctx->group || !arr) return -1;
    lv_obj_t *f = lv_group_get_focused(ctx->group);
    if (!f) return -1;
    for (size_t i = 0; i < count; ++i) {
        if (arr[i] == f) return (int)i;
    }
    return -1;
}

static size_t first_valid_body_index(nav_ctx_t *ctx) {
    if (!ctx || !ctx->body_items) return NAV_INDEX_NONE;
    for (size_t i = 0; i < ctx->body_count; ++i) {
        if (ctx->body_items[i]) return i;
    }
    return NAV_INDEX_NONE;
}

static void nav_key_handler(lv_event_t *e) {
    if (!e) return;
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;

    nav_ctx_t *ctx = (nav_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;

    uint32_t key = lv_event_get_key(e);

    int aux_idx = 0;
    if (is_aux_key(key, &aux_idx)) {
        nav_aux_action_t action = action_for_aux(ctx, aux_idx);
        if (action == NAV_AUX_ENTER) {
            activate_focused(ctx);
        } else if (action == NAV_AUX_EMIT) {
            if (aux_idx == 1) seedsigner_lvgl_on_aux_key("KEY1");
            else if (aux_idx == 2) seedsigner_lvgl_on_aux_key("KEY2");
            else if (aux_idx == 3) seedsigner_lvgl_on_aux_key("KEY3");
        }
        return;
    }

    if (key == LV_KEY_ENTER) {
        activate_focused(ctx);
        return;
    }

    int top_i = focused_index_in(ctx, ctx->top_items, ctx->top_count);
    int body_i = focused_index_in(ctx, ctx->body_items, ctx->body_count);

    if (top_i >= 0) ctx->zone = NAV_ZONE_TOP;
    if (body_i >= 0) ctx->zone = NAV_ZONE_BODY;

    if (ctx->body_layout == NAV_BODY_VERTICAL) {
        if (key == LV_KEY_UP) {
            if (ctx->zone == NAV_ZONE_BODY) {
                if (body_i > 0) {
                    focus_body(ctx, (size_t)(body_i - 1));
                } else if (body_i == 0 && ctx->top_count > 0) {
                    focus_top(ctx, 0);
                }
            }
            return;
        }

        if (key == LV_KEY_DOWN) {
            if (ctx->zone == NAV_ZONE_TOP) {
                if (ctx->body_count > 0) focus_body(ctx, 0);
            } else if (ctx->zone == NAV_ZONE_BODY && body_i >= 0 && (size_t)(body_i + 1) < ctx->body_count) {
                focus_body(ctx, (size_t)(body_i + 1));
            }
            return;
        }

        if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
            if (ctx->zone == NAV_ZONE_TOP && ctx->top_count > 1 && top_i >= 0) {
                if (key == LV_KEY_LEFT && top_i > 0) focus_top(ctx, (size_t)(top_i - 1));
                else if (key == LV_KEY_RIGHT && (size_t)(top_i + 1) < ctx->top_count) focus_top(ctx, (size_t)(top_i + 1));
            }
            return;
        }
    }
}

static void nav_cleanup_handler(lv_event_t *e) {
    if (!e) return;
    if (lv_event_get_code(e) != LV_EVENT_DELETE) return;

    nav_ctx_t *ctx = (nav_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;

    if (ctx->group) {
        lv_group_del(ctx->group);
        ctx->group = NULL;
    }
    if (ctx->body_items) {
        lv_mem_free(ctx->body_items);
        ctx->body_items = NULL;
    }
    lv_mem_free(ctx);
}

void nav_bind(const nav_config_t *cfg) {
    if (!cfg || !cfg->screen) return;

    nav_ctx_t *ctx = (nav_ctx_t *)lv_mem_alloc(sizeof(nav_ctx_t));
    if (!ctx) return;
    lv_memset_00(ctx, sizeof(*ctx));

    ctx->group = lv_group_create();
    ctx->body_count = cfg->body_item_count;
    if (ctx->body_count > 0) {
        ctx->body_items = (lv_obj_t **)lv_mem_alloc(sizeof(lv_obj_t *) * ctx->body_count);
        if (!ctx->body_items) {
            if (ctx->group) lv_group_del(ctx->group);
            lv_mem_free(ctx);
            return;
        }
        for (size_t i = 0; i < ctx->body_count; ++i) {
            ctx->body_items[i] = cfg->body_items ? cfg->body_items[i] : NULL;
        }
    }
    ctx->body_layout = cfg->body_layout;
    ctx->aux_policy = cfg->aux_policy;

    if (cfg->top_back_btn) {
        ctx->top_items[ctx->top_count++] = cfg->top_back_btn;
    }
    if (cfg->top_power_btn) {
        ctx->top_items[ctx->top_count++] = cfg->top_power_btn;
    }

    for (size_t i = 0; i < ctx->top_count; ++i) {
        lv_group_add_obj(ctx->group, ctx->top_items[i]);
    }
    for (size_t i = 0; i < ctx->body_count; ++i) {
        if (ctx->body_items[i]) lv_group_add_obj(ctx->group, ctx->body_items[i]);
    }

    input_mode_t mode = cfg->has_input_mode_override ? cfg->input_mode_override : input_profile_get_mode();
    if (mode == INPUT_MODE_HARDWARE) {
        size_t target = NAV_INDEX_NONE;
        if (cfg->initial_body_index != NAV_INDEX_NONE && cfg->initial_body_index < ctx->body_count && ctx->body_items[cfg->initial_body_index]) {
            target = cfg->initial_body_index;
        } else {
            target = first_valid_body_index(ctx);
        }

        if (target != NAV_INDEX_NONE) {
            focus_body(ctx, target);
        } else if (ctx->top_count > 0) {
            focus_top(ctx, 0);
        }
    }

    lv_indev_t *indev = NULL;
    while ((indev = lv_indev_get_next(indev)) != NULL) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD || lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER) {
            lv_indev_set_group(indev, ctx->group);
        }
    }

    for (size_t i = 0; i < ctx->top_count; ++i) {
        if (ctx->top_items[i]) lv_obj_add_event_cb(ctx->top_items[i], nav_key_handler, LV_EVENT_KEY, ctx);
    }
    for (size_t i = 0; i < ctx->body_count; ++i) {
        if (ctx->body_items[i]) lv_obj_add_event_cb(ctx->body_items[i], nav_key_handler, LV_EVENT_KEY, ctx);
    }
    lv_obj_add_event_cb(cfg->screen, nav_cleanup_handler, LV_EVENT_DELETE, ctx);
}
