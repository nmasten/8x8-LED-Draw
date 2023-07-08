/*
 * uart.h
 *
 *  Created on: May 2, 2023
 *      Author: noahmasten
 */

#ifndef SRC_UART_H_
#include "stm32l476xx.h"

#define SRC_UART_H_

#define BAUD_RATE 115200 // 115.2 kpbs
#define USART_DIV 625 // clock frequency divided by baud rate, rounded up

void UART_Init(void);
void UART_print(char* data);
void UART_ESC_Code(char *input_string);
void UART_print_char(int input_char);
void UART_print_int(uint8_t integer);

#endif /* SRC_UART_H_ */
