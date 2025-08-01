/*______                              _
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2024 Semtech

Description:
    LoRaHub Hardware Abstraction Layer - RX

License: Revised BSD License, see LICENSE.TXT file include in the project
*/

#ifndef _LORAHUB_HAL_RX_H
#define _LORAHUB_HAL_RX_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDENCIES --------------------------------------------------------- */

#include "ral.h"
#include "lorahub_hal.h"

/* -------------------------------------------------------------------------- */
/* --- PUBLIC MACROS -------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC CONSTANTS ----------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC TYPES --------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

int sx126x_radio_init_rx( const ral_t* ral );

int sx126x_radio_configure_rx( const ral_t* ral, uint32_t freq_hz, const struct sx126x_conf_rxif_s* modulation_params );

int sx126x_radio_set_rx( const ral_t* ral );

int sx126x_radio_get_pkt( const ral_t* ral, bool* irq_received, uint32_t* count_us, uint8_t* sf, int8_t* rssi, int8_t* snr,
                       uint8_t* status, uint16_t* size, uint8_t* payload );

uint32_t sx126x_radio_timestamp_correction( uint8_t sf, uint8_t bw );

#endif  // _LORAHUB_HAL_RX_H

/* --- EOF ------------------------------------------------------------------ */
