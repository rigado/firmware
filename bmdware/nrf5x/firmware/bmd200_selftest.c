#include <stdbool.h>

#include "bmd200_selftest.h"
#include "bmdware_gpio_def.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"

#define wait()                    nrf_delay_us(2000)
#define cnt_of_array( _arr )      (sizeof(_arr)/sizeof(_arr[0]))
#define config_output(pin)        nrf_gpio_cfg_output(pin)
#define config_input(pin, pull)   nrf_gpio_cfg_input(pin,pull)
#define config_input_hiZ(pin)     config_input(pin,NRF_GPIO_PIN_NOPULL)
#define read(pin)                 nrf_gpio_pin_read(pin)
#define write(pin,val)            nrf_gpio_pin_write(pin,val)

#define is_uart_pin(pin)          ((pin) == BMD_DTM_UART_RXD || (pin) == BMD_DTM_UART_TXD)

/* BMD-200 GPIO Pins */
#define BMD200_PIN_5  24
#define BMD200_PIN_6  25
#define BMD200_PIN_8  26
#define BMD200_PIN_9  27
#define BMD200_PIN_11 0
#define BMD200_PIN_12 1
#define BMD200_PIN_13 2
#define BMD200_PIN_14 3
#define BMD200_PIN_15 4
#define BMD200_PIN_16 5
#define BMD200_PIN_17 6
#define BMD200_PIN_20 8
#define BMD200_PIN_21 9
#define BMD200_PIN_22 10
#define BMD200_PIN_23 11

static const uint32_t test_pins[] = { 
    BMD200_PIN_5, BMD200_PIN_6, BMD200_PIN_8, BMD200_PIN_9, BMD200_PIN_11, BMD200_PIN_12, 
    BMD200_PIN_13, BMD200_PIN_14, BMD200_PIN_15, BMD200_PIN_16, BMD200_PIN_17, BMD200_PIN_20, 
    BMD200_PIN_21, BMD200_PIN_22, BMD200_PIN_23 
};
//static const uint32_t unused_pins[] = { 7, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 28, 29, 30, 31 };

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

uint32_t bmd200_selftest(void)
{ 
    uint32_t pin;
    uint32_t i;
    uint32_t result = 0; /* bitfield for errors */

    //all pins to hiZ, except UART
    for(pin = 0; pin < 32; pin++)
    {
        if(!is_uart_pin(pin))
            config_input_hiZ(pin);
    }
    wait();

    for(i = 0; i < cnt_of_array(test_pins); i++)
    {
        uint32_t out_pin = test_pins[i];

        //don't test the uart pins
        if(is_uart_pin(out_pin))
            continue;
        else
            config_output(out_pin);

        //bounds check test_pins[]
        if(i > 0)
        {
            uint32_t adj1 = test_pins[i-1];

            if(!is_uart_pin(adj1))
            { 
                //drive output high
                write(out_pin, 1);

                //pull adjacent pin low
                config_input( adj1, NRF_GPIO_PIN_PULLDOWN );

                //check adjacent pin is low
                wait();
                result |= (read(adj1)!=0) << adj1;

                //drive output low
                write(out_pin, 0);

                //pull adjacent pin high
                config_input( adj1, NRF_GPIO_PIN_PULLUP );

                //check adjacent pin is high
                wait();
                result |= (read(adj1)!=1) << adj1;

                //cleanup
                config_input_hiZ(adj1);
            }
        }

        //bounds check test_pins[]
        if(i < (cnt_of_array(test_pins)-1) )
        {
            uint32_t adj2 = test_pins[i+1];

            if(!is_uart_pin(adj2))
            {	   
                //drive output high
                write(out_pin, 1);

                //pull adjacent pin low
                config_input( adj2, NRF_GPIO_PIN_PULLDOWN );

                //check adjacent pin is low
                wait();
                result |= (read(adj2)!=0) << adj2;

                //drive output low
                write(out_pin, 0);

                //pull adjacent pin high
                config_input( adj2, NRF_GPIO_PIN_PULLUP );

                //check adjacent pin is high
                wait();
                result |= (read(adj2)!=1) << adj2;	   

                //cleanup
                config_input_hiZ(adj2);
            }
        }

        // everything is hiZ except output pin here
        //drive output high
        write(out_pin,1);
        wait();

        //test all floating pins are high, ignore the UART pins
        result |= (~(1<<out_pin|1<<BMD_DTM_UART_TXD|1<<BMD_DTM_UART_RXD) & check_test_pin_state(1));

        //drive output low
        write(out_pin,0);
        wait();

        //test all floating pins are low, ignore the UART pins
        result |= (~(1<<out_pin|1<<BMD_DTM_UART_TXD|1<<BMD_DTM_UART_RXD) & check_test_pin_state(0));

        config_input_hiZ(out_pin);
    }

    bmd200_selftest_init_gpio();
    wait();

    return result;
}


void bmd200_selftest_init_gpio(void)
{
    int pin;
  
    //config everything (except UART) to input pulldown
    for(pin = 0; pin < 32; pin++)
    {
        if(!is_uart_pin(pin))
        {
            config_input( pin, NRF_GPIO_PIN_PULLDOWN );
        }
    }
}
