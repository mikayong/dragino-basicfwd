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

/* -------------------------------------------------------------------------- */
/* --- DEPENDENCIES --------------------------------------------------------- */

#include <string.h>

#include "lorahub_log.h"
#include "lorahub_aux.h"
#include "lorahub_hal.h"
#include "lorahub_hal_rx.h"

#include "ral.h"
#include "radio_context.h"


/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS -------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ----------------------------------------------------- */


#define RX_TIMEOUT_MS 120000 /* 2 minutes */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE TYPES --------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */

static volatile bool irq_fired    = false;
static uint32_t      irq_count_us = 0;

static bool flag_rx_done      = false;
static bool flag_rx_crc_error = false;
static bool flag_rx_timeout   = false;

static uint8_t main_detector_sf = DR_UNDEFINED;
static uint8_t side_detector_sf = DR_UNDEFINED;

static uint32_t configured_freq_hz = 0;
static uint8_t  configured_bw      = BW_UNDEFINED;

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

static void IRAM_ATTR radio_on_dio_irq( void* args )
{
    irq_fired = true;
    sx126x_get_instcnt( &irq_count_us );
}

void radio_irq_process( const ral_t* ral )
{
    if( irq_fired == true )
    {
        irq_fired = false;

        ral_irq_t irq_regs;
        ral_get_and_clear_irq_status( ral, &irq_regs );
        if( ( irq_regs & RAL_IRQ_RX_DONE ) == RAL_IRQ_RX_DONE )
        {
            // printf("%lu: IRQ_RX_DONE\n", irq_count_us);
            flag_rx_done = true;
        }

        if( ( irq_regs & RAL_IRQ_RX_CRC_ERROR ) == RAL_IRQ_RX_CRC_ERROR )
        {
            flag_rx_crc_error = true;
        }

        if( ( irq_regs & RAL_IRQ_RX_TIMEOUT ) == RAL_IRQ_RX_TIMEOUT )
        {
            printf( "%lu: RX:IRQ_TIMEOUT", irq_count_us );
            flag_rx_timeout = true;
        }
    }
}

static void set_led_rx( const ral_t* ral, bool on )
{
    const radio_context_t* radio_context = ( const radio_context_t* ) ( ral->context );

    if( radio_context->gpio_led_rx != 0xFF )
    {
        gpio_set_level( radio_context->gpio_led_rx, ( on == true ) ? 1 : 0 );
    }
}

static void set_led_tx( const ral_t* ral, bool on )
{
    const radio_context_t* radio_context = ( const radio_context_t* ) ( ral->context );

    if( radio_context->gpio_led_tx != 0xFF )
    {
        gpio_set_level( radio_context->gpio_led_tx, ( on == true ) ? 1 : 0 );
    }
}

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

