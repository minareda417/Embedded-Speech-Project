/* 
 * File:   uart.h
 * Author: habib
 *
 * Created on March 16, 2026, 11:48 PM
 */
#include <stdio.h>
#ifndef UART_H
#define	UART_H

#ifdef	__cplusplus
extern "C" {
#endif




#ifdef	__cplusplus
}
#endif
#define F_CPU 16000000UL
#define BAUD_PRESCALE (((F_CPU/(USART_BAUDRATE * 16UL))) - 1)
void UART_init(long USART_BAUDRATE);
int UART_getChar(FILE *stream);
int UART_putChar(char c,FILE *stream);
uint8_t UART_dataAvailable(void);
int UART_getChar_NonBlocking(void);
void UART_putByte(int8_t b);
void uart_putString(const char *s);

#endif	/* UART_H */
