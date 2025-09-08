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

/* -------------------------------------------------------------------------- */
/* --- DEPENDENCIES --------------------------------------------------------- */

#include <string.h>

#include "lorahub_aux.h"
#include "lorahub_log.h"
#include "lorahub_hal.h"
#include "lorahub_hal_rx.h"
#include "lorahub_hal_tx.h"

#include "radio_context.h"
#include "ral.h"

#include "ral_sx126x.h"
#include "ral_sx126x_bsp.h"
#include "smtc_shield_sx126x.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define CHECK_NULL( a )       \
    if( a == NULL )           \
    {                         \
        return LGW_HAL_ERROR; \
    }

#define TCXO_STARTUP_TIME_US( tick, freq ) ( ( tick ) * 1000000 / ( freq ) )

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define SPI_SPEED 2000000

#define SX126X_RTC_FREQ_IN_HZ 64000UL

/* -------------------------------------------------------------------------- */
/* --- PRIVATE TYPES -------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */

static bool    is_started = false;
static uint8_t rx_status  = RX_STATUS_UNKNOWN;
static uint8_t tx_status  = TX_STATUS_UNKNOWN;

static struct sx126x_conf_rxrf_s rxrf_conf = { .freq_hz = 0, .rssi_offset = 0.0, .tx_enable = false };

static struct sx126x_conf_rxif_s rxif_conf = { .bandwidth  = BW_UNDEFINED,
                                            .coderate   = CR_UNDEFINED,
                                            .datarate   = { DR_UNDEFINED, DR_UNDEFINED },
                                            .modulation = MOD_UNDEFINED };

static radio_context_t radio_context = { 0 };
#define RADIO_CONTEXT ( ( void* ) &radio_context )

const ral_t sx126x_ral = RAL_SX126X_INSTANTIATE( RADIO_CONTEXT );

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

