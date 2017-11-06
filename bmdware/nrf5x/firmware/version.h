/** @file version.h
*
* @brief Contains firmware version and string macros for display
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef _VERSION_H_
#define _VERSION_H_

#define FIRMWARE_MAJOR_VERSION      3
#define FIRMWARE_MINOR_VERSION      2
#define FIRMWARE_BUILD_NUMBER       2

#define BUILD_VERSION_NUMBER        61
#define API_PROTOCOL_VERSION        3

#define _V_STR( x ) #x
#if defined(BMD_DEBUG)
    #if defined(BMD200_EVAL_V31)
        #define _VERSION_TO_STRING( major, minor, build, build_ver ) _V_STR(major) "." _V_STR(minor) "." _V_STR(build) "-eval-debug"
    #else
        #define _VERSION_TO_STRING( major, minor, build, build_ver ) _V_STR(major) "." _V_STR(minor) "." _V_STR(build) "-debug " " (" _V_STR(build_ver) ")"
    #endif
#elif defined(BMD200_EVAL_V31)
    #if defined(BMD_DEBUG)
        #define _VERSION_TO_STRING( major, minor, build, build_ver ) _V_STR(major) "." _V_STR(minor) "." _V_STR(build) "-eval-debug"
    #else
        #define _VERSION_TO_STRING( major, minor, build, build_ver ) _V_STR(major) "." _V_STR(minor) "." _V_STR(build) "-eval-rel"
    #endif
#else
    #define _VERSION_TO_STRING( major, minor, build, build_ver ) _V_STR(major) "." _V_STR(minor) "." _V_STR(build) " (" _V_STR(build_ver) ")"
#endif
    #define _VERSION_TO_HW_STRING( major, minor ) _V_STR(major) _V_STR(minor)

#define FIRMWARE_VERSION_STRING \
    _VERSION_TO_STRING( FIRMWARE_MAJOR_VERSION, FIRMWARE_MINOR_VERSION, FIRMWARE_BUILD_NUMBER, BUILD_VERSION_NUMBER )

#define HARDWARE_MAJOR_VERSION      0
#define HARDWARE_MINOR_VERSION      1

#define HARDWARE_VERSION_STRING \
    _VERSION_TO_STRING( HARDWARE_MAJOR_VERSION, HARDWARE_MINOR_VERSION )
#endif

#if defined(NRF52)
	#define MODEL_STRING "BMDware BMD-300"
#else
	#define MODEL_STRING "BMDware BMD-200"
#endif

#define MFG_NAME     "Rigado"
