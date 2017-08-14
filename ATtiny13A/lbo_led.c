#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>

ISR(INT0_vect)
{
    PORTB |= (1 << PORTB4);
    _delay_ms(1);
    PORTB &= ~(1 << PORTB4);
}

int main(int argc, char** argv)
{
    MCUCR = 0;
    GIMSK = 0;
    DDRB = 0;
    PORTB = 0;

    // Set PB4 as output for LED
    DDRB |= (1 << DDB4);

    // Set PB1/INT0 as input with pullup enabled
    //PORTB |= (1 << PORTB1);

    // Set interrupt INT0 as active low
    // This will trigger when LBO from PowerBoost goes low
    GIMSK |= (1 << INT0);

    // Setup MCU for Power-down mode
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_bod_disable();

    // Enable interrupts
    sei();

    // Just sleep in power-down mode only waking for INT0 interrupt
    while (1)
        sleep_cpu();

    return 0;
}