int sx126x_connect( void )
{
    int ret;

    const smtc_shield_sx126x_t*        shield        = ral_sx126x_get_shield( );
    const smtc_shield_sx126x_pinout_t* shield_pinout = shield->get_pinout( );

    /* Initialize radio context */
    radio_context.spi_nss     = shield_pinout->nss;
    radio_context.spi_sclk    = shield_pinout->sclk;
    radio_context.spi_miso    = shield_pinout->miso;
    radio_context.spi_mosi    = shield_pinout->mosi;
    radio_context.gpio_rst    = shield_pinout->reset;
    radio_context.gpio_busy   = shield_pinout->busy;
    radio_context.gpio_dio1   = shield_pinout->irq;
    radio_context.gpio_led_tx = shield_pinout->led_tx;
    radio_context.gpio_led_rx = shield_pinout->led_rx;

    /* GPIO configuration for radio */
    gpio_reset_pin( radio_context.gpio_busy );
    gpio_set_direction( radio_context.gpio_busy, GPIO_MODE_INPUT );

    gpio_reset_pin( radio_context.spi_nss );
    gpio_set_direction( radio_context.spi_nss, GPIO_MODE_OUTPUT );
    gpio_set_level( radio_context.spi_nss, 1 );

    gpio_reset_pin( radio_context.gpio_rst );
    gpio_set_direction( radio_context.gpio_rst, GPIO_MODE_OUTPUT );

    gpio_reset_pin( radio_context.gpio_dio1 );
    gpio_set_direction( radio_context.gpio_dio1, GPIO_MODE_INPUT );
    gpio_set_intr_type( radio_context.gpio_dio1, GPIO_INTR_POSEDGE );

    /* GPIO configuration for antenna switch */
    if( shield_pinout->antenna_sw != 0xFF )
    {
        gpio_reset_pin( shield_pinout->antenna_sw );
        gpio_set_direction( shield_pinout->antenna_sw, GPIO_MODE_OUTPUT );
        gpio_set_level( shield_pinout->antenna_sw, 1 );
    }

    /* GPIO configuration for radio shields RX/TX LEDs */
    if( shield_pinout->led_rx != 0xFF )
    {
        gpio_reset_pin( shield_pinout->led_rx );
        gpio_set_direction( shield_pinout->led_rx, GPIO_MODE_OUTPUT );
        gpio_set_level( shield_pinout->led_rx, 0 );
    }

    if( shield_pinout->led_tx != 0xFF )
    {
        gpio_reset_pin( shield_pinout->led_tx );
        gpio_set_direction( shield_pinout->led_tx, GPIO_MODE_OUTPUT );
        gpio_set_level( shield_pinout->led_tx, 0 );
    }

    return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int sx126x_radio_setup( void )
{
    ASSERT_RAL_RC( ral_reset( &sx126x_ral ) );
    ASSERT_RAL_RC( ral_init( &sx126x_ral ) );

    ASSERT_RAL_RC( ral_set_rx_tx_fallback_mode( &sx126x_ral, RAL_FALLBACK_STDBY_RC ) );

    /* Install interrupt handler for RX IRQs */
    sx126x_radio_init_rx( &sx126x_ral );

    return LGW_HAL_SUCCESS;
}

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

int sx126x_rxrf_setconf( struct sx126x_conf_rxrf_s* conf )
{
    CHECK_NULL( conf );

    /* check if the concentrator is running */
    if( is_started == true )
    {
        printf( "ERROR: CONCENTRATOR IS RUNNING, STOP IT BEFORE CHANGING CONFIGURATION\n" );
        return LGW_HAL_ERROR;
    }

    memcpy( &rxrf_conf, conf, sizeof( struct sx126x_conf_rxrf_s ) );

    return LGW_HAL_SUCCESS;
};

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int sx126x_rxif_setconf( struct sx126x_conf_rxif_s* conf )
{
    CHECK_NULL( conf );

    /* check if the concentrator is running */
    if( is_started == true )
    {
        printf( "ERROR: CONCENTRATOR IS RUNNING, STOP IT BEFORE CHANGING CONFIGURATION\n" );
        return LGW_HAL_ERROR;
    }

    if( IS_LORA_DR( conf->datarate[0] ) == false )
    {
        printf( "ERROR: wrong datarate[0] - %s\n", __FUNCTION__ );
        return LGW_HAL_ERROR;
    }

    if( ( conf->datarate[1] != DR_UNDEFINED ) && ( IS_LORA_DR( conf->datarate[1] ) == false ) )
    {
        printf( "ERROR: wrong datarate[1] - %s\n", __FUNCTION__ );
        return LGW_HAL_ERROR;
    }

    if( IS_LORA_BW( conf->bandwidth ) == false )
    {
        printf( "ERROR: wrong bandwidth - %s\n", __FUNCTION__ );
        return LGW_HAL_ERROR;
    }

    if( IS_LORA_CR( conf->coderate ) == false )
    {
        printf( "ERROR: wrong coderate - %s\n", __FUNCTION__ );
        return LGW_HAL_ERROR;
    }

    memcpy( &rxif_conf, conf, sizeof( struct sx126x_conf_rxif_s ) );

    return LGW_HAL_SUCCESS;
};

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int sx126x_start( void )
{
    int err;

    if( is_started == true )
    {
        printf( "Note: LoRa concentrator already started, restarting it now\n" );
    }

    /* Check configuration */
    if( rxrf_conf.freq_hz == 0 )
    {
        printf( "ERROR: radio frequency not configured\n" );
        return LGW_HAL_ERROR;
    }
    if( rxif_conf.modulation == MOD_UNDEFINED )
    {
        printf( "ERROR: modulation type not configured\n" );
        return LGW_HAL_ERROR;
    }
    if( rxif_conf.bandwidth == BW_UNDEFINED )
    {
        printf( "ERROR: modulation bandwidth not configured\n" );
        return LGW_HAL_ERROR;
    }
    if( rxif_conf.coderate == CR_UNDEFINED )
    {
        printf( "ERROR: modulation coderate not configured\n" );
        return LGW_HAL_ERROR;
    }
    if( rxif_conf.datarate[0] == DR_UNDEFINED )
    {
        printf( "ERROR: modulation datarate not configured\n" );
        return LGW_HAL_ERROR;
    }

    /* Configure SPI and GPIOs */
    err = sx126x_connect( );
    if( err == LGW_HAL_ERROR )
    {
        printf( "ERROR: FAILED TO CONNECT BOARD\n" );
        return LGW_HAL_ERROR;
    }

    /* Configure radio */
    err = sx126x_radio_setup( );
    if( err == LGW_HAL_ERROR )
    {
        printf( "ERROR: FAILED TO SETUP RADIO\n" );
        return LGW_HAL_ERROR;
    }

    /* Update RX status */
    rx_status = RX_OFF;

    /* Set RX */
    err = sx126x_radio_configure_rx( &sx126x_ral, rxrf_conf.freq_hz, &rxif_conf );
    if( err == LGW_HAL_ERROR )
    {
        printf( "ERROR: FAILED TO CONFIGURE RADIO FOR RX" );
        return LGW_HAL_ERROR;
    }
    sx126x_radio_set_rx( &sx126x_ral );

    /* Update RX status */
    rx_status = RX_ON;

    /* Update TX status */
    if( rxrf_conf.tx_enable == false )
    {
        tx_status = TX_OFF;
    }
    else
    {
        tx_status = TX_FREE;
    }

    /* set hal state */
    is_started = true;

    return LGW_HAL_SUCCESS;
};

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int sx126x_stop( void )
{

    if( is_started == false )
    {
        printf( "Note: LoRa concentrator was not started...\n" );
        return LGW_HAL_SUCCESS;
    }

    /* set hal state */
    is_started = false;

    return LGW_HAL_SUCCESS;
};

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int sx126x_receive( uint8_t max_pkt, struct lgw_pkt_rx_s* pkt_data )
{
    struct lgw_pkt_rx_s* p = &pkt_data[0];
    uint32_t             count_us;
    int8_t               rssi, snr;
    uint8_t              status, sf;
    uint16_t             size;
    bool                 irq_received;
    int                  nb_packet_received = 0;

    /* max_pkt is ignored, only 1 packet can be received at a time */

    /* check if the concentrator is running */
    if( is_started == false )
    {
        printf( "ERROR: CONCENTRATOR IS NOT RUNNING, START IT BEFORE RECEIVING\n" );
        return LGW_HAL_ERROR;
    }

    memset( p, 0, sizeof( struct lgw_pkt_rx_s ) );
    nb_packet_received =
        sx126x_radio_get_pkt( &sx126x_ral, &irq_received, &count_us, &sf, &rssi, &snr, &status, &size, p->payload );
    if( nb_packet_received > 0 )
    {
        p->count_us   = count_us;
        p->freq_hz    = rxrf_conf.freq_hz;
        p->if_chain   = 0;
        p->rf_chain   = 0;
        p->status     = status;
        p->modulation = rxif_conf.modulation;
        p->datarate   = sf;
        p->bandwidth  = rxif_conf.bandwidth;
        p->coderate   = rxif_conf.coderate;
        p->rssic      = ( float ) rssi;
        p->snr        = ( float ) snr;
        p->size       = size;

        /* Compensate timestamp with for radio processing delay */
        uint32_t count_us_correction = sx126x_radio_timestamp_correction( p->datarate, p->bandwidth );
        p->count_us -= count_us_correction;
    }

    if( irq_received == true )
    {
        /* re-arm RX */
        sx126x_radio_set_rx( &sx126x_ral );
    }

    return nb_packet_received;
};

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int sx126x_send( struct lgw_pkt_tx_s* pkt_data )
{
    int err;

    /* check if the concentrator is running */
    if( is_started == false )
    {
        printf( "ERROR: CONCENTRATOR IS NOT RUNNING, START IT BEFORE SENDING\n" );
        return LGW_HAL_ERROR;
    }

    /* Update RX status */
    rx_status = RX_SUSPENDED;

    /* Configure for TX */
    err = sx126x_radio_configure_tx( &sx126x_ral, pkt_data );
    if( err == LGW_HAL_ERROR )
    {
        printf( "ERROR: FAILED TO CONFIGURE RADIO FOR TX" );
        /* Back to RX */
        sx126x_radio_configure_rx( &sx126x_ral, rxrf_conf.freq_hz, &rxif_conf );
        sx126x_radio_set_rx( &sx126x_ral );
        return LGW_HAL_ERROR;
    }

    /* Update TX status */
    tx_status = TX_SCHEDULED;

    /* Get TCXO startup time, if any */
    uint32_t tcxo_startup_time_in_tick = 0;
    uint32_t rtc_freq_in_hz            = 0;

    ral_sx126x_bsp_get_xosc_cfg( NULL, NULL, NULL, &tcxo_startup_time_in_tick );
    rtc_freq_in_hz = SX126X_RTC_FREQ_IN_HZ;

    uint32_t tcxo_startup_time_us = TCXO_STARTUP_TIME_US( tcxo_startup_time_in_tick, rtc_freq_in_hz );

    /* Wait for time to send packet */
    uint32_t count_us_now;
    do
    {
        sx126x_get_instcnt( &count_us_now );
        wait_ms( 100 );
    } while( ( int32_t ) ( pkt_data->count_us - count_us_now ) > ( int32_t ) tcxo_startup_time_us );

    /* Send packet */
    ASSERT_RAL_RC( ral_set_tx( &sx126x_ral ) );

    /* Update TX status */
    tx_status = TX_EMITTING;

    /* Wait for TX_DONE */
    bool      flag_tx_done    = false;
    bool      flag_tx_timeout = false;
    ral_irq_t irq_regs;
    do
    {
        ASSERT_RAL_RC( ral_get_and_clear_irq_status( &sx126x_ral, &irq_regs ) );
        if( ( irq_regs & RAL_IRQ_TX_DONE ) == RAL_IRQ_TX_DONE )
        {
            sx126x_get_instcnt( &count_us_now );
            printf( "%u: IRQ_TX_DONE", count_us_now );
            flag_tx_done = true;
        }
        if( ( irq_regs & RAL_IRQ_RX_TIMEOUT ) == RAL_IRQ_RX_TIMEOUT )
        {  // TODO: check if IRQ also valid for TX
            sx126x_get_instcnt( &count_us_now );
            printf( "%u: TX:IRQ_TIMEOUT", count_us_now );
            flag_tx_timeout = true;
        }

        /* Yield for 10ms (avoid TWDT watchdog timeout) for long TX */
        wait_ms( 10 );
    } while( ( flag_tx_done == false ) && ( flag_tx_timeout == false ) );

    printf( "TCXO startup time: %u", tcxo_startup_time_us );

    /* Update TX status */
    tx_status = TX_FREE;

    /* Back to RX config */
    sx126x_radio_configure_rx( &sx126x_ral, rxrf_conf.freq_hz, &rxif_conf );
    sx126x_radio_set_rx( &sx126x_ral );

    /* Update RX status */
    rx_status = RX_ON;

    return ( flag_tx_timeout == false ) ? LGW_HAL_SUCCESS : LGW_HAL_ERROR;
};

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int sx126x_status( uint8_t rf_chain, uint8_t select, uint8_t* code )
{
    /* check input variables */
    CHECK_NULL( code );
    if( rf_chain >= SX126X_RF_CHAIN_NB )
    {
        printf( "ERROR: NOT A VALID RF_CHAIN NUMBER\n" );
        return LGW_HAL_ERROR;
    }

    /* Get status */
    if( select == TX_STATUS )
    {
        if( is_started == false )
        {
            *code = TX_OFF;
        }
        else
        {
            *code = tx_status;
        }
    }
    else if( select == RX_STATUS )
    {
        if( is_started == false )
        {
            *code = RX_OFF;
        }
        else
        {
            *code = rx_status;
        }
    }
    else
    {
        printf( "ERROR: SELECTION INVALID, NO STATUS TO RETURN\n" );
        return LGW_HAL_ERROR;
    }

    return LGW_HAL_SUCCESS;
};

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* TODO */
int sx126x_get_instcnt( uint32_t* inst_cnt_us )
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    *inst_cnt_us = (uint32_t)tv.tv_sec * 1000000UL + tv.tv_usec;

    return LGW_HAL_SUCCESS;
};

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

uint32_t sx126x_time_on_air( const struct lgw_pkt_tx_s* packet )
{
    uint32_t toa_ms = 0;

    if( packet == NULL )
    {
        printf( "ERROR: Failed to compute time on air, wrong parameter\n" );
        return 0;
    }

    if( packet->modulation == MOD_LORA )
    {
        if( sx126x_check_lora_mod_params( packet->freq_hz, packet->bandwidth, packet->coderate ) != LGW_HAL_SUCCESS )
        {
            printf( "ERROR: Failed to compute time on air, wrong modulation parameters\n" );
            return 0;
        }

        ral_lora_pkt_params_t ral_pkt_params;
        ral_pkt_params.preamble_len_in_symb = packet->preamble;
        ral_pkt_params.header_type = ( packet->no_header == false ) ? RAL_LORA_PKT_EXPLICIT : RAL_LORA_PKT_IMPLICIT;
        ral_pkt_params.pld_len_in_bytes = packet->size;
        ral_pkt_params.crc_is_on        = ( packet->no_crc == false ) ? true : false;
        ral_pkt_params.invert_iq_is_on  = packet->invert_pol;

        ral_lora_sf_t         ral_sf = sx126x_convert_hal_to_ral_sf( packet->datarate );
        ral_lora_bw_t         ral_bw = sx126x_convert_hal_to_ral_bw( packet->bandwidth );
        ral_lora_cr_t         ral_cr = sx126x_convert_hal_to_ral_cr( packet->coderate );
        ral_lora_mod_params_t ral_mod_params;
        ral_mod_params.sf   = ral_sf;
        ral_mod_params.bw   = ral_bw;
        ral_mod_params.cr   = ral_cr;
        ral_mod_params.ldro = ral_compute_lora_ldro( ral_sf, ral_bw );
        toa_ms              = ral_get_lora_time_on_air_in_ms( &sx126x_ral, &ral_pkt_params, &ral_mod_params );
    }
    else
    {
        toa_ms = 0;
        printf( "ERROR: Cannot compute time on air for this packet, unsupported modulation (0x%02X)\n",
                  packet->modulation );
    }

    return toa_ms;
};

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int sx126x_get_min_max_freq_hz( uint32_t* min_freq_hz, uint32_t* max_freq_hz )
{
    if( rxrf_conf.freq_hz == 0 )
    {
        printf( "ERROR: NOT CONFIGURED (RX FREQ)\n" );
        return LGW_HAL_ERROR;
    }

    const smtc_shield_sx126x_t*              shield              = ral_sx126x_get_shield( );
    const smtc_shield_sx126x_capabilities_t* shield_capabilities = shield->get_capabilities( );
    *min_freq_hz                                                 = shield_capabilities->freq_hz_min;
    *max_freq_hz                                                 = shield_capabilities->freq_hz_max;

    return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int sx126x_get_min_max_power_dbm( int8_t* min_power_dbm, int8_t* max_power_dbm )
{
    if( rxrf_conf.freq_hz == 0 )
    {
        printf( "ERROR: NOT CONFIGURED (RX FREQ)\n" );
        return LGW_HAL_ERROR;
    }

    const smtc_shield_sx126x_t*              shield              = ral_sx126x_get_shield( );
    const smtc_shield_sx126x_capabilities_t* shield_capabilities = shield->get_capabilities( );
    *min_power_dbm                                               = shield_capabilities->power_dbm_min;
    *max_power_dbm                                               = shield_capabilities->power_dbm_max;

    return LGW_HAL_SUCCESS;
}

/* --- EOF ------------------------------------------------------------------ */
