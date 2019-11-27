/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-11-06     SummerGift   first version
 * 2018-11-19     flybreak     add stm32f407-atk-explorer bsp
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <lcd.h>
#include "main.h"
#include "gps.h"

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
		LCD_ShowString(80, 20, 100, 16, 16, (rt_uint8_t *)"Car Locator");
		LCD_ShowString(20, 80, 100, 16, 16, (rt_uint8_t *)"longitude:");
		LCD_ShowString(20, 120, 100, 16, 16, (rt_uint8_t *)"latitude:");
		LCD_ShowString(20, 160, 100, 16, 16, (rt_uint8_t *)"Speed:");
    
		int count = 1;

    while (count++)
    {
        rt_pin_write(LED0_PIN, PIN_HIGH);
				rt_pin_write(LED1_PIN, PIN_LOW);
        rt_thread_mdelay(500);
        rt_pin_write(LED0_PIN, PIN_LOW);
				rt_pin_write(LED1_PIN, PIN_HIGH);
        rt_thread_mdelay(500);
//				rt_kprintf("GPS--%d\n", gps_data.latitude);
    }

    return RT_EOK;
}
