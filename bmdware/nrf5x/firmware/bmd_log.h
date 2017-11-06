#ifndef BMD_LOG_H_
#define BMD_LOG_H_

//#define BMD_LOG_ENABLED

#if defined(BMD_LOG_ENABLED) || defined(BMD_DEBUG)
    #define NRF_LOG_USES_RTT    1
    #define RTT_CHANNEL         0

    #include <stdarg.h>
    #include <stdlib.h>
    #include "SEGGER_RTT.h"

    #define bmd_log_init()  SEGGER_RTT_Init();
    #define bmd_log(...)    SEGGER_RTT_printf(RTT_CHANNEL, __VA_ARGS__);    
#else
    #define bmd_log_init()
    #define bmd_log(...)    
#endif

#endif
