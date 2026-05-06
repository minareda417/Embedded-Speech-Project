/*
 * File:   main.c
 * Author: habib
 */
#define F_CPU 16000000UL
#include <stdio.h>
#include <stdlib.h>
#include <avr/io.h>
#include <inttypes.h>
#include <avr/interrupt.h>
#include "uart.h"

static volatile uint8_t send_flag = 0;
static volatile uint8_t rec_flag = 0;
static volatile uint8_t stop_flag = 0;
static volatile uint8_t adc_val = 0;
static volatile uint8_t button_pressed = 0;
static volatile uint16_t sample_count = 0;

#define SAMPLES_PER_SEC  8000u

ISR(ADC_vect) {
    adc_val = ADCH;
}

ISR(INT0_vect) {
    button_pressed = 1;
}

ISR(TIMER1_COMPA_vect) {
    if (rec_flag) {
        send_flag = 1;
        sample_count++;
        if (sample_count >= SAMPLES_PER_SEC) {
            sample_count = 0;
            rec_flag = 0;
            stop_flag = 1;
        }
    }
}


static FILE uart_str = FDEV_SETUP_STREAM(UART_putChar, UART_getChar, _FDEV_SETUP_RW);

int main(void) {
    cli();

    // int0 for button
    DDRD &= ~(1 << DDD2);
    PORTD |= (1 << PIND2); // pull-up
    GICR |= (1 << INT0);
    MCUCR = (1 << ISC01) | (0 << ISC00); // falling edge
    GIFR = (1 << INTF0); // clear pending flag

    // timer1 ctc mode
    TCCR1A = 0x00;
    TCCR1B = (1 << WGM12) | (1 << CS11); // ctc, prescaler=8
    OCR1A = 249; // TOP
    TIMSK |= (1 << OCIE1A);

    // adc left adjust on adc0
    ADMUX = (1 << REFS0) // AVCC reference
            | (1 << ADLAR); // left adjust (8-bit in ADCH) // ADC0

    SFIOR &= ~((1 << ADTS2) | (1 << ADTS1) | (1 << ADTS0));
    ADCSRA = (1 << ADEN) // enable
            | (1 << ADATE) // auto-trigger
            | (1 << ADIE) // interrupt
            | (1 << ADPS2) | (1 << ADPS1); // prescaler=64
    ADCSRA |= (1 << ADSC); // start first conversion

    // leds
    DDRC |= (1 << DDC0) | (1 << DDC1);
    PORTC |= (1 << PORTC0) | (1 << PORTC1);

    // uart
    UART_init(125000);
    stdin = stdout = &uart_str;

    sei();

    while (1) {
        if (button_pressed) {
            button_pressed = 0;
            sample_count = 0;
            send_flag = 0;
            uart_putString("##START##");
            TCNT1 = 0;
            rec_flag = 1; // sync timer to button press

        }

        if (send_flag) {
            send_flag = 0;
            UART_putByte(adc_val);
            //            printf("%d\r\n",val);
        }

        if (stop_flag) {
            stop_flag = 0;
            uart_putString("##STOP##");
        }
    }

    return EXIT_SUCCESS;
}