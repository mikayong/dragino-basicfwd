/*______                              _
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2024 Semtech

Description:
    LoRaHub HAL common auxiliary functions

License: Revised BSD License, see LICENSE.TXT file include in the project
*/

#ifndef _LORAHUB_AUX_H
#define _LORAHUB_AUX_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDENCIES --------------------------------------------------------- */
#include <stdio.h>
#include <sys/time.h>   /* gettimeofday, structtimeval */

#include "ral.h"

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
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

void wait_us(unsigned long delay_us);
void wait_ms(unsigned long delay_ms);
void timeout_start(struct timeval * start);
int timeout_check(struct timeval start, uint32_t timeout_ms);

//uint32_t sx126x_get_lora_bw_in_hz( uint8_t bw );

ral_lora_sf_t sx126x_convert_hal_to_ral_sf( uint8_t sf );

ral_lora_bw_t sx126x_convert_hal_to_ral_bw( uint8_t bw );

ral_lora_cr_t sx126x_convert_hal_to_ral_cr( uint8_t cr );

int sx126x_check_lora_mod_params( uint32_t freq_hz, uint8_t bw, uint8_t cr );

uint8_t sx126x_get_lora_sync_word( uint32_t freq_hz, uint8_t sf );

int sx126x_check_lora_dualsf_conf( uint8_t bw, uint8_t sf1, uint8_t sf2 );

#endif

/* --- EOF ------------------------------------------------------------------ */
