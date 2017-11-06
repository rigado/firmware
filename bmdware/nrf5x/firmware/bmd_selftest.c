/** @file bmd_selftest.c
*
* @brief Module self test for factory
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <stdbool.h>

#include "bmd_selftest.h"
#include "bmdware_gpio_def.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"

#define PIN_SET                     1
#define PIN_CLEAR                   0

#define wait()                      (nrf_delay_us(2000))
#define cnt_of_array( _arr )        (sizeof(_arr)/sizeof(_arr[0]))
#define config_output(pin)          (nrf_gpio_cfg_output(pin))
#define config_input(pin, pull)     (nrf_gpio_cfg_input(pin,pull))
#define config_input_hiZ(pin)       (config_input(pin,NRF_GPIO_PIN_NOPULL))
#define read(pin)                   (nrf_gpio_pin_read(pin))
#define write(pin,val)              (nrf_gpio_pin_write(pin,val))

/* This array does not include the UART pins
   If the UART pins are changed, this array needs to be updated */
static const uint8_t test_pins[] = {  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 
                                      13, 14, 15, 16, 17, 18, 19, 20, 21, 
                                      22, 23, 24, 25, 26, 27, 28, 29, 30, 31 
};

static uint32_t check_test_pin_state(bool test_val)
{
    int j;
    uint32_t result = 0;
  
    for(j = 0; j < cnt_of_array(test_pins); j++)
    {
        uint32_t test_pin = test_pins[j];
    
        result |= ((read(test_pin) != test_val) << test_pin);
    }
  
    return result;
}

uint32_t bmd_selftest(void)
{ 
    uint32_t pin;
    uint32_t i;
    uint32_t result = 0; /* bitfield for errors */

    uint8_t num_pins = cnt_of_array(test_pins);
    //all pins to hiZ, except UART
    for(pin = 0; pin < num_pins; pin++)
    {
        config_input_hiZ(test_pins[pin]);
    }
    wait();

    for(i = 0; i < cnt_of_array(test_pins); i++)
    {
        uint32_t out_pin = test_pins[i];
    
        config_output(out_pin);

        //bounds check test_pins[]
        if(i > 0)
        {
            uint32_t adj1 = test_pins[i-1];

            //drive output high
            write(out_pin, PIN_SET);

            //pull adjacent pin low
            config_input( adj1, NRF_GPIO_PIN_PULLDOWN );

            //check adjacent pin is low
            wait();
            result |= ((read(adj1) != PIN_CLEAR) << adj1);

            //drive output low
            write(out_pin, PIN_CLEAR);

            //pull adjacent pin high
            config_input( adj1, NRF_GPIO_PIN_PULLUP );

            //check adjacent pin is high
            wait();
            result |= ((read(adj1) != PIN_SET) << adj1);

            //cleanup
            config_input_hiZ(adj1);
        }

        //bounds check test_pins[]
        if(i < (cnt_of_array(test_pins)-1) )
        {
            uint32_t adj2 = test_pins[i+1];
               
            //drive output high
            write(out_pin, PIN_SET);

            //pull adjacent pin low
            config_input( adj2, NRF_GPIO_PIN_PULLDOWN );

            //check adjacent pin is low
            wait();
            result |= ((read(adj2) != PIN_CLEAR) << adj2);

            //drive output low
            write(out_pin, PIN_CLEAR);

            //pull adjacent pin high
            config_input( adj2, NRF_GPIO_PIN_PULLUP );

            //check adjacent pin is high
            wait();
            result |= ((read(adj2) != PIN_SET) << adj2);	   

            //cleanup
            config_input_hiZ(adj2);
        }

        // everything is hiZ except output pin here
        //drive output high
        write(out_pin, PIN_SET);
        wait();

        //test all floating pins are high, ignore the UART pins
        result |= (~(1<<out_pin|1<<BMD_DTM_UART_TXD|1<<BMD_DTM_UART_RXD) & check_test_pin_state(1));
        //drive output low
        write(out_pin, PIN_CLEAR);
        wait();

        //test all floating pins are low, ignore the UART pins
        result |= (~(1<<out_pin|1<<BMD_DTM_UART_TXD|1<<BMD_DTM_UART_RXD) & check_test_pin_state(0));
        config_input_hiZ(out_pin);
    }

    bmd_selftest_init_gpio();
    wait();

    return result;
}


void bmd_selftest_init_gpio(void)
{
    int pin;
  
    //config everything (except UART) to input pulldown
    for(pin = 0; pin < cnt_of_array(test_pins); pin++)
    {
        config_input( pin, NRF_GPIO_PIN_PULLDOWN );
    }
}
