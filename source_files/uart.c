#include <avr/io.h>
#include "uart.h"

int UART_getChar(FILE *stream) {
    while ((UCSRA & (1 << RXC)) == 0); // wait for data
    return (UDR); // return byte
}

int UART_putChar(char c, FILE *stream) {
    while (!(UCSRA & (1 << UDRE))); // wait for empty buffer
    UDR = c;
    return 0;
}

void UART_init(long USART_BAUDRATE) {
    UBRRL = BAUD_PRESCALE; // lower 8 bits
    UBRRH = (BAUD_PRESCALE >> 8); // upper 8 bits
    UCSRB |= (1 << RXEN) | (1 << TXEN); // turn on Tx and Rx
    UCSRC |= (1 << URSEL) | (1 << UCSZ0) | (1 << UCSZ1);
}

uint8_t UART_dataAvailable(void) {
    return (UCSRA & (1 << RXC));
}

int UART_getChar_NonBlocking(void) {
    if (UCSRA & (1 << RXC))
        return UDR;
    else
        return -1;
}

void UART_putByte(int8_t b) {
    while (!(UCSRA & (1 << UDRE))); // wait for empty buffer
    UDR = b;
}

void uart_putString(const char *s){
 while (*s) UART_putByte((uint8_t)*s++);   
}