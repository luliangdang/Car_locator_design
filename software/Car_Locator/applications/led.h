#ifndef __LED_H__
#define __LED_H__

#include <board.h>

/* defined the LED0 pin: PB12 */
#define LED0_PIN    GET_PIN(B, 12)
/* defined the LED1 pin: PB13 */
#define LED1_PIN		GET_PIN(B, 13)

/* function declaration */
int rt_hw_led_init(void);
void led_on(uint32_t pin);
void led_off(uint32_t pin);
void led_blink(uint32_t pin, rt_tick_t time);

#endif


