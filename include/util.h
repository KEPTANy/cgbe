#ifndef UTIL_H
#define UTIL_H

#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || defined(__BIG_ENDIAN__) ||            \
    defined(__ARMEB__) || defined(__THUMBEB__) || defined(__AARCH64EB__) || defined(_MIBSEB) ||    \
    defined(__MIBSEB) || defined(__MIBSEB__)
#define BIG_ENDIAN
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN || defined(__LITTLE_ENDIAN__) ||    \
    defined(__ARMEL__) || defined(__THUMBEL__) || defined(__AARCH64EL__) || defined(_MIPSEL) ||    \
    defined(__MIPSEL) || defined(__MIPSEL__)
#define LITTLE_ENDIAN
#else
#error "Failed to detect endianness"
#endif

#endif
