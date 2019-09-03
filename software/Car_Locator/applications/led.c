#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "led.h"

/* led引脚初始化 */
int rt_hw_led_init(void)
{
		rt_pin_mode(LED0_PIN, PIN_MODE_OUTPUT);
		rt_pin_mode(LED1_PIN, PIN_MODE_OUTPUT);
		
		return 0;
}
INIT_DEVICE_EXPORT(rt_hw_led_init);
/* 开启led */
void led_on(uint32_t pin)
{
		rt_pin_write(pin, PIN_LOW);
}
/* 关闭led */
void led_off(uint32_t pin)
{
		rt_pin_write(pin, PIN_HIGH);
}
/* le闪烁 */
void led_blink(uint32_t pin, rt_tick_t time)
{
			led_on(pin);
			rt_thread_delay(time);
			led_off(pin);
			rt_thread_delay(time);
}
//MSH_CMD_EXPORT(led_blink, led blink sample);


