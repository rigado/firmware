/** @file sw_irq_manager.c
*
* @brief This module allows for sharing of the only available SWI (SWI0)
*
* @note Typically, SWI0 is used by app timer. This module allows app timer and
*       other modules to share SWI0.
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */
#include <stdint.h>
#include <string.h>

#include "app_util_platform.h"
#include "nrf.h"
#include "nrf_error.h"
#include "app_util.h"

#include "sw_irq_manager.h"

#define RTC1_IRQ_PRI            APP_IRQ_PRIORITY_LOW                        /**< Priority of the RTC1 interrupt (used for checking for timeouts and executing timeout handlers). */

/*-------------------------------------------------------------------*/
//#include "app_timer.h" //for RTC IRQ priority only
//#include "timer_intf.h" //for TIMER1 IRQ priority only
#define SWI0_IRQ_PRI            APP_IRQ_PRIORITY_LOW                        /**< Priority of the SWI0 interrupt (used for updating the timer list). */

/* Note: The SWI0 IRQ must match the RTC1 IRQ so that they cannot interrupt each other.
 * Also, the SWI0 IRQ should match NRF_TIMER1 IRQ that is used by the timer_intf.  This
 * ensures bus transactions won't be interrupted by an interrupt that may also run a
 * bus transaction */
STATIC_ASSERT(SWI0_IRQ_PRI == RTC1_IRQ_PRI);
//STATIC_ASSERT(SWI0_IRQ_PRI == TIMER1_IRQ_PRI);
/*-------------------------------------------------------------------*/

#define MAX_CALLBACKS       8

struct swi_callback_s
{
    sw_irq_callback_t callback;
    void * param;
};

static struct swi_callback_s callback_list[MAX_CALLBACKS];
static uint8_t triggered_id_list;

void sw_irq_manager_init(void)
{
    memset(callback_list, 0, sizeof(callback_list));
    NVIC_SetPriority(SWI0_IRQn, APP_IRQ_PRIORITY_LOW);
    NVIC_EnableIRQ(SWI0_IRQn);
}

uint32_t sw_irq_manager_register_callback(sw_irq_callback_t callback, 
    sw_irq_callback_id_t * sw_callback_id, void * param)
{
    if(sw_callback_id == NULL || callback == NULL)
    {
        return NRF_ERROR_INVALID_PARAM;
    }
    
    for(uint8_t i = 0; i < MAX_CALLBACKS; i++)
    {
        if(callback_list[i].callback == NULL)
        {
            callback_list[i].callback = callback;
            callback_list[i].param = param;
            *sw_callback_id = i;
            return NRF_SUCCESS;
        }
    }
    
    return NRF_ERROR_NO_MEM;
}

uint32_t sw_irq_manager_trigger_int(sw_irq_callback_id_t callback_id)
{
    if(callback_id >= MAX_CALLBACKS)
    {
        return NRF_ERROR_INVALID_PARAM;
    }
    
    if(callback_list[callback_id].callback == NULL)
    {
        return NRF_ERROR_INVALID_DATA;
    }
    
    triggered_id_list |= (1 << callback_id);
    //app_trace_log("[trig]: %02x\n", triggered_id_list);
    NVIC_SetPendingIRQ(SWI0_IRQn);
    
    return NRF_SUCCESS;
}

void SWI0_IRQHandler(void)
{
    NVIC_ClearPendingIRQ(SWI0_IRQn);
    volatile uint8_t local_id_list = triggered_id_list;
    for(uint8_t i = 0; i < MAX_CALLBACKS; i++)
    {
        //app_trace_log("[int]: %02x\n", triggered_id_list);
        if((local_id_list & (1 << i)) != 0)
        {
            triggered_id_list &= ~(1 << i);
            void * param = callback_list[i].param;
            if(callback_list[i].callback != NULL)
            {
                callback_list[i].callback(param);
            }
        }
    }
}
