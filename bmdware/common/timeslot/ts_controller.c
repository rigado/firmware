/***********************************************************************************
Copyright (c) Nordic Semiconductor ASA
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

  3. Neither the name of Nordic Semiconductor ASA nor the names of other
  contributors to this software may be used to endorse or promote products
  derived from this software without specific prior written permission.

  4. This software must only be used in a processor manufactured by Nordic
  Semiconductor ASA, or in a processor manufactured by a third party that
  is used in combination with a processor manufactured by Nordic Semiconductor.


THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
************************************************************************************/

#include "ts_controller.h"

#include <string.h>
#include <stdio.h>
#include "ts_rng.h"
#include "nrf_gpio.h"
//#include "boards.h"
#include "simple_uart.h"
#include "nrf_assert.h"
#include "app_error.h"	

#include "btle.h"
#include "ts_peripheral.h"
#include "nrf_advertiser.h"

#include "app_timer.h"

//#define DEBUG_TIMESLOT

/*****************************************************************************
* Local Definitions
*****************************************************************************/


/* Disable this flag to disable listening for scan requests, and jump straight to 
* next advertisement 
*/
#define TS_SEND_SCAN_RSP (0)

/* Quick macro to scale the interval to the timeslot scale */
#define ADV_INTERVAL_TRANSLATE(interval) (625 * (interval))

/* short macro to check whether the given event is triggered */
#define RADIO_EVENT(x) (NRF_RADIO->x != 0)

/*****************************************************************************
* Static Globals
*****************************************************************************/


/* Buffer for advertisement data */
static uint8_t ble_adv_data[BLE_ADDR_OFFSET + BLE_ADDR_LEN + BLE_PAYLOAD_MAXLEN];

/* Store the radio channel */
static uint8_t channel = 36;

/* running flag for advertiser. Decision point: end of timeslot (adv event) */
static bool sm_adv_run;

/* Channel map for advertisement */
static btle_dd_channel_map_t channel_map;
	
/* min advertisement interval */
static btle_adv_interval_t adv_int_min;

/* statemachine state */
static ts_state_t sm_state;

/* advertisement param->type to spec type map */
static const uint8_t ble_adv_type_raw[] = {0, 1, 6, 2};

/* Pool of 255 (the range of the RNG-peripheral) psedorandomly generated values */
static uint8_t rng_pool[255];

#if (SDK_VERSION >= 10)
    APP_TIMER_DEF(m_timeslot_timer);
#else
    #error "nRF IC Not Defined!"
#endif

/*****************************************************************************
* Globals
*****************************************************************************/

/* return param in signal handler */
nrf_radio_signal_callback_return_param_t g_signal_callback_return_param;


/* timeslot request EARLIEST. Used to send the first timeslot request */
static nrf_radio_request_t g_timeslot_req_earliest = 
{
    NRF_RADIO_REQ_TYPE_EARLIEST, 
	.params.earliest = 
                {
					HFCLK, 
					NRF_RADIO_PRIORITY_NORMAL, 
					TIMESLOT_LENGTH, 		
					10000
                }
};

/*****************************************************************************
* Static Functions
*****************************************************************************/
/**
* Get next channel to use. Affected by the tsa adv channels.
*/			
static __INLINE void channel_iterate(void)
{
    //channel_map & (1 << (++channel - 37)) == 0
    channel++;
    uint8_t index = channel - 37;
    uint32_t channel_mask = 1 << index;
    while (((channel_map & channel_mask) == 0) && channel < 40)
    {
        channel++;
        index = channel - 37;
        channel_mask = 1 << index;
    };    

	//while (((channel_map & (1 << (++channel - 37))) == 0) && channel < 40);	
}

#include "bmd_log.h"
/**
* Send initial time slot request to API. 
*/
static __INLINE void timeslot_req_initial(void)
{	
//	DEBUG_PIN_POKE(1);
	/* send to sd: */
	uint32_t error_code = sd_radio_request(&g_timeslot_req_earliest);
    if(error_code != NRF_SUCCESS)
    {
        bmd_log("Radio request returned an error: %02x\n", error_code);
    }
	//APP_ERROR_CHECK(error_code);
}

/**
* Doing the setup actions before the first adv_send state actions
*/
static __INLINE void adv_evt_setup(void)
{
	periph_radio_setup();
	
	/* set channel to first in sequence */
	channel = 36; /* will be iterated by channel_iterate() */
	channel_iterate();
}	

/******************************************
* Functions for start/end of adv_send state 
******************************************/
static void sm_enter_adv_send(void)
{
	sm_state = STATE_ADV_SEND;
	periph_radio_ch_set(channel);
	
	/* trigger task early, the rest of the setup can be done in RXRU */
	PERIPHERAL_TASK_TRIGGER(NRF_RADIO->TASKS_TXEN);
	
	periph_radio_packet_ptr_set(&ble_adv_data[0]);
	
    /* Enable radio interrupt propagation */
	NVIC_EnableIRQ(RADIO_IRQn);

	periph_radio_shorts_set( RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk);
	
	periph_radio_intenset(RADIO_INTENSET_DISABLED_Msk);
}

static void sm_exit_adv_send(void)
{
	/* wipe events and interrupts triggered by this state */
	periph_radio_intenclr(RADIO_INTENCLR_DISABLED_Msk);
	PERIPHERAL_EVENT_CLR(NRF_RADIO->EVENTS_DISABLED);
}

/*****************************************
* Functions for start/end of WAIT_FOR_IDLE
******************************************/
static void sm_enter_wait_for_idle(bool req_rx_accepted)
{
	sm_state = STATE_WAIT_FOR_IDLE;
	/* enable disabled interrupt to avoid race conditions */
	periph_radio_intenset(RADIO_INTENSET_DISABLED_Msk);
	
	/* different behaviour depending on whether we actually 
	received a scan request or not */
	if (!req_rx_accepted)
	{	
		/* remove shorts and disable radio */
		periph_radio_shorts_set(0);
		PERIPHERAL_TASK_TRIGGER(NRF_RADIO->TASKS_DISABLE);
	}
}

static bool sm_exit_wait_for_idle(void)
{
	periph_radio_intenclr(RADIO_INTENCLR_DISABLED_Msk);
	PERIPHERAL_EVENT_CLR(NRF_RADIO->EVENTS_DISABLED);
	
	channel_iterate();
	
	/* return whether the advertisement event is done */
	return (channel > 39);
}

static void timeslot_timer_handler(void * p_context)
{
    (void)p_context;
    ctrl_timeslot_order();
}

/*****************************************************************************
* Interface Functions
*****************************************************************************/
void ctrl_init(void)
{
	/* set the contents of advertisement and scan response to something
	that is in line with BLE spec */
	
	/* erase package buffers */
	memset(&ble_adv_data[0], 0, 40);
	
	/* set message type to ADV_IND_NONCONN, RANDOM in type byte of adv data */
	ble_adv_data[BLE_TYPE_OFFSET] = 0x42;
	
	/* set message length to only address */
	ble_adv_data[BLE_SIZE_OFFSET] = 0x06;
	
	/* generate rng sequence */
	adv_rng_init(rng_pool);
    
    #ifdef DEBUG_TIMESLOT
    nrf_gpio_cfg_output(0);
    nrf_gpio_cfg_output(1);
    nrf_gpio_cfg_output(2);
    nrf_gpio_cfg_output(3);
    nrf_gpio_cfg_output(4);
    nrf_gpio_cfg_output(5);
    nrf_gpio_cfg_output(6);
    nrf_gpio_cfg_output(8);
    nrf_gpio_cfg_output(11);
    #endif
	
    app_timer_create(&m_timeslot_timer, APP_TIMER_MODE_SINGLE_SHOT, timeslot_timer_handler);
}

void ctrl_signal_handler(uint8_t sig)
{
	switch (sig)
	{
		case NRF_RADIO_CALLBACK_SIGNAL_TYPE_START:
            DEBUG_PIN_SET(1);
			adv_evt_setup();
			sm_enter_adv_send();
			break;
		
		case NRF_RADIO_CALLBACK_SIGNAL_TYPE_RADIO:
		{	
			/* check state, and act accordingly */
			switch (sm_state)
			{
				case STATE_ADV_SEND:
					if (RADIO_EVENT(EVENTS_DISABLED))
					{
						sm_exit_adv_send();
						sm_enter_wait_for_idle(false);
						PERIPHERAL_TASK_TRIGGER(NRF_RADIO->TASKS_DISABLE);
					}
					break;
				case STATE_WAIT_FOR_IDLE:
					if (RADIO_EVENT(EVENTS_DISABLED))
					{
						/* state exit function returns whether the adv event is complete */
						bool adv_evt_done = sm_exit_wait_for_idle();
						
						if (adv_evt_done)
						{
                            DEBUG_PIN_CLEAR(1);
							//next_timeslot_schedule();
                            //DEBUG_PIN_POKE(1);
                            if(sm_adv_run)
                            {
                                app_timer_start(m_timeslot_timer, APP_TIMER_TICKS(adv_int_min, 0), NULL);
                            }
						}
						else
						{
							sm_enter_adv_send();
						}
					}
					break;							
			
				default:
					/* Shouldn't happen */
					ASSERT(false);
				}
		}
			break;	
		
		default:
			/* shouldn't happen in this advertiser. */
            break;	
	}
	
}	

bool ctrl_adv_param_set(btle_cmd_param_le_write_advertising_parameters_t* adv_params)
{
	ASSERT(adv_params != NULL);
	/* Checks for error */
	
	/* channel map */
	if (0x00 == adv_params->channel_map || 0x07 < adv_params->channel_map)
	{
		return false;
	}
	
	/* address */
    uint8_t zero_address[BTLE_DEVICE_ADDRESS__SIZE];
    memset(zero_address, 0, BTLE_DEVICE_ADDRESS__SIZE);
	if (memcmp(adv_params->direct_address, zero_address, 
            BTLE_DEVICE_ADDRESS__SIZE) == 0)
	{
		return false;
	}
	
	/* set channel map */
	channel_map = adv_params->channel_map;
	
	/* put address into advertisement packet buffer */
	memcpy((void*) &ble_adv_data[BLE_ADDR_OFFSET], (void*) &adv_params->direct_address[0], BLE_ADDR_LEN);
	
	/* address type */
	if (BTLE_ADDR_TYPE_PUBLIC == adv_params->own_address_type)
	{
		ble_adv_data[BLE_TYPE_OFFSET] 			&= ~(1 << BLE_TXADD_OFFSET);
	}
	else /* address type is private */
	{
		ble_adv_data[BLE_TYPE_OFFSET] 			|= (1 << BLE_TXADD_OFFSET);
	}
	
	/* Advertisement interval */
	adv_int_min = adv_params->interval_min;
	
	/* adv type is locked to nonconn */
	ble_adv_data[BLE_TYPE_OFFSET] &= ~BLE_TYPE_MASK;
	ble_adv_data[BLE_TYPE_OFFSET] |= (ble_adv_type_raw[BTLE_ADV_TYPE_NONCONN_IND] & BLE_TYPE_MASK);

	return true;
}

void ctrl_timeslot_order(void)
{
    DEBUG_PIN_POKE(2);
    //if(sm_adv_run == true)
    //    return;
    
	sm_adv_run = true;
	timeslot_req_initial();
}

void ctrl_timeslot_abort(void)
{
    DEBUG_PIN_POKE(3);
	  sm_adv_run = false;
    app_timer_stop(m_timeslot_timer);
}

bool ctrl_adv_data_set(btle_cmd_param_le_write_advertising_data_t* adv_data)
{	
	ASSERT(adv_data != NULL);
	
	/* length cannot exceed 31 bytes */
	uint8_t len = ((adv_data->data_length <= BLE_PAYLOAD_MAXLEN)? 
									adv_data->data_length : 
									BLE_PAYLOAD_MAXLEN);
	
	/* put into packet buffer */
	memcpy((void*) &ble_adv_data[BLE_PAYLOAD_OFFSET], (void*) &adv_data->advertising_data[0], len);
	
	/* set length of packet in length byte. Account for 6 address bytes */
	ble_adv_data[BLE_SIZE_OFFSET] = (BLE_ADDR_LEN + len);
	
	return true;
}

bool ctrl_scan_data_set(btle_cmd_param_le_write_scan_response_data_t* data)
{	
	return true;
}

bool ctrl_is_active(void)
{
    return (!(channel > 39));
}
