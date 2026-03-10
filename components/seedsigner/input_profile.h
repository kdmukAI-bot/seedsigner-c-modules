#ifndef SEEDSIGNER_INPUT_PROFILE_H
#define SEEDSIGNER_INPUT_PROFILE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INPUT_MODE_TOUCH = 0,
    INPUT_MODE_HARDWARE = 1,
} input_mode_t;

void input_profile_set_mode(input_mode_t mode);
input_mode_t input_profile_get_mode(void);

#ifdef __cplusplus
}
#endif

#endif // SEEDSIGNER_INPUT_PROFILE_H
