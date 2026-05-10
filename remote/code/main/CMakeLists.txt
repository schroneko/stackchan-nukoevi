set(SOURCES
    esp_now/esp_now_init.c
    joystick/joystick_handle.c
    ui/ui_running_screen.c
    ui/ui_setup_screen.c
    ui/ui_imu_screen.c
    ui/ui.c
    ui/updown_img.c
    lvgl_port.cpp
    StackChan-RemoteControl-ESPNow.cpp
)

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS 
                    "."
                    esp_now
                    joystick
                    ui
                    )
