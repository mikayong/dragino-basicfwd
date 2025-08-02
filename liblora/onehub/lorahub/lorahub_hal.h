/*______                              _
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2024 Semtech

Description:
    LoRaHub Hardware Abstraction Layer

License: Revised BSD License, see LICENSE.TXT file include in the project
*/

#ifndef _LORAHUB_HAL_H
#define _LORAHUB_HAL_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDENCIES --------------------------------------------------------- */

#include "basic_hal.h"

/* -------------------------------------------------------------------------- */
/* --- PUBLIC CONSTANTS ----------------------------------------------------- */

/* return status code */
#define LGW_HAL_SUCCESS 0
#define LGW_HAL_ERROR -1

/* radio-specific parameters */
#define SX126X_RF_CHAIN_NB 1 /* number of RF chains */
#define LGW_MULTI_SF_NB 2 /* maximum number of spreading factor supported (dual-sf on LR11xx) */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC TYPES --------------------------------------------------------- */

/**
@struct sx126x_conf_rxrf_s
@brief Radio configuration structure
*/
struct sx126x_conf_rxrf_s
{
    uint32_t freq_hz;     /*!> center frequency of the radio in Hz */
    float    rssi_offset; /*!> Board-specific RSSI correction factor */
    bool     tx_enable;   /*!> enable or disable TX on that RF chain */
};

/**
@struct sx126x_conf_rxif_s
@brief Modulation configuration structure
*/
struct sx126x_conf_rxif_s
{
    uint8_t modulation;                /*!> RX modulation */
    uint8_t bandwidth;                 /*!> RX bandwidth */
    uint8_t datarate[LGW_MULTI_SF_NB]; /*!> RX spreading factor(s) */
    uint8_t coderate;                  /*!> RX coding rate */
};

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

/**
@brief Configure the radio parameters (must configure before start)
@param conf structure containing the configuration parameters
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int sx126x_rxrf_setconf( struct sx126x_conf_rxrf_s* conf );

/**
@brief Configure the modulation parameters (must configure before start)
@param conf structure containing the configuration parameters
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int sx126x_rxif_setconf( struct sx126x_conf_rxif_s* conf );

/**
@brief Connect to the LoRa concentrator, reset it and configure it according to previously set parameters
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int sx126x_start( void );

/**
@brief Stop the LoRa concentrator and disconnect it
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int sx126x_stop( void );

/**
@brief A non-blocking function that will fetch up to 'max_pkt' packets from the LoRa concentrator FIFO and data buffer
@param max_pkt maximum number of packet that must be retrieved (equal to the size of the array of struct)
@param pkt_data pointer to an array of struct that will receive the packet metadata and payload pointers
@return LGW_HAL_ERROR id the operation failed, else the number of packets retrieved
*/
int sx126x_receive( uint8_t max_pkt, struct lgw_pkt_rx_s* pkt_data );

/**
@brief Schedule a packet to be send immediately or after a delay depending on tx_mode
@param pkt_data structure containing the data and metadata for the packet to send
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else

/!\ When sending a packet, there is a delay (approx 1.5ms) for the analog
circuitry to start and be stable. This delay is adjusted by the HAL depending
on the board version (sx126x_i_tx_start_delay_us).

In 'timestamp' mode, this is transparent: the modem is started
sx126x_i_tx_start_delay_us microseconds before the user-set timestamp value is
reached, the preamble of the packet start right when the internal timestamp
counter reach target value.

In 'immediate' mode, the packet is emitted as soon as possible: transferring the
packet (and its parameters) from the host to the concentrator takes some time,
then there is the sx126x_i_tx_start_delay_us, then the packet is emitted.

In 'triggered' mode (aka PPS/GPS mode), the packet, typically a beacon, is
emitted sx126x_i_tx_start_delay_us microsenconds after a rising edge of the
trigger signal. Because there is no way to anticipate the triggering event and
start the analog circuitry beforehand, that delay must be taken into account in
the protocol.
*/
int sx126x_send( struct lgw_pkt_tx_s* pkt_data );

/**
@brief Give the the status of different part of the LoRa concentrator
@param select is used to select what status we want to know
@param code is used to return the status code
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int sx126x_status( uint8_t rf_chain, uint8_t select, uint8_t* code );

/**
@brief Return instateneous value of internal counter
@param inst_cnt_us pointer to receive timestamp value
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int sx126x_get_instcnt( uint32_t* inst_cnt_us );

/**
@brief Return time on air of given packet, in milliseconds
@param packet is a pointer to the packet structure
@return the packet time on air in milliseconds
*/
uint32_t sx126x_time_on_air( const struct lgw_pkt_tx_s* packet );

/**
@brief Return minimum and maximum frequency supported by the configured radio (in Hz)
@param  min_freq_hz pointer to hold the minimum frequency supported
@param  max_freq_hz pointer to hold the maximum frequency supported
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int sx126x_get_min_max_freq_hz( uint32_t* min_freq_hz, uint32_t* max_freq_hz );

/**
@brief Return minimum and maximum TX power supported by the configured radio (in dBm)
@param  min_freq_hz pointer to hold the minimum TX power supported
@param  max_freq_hz pointer to hold the maximum TX power supported
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int sx126x_get_min_max_power_dbm( int8_t* min_power_dbm, int8_t* max_power_dbm );

#endif  // _LORAHUB_HAL_H

/* --- EOF ------------------------------------------------------------------ */
