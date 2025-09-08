/**
 * @file      sx126x_hal.c
 *
 * @brief     Implements the SX126x radio HAL functions
 *
 * The Clear BSD License
 * Copyright Semtech Corporation 2022. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Semtech corporation nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 * NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */

#include <stddef.h>
#include <string.h>
#include <stdio.h>     /* C99 types */
#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */

#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/time.h>   /* gettimeofday, structtimeval */
#include <time.h>       /* clock_nanosleep */

#include "sx126x_hal.h"
#include "radio_context.h"

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS ----------------------------------------------------------
 */

#define SPI_TRANS_USE_TXDATA (1<<0)
#define SPI_TRANS_USE_RXDATA (1<<1)

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

typedef struct {
    uint32_t flags;         // 传输标志
    uint16_t cmd;           // 命令字段
    uint32_t addr;          // 地址字段
    size_t length;          // 数据长度
    void *tx_buffer;        // 发送缓冲区
    void *rx_buffer;        // 接收缓冲区
    uint32_t dummy_bits;    // 空位数量
} spi_transaction_t;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

int spi_device_transmit(const char *spidev_path, spi_transaction_t *trans);

static void sleep_ms(unsigned long delay_ms);

/**
 * @brief Wait until radio busy pin returns to 0
 */
void sx126x_hal_wait_on_busy( const void* context );

/**
 * @brief SPI tranfer
 */
static uint8_t spi_transfer( const void* context, uint8_t address );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS PROTOTYPES ---------------------------------------------
 */

sx126x_hal_status_t sx126x_hal_reset( const void* context )
{
    const radio_context_t* sx126x_context = ( const radio_context_t* ) context;

    gpio_set_level( sx126x_context->gpio_rst, 0 );
    sleep_ms( 5 );
    gpio_set_level( sx126x_context->gpio_rst, 1 );
    sleep_ms( 5 );

    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_wakeup( const void* context )
{
    const radio_context_t* sx126x_context = ( const radio_context_t* ) context;

    gpio_set_level( sx126x_context->spi_nss, 0 );
    sleep_ms( 1 );
    gpio_set_level( sx126x_context->spi_nss, 1 );

    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_write( const void* context, const uint8_t* command, const uint16_t command_length,
                                      const uint8_t* data, const uint16_t data_length )

{
    const radio_context_t* sx126x_context = ( const radio_context_t* ) context;

    int i;

    sx126x_hal_wait_on_busy( context );

    gpio_set_level( sx126x_context->spi_nss, 0 );

    /* Write command */
    for( i = 0; i < command_length; i++ )
    {
        spi_transfer( context, command[i] );
    }
    /* Write data */
    for( i = 0; i < data_length; i++ )
    {
        spi_transfer( context, data[i] );
    }

    gpio_set_level( sx126x_context->spi_nss, 1 );

    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_read( const void* context, const uint8_t* command, const uint16_t command_length,
                                     uint8_t* data, const uint16_t data_length )
{
    const radio_context_t* sx126x_context = ( const radio_context_t* ) context;

    int i;

    sx126x_hal_wait_on_busy( context );

    gpio_set_level( sx126x_context->spi_nss, 0 );

    /* Write command */
    for( i = 0; i < command_length; i++ )
    {
        spi_transfer( context, command[i] );
    }
    /* Read data */
    for( i = 0; i < data_length; i++ )
    {
        data[i] = spi_transfer( context, 0x00 );
    }

    gpio_set_level( sx126x_context->spi_nss, 1 );

    return SX126X_HAL_STATUS_OK;
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

void sx126x_hal_wait_on_busy( const void* context )
{
    const radio_context_t* sx126x_context = ( const radio_context_t* ) context;
    int                    gpio_state;
    do
    {
        gpio_get_level( sx126x_context->gpio_busy, &gpio_state );
        sleep_ms( 1 );
    } while( gpio_state == 1 );
}

bool spi_rw_byte( const void* context, uint8_t* data_in, uint8_t* data_out, size_t length )
{
    const radio_context_t*   sx126x_context = ( const radio_context_t* ) context;
    static spi_transaction_t spi_transaction;

    if( length > 0 )
    {
        memset( &spi_transaction, 0, sizeof( spi_transaction_t ) );
		spi_transaction.flags     = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
        spi_transaction.length    = length; 
        spi_transaction.tx_buffer = data_out;
        spi_transaction.rx_buffer = data_in;
        spi_device_transmit( sx126x_context->spi_handle, &spi_transaction );
    }

    return true;
}

static uint8_t spi_transfer( const void* context, uint8_t address )
{
    uint8_t data_in;
    uint8_t data_out = address;
    spi_rw_byte( context, &data_in, &data_out, 1 );
    return data_in;
}

int spi_device_transmit(const char *spidev_path, spi_transaction_t *trans) {
    int fd;
    struct spi_ioc_transfer xfer[1] = {0};
    uint8_t tx[trans->length + 4]; // 假设最大需要 cmd + addr + data
    uint8_t rx[trans->length + 4];
    
    // 打开 SPI 设备
    fd = open(spidev_path, O_RDWR);
    if (fd < 0) {
        perror("Can't open SPI device");
        return -1;
    }
    
    // 准备传输数据
    if (trans->flags & SPI_TRANS_USE_TXDATA) {
        // 如果有发送数据
        if (trans->cmd) {
            // 如果有命令字段
            tx[0] = (uint8_t)(trans->cmd >> 8);
            tx[1] = (uint8_t)(trans->cmd & 0xFF);
            memcpy(&tx[2], trans->tx_buffer, trans->length);
        } else {
            memcpy(tx, trans->tx_buffer, trans->length);
        }
    }
    
    // 设置 SPI 传输结构
    xfer[0].tx_buf = (unsigned long)tx;
    xfer[0].rx_buf = (unsigned long)rx;
    xfer[0].len = trans->length + (trans->cmd ? 2 : 0);
    xfer[0].speed_hz = 1000000; // 1MHz
    xfer[0].bits_per_word = 8;
    xfer[0].delay_usecs = 10;
    
    // 执行 SPI 传输
    if (ioctl(fd, SPI_IOC_MESSAGE(1), xfer) < 0) {
        perror("SPI transfer failed");
        close(fd);
        return -1;
    }
    
    // 如果有接收数据
	if (trans->flags & SPI_TRANS_USE_RXDATA) {
    	memcpy(trans->rx_buffer, rx + (trans->cmd ? 2 : 0), trans->length);
	}
    
    close(fd);
    return 0;
}

static void sleep_ms(unsigned long delay_ms) {
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

/* --- EOF ------------------------------------------------------------------ */
