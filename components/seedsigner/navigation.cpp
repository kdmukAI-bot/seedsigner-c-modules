#include "navigation.h"
#include "components.h"

#if defined(__has_include)
#  if __has_include("lv_gridnav.h")
#    include "lv_gridnav.h"
#    define SS_HAVE_GRIDNAV 1
#  elif __has_include("extra/layouts/gridnav/lv_gridnav.h")
#    include "extra/layouts/gridnav/lv_gridnav.h"
#    define SS_HAVE_GRIDNAV 1
#  elif __has_include("lvgl/src/extra/layouts/gridnav/lv_gridnav.h")
#    include "lvgl/src/extra/layouts/gridnav/lv_gridnav.h"
#    define SS_HAVE_GRIDNAV 1
#  endif
#endif

#ifndef SS_HAVE_GRIDNAV
#  define SS_HAVE_GRIDNAV 0
#endif

// Optional host callback for aux-key emit mode (weak by design).
extern "C" __attribute__((weak)) void seedsigner_lvgl_on_aux_key(const char *key_name) {
    (void)key_name;
}

typedef struct {
    lv_group_t *group;
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
    if (!obj || !lv_obj_is_valid(obj)) return;
    lv_event_send(obj, LV_EVENT_CLICKED, NULL);
}

static void nav_aux_key_handler(lv_event_t *e) {
    if (!e || lv_event_get_code(e) != LV_EVENT_KEY) return;

    nav_ctx_t *ctx = (nav_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;

    uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_ENTER) {
        activate_focused(ctx);
        lv_event_stop_bubbling(e);
        lv_event_stop_processing(e);
        return;
    }

    int aux_idx = 0;
    if (!is_aux_key(key, &aux_idx)) return;

    nav_aux_action_t action = action_for_aux(ctx, aux_idx);
    if (action == NAV_AUX_ENTER) {
        activate_focused(ctx);
    } else if (action == NAV_AUX_EMIT) {
        if (aux_idx == 1) seedsigner_lvgl_on_aux_key("KEY1");
        else if (aux_idx == 2) seedsigner_lvgl_on_aux_key("KEY2");
        else if (aux_idx == 3) seedsigner_lvgl_on_aux_key("KEY3");
    }

    lv_event_stop_bubbling(e);
    lv_event_stop_processing(e);
}

static void nav_cleanup_handler(lv_event_t *e) {
    if (!e || lv_event_get_code(e) != LV_EVENT_DELETE) return;

    nav_ctx_t *ctx = (nav_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;

    if (ctx->group) {
        lv_group_del(ctx->group);
        ctx->group = NULL;
    }
    lv_mem_free(ctx);
}

void nav_bind(const nav_config_t *cfg) {
    if (!cfg || !cfg->screen) return;

    nav_ctx_t *ctx = (nav_ctx_t *)lv_mem_alloc(sizeof(nav_ctx_t));
    if (!ctx) return;
    lv_memset_00(ctx, sizeof(*ctx));

    ctx->group = lv_group_create();
    lv_group_set_wrap(ctx->group, false);
    lv_group_focus_freeze(ctx->group, true);
    ctx->aux_policy = cfg->aux_policy;

    // Simplest native setup: all focusables in one LVGL group.
    if (cfg->top_back_btn && lv_obj_is_valid(cfg->top_back_btn)) {
        lv_group_add_obj(ctx->group, cfg->top_back_btn);
    }
    if (cfg->top_power_btn && lv_obj_is_valid(cfg->top_power_btn)) {
        lv_group_add_obj(ctx->group, cfg->top_power_btn);
    }
    for (size_t i = 0; i < cfg->body_item_count; ++i) {
        lv_obj_t *obj = cfg->body_items ? cfg->body_items[i] : NULL;
        if (obj && lv_obj_is_valid(obj)) {
            lv_group_add_obj(ctx->group, obj);
        }
    }

#if SS_HAVE_GRIDNAV
    // Native directional navigation on focus-container parents.
    // Avoid attaching on screen root (observed unstable in harness).
    lv_obj_t *top_parent = NULL;
    if (cfg->top_back_btn && lv_obj_is_valid(cfg->top_back_btn)) top_parent = lv_obj_get_parent(cfg->top_back_btn);
    else if (cfg->top_power_btn && lv_obj_is_valid(cfg->top_power_btn)) top_parent = lv_obj_get_parent(cfg->top_power_btn);

    lv_obj_t *body_parent = NULL;
    if (cfg->body_items && cfg->body_item_count > 0 && cfg->body_items[0] && lv_obj_is_valid(cfg->body_items[0])) {
        body_parent = lv_obj_get_parent(cfg->body_items[0]);
    }

    if (top_parent) lv_gridnav_add(top_parent, LV_GRIDNAV_CTRL_NONE);
    if (body_parent && body_parent != top_parent) lv_gridnav_add(body_parent, LV_GRIDNAV_CTRL_NONE);
#endif

    lv_group_focus_freeze(ctx->group, false);

    // Default initial focus for hardware mode only.
    input_mode_t mode = cfg->has_input_mode_override ? cfg->input_mode_override : input_profile_get_mode();
    if (mode == INPUT_MODE_HARDWARE) {
        if (cfg->body_items && cfg->body_item_count > 0) {
            size_t target = 0;
            if (cfg->initial_body_index < cfg->body_item_count && cfg->body_items[cfg->initial_body_index]) {
                target = cfg->initial_body_index;
            }
            if (cfg->body_items[target] && lv_obj_is_valid(cfg->body_items[target])) {
                lv_group_focus_obj(cfg->body_items[target]);
            }
        } else if (cfg->top_back_btn && lv_obj_is_valid(cfg->top_back_btn)) {
            lv_group_focus_obj(cfg->top_back_btn);
        } else if (cfg->top_power_btn && lv_obj_is_valid(cfg->top_power_btn)) {
            lv_group_focus_obj(cfg->top_power_btn);
        }
    }

    // Bind keypad/encoder indevs to this screen's group.
    lv_indev_t *indev = NULL;
    while ((indev = lv_indev_get_next(indev)) != NULL) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD ||
            lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER) {
            lv_indev_set_group(indev, ctx->group);
        }
    }

    // Attach optional aux-key handler.
    if (cfg->top_back_btn) lv_obj_add_event_cb(cfg->top_back_btn, nav_aux_key_handler, LV_EVENT_KEY, ctx);
    if (cfg->top_power_btn) lv_obj_add_event_cb(cfg->top_power_btn, nav_aux_key_handler, LV_EVENT_KEY, ctx);
    for (size_t i = 0; i < cfg->body_item_count; ++i) {
        if (cfg->body_items && cfg->body_items[i]) {
            lv_obj_add_event_cb(cfg->body_items[i], nav_aux_key_handler, LV_EVENT_KEY, ctx);
        }
    }

    lv_obj_add_event_cb(cfg->screen, nav_cleanup_handler, LV_EVENT_DELETE, ctx);
}
