/*______                              _
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2024 Semtech

Description:
    LoRaHub HAL auxiliary functions

License: Revised BSD License, see LICENSE.TXT file include in the project
*/

/* -------------------------------------------------------------------------- */
/* --- DEPENDENCIES --------------------------------------------------------- */

#include <math.h>       /* pow, ceil */
#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */
#include <time.h>    

#include "lorahub_log.h"
#include "lorahub_aux.h"
#include "lorahub_hal.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ----------------------------------------------------- */

#define FREQ_HZ_SUBGHZ_MIN 150000000
#define FREQ_HZ_SUBGHZ_MAX 960000000
#define FREQ_HZ_2_4GHZ_MIN 2400000000
#define FREQ_HZ_2_4GHZ_MAX 2500000000

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

/*
uint32_t sx126x_get_lora_bw_in_hz( uint8_t bw )
{
    uint32_t bw_in_hz = 0;

    switch( bw )
    {
    case BW_125KHZ:
        bw_in_hz = 125000UL;
        break;
    case BW_250KHZ:
        bw_in_hz = 250000UL;
        break;
    case BW_500KHZ:
        bw_in_hz = 500000UL;
        break;
    case BW_200KHZ:
        bw_in_hz = 203000UL;
        break;
    case BW_400KHZ:
        bw_in_hz = 406000UL;
        break;
    case BW_800KHZ:
        bw_in_hz = 812000UL;
        break;
    default:
        printf( "bandwidth %u not supported", bw );
        break;
    }

    return bw_in_hz;
}
*/

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

ral_lora_sf_t sx126x_convert_hal_to_ral_sf( uint8_t sf )
{
    switch( sf )
    {
    case DR_LORA_SF5:
        return RAL_LORA_SF5;
    case DR_LORA_SF6:
        return RAL_LORA_SF6;
    case DR_LORA_SF7:
        return RAL_LORA_SF7;
    case DR_LORA_SF8:
        return RAL_LORA_SF8;
    case DR_LORA_SF9:
        return RAL_LORA_SF9;
    case DR_LORA_SF10:
        return RAL_LORA_SF10;
    case DR_LORA_SF11:
        return RAL_LORA_SF11;
    case DR_LORA_SF12:
        return RAL_LORA_SF12;
    default:
        printf( "spreading factor %u not supported", sf );
        return -1;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

ral_lora_bw_t sx126x_convert_hal_to_ral_bw( uint8_t bw )
{
    switch( bw )
    {
    /* Sub-Ghz bandwidths */
    case BW_125KHZ:
        return RAL_LORA_BW_125_KHZ;
    case BW_250KHZ:
        return RAL_LORA_BW_250_KHZ;
    case BW_500KHZ:
        return RAL_LORA_BW_500_KHZ;
    default:
        printf( "bandwidth %u not supported", bw );
        return -1;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

ral_lora_cr_t sx126x_convert_hal_to_ral_cr( uint8_t cr )
{
    switch( cr )
    {
    case CR_LORA_4_5:
        return RAL_LORA_CR_4_5;
    case CR_LORA_4_6:
        return RAL_LORA_CR_4_6;
    case CR_LORA_4_7:
        return RAL_LORA_CR_4_7;
    case CR_LORA_4_8:
        return RAL_LORA_CR_4_8;
    default:
        printf( "coderate %u not supported", cr );
        return -1;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int sx126x_check_lora_mod_params( uint32_t freq_hz, uint8_t bw, uint8_t cr )
{
    if( ( freq_hz < FREQ_HZ_SUBGHZ_MIN ) || ( freq_hz > FREQ_HZ_SUBGHZ_MAX ) )
    {
        printf( "frequency not supported" );
        return LGW_HAL_ERROR;
    }

    if( ( bw != BW_125KHZ ) && ( bw != BW_250KHZ ) && ( bw != BW_500KHZ ) )
    {
        printf( "bandwidth not supported" );
        return LGW_HAL_ERROR;
    }

    if( ( cr != CR_LORA_4_5 ) && ( cr != CR_LORA_4_6 ) && ( cr != CR_LORA_4_7 ) && ( cr != CR_LORA_4_8 ) )
    {
        printf( "coderate not supported" );
        return LGW_HAL_ERROR;
    }

    return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

uint8_t sx126x_get_lora_sync_word( uint32_t freq_hz, uint8_t sf )
{
    uint8_t lora_sync_word = LORA_SYNC_WORD_PRIVATE;

    if( LPWAN_NETWORK_TYPE == LPWAN_NETWORK_TYPE_PUBLIC )
    {
        if( freq_hz >= 2400000000 )
        {
            /* 2.4ghz */
            lora_sync_word = LORA_SYNC_WORD_PUBLIC_WW2G4;
        }
        else
        {
            /* sub-ghz */
            if( sf >= DR_LORA_SF7 )
            {
                lora_sync_word = LORA_SYNC_WORD_PUBLIC_SUBGHZ;
            }
        }
    }
    printf( "LoRa sync word: 0x%02X", lora_sync_word );

    return lora_sync_word;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int sx126x_check_lora_dualsf_conf( uint8_t bw, uint8_t sf1, uint8_t sf2 )
{
    return LGW_HAL_SUCCESS;
}

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

/* --- EOF ------------------------------------------------------------------ */
