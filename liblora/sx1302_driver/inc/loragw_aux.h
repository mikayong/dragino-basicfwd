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


#ifndef _LORAGW_AUX_H
#define _LORAGW_AUX_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */

#include "config.h"     /* library configuration options (dynamically generated) */
#include "basic_aux.h"

/* -------------------------------------------------------------------------- */
/* --- PUBLIC CONSTANTS ----------------------------------------------------- */

#define DEBUG_PERF 0   /* Debug timing performances: level [0..4] */


/**
@brief Calculate the time on air of a LoRa packet in microseconds
@param bw packet bandwidth
@param sf packet spreading factor
@param cr packet coding rate
@param n_symbol_preamble packet preamble length (number of symbols)
@param no_header true if packet has no header
@param no_crc true if packet has no CRC
@param size packet size in bytes
@param nb_symbols pointer to return the total number of symbols in packet
@param nb_symbols_payload pointer to return the number of symbols in packet payload
@param t_symbol_us pointer to return the duration of a symbol in microseconds
@return the packet time on air in microseconds
*/
uint32_t lora_packet_time_on_air( const uint8_t bw,
                                  const uint8_t sf,
                                  const uint8_t cr,
                                  const uint16_t n_symbol_preamble,
                                  const bool no_header,
                                  const bool no_crc,
                                  const uint8_t size,
                                  double * nb_symbols,
                                  uint32_t * nb_symbols_payload,
                                  uint16_t * t_symbol_us);

/**
@brief Record the current time, for measure start
@param tm Pointer to the current time value
*/
void _meas_time_start(struct timeval *tm);

/**
@brief Measure the ellapsed time since given time
@param debug_level  debug print debug level to be used
@param start_time   start time of the measure to be used
@param str          string to be used for debug print
*/
void _meas_time_stop(int debug_level, struct timeval start_time, const char *str);

#endif

/* --- EOF ------------------------------------------------------------------ */
