#ifndef _JOYSTICK_BASIC_H_
#define _JOYSTICK_BASIC_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int channel;
    int id;
    int8_t bat;
    uint16_t joyX;
    uint16_t joyY;
    uint8_t screen_mode;
    uint8_t select_mode;
    bool btnB_status;
    float accel_x;
    float accel_y;
    float accel_z;

} joystick_data_t;

extern joystick_data_t joystick_data;

#ifdef __cplusplus
}
#endif

#endif