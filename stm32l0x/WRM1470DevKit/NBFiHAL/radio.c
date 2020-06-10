#include <stm32l0xx_hal.h>
#include "defines.h"
#include "radio.h"

#define MODEM_ID  ((uint32_t *)0x0801ff80)
#define KEY  ((uint32_t *)0x0801ff84)    

//#define AUX_MODEM_ID  ((uint32_t *)0x0801ffb0)
//#define AUX_KEY  ((uint32_t *)0x0801ffb4)    

//const uint32_t sr_modem_id = 7959442;


#define MANUFACTURER_ID         0x8888 //Waviot
#define HARDWARE_TYPE_ID        0x3       //ASIC_PROTOTYPE
#define PROTOCOL_ID             0       //undefined
#define TX_MAX_POWER            15
#define TX_MIN_POWER            -13
#define SEND_INFO_PERIOD	2592000         //one time per month

#define BAND         UL868800_DL869150          


#if BAND == UL868800_DL446000
#define NBFI_UL_FREQ_BASE       868800000
#define NBFI_DL_FREQ_BASE       446000000
#elif BAND == UL868800_DL864000
#define NBFI_UL_FREQ_BASE       868800000
#define NBFI_DL_FREQ_BASE       864000000
#elif BAND == UL868800_DL446000_DL864000
#define NBFI_UL_FREQ_BASE       868800000
#define NBFI_DL_FREQ_BASE       864000000
#elif BAND == UL867950_DL446000
#define NBFI_UL_FREQ_BASE       867950000
#define NBFI_DL_FREQ_BASE       446000000
#elif BAND == UL868500_DL446000
#define NBFI_UL_FREQ_BASE       868500000
#define NBFI_DL_FREQ_BASE       446000000
#elif BAND == UL868100_DL446000
#define NBFI_UL_FREQ_BASE       868100000
#define NBFI_DL_FREQ_BASE       446000000
#elif BAND == UL864000_DL446000
#define NBFI_UL_FREQ_BASE       864000000
#define NBFI_DL_FREQ_BASE       446000000
#elif BAND == UL863175_DL446000
#define NBFI_UL_FREQ_BASE       863175000
#define NBFI_DL_FREQ_BASE       446000000
#elif BAND == UL864000_DL875000
#define NBFI_UL_FREQ_BASE       864000000
#define NBFI_DL_FREQ_BASE       875000000
#elif BAND == UL868800_DL868000
#define NBFI_UL_FREQ_BASE       868800000
#define NBFI_DL_FREQ_BASE       868800000
#elif BAND == UL868800_DL869150
#define NBFI_UL_FREQ_BASE       868800000    
#define NBFI_DL_FREQ_BASE       869150000
#endif 


#define TRY_LOW_PHY_ALTERNATIVE   {4, UL_DBPSK_50_PROT_E, DL_DBPSK_50_PROT_D, NBFI_UL_FREQ_PLAN_NO_CHANGE + NBFI_DL_FREQ_PLAN_NO_CHANGE} 
#define TRY_MINIMAL_UL_BAND_AND_LOW_PHY_ALTERNATIVE   {8, UL_DBPSK_50_PROT_E, DL_DBPSK_50_PROT_D, NBFI_FREQ_PLAN_MINIMAL + NBFI_DL_FREQ_PLAN_NO_CHANGE}

const nbfi_settings_t nbfi_default_settings =
{      
    MODEM_ID, 
    KEY,
    CRX,//mode;
    UL_DBPSK_50_PROT_E,//UL_DBPSK_50_PROT_D, // tx_phy_channel;
    DL_DBPSK_50_PROT_D, // rx_phy_channel;
    HANDSHAKE_SIMPLE,
    MACK_1,             //mack_mode
    0x82,                  //num_of_retries;
    8,                  //max_payload_len;
    //0,                  //dl_ID;
    0,                  //wait_ack_timeout
    0,                  //tx_freq;
    0,//858090000,//868791000,//0,//868790000,//0,//868735500,//868710000,//868800000,                  //rx_freq;
    PCB,                //tx_antenna;
    PCB,                //rx_antenna;
    TX_MAX_POWER,       //tx_pwr;
    60*5,             //heartbeat_interval
    255,                //heartbeat_num
    0,//NBFI_FLG_FIXED_BAUD_RATE,                  //additional_flags
    NBFI_UL_FREQ_BASE,
    NBFI_DL_FREQ_BASE,
    NBFI_UL_FREQ_PLAN_51200_0 + NBFI_FREQ_PLAN_MINIMAL,
    {
      TRY_MINIMAL_UL_BAND_AND_LOW_PHY_ALTERNATIVE,
      TRY_LOW_PHY_ALTERNATIVE,
      NBFI_VOID_ALTERNATIVE,
      NBFI_VOID_ALTERNATIVE
    }
};

const nbfi_dev_info_t nbfi_info = {TX_MIN_POWER, TX_MAX_POWER, MANUFACTURER_ID, HARDWARE_TYPE_ID, PROTOCOL_ID, BAND, SEND_INFO_PERIOD};


const nbfi_settings_t nbfi_short_range_settings =
{      
    MODEM_ID, 
    KEY,
    CRX,//mode;
    DL_DBPSK_25600_PROT_D, // tx_phy_channel;
    DL_DBPSK_25600_PROT_D, // rx_phy_channel;
    HANDSHAKE_SIMPLE,
    MACK_1,             //mack_mode
    0,                  //num_of_retries;
    8,                  //max_payload_len;
    //0,                  //dl_ID;
    120,                //wait_ack_timeout
    0,                  //tx_freq;
    0,                  //rx_freq;
    PCB,                //tx_antenna;
    PCB,                //rx_antenna;
    TX_MAX_POWER,       //tx_pwr;
    1,//3600*6,         //heartbeat_interval
    0,                  //heartbeat_num
    NBFI_FLG_FIXED_BAUD_RATE|NBFI_FLG_SHORT_RANGE_CRYPTO|NBFI_FLG_NO_RESET_TO_DEFAULTS|NBFI_FLG_NO_SENDINFO|NBFI_FLG_NO_REDUCE_TX_PWR,                  //additional_flags
    NBFI_UL_FREQ_BASE,
    NBFI_DL_FREQ_BASE,
    NBFI_FREQ_PLAN_MINIMAL + NBFI_DL_FREQ_PLAN_819200_M2457600,
    {
      NBFI_VOID_ALTERNATIVE,
      NBFI_VOID_ALTERNATIVE,
      NBFI_VOID_ALTERNATIVE,
      NBFI_VOID_ALTERNATIVE
    }
};

void radio_switch_to_from_short_range(_Bool en)
{
    nbfi_crypto_iterator_t it = {0,0};
    NBFi_switch_to_another_settings((nbfi_settings_t *)&nbfi_short_range_settings, &it, en);
}


void radio_init(void)
{
  
        scheduler_HAL_init();

        wa1470_HAL_reg_data_received_callback((void*)NBFi_MAC_RX_ProtocolD);
        wa1470_HAL_reg_tx_finished_callback((void*)NBFi_RF_TX_Finished);       
        wa1470_HAL_init();        
	nbfi_HAL_init(&nbfi_default_settings, (nbfi_dev_info_t*)&nbfi_info);  
        scheduler_run_callbacks();
        //NBFi_clear_Saved_Configuration(); //if you need to clear previously saved nbfi configuration in EEPROM
}
