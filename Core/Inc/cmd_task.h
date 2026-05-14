#ifndef CMD_TASK_H
#define CMD_TASK_H

void cmd_task_create(void);
void cmd_task_uart_idle_isr(void);  /* USART2_IRQHandler'dan çağrılır */

#endif /* CMD_TASK_H */
