#ifndef __SW_IRQ_MANAGER_H__
#define __SW_IRQ_MANAGER_H__

#include <stdint.h>

typedef void (*sw_irq_callback_t)(void);
typedef uint8_t sw_irq_callback_id_t;

void sw_irq_manager_init(void);
uint32_t sw_irq_manager_register_callback(sw_irq_callback_t callback, sw_irq_callback_id_t * sw_callback_id);
uint32_t sw_irq_manager_trigger_int(sw_irq_callback_id_t callback_id);

#endif
