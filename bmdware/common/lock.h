/** @file lock.h
*
* @brief This module manages security lock/unlock for BMDware settings
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef __LOCK_H__
#define __LOCK_H__

#include <stdbool.h>
#include <stdint.h>

#define LOCK_DEFAULT_PASSWORD		"password1234"

typedef struct
{
	uint8_t password[19];
	uint8_t pad; //word align pad
} lock_password_t;
	
void lock_init(void);
void lock_set(void);
bool lock_clear(const lock_password_t * password);
bool lock_is_locked(void);
bool lock_set_password(const lock_password_t * new_password);

#endif