int sx126x_radio_init_rx( const ral_t* ral )
{
    const radio_context_t* radio_context = ( const radio_context_t* ) ( ral->context );

    gpio_install_isr_service( 0 );
    gpio_isr_handler_add( radio_context->gpio_dio1, radio_on_dio_irq, NULL );

    return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int sx126x_radio_configure_rx( const ral_t* ral, uint32_t freq_hz, const struct sx126x_conf_rxif_s* modulation_params )
{
    set_led_rx( ral, false );
    set_led_tx( ral, false );

    /* Check parameters */
    if( sx126x_check_lora_mod_params( freq_hz, modulation_params->bandwidth, modulation_params->coderate ) !=
        LGW_HAL_SUCCESS )
    {
        return LGW_HAL_ERROR;
    }

    ASSERT_RAL_RC( ral_set_standby( ral, RAL_STANDBY_CFG_RC ) );

    ASSERT_RAL_RC( ral_set_pkt_type( ral, RAL_PKT_TYPE_LORA ) );

    /* Configure main LoRa detector */
    main_detector_sf = modulation_params->datarate[0];

    /* Keep bandiwdth configuration for later use */
    configured_freq_hz = freq_hz;
    configured_bw      = modulation_params->bandwidth;

    /* Configure Dual-SF if enabled and supported by the radio */

    int err = sx126x_check_lora_dualsf_conf( modulation_params->bandwidth, main_detector_sf, side_detector_sf );
    if( err != LGW_HAL_SUCCESS )
    {
        main_detector_sf = modulation_params->datarate[0];
        side_detector_sf = DR_UNDEFINED;
        printf( "invalid dual-SF configuration, use single-SF with SF%u", main_detector_sf );
    }

    /* Configure main LoRa detector/demodulator */
    ral_lora_sf_t         ral_sf          = sx126x_convert_hal_to_ral_sf( main_detector_sf );
    ral_lora_bw_t         ral_bw          = sx126x_convert_hal_to_ral_bw( modulation_params->bandwidth );
    ral_lora_cr_t         ral_cr          = sx126x_convert_hal_to_ral_cr( modulation_params->coderate );
    ral_lora_mod_params_t lora_mod_params = {
        .sf = ral_sf, .bw = ral_bw, .cr = ral_cr, .ldro = ral_compute_lora_ldro( ral_sf, ral_bw )
    };
    ASSERT_RAL_RC( ral_set_lora_mod_params( ral, &lora_mod_params ) );

    const ral_lora_pkt_params_t lora_pkt_params = {
        .preamble_len_in_symb = ( main_detector_sf < DR_LORA_SF7 ) ? HDR_LORA_PREAMBLE : STD_LORA_PREAMBLE,
        .header_type          = RAL_LORA_PKT_EXPLICIT,
        .pld_len_in_bytes     = 0,
        .crc_is_on            = true,
        .invert_iq_is_on      = false,
    };
    ASSERT_RAL_RC( ral_set_lora_pkt_params( ral, &lora_pkt_params ) );

    printf( "Main detector configured for SF%u", main_detector_sf );

    /* Prepare for RX */
    ASSERT_RAL_RC( ral_set_lora_sync_word( ral, sx126x_get_lora_sync_word( freq_hz, main_detector_sf ) ) );
    ASSERT_RAL_RC( ral_set_rf_freq( ral, freq_hz ) );
    uint32_t freq_mhz_low  = freq_hz / 1E6; /* floor */
    uint32_t freq_mhz_high = freq_mhz_low + 1;
    ASSERT_RAL_RC( ral_cal_img( ral, ( uint16_t ) freq_mhz_low, ( uint16_t ) freq_mhz_high ) );
    ASSERT_RAL_RC( ral_set_lora_symb_nb_timeout( ral, 0 ) );

    return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int sx126x_radio_set_rx( const ral_t* ral )
{
    const ral_irq_t rx_irq_mask = RAL_IRQ_RX_DONE | RAL_IRQ_RX_CRC_ERROR | RAL_IRQ_RX_TIMEOUT;
    ASSERT_RAL_RC( ral_set_dio_irq_params( ral, rx_irq_mask ) );
    ASSERT_RAL_RC( ral_clear_irq_status( ral, RAL_IRQ_ALL ) );

    ASSERT_RAL_RC( ral_set_rx( ral, RX_TIMEOUT_MS ) );

    return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int sx126x_radio_get_pkt( const ral_t* ral, bool* irq_received, uint32_t* count_us, uint8_t* sf, int8_t* rssi, int8_t* snr,
                       uint8_t* status, uint16_t* size, uint8_t* payload )
{
    int     nb_pkt_received = 0;
    int16_t rssi_offset     = 0;

    /* Initialize return values */
    *count_us     = 0;
    *rssi         = 0;
    *snr          = 0;
    *status       = STAT_UNDEFINED;
    *size         = 0;
    *irq_received = false;
    *sf           = main_detector_sf;

    /* Check if a packet has been received */
    radio_irq_process( ral );
    if( ( flag_rx_done == true ) || ( flag_rx_crc_error == true ) )
    {
        set_led_rx( ral, true );

        *irq_received = true;
        *count_us     = irq_count_us;

        ral_lora_rx_pkt_status_t pkt_status_lora;
        ASSERT_RAL_RC( ral_get_lora_rx_pkt_status( ral, &pkt_status_lora ) );
        *rssi = pkt_status_lora.rssi_pkt_in_dbm + rssi_offset;
        *snr  = pkt_status_lora.snr_pkt_in_db;

        if( flag_rx_crc_error == true )
        {
            *status = STAT_CRC_BAD;
        }
        else
        {
            *status = STAT_CRC_OK;

            /* Get packet payload */
            ASSERT_RAL_RC( ral_get_pkt_payload( ral, 256, payload, size ) );
            printf( "%d byte packet received:", *size );
        }

        /* Update packet datarate based on LoRa detector trigger (main/side) */
        if( side_detector_sf != DR_UNDEFINED )
        {
            printf( "Dual-SF not supported for current radio" );
        }

        /* New packet received */
        nb_pkt_received += 1;

        /* Update status */
        flag_rx_done      = false;
        flag_rx_crc_error = false;

        set_led_rx( ral, false );
    }
    else if( flag_rx_timeout == true )
    {
        *irq_received = true;

        /* Update status */
        flag_rx_timeout = false;
    }

    return nb_pkt_received;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

uint32_t sx126x_radio_timestamp_correction( uint8_t sf, uint8_t bw )
{
    uint32_t t_symbol_us = 0;

    uint32_t bw_in_hz = sx126x_get_lora_bw_in_hz( bw );
    if( bw_in_hz != 0 )
    {
        t_symbol_us = ( 1 << ( uint8_t ) sf ) * 1000000 / bw_in_hz;
    }

    return t_symbol_us;
}

/* --- EOF ------------------------------------------------------------------ */
