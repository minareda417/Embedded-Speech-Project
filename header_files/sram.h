/*
 * File:   sram.h
 * Author: habib
 */

#ifndef SRAM_H
#define SRAM_H

#include <avr/io.h>

#ifdef __cplusplus
extern "C" {
#endif


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


void    sram_init(void);
void    sram_write(uint16_t addr, uint8_t data);
uint8_t sram_read(uint16_t addr);
void    sram_write_block(uint16_t start_addr, uint8_t *buff, uint16_t buff_len);
void    sram_test_write(void);
uint8_t sram_test_read(void);

#ifdef __cplusplus
}
#endif

#endif /* SRAM_H */