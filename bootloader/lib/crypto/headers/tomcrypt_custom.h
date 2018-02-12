#ifndef TOMCRYPT_CUSTOM_H_
#define TOMCRYPT_CUSTOM_H_

#define LTC_EAX_MODE
#define LTC_CTR_MODE
#define LTC_OMAC

#define LTC_SMALL_CODE
#define LTC_NO_FILE
#define LTC_NO_ASM

#define ARGTYPE 3

#define XMALLOC no_malloc
#define XREALLOC no_realloc
#define XCALLOC no_calloc
#define XFREE no_free
#define XCLOCK no_clock
#define XQSORT no_qsort
#define XMEMCPY memcpy

#define LTC_MUTEX_GLOBAL(x)
#define LTC_MUTEX_PROTO(x)
#define LTC_MUTEX_TYPE(x)
#define LTC_MUTEX_INIT(x)
#define LTC_MUTEX_LOCK(x)
#define LTC_MUTEX_UNLOCK(x)

#endif
