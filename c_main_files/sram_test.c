/* 
 * File:   newmain.c
 * Author: habib
 *
 * Created on May 6, 2026, 2:50 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "uart.h"


#define ADDR_LOW_PORT   PORTA
#define ADDR_LOW_DDR    DDRA

#define ADDR_HIGH_PORT  PORTC
#define ADDR_HIGH_DDR   DDRC

#define DATA_PORT       PORTB
#define DATA_DDR        DDRB
#define DATA_PIN        PINB

#define CTRL_PORT       PORTD
#define CTRL_DDR        DDRD
#define WE_PIN          PD3
#define OE_PIN          PD4

void sram_init(void) {
    // set address port as output
    ADDR_LOW_DDR = 0xFF;
    ADDR_HIGH_DDR = 0xFF;

    // set control pins as output
    CTRL_DDR |= (1 << WE_PIN) | (1 << OE_PIN);
    CTRL_PORT |= (1 << WE_PIN) | (1 << OE_PIN);

    // set data port as input
    DATA_DDR = 0x00;
    DATA_PORT = 0x00;
}

static inline void sram_set_address(uint16_t addr) {
    // A0?A6 go to PA1?PA7, skip A0
    ADDR_LOW_PORT = (addr & 0x007F) << 1;
    // A7?A14 go to PC0?PC7
    ADDR_HIGH_PORT = (addr >> 7) & 0xFF;
}

void sram_write(uint16_t addr, uint8_t data) {
    sram_set_address(addr);
    // write cycle
    DATA_DDR = 0xFF;
    DATA_PORT = data;
    CTRL_PORT &= ~(1 << WE_PIN); // WE low
    __asm__ __volatile__("nop");
    CTRL_PORT |= (1 << WE_PIN); // WE high

    DATA_DDR = 0x00;
    DATA_PORT = 0x00;
}

uint8_t sram_read(uint16_t addr) {
    sram_set_address(addr);

    // set data port as output
    DATA_DDR = 0x00;
    DATA_PORT = 0x00;

    // read cycle
    CTRL_PORT &= ~(1 << OE_PIN); // OE low
    __asm__ __volatile__("nop");
    uint8_t val = DATA_PIN;
    CTRL_PORT |= (1 << OE_PIN); // OE high

    return val;
}

void sram_write_block(uint16_t start_addr, uint8_t *buff, uint16_t buff_len) {
    uint16_t i;
    uint16_t j = 0;
    uint16_t fin = start_addr + buff_len;
    for (i = start_addr; i < fin; i++) {
        sram_write(i, buff[j++]);
    }

}

void sram_test_write() {
    for (uint16_t i = 0; i < 0x8000; i++) {
        sram_write(i, 'a');
    }
}

uint8_t sram_test_read() {
    for (uint16_t i = 0; i < 0x8000; i++) {
        uint8_t var = sram_read(i);
        if (var != 'a') {
            return 1;
        }

    }
}

static FILE uart_str = FDEV_SETUP_STREAM(UART_putChar, UART_getChar, _FDEV_SETUP_RW);

int main(int argc, char** argv) {
    UART_init(125000);
    stdin = stdout = &uart_str;
    sei();

    sram_init();
    sram_write(0, 1);
    sram_write(0xFF, 'b');
    sram_write(2, 'c');
    uint8_t var1 = sram_read(0);
    uint8_t var2 = sram_read(0xFF);
    uint8_t var3 = sram_read(2);

    
    printf("Read from RAM: %d,%c,%c\r\n", var1, var2, var3);
    
    _delay_ms(500);
    sram_test_write();
    
    
    for (uint16_t i = 0; i < 0x8000; i++) {
        uint8_t var = sram_read(i);
        if (var != 'a') {
            printf("Error at index: %d\r\n",i);
//            return 1;
        }
//        printf("Read character %c from location %d\r\n",var,i);

    }
    while (1);
    return (EXIT_SUCCESS);
}

