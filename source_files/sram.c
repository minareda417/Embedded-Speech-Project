/*
 * File:   sram.c
 * Author: habib
 */

#include <avr/io.h>
#include "sram.h"


void sram_init(void)
{
    // PA1 to PA7 outputs
    ADDR_LOW_DDR  = (ADDR_LOW_DDR & 0x01) | 0xFE;
    ADDR_HIGH_DDR = 0xFF;

  
    CTRL_DDR  |= (1 << WE_PIN) | (1 << OE_PIN);
    CTRL_PORT |= (1 << WE_PIN) | (1 << OE_PIN);

    // data bus as input
    DATA_DDR  = 0x00;
    DATA_PORT = 0x00;
}

static inline void sram_set_address(uint16_t addr)
{
    ADDR_LOW_PORT  = (ADDR_LOW_PORT & 0x01) | ((addr & 0x007F) << 1);
    ADDR_HIGH_PORT = (addr >> 7) & 0xFF;
}


void sram_write(uint16_t addr, uint8_t data)
{
    sram_set_address(addr);

    DATA_DDR  = 0xFF;
    DATA_PORT = data;

    CTRL_PORT &= ~(1 << WE_PIN);    
    __asm__ __volatile__("nop");
    CTRL_PORT |=  (1 << WE_PIN);       

    DATA_DDR  = 0x00;
    DATA_PORT = 0x00;
}


uint8_t sram_read(uint16_t addr)
{
    sram_set_address(addr);

    DATA_DDR  = 0x00;
    DATA_PORT = 0x00;

    CTRL_PORT &= ~(1 << OE_PIN);       
    __asm__ __volatile__("nop");
    uint8_t val = DATA_PIN;
    CTRL_PORT |=  (1 << OE_PIN);      

    return val;
}


void sram_write_block(uint16_t start_addr, uint8_t *buff, uint16_t buff_len)
{
    uint16_t i;
    uint16_t j   = 0;
    uint16_t fin = start_addr + buff_len;

    if (fin > 0x8000) fin = 0x8000;    // end of RAM is reached

    for (i = start_addr; i < fin; i++)
        sram_write(i, buff[j++]);
}


void sram_test_write(void)
{
    for (uint16_t i = 0; i < 0x8000; i++)
        sram_write(i, 'a');
}


uint8_t sram_test_read(void)
{
    for (uint16_t i = 0; i < 0x8000; i++) {
        if (sram_read(i) != 'a')
            return 1;                  
    }
    return 0;                          
}