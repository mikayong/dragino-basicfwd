/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2019 Semtech

Description:
    LoRa concentrator Hardware Abstraction Layer

License: Revised BSD License, see LICENSE.TXT file include in the project
*/

#ifndef _BASIC_HAL_H
#define _BASIC_HAL_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC MACROS -------------------------------------------------------- */

#define IS_LORA_BW(bw)          ( (bw == BW_125KHZ) || (bw == BW_250KHZ) || (bw == BW_500KHZ) || ( bw == BW_400KHZ ) || ( bw == BW_800KHZ ) )
#define IS_LORA_DR(dr)          ( (dr == DR_LORA_SF5) || (dr == DR_LORA_SF6) || (dr == DR_LORA_SF7) || (dr == DR_LORA_SF8) || (dr == DR_LORA_SF9) || (dr == DR_LORA_SF10) || (dr == DR_LORA_SF11) || (dr == DR_LORA_SF12) )
#define IS_LORA_CR(cr)          ((cr == CR_LORA_4_5) || (cr == CR_LORA_4_6) || (cr == CR_LORA_4_7) || (cr == CR_LORA_4_8))

#define IS_FSK_BW(bw)           ((bw >= 1) && (bw <= 7))
#define IS_FSK_DR(dr)           ((dr >= DR_FSK_MIN) && (dr <= DR_FSK_MAX))

#define IS_TX_MODE(mode)        ((mode == IMMEDIATE) || (mode == TIMESTAMPED) || (mode == ON_GPS))

/* -------------------------------------------------------------------------- */
/* --- PUBLIC CONSTANTS ----------------------------------------------------- */

/* return status code */
#define LGW_HAL_SUCCESS     0
#define LGW_HAL_ERROR       -1
#define LGW_LBT_ISSUE       1
#define LGW_LBT_NOT_ALLOWED 1

/* values available for the 'modulation' parameters */
/* NOTE: arbitrary values */
#define MOD_UNDEFINED   0
#define MOD_CW          0x08
#define MOD_LORA        0x10
#define MOD_FSK         0x20

/* values available for the 'bandwidth' parameters (LoRa & FSK) */
/* NOTE: directly encode FSK RX bandwidth, do not change */
#define BW_UNDEFINED    0
#define BW_500KHZ       0x06
#define BW_250KHZ       0x05
#define BW_125KHZ       0x04
#define BW_800KHZ     0x0F /* 2.4Ghz bandwidth */
#define BW_400KHZ     0x0E /* 2.4Ghz bandwidth */
#define BW_200KHZ     0x0D /* 2.4Ghz bandwidth */

/* values available for the 'datarate' parameters */
/* NOTE: LoRa values used directly to code SF bitmask in 'multi' modem, do not change */
#define DR_UNDEFINED    0
#define DR_LORA_SF5     5
#define DR_LORA_SF6     6
#define DR_LORA_SF7     7
#define DR_LORA_SF8     8
#define DR_LORA_SF9     9
#define DR_LORA_SF10    10
#define DR_LORA_SF11    11
#define DR_LORA_SF12    12
/* NOTE: for FSK directly use baudrate between 500 bauds and 250 kbauds */
#define DR_FSK_MIN      500
#define DR_FSK_MAX      250000

/* values available for the 'coderate' parameters (LoRa only) */
/* NOTE: arbitrary values */
#define CR_UNDEFINED    0   /* CR0 exists but is not recommended, so consider it as invalid */
#define CR_LORA_4_5     0x01
#define CR_LORA_4_6     0x02
#define CR_LORA_4_7     0x03
#define CR_LORA_4_8     0x04
#define CR_LORA_LI_4_5 0x05
#define CR_LORA_LI_4_6 0x06
#define CR_LORA_LI_4_8 0x07

/* values available for the 'status' parameter */
/* NOTE: values according to hardware specification */
#define STAT_UNDEFINED  0x00
#define STAT_NO_CRC     0x01
#define STAT_CRC_BAD    0x11
#define STAT_CRC_OK     0x10

/* values available for the 'tx_mode' parameter */
#define IMMEDIATE       0
#define TIMESTAMPED     1
#define ON_GPS          2

/* values available for 'select' in the status function */
#define TX_STATUS       1
#define RX_STATUS       2

/* status code for TX_STATUS */
/* NOTE: arbitrary values */
#define TX_STATUS_UNKNOWN   0
#define TX_OFF              1    /* TX modem disabled, it will ignore commands */
#define TX_FREE             2    /* TX modem is free, ready to receive a command */
#define TX_SCHEDULED        3    /* TX modem is loaded, ready to send the packet after an event and/or delay */
#define TX_EMITTING         4    /* TX modem is emitting */

/* status code for RX_STATUS */
/* NOTE: arbitrary values */
#define RX_STATUS_UNKNOWN   0
#define RX_OFF              1    /* RX modem is disabled, it will ignore commands  */
#define RX_ON               2    /* RX modem is receiving */
#define RX_SUSPENDED        3    /* RX is suspended while a TX is ongoing */

#define MIN_LORA_PREAMBLE 6
#define STD_LORA_PREAMBLE 8
#define HDR_LORA_PREAMBLE 12

#define LORA_SYNC_WORD_PRIVATE 0x12       /* Private Network */
#define LORA_SYNC_WORD_PUBLIC_SUBGHZ 0x34 /* Public Network for Sub-Ghz regions */
#define LORA_SYNC_WORD_PUBLIC_WW2G4 0x21  /* Public Network for 2.4Ghz worldwide region */

/* values available for the 'network type' */
/* NOTE: arbitrary values */
#define LPWAN_NETWORK_TYPE_PUBLIC 0
#define LPWAN_NETWORK_TYPE_PRIVATE 1

/* Network type configuration (TODO: make it configurable) */
#define LPWAN_NETWORK_TYPE LPWAN_NETWORK_TYPE_PUBLIC

/* Spectral Scan */
#define LGW_SPECTRAL_SCAN_RESULT_SIZE 33 /* The number of results returned by spectral scan function, to be used for memory allocation */

/**
@struct lgw_pkt_rx_s
@brief Structure containing the metadata of a packet that was received and a pointer to the payload
*/
struct lgw_pkt_rx_s {
    uint32_t    freq_hz;        /*!> central frequency of the IF chain */
    int32_t     freq_offset;
    uint8_t     if_chain;       /*!> by which IF chain was packet received */
    uint8_t     status;         /*!> status of the received packet */
    uint32_t    count_us;       /*!> internal concentrator counter for timestamping, 1 microsecond resolution */
    uint8_t     rf_chain;       /*!> through which RF chain the packet was received */
    uint8_t     modem_id;
    uint8_t     modulation;     /*!> modulation used by the packet */
    uint8_t     bandwidth;      /*!> modulation bandwidth (LoRa only) */
    uint32_t    datarate;       /*!> RX datarate of the packet (SF for LoRa) */
    uint8_t     coderate;       /*!> error-correcting code of the packet (LoRa only) */
    float       rssic;          /*!> average RSSI of the channel in dB */
    float       rssis;          /*!> average RSSI of the signal in dB */
    float       snr;            /*!> average packet SNR, in dB (LoRa only) */
    float       snr_min;        /*!> minimum packet SNR, in dB (LoRa only) */
    float       snr_max;        /*!> maximum packet SNR, in dB (LoRa only) */
    uint16_t    crc;            /*!> CRC that was received in the payload */
    uint16_t    size;           /*!> payload size in bytes */
    uint8_t     payload[256];   /*!> buffer containing the payload */
    bool        ftime_received; /*!> a fine timestamp has been received */
    uint32_t    ftime;          /*!> packet fine timestamp (nanoseconds since last PPS) */
};

/**
@struct lgw_pkt_tx_s
@brief Structure containing the configuration of a packet to send and a pointer to the payload
*/
struct lgw_pkt_tx_s {
    uint32_t    freq_hz;        /*!> center frequency of TX */
    uint8_t     tx_mode;        /*!> select on what event/time the TX is triggered */
    uint32_t    count_us;       /*!> timestamp or delay in microseconds for TX trigger */
    uint8_t     rf_chain;       /*!> through which RF chain will the packet be sent */
    int8_t      rf_power;       /*!> TX power, in dBm */
    uint8_t     modulation;     /*!> modulation to use for the packet */
    int8_t      freq_offset;    /*!> frequency offset from Radio Tx frequency (CW mode) */
    uint8_t     bandwidth;      /*!> modulation bandwidth (LoRa only) */
    uint32_t    datarate;       /*!> TX datarate (baudrate for FSK, SF for LoRa) */
    uint8_t     coderate;       /*!> error-correcting code of the packet (LoRa only) */
    bool        invert_pol;     /*!> invert signal polarity, for orthogonal downlinks (LoRa only) */
    uint8_t     f_dev;          /*!> frequency deviation, in kHz (FSK only) */
    uint16_t    preamble;       /*!> set the preamble length, 0 for default */
    bool        no_crc;         /*!> if true, do not send a CRC in the packet */
    bool        no_header;      /*!> if true, enable implicit header mode (LoRa), fixed length (FSK) */
    uint16_t    size;           /*!> payload size in bytes */
    uint8_t     payload[256];   /*!> buffer containing the payload */
};

#endif

/* --- EOF ------------------------------------------------------------------ */
