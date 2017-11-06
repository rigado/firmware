/** @file lock.c
*
* @brief This module manages security lock/unlock for BMDware settings
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "storage_intf.h"

#include "lock.h"

static bool m_locked = true;

static void get_cur_password(lock_password_t * pin);
static bool is_password_default(void);
static bool check_password(const lock_password_t *pw);

void lock_init(void)
{
    /* Default state is locked unless the default pin is being used
     * in which case we are always unlocked */
    m_locked = !is_password_default();
}

void lock_set(void)
{
    if(is_password_default())
    {
        m_locked = false;
        return;
    }
    
    m_locked = true;
}

bool lock_clear(const lock_password_t * password)
{
    bool result = false;
    lock_password_t cur;
  
    if(is_password_default())
    {
      m_locked = false;
      return true;
    }
		
    get_cur_password(&cur);
    if(strncmp((char*)&cur.password[0], (char*)&password->password[0], sizeof(cur.password)) == 0)
    {
        m_locked = false;
        result = true;
    }
    
    return result;
}

bool lock_is_locked(void)
{
  if(is_password_default())
  {
    m_locked = false;
    return false;
  }
  
  return m_locked;
}

bool lock_set_password(const lock_password_t * new_password)
{
    const default_app_settings_t * settings;
    default_app_settings_t new_settings;
  
    if(m_locked || !check_password(new_password))
    {
        return false;
    }
    
    settings = storage_intf_get();
    memcpy(&new_settings, settings, sizeof(new_settings));
    memcpy(&new_settings.password, new_password, sizeof(lock_password_t));  
    storage_intf_set(&new_settings);
    
    return true;
}

static void get_cur_password(lock_password_t * password)
{
    const default_app_settings_t * settings;
    settings = storage_intf_get();
    memcpy(password, &settings->password, sizeof(lock_password_t));
}

static bool is_password_default(void)
{ 
  bool result;
  lock_password_t pw;
  get_cur_password(&pw);
  
  result = (strncmp((char*)&pw.password[0], LOCK_DEFAULT_PASSWORD, sizeof(pw.password)) == 0);
  
  return result;
}


static bool check_password(const lock_password_t *pw)
{
  bool result = false;
  
  if( pw != NULL )
  {
    uint8_t idx = 0;
    bool foundNull = false;
    char ch;
    
    while(idx < sizeof(pw->password))
    {
      ch = pw->password[idx];
      
      //allow any printable char, except space
      if( (!foundNull &&
    		  //isprint(ch) &&
			  ch != ' ') ||
    		  ch == 0x00 )
      {
        if(ch==0x00)
          foundNull = true;
        
        idx++;
      }
      else
      {
        return false;
      }
    }
    
    result = true;
  }
  
  return result;
}
