#ifndef _GPIO_MANAGER_H_
#define _GPIO_MANAGER_H_

void initialize_gpio(void);
int get_touch_button_state(int *on_or_off_out);
int set_rgb_led_value(int r, int g, int b);

#endif //_GPIO_MANAGER_H_