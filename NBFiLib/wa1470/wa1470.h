#ifndef _wa1470_H
#define _wa1470_H
#include <stm32l0xx_hal.h>
#include "wa1470rfe.h"
#include "wa1470dem.h"
#include "wa1470mod.h"

//#define TX_BPSK_PIN_MODE

enum wa1470_func_name_t
{
	WARADIO_ENABLE_GLOBAL_IRQ = 0,
	WARADIO_DISABLE_GLOBAL_IRQ,
	WARADIO_ENABLE_IRQ_PIN,
	WARADIO_DISABLE_IRQ_PIN,
        WARADIO_CHIP_ENABLE,
	WARADIO_CHIP_DISABLE,
	WARADIO_GET_IRQ_PIN,
	WARADIO_SPI_RX,
	WARADIO_SPI_TX,
	WARADIO_SPI_TX_RX,
	WARADIO_SPI_CS_WRITE,
        WARADIO_DATA_RECEIVED,
        WARADIO_TX_FINISHED,
        WARADIO_NOP_DELAY_MS
};

void wa1470_reg_func(uint8_t name, void * fn);
void wa1470_init();
void wa1470_spi_write(uint16_t address, uint8_t *data, uint8_t length);
void wa1470_spi_read(uint16_t address, uint8_t *data, uint8_t length);
void wa1470_spi_write8(uint16_t address, uint8_t data);
uint8_t wa1470_spi_read8(uint16_t address);
_Bool wa1470_spi_wait_for(uint16_t address, uint8_t value, uint8_t mask);

void wa1470_isr();
_Bool wa1470_cansleep();
//void wa1470_tcxo_set_reset(uint8_t set);
void wa1470_test();
#endif