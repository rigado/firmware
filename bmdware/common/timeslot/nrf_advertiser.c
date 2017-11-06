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

#include "nrf_advertiser.h"

#include "ts_controller.h"
#include "ts_peripheral.h"

#include "nrf_soc.h"
#include "nrf_sdm.h"
#include "ble.h"
#include "nrf_assert.h"
#include "app_error.h"	
#include "simple_uart.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>

static bool m_radio_session_open = false;
void (*adv_event_callback)(uint32_t event);

/*****************************************************************************
* Static Functions
*****************************************************************************/


/**
* Callback for timeslot signals. Is only called when in a timeslot.
* All radio events during timeslot is redirected here
*/
static nrf_radio_signal_callback_return_param_t* radio_signal_callback(uint8_t sig)
{	
	/* default return value is none */
	g_signal_callback_return_param.callback_action = NRF_RADIO_SIGNAL_CALLBACK_ACTION_NONE;
	
	/* Send signal to the state machine, and let it decide how the event affects the flow */
	ctrl_signal_handler(sig);
	
	/* indicating that the timeslot ended */
	if (NRF_RADIO_SIGNAL_CALLBACK_ACTION_REQUEST_AND_END 	
				== g_signal_callback_return_param.callback_action)
	{
	}
	
	return &g_signal_callback_return_param;
}

/*****************************************************************************
* Interface Functions
*****************************************************************************/


/**
* callback to handle timeslot specific events. Should never be called, 
* as the advertiser never is to go idle or shut down 
*/
void btle_hci_adv_sd_evt_handler(uint32_t event)
{
	switch (event)
	{
		case NRF_EVT_RADIO_SESSION_IDLE:
			/* Means the user stopped the advertiser. Do nothing. */			
			break;
        
		case NRF_EVT_RADIO_SESSION_CLOSED:
			/* session close accepted, lets just sleep forever.
			   shouldn't really happen. */
			ASSERT(false);
            break;
        
		case NRF_EVT_RADIO_BLOCKED:
			/* The request was blocked by a softdevice event. 
					Attempt to reschedule the timeslot for as soon as possible */
			DEBUG_PIN_POKE(4);
            ctrl_timeslot_abort();
            ctrl_timeslot_order();
			break;
        
		case NRF_EVT_RADIO_SIGNAL_CALLBACK_INVALID_RETURN:
			break;
        
		case NRF_EVT_RADIO_CANCELED:
			/* The softdevice decided to cancel an ongoing timeslot. 
					Attempt to reschedule the timeslot for as soon as possible */
			DEBUG_PIN_POKE(5);
			ctrl_timeslot_order();
			break;
        
		default:
			/* Invalid event type, ignore */
			break;
	}
}


void btle_hci_adv_init(void)
{
	uint8_t error_code;	
	
	/* init controller layer */
	ctrl_init();
	
	/* initiate timeslot session: */
	error_code = sd_radio_session_open(&radio_signal_callback);
	APP_ERROR_CHECK(error_code);
    
    m_radio_session_open = true;
}

void btle_hci_adv_deinit(void)
{
    uint32_t error_code;
    
    if(!m_radio_session_open)
    {
        return;
    }
    
    error_code = sd_radio_session_close();
    APP_ERROR_CHECK(error_code);
    
    m_radio_session_open = false;
}

void btle_hci_adv_params_set(btle_cmd_param_le_write_advertising_parameters_t* adv_params)
{
	ctrl_adv_param_set(adv_params);
}

void btle_hci_adv_enable(btle_adv_mode_t adv_enable)
{
	if (BTLE_ADV_ENABLE == adv_enable)
	{
		/* send first timeslot request to get the session started: */
		ctrl_timeslot_order();
	}
	else if (BTLE_ADV_DISABLE == adv_enable)
	{
		/* stop advertisement after next advertisement event */
		ctrl_timeslot_abort();
	}
	else
	{
		/* invalid parameter */
		ASSERT(false);
	}
}

void btle_hci_adv_callback(void (*callback)(uint32_t event))
{
    adv_event_callback = callback;
}

void btle_hci_adv_data_set(btle_cmd_param_le_write_advertising_data_t* adv_data)
{
	ctrl_adv_data_set(adv_data);
}

void btle_hci_adv_scan_rsp_data_set(btle_cmd_param_le_write_scan_response_data_t* scan_rsp)
{
	ctrl_scan_data_set(scan_rsp);
}
