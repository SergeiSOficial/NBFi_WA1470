#include "nbfi.h"
#include "nbfi_config.h"
#include "nbfi_misc.h"
#include "nbfi_phy.h"
#include "rf.h"
#include "xtea.h"
#include "zcode.h"
#include <string.h>
#include <stdlib.h> 

#define memset_xdata memset
#define memcpy_xdata memcpy
#define memcpy_xdatageneric memcpy
#define memcpy_genericxdata memcpy


uint32_t last_pkt_crc = 0;

const uint8_t protD_preambula[] = {0x97, 0x15, 0x7A, 0x6F};

/*
struct axradio_address  fastdladdress = {
	{ 0x6f, 0x6f, 0x6f, 0x6f}
};*/


uint32_t tx_freq, rx_freq;

extern nbfi_state_t nbfi_state;
extern nbfi_transport_packet_t* nbfi_active_pkt;
extern _Bool wait_RxEnd;

nbfi_status_t NBFi_TX_ProtocolD(nbfi_transport_packet_t* pkt)
{
    uint8_t ul_buf[64];
    uint8_t len = 0;
    static _Bool parity = 0;
    uint8_t lastcrc8;
    _Bool downlink;

    memset_xdata(ul_buf,0,sizeof(ul_buf));

    if(nbfi.mode == TRANSPARENT) pkt->phy_data_length--;

    for(int i=0; i<sizeof(protD_preambula); i++)
    {
        ul_buf[len++] = protD_preambula[i];
    }

    switch(nbfi.tx_phy_channel)
    {
        case DL_DBPSK_50_PROT_D:
        case DL_DBPSK_400_PROT_D:
        case DL_DBPSK_3200_PROT_D:
        case DL_DBPSK_25600_PROT_D:
            ul_buf[len++] = nbfi.dl_ID[0];
            ul_buf[len++] = nbfi.dl_ID[1];
            ul_buf[len++] = nbfi.dl_ID[2];
            downlink = 1;
            break;
        default:
            ul_buf[len++] = nbfi.temp_ID[0];
            ul_buf[len++] = nbfi.temp_ID[1];
            ul_buf[len++] = nbfi.temp_ID[2];
            downlink = 0;
            break;

    }

    if(nbfi.tx_phy_channel == DL_DBPSK_50_PROT_D) nbfi.tx_phy_channel = UL_DBPSK_50_PROT_D;
    else if(nbfi.tx_phy_channel == DL_DBPSK_400_PROT_D) nbfi.tx_phy_channel = UL_DBPSK_400_PROT_D;
    else if(nbfi.tx_phy_channel == DL_DBPSK_3200_PROT_D) nbfi.tx_phy_channel = UL_DBPSK_3200_PROT_D;
    else if(nbfi.tx_phy_channel == DL_DBPSK_25600_PROT_D) nbfi.tx_phy_channel = UL_DBPSK_25600_PROT_D;

    ul_buf[len++] = pkt->phy_data.header;

    memcpy_xdatageneric(&ul_buf[len], pkt->phy_data.payload, pkt->phy_data_length);

    lastcrc8 =  CRC8(&ul_buf[len], 8);

    if(XTEA_Enabled() && XTEA_Available() && !(nbfi.additional_flags&NBFI_FLG_NO_XTEA))
    {
        XTEA_Encode(&ul_buf[len]);
    }
    len += 8;

    if(nbfi.mode == TRANSPARENT)
    {
        ul_buf[len++] = pkt->phy_data.payload[8];
    }
    else  ul_buf[len++] = lastcrc8;

    last_pkt_crc = CRC32(ul_buf + 4, 13); 

    ul_buf[len++] = (uint8_t)(last_pkt_crc >> 16);
    ul_buf[len++] = (uint8_t)(last_pkt_crc >> 8);
    ul_buf[len++] = (uint8_t)(last_pkt_crc);

    if(nbfi.tx_freq)
    {
        tx_freq = nbfi.tx_freq ;
        parity = (nbfi.tx_freq > (nbfi.ul_freq_base + 25000));
    }
    else
    {
        if(nbfi.tx_phy_channel < UL_DBPSK_3200_PROT_D)
        {
                tx_freq = nbfi.ul_freq_base + (((*((const uint32_t*)FULL_ID)+lastcrc8)%226)*100);
                if(parity) tx_freq = tx_freq + 27500;
        }
        else
        {
            tx_freq = nbfi.ul_freq_base + 1600 + (((*((const uint32_t*)FULL_ID)+lastcrc8)%210)*100);
            if(parity) tx_freq = tx_freq + 27500 - 1600;
        }
    }

    if((nbfi.tx_phy_channel < UL_DBPSK_3200_PROT_D) && !downlink)
    {
      ZCODE_Append(&ul_buf[4], &ul_buf[len], parity);
    }
    else
    {          
      ZCODE_Append(&ul_buf[4], &ul_buf[len], 1);
    }

    if(!nbfi.tx_freq) parity = !parity;

    if((nbfi.mode == NRX) && parity) // For NRX send in ALOHA mode
    {
      
      RF_Init(nbfi.tx_phy_channel, (rf_antenna_t)nbfi.tx_antenna, nbfi.tx_pwr, tx_freq);
      
      RF_Transmit(ul_buf, len + ZCODE_LEN, nbfi.tx_phy_channel, BLOCKING);
      
      nbfi_state.UL_total++;
      
      return NBFi_TX_ProtocolD(pkt);
    }
    
    RF_Init(nbfi.tx_phy_channel, (rf_antenna_t)nbfi.tx_antenna, nbfi.tx_pwr, tx_freq);

    RF_Transmit(ul_buf, len + ZCODE_LEN, nbfi.tx_phy_channel, NONBLOCKING);

    nbfi_state.UL_total++;

    return OK;

}

