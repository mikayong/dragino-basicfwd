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

#include <stdint.h>   /* C99 types */
#include <stdbool.h>  /* bool type */
#include <sys/time.h> /* gettimeofday, structtimeval */

#include "ral.h"
#include "basic_aux.h"

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

uint32_t sx126x_get_lora_bw_in_hz( uint8_t bw );

ral_lora_sf_t sx126x_convert_hal_to_ral_sf( uint8_t sf );

ral_lora_bw_t sx126x_convert_hal_to_ral_bw( uint8_t bw );

ral_lora_cr_t sx126x_convert_hal_to_ral_cr( uint8_t cr );

int sx126x_check_lora_mod_params( uint32_t freq_hz, uint8_t bw, uint8_t cr );

uint8_t sx126x_get_lora_sync_word( uint32_t freq_hz, uint8_t sf );

int sx126x_check_lora_dualsf_conf( uint8_t bw, uint8_t sf1, uint8_t sf2 );

#endif

/* --- EOF ------------------------------------------------------------------ */
