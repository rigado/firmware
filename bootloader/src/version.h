#ifndef VERSION_H
#define VERSION_H

/* Keep these three representations in sync */
#define FIRMWARE_MAJOR_VERSION      3
#define FIRMWARE_MINOR_VERSION      4
#define FIRMWARE_BUILD_NUMBER       0

#define BUILD_VERSION_NUMBER        47
#define API_PROTOCOL_VERSION        3

#define __V_STR(x) #x
#ifdef RELEASE
    #define _V_STR(major, minor, build, build_version) __V_STR(major) "." __V_STR(minor) "." __V_STR(build) " (" __V_STR(build_version) ")"
#else
    #define _V_STR(major, minor, build, build_version) __V_STR(major) "." __V_STR(minor) "." __V_STR(build) "_debug (" __V_STR(build_version) ")"
#endif
#define RIGDFU_VERSION _V_STR(FIRMWARE_MAJOR_VERSION, FIRMWARE_MINOR_VERSION, FIRMWARE_BUILD_NUMBER, BUILD_VERSION_NUMBER)

#endif
