#ifndef __KEY_H_
#define __KEY_H_

#include <board.h>

/* defined the KEY1 pin: PG2 */
#define KEY1_PIN	GET_PIN(G,2)
/* defined the KEY2 pin: PG3 */
#define KEY2_PIN	GET_PIN(G,3)
/* defined the KEY3 pin: PG4 */
#define KEY3_PIN	GET_PIN(G,4)

/* function declaration */
int rt_hw_key_init(void);
void key_sample(void);

#endif