_Bool NBFi_Match_ID(uint8_t * addr)
{
    uint8_t i;
    for( i = 0; i !=3; i++) if(nbfi.temp_ID[i] != addr[i]) break;
    if(i == 3)  return 1;

    for(i = 0; i !=3; i++) if(nbfi.broadcast_ID[i] != addr[i]) break;
    if(i == 3)  return 1;

    return 0;
}

nbfi_status_t NBFi_TX(nbfi_transport_packet_t* pkt)
{
    
    //if((pkt->phy_data_length==0)&&(pkt->phy_data_length>240)) return ERR; // len check
    switch(nbfi.tx_phy_channel)
    {
    case UL_DBPSK_50_PROT_D:
    case UL_DBPSK_400_PROT_D:
    case UL_DBPSK_3200_PROT_D:
    case UL_DBPSK_25600_PROT_D:
    case DL_DBPSK_50_PROT_D:
    case DL_DBPSK_400_PROT_D:
    case DL_DBPSK_3200_PROT_D:
    case DL_DBPSK_25600_PROT_D:
        return NBFi_TX_ProtocolD(pkt);
    case UL_PSK_FASTDL:
    case UL_PSK_200:
    case UL_PSK_500:
    case UL_PSK_5000:
        break;

    }

    return OK;
}


nbfi_status_t NBFi_RX_Controller()
{
    switch(nbfi.mode)
    {
    case  DRX:
    case  NRX:
        if(wait_RxEnd ) if(rf_state != STATE_RX)return NBFi_RX();
        else break;
        switch(nbfi_active_pkt->state)
        {
        case PACKET_WAIT_ACK:
        case PACKET_QUEUED_AGAIN:
        case PACKET_WAIT_FOR_EXTRA_PACKETS:
            if(rf_state != STATE_RX) return NBFi_RX();
            break;
        default:
            if(rf_state != STATE_OFF)  return RF_Deinit();
        }
        break;
    case CRX:
    case TRANSPARENT:
        if(rf_state != STATE_RX) return NBFi_RX();
        break;
    case OFF:
        if(rf_state != STATE_OFF)  return RF_Deinit();
        break;
    }
    return OK;
}

nbfi_status_t NBFi_RX()
{
    nbfi_status_t result;
    if(nbfi.rx_freq == 0) rx_freq = nbfi.dl_freq_base + ((*((const uint32_t*)FULL_ID)%276)*363);
    else rx_freq = nbfi.rx_freq;

    result = RF_Init(nbfi.rx_phy_channel, (rf_antenna_t)nbfi.rx_antenna, 0, rx_freq);
    return result;
}


void NBFi_XTEA_OFB(uint8_t* buf, uint8_t len, uint8_t iter)
{
 uint8_t vector[8];
 for(uint8_t i = 0; i != 3; i++)
 {
    vector[i] = 0;
    vector[i+5] = nbfi.temp_ID[i];
 }
 vector[3] = 0;
 vector[4] = iter;

 uint8_t n = 0;// number of cyphered bytes

 while(n < len)
 {

  if((n % 8) == 0) XTEA_Encode(vector); // next block

  buf[n] = vector[n%8] ^ buf[n];
  n++;
 }
}
