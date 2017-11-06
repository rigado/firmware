#ifndef _VERSION_H_
#define _VERSION_H_

#define FIRMWARE_MAJOR_VERSION      3           /**< Text based major version number */
#define FIRMWARE_MINOR_VERSION      1           /**< Text based minor version number */
#define FIRMWARE_BUILD_NUMBER       1           /**< Text based build revision number */

#ifdef RELEASE
    #define BUILD_VERSION_NUMBER        51          /**< Represents the actual version number being released */
#else
    #define BUILD_VERSION_NUMBER        999
#endif

#define API_PROTOCOL_VERSION        2

#define _V_STR( x ) #x
#if defined(BMD_DEBUG)
    #if defined(BMD200_EVAL_V31)
        #define _VERSION_TO_STRING( major, minor, build, build_ver ) _V_STR(major) "." _V_STR(minor) "." _V_STR(build) "-eval-debug"
    #else
        #define _VERSION_TO_STRING( major, minor, build, build_ver ) _V_STR(major) "." _V_STR(minor) "." _V_STR(build) "-debug" "(" _V_STR(build_ver) ")"
    #endif
#elif defined(BMD200_EVAL_V31)
    #if defined(BMD_DEBUG)
        #define _VERSION_TO_STRING( major, minor, build, build_ver ) _V_STR(major) "." _V_STR(minor) "." _V_STR(build) "-eval-debug"
    #else
        #if defined (TEST_S120)
            #define _VERSION_TO_STRING( major, minor, build, build_ver ) _V_STR(major) "." _V_STR(minor) "." _V_STR(build) "-test-s120"
        #elif defined (TEST_S130)
            #define _VERSION_TO_STRING( major, minor, build, build_ver ) _V_STR(major) "." _V_STR(minor) "." _V_STR(build) "-test-s130"
        #else
            #define _VERSION_TO_STRING( major, minor, build, build_ver ) _V_STR(major) "." _V_STR(minor) "." _V_STR(build) "-eval (" _V_STR(build_ver) ")"
        #endif
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

#define MFG_NAME        "Rigado"
#define MODEL_STRING    "BMDware BMD-200"
