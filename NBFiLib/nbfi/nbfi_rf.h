#ifndef NBFI_RF_H
#define NBFI_RF_H

#include "nbfi.h"

#define PA_DIFFERENTIAL (0x1)
#define PA_SINGLE_ENDED (0x2)
#define PA_SHAPING      (0x4)

typedef enum
{
    STATE_OFF,
    STATE_RX,
    STATE_TX,
    STATE_CHANGED
}nbfi_rf_state_s;

/*
typedef enum
{
    NO_PADDING = 0,
    PADDING_4TO1
}rf_padding_t;*/

typedef enum
{
    NONBLOCKING,
    BLOCKING,
}rf_blocking_t;

/*
typedef struct
{
  uint8_t sysclk;
  uint8_t dclk;
  uint8_t data;
  uint8_t antsel;
  uint8_t cfga;
  uint16_t txpwr;
} NBFi_wa1470_pins_s;
*/

extern  struct axradio_address fastdladdress;
//extern _Bool send_auto_corr;
//extern uint8_t PSK_BAND;
extern nbfi_rf_state_s rf_state;
extern nbfi_phy_channel_t nbfi_phy_channel;
extern _Bool rf_busy;
extern _Bool transmit;

//void axradio_statuschange(struct axradio_status  *st);

//void            RF_SetModeAndPower(int8_t dBm, rf_direction_t mode, rf_antenna_t ant);
//void            RF_SetDstAddress(uint8_t * addr);
//void            RF_SetLocalAddress(uint8_t * addr);
nbfi_status_t   NBFi_RF_Init(nbfi_phy_channel_t  phy_channel,
                        rf_antenna_t        antenna,
                        int8_t              power,
                        uint32_t            freq);
nbfi_status_t   NBFi_RF_Deinit();
nbfi_status_t   NBFi_RF_Transmit(uint8_t* pkt, uint8_t len, nbfi_phy_channel_t  phy_channel,  rf_blocking_t blocking);
void            NBFi_RF_TX_Finished();

#endif // NBFI_RF_H