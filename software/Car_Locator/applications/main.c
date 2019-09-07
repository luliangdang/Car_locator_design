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

int main(void)
{
    int count = 1;

    while (count++)
    {
        rt_pin_write(LED0_PIN, PIN_HIGH);
				rt_pin_write(LED1_PIN, PIN_LOW);
        rt_thread_mdelay(500);
        rt_pin_write(LED0_PIN, PIN_LOW);
				rt_pin_write(LED1_PIN, PIN_HIGH);
        rt_thread_mdelay(500);
    }

    return RT_EOK;
}
