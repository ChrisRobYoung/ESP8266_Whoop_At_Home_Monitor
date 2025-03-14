#include "esp_log.h"
//#include "esp_system.h"

//#include "esp8266/gpio_register.h"
//#include "esp8266/pin_mux_register.h"

#include "driver/pwm.h"
#include "driver/gpio.h"

// Types and Definitions
#define PWM_RED_OUT_IO_NUM   0
#define PWM_GREEN_OUT_IO_NUM   12
#define PWM_BLUE_OUT_IO_NUM   15
#define PWM_PERIOD    (1000)

#define BUTTON_PIN  GPIO_Pin_5
// Local Variables
const uint32_t pin_num[3] = 
{
    PWM_RED_OUT_IO_NUM,
    PWM_GREEN_OUT_IO_NUM,
    PWM_BLUE_OUT_IO_NUM
};
uint32_t duties[3] = 
{
    PWM_PERIOD, PWM_PERIOD, PWM_PERIOD,
};
float phase[3] = 
{
    0, 0, 0
};

gpio_config_t button_io_conf = 
{
    BUTTON_PIN,
    GPIO_MODE_INPUT,
    GPIO_PULLUP_DISABLE,
    GPIO_PULLDOWN_ENABLE,
    GPIO_INTR_DISABLE
};

static const char *TAG = "GPIO_MANAGER";

//Global Variables

// Local Functions
//Color request of [0 - 255] normalized to [1000 - 0]
uint32_t normalize_color_request_value(int color_request)
{
    uint32_t value = (255 - color_request) * 1000;
    return (value / 255);
}

//Global Functions
void initialize_gpio(void)
{
    //PWM
    pwm_init(PWM_PERIOD, duties, 3, pin_num);
    pwm_set_phases(phase);
    pwm_start();

    //Button IO
    gpio_config(&button_io_conf);
}
int get_touch_button_state(int *on_or_off_out)
{
    *on_or_off_out = gpio_get_level(BUTTON_PIN);
    return 0;
}
int set_rgb_led_value(int r, int g, int b)
{
    int status = 0;
    duties[0] = normalize_color_request_value(r);
    duties[1] = normalize_color_request_value(g);
    duties[2] = normalize_color_request_value(b);
    status = pwm_set_duties(duties);
    if(!status)
    {
        ESP_LOGI(TAG, "Setting RBG: [%d, %d, %d]", r, g, b);
        pwm_start();
    }
    else
    {
        ESP_LOGI(TAG, "Error starting PWM: %d", status);
    }
    return status;
}