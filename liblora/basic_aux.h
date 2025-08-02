/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2019 Semtech

Description:
    LoRa concentrator HAL common auxiliary functions

License: Revised BSD License, see LICENSE.TXT file include in the project
*/


#ifndef _BASIC_AUX_H
#define _BASIC_AUX_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */
#include <sys/time.h>   /* gettimeofday, structtimeval */

#include <time.h>       /* clock_nanosleep */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC CONSTANTS ----------------------------------------------------- */

#define DEBUG_PERF 0   /* Debug timing performances: level [0..4] */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC MACROS -------------------------------------------------------- */

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

/**
@brief Get a particular bit value from a byte
@param b [in]   Any byte from which we want a bit value
@param p [in]   Position of the bit in the byte [0..7]
@param n [in]   Number of bits we want to get
@return The value corresponding the requested bits
*/
#define TAKE_N_BITS_FROM(b, p, n) (((b) >> (p)) & ((1 << (n)) - 1))

/**
@brief Substract struct timeval values
@param a [in]   struct timeval a
@param b [in]   struct timeval b
@param b [out]  struct timeval resulting from (a - b)
*/
#define TIMER_SUB(a, b, result)                                                \
    do  {                                                                      \
        (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                          \
        (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;                       \
        if ((result)->tv_usec < 0) {                                           \
            --(result)->tv_sec;                                                \
            (result)->tv_usec += 1000000;                                      \
        }                                                                      \
    } while (0)

/*!
 * @brief Stringify constants
 */
#define xstr( a ) str( a )
#define str( a ) #a

/*!
 * @brief Helper macro that returned a human-friendly message if a command does not return RAL_STATUS_OK
 *
 * @remark The macro is implemented to be used with functions returning a @ref ral_status_t
 *
 * @param[in] rc  Return code
 */
#define ASSERT_RAL_RC( rc )                                                                                     \
    {                                                                                                           \
        const ral_status_t status = rc;                                                                         \
        if( status != RAL_STATUS_OK )                                                                           \
        {                                                                                                       \
            if( status == RAL_STATUS_ERROR )                                                                    \
            {                                                                                                   \
                printf( "In %s - %s (line %d): %s", __FILE__, __func__, __LINE__,               \
                          xstr( RAL_STATUS_ERROR ) );                                                           \
                return -1;                                                                                      \
            }                                                                                                   \
            else                                                                                                \
            {                                                                                                   \
                printf( "In %s - %s (line %d): Status code = %d", __FILE__, __func__, __LINE__, \
                          status );                                                                             \
                return -1;                                                                                      \
            }                                                                                                   \
        }                                                                                                       \
    }


/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

void wait_us(unsigned long delay_us) {
    struct timespec dly;
    struct timespec rem;

    dly.tv_sec = delay_us / 1000000;
    dly.tv_nsec = (delay_us % 1000000) * 1000;

    //DEBUG_PRINTF("NOTE dly: %ld sec %ld ns\n", dly.tv_sec, dly.tv_nsec);

    while ((dly.tv_sec > 0) || (dly.tv_nsec > 1000)) {
        /*
        rem is set ONLY if clock_nanosleep is interrupted (eg. by a signal).
        Must be zeroed each time or will get into an infinite loop after an IT.
        */
        rem.tv_sec = 0;
        rem.tv_nsec = 0;
        clock_nanosleep(CLOCK_MONOTONIC, 0, &dly, &rem);
        //DEBUG_PRINTF("NOTE remain: %ld sec %ld ns\n", rem.tv_sec, rem.tv_nsec);
        dly = rem;
    }

    return;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void wait_ms(unsigned long delay_ms) {
    struct timespec dly;
    struct timespec rem;

    dly.tv_sec = delay_ms / 1000;
    dly.tv_nsec = ((long)delay_ms % 1000) * 1000000;

    //DEBUG_PRINTF("NOTE dly: %ld sec %ld ns\n", dly.tv_sec, dly.tv_nsec);

    if((dly.tv_sec > 0) || ((dly.tv_sec == 0) && (dly.tv_nsec > 100000))) {
        clock_nanosleep(CLOCK_MONOTONIC, 0, &dly, &rem);
        //DEBUG_PRINTF("NOTE remain: %ld sec %ld ns\n", rem.tv_sec, rem.tv_nsec);
    }
    return;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void timeout_start(struct timeval * start) {
    gettimeofday(start, NULL);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int timeout_check(struct timeval start, uint32_t timeout_ms) {
    struct timeval tm;
    struct timeval diff;
    uint32_t ms;

    gettimeofday(&tm, NULL);

    TIMER_SUB(&tm, &start, &diff);

    ms = diff.tv_sec * 1000 + diff.tv_usec / 1000;
    if (ms >= timeout_ms) {
        return -1;
    } else {
        return 0;
    }
}

#endif

/* --- EOF ------------------------------------------------------------------ */
