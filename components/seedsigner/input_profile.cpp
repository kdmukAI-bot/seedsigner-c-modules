#include "input_profile.h"

static input_mode_t g_input_mode = INPUT_MODE_TOUCH;

void input_profile_set_mode(input_mode_t mode) {
    g_input_mode = mode;
}

input_mode_t input_profile_get_mode(void) {
    return g_input_mode;
}
