#include "nbfi.h"
#include <string.h>
#include <stdlib.h>



nbfi_HAL_st* nbfi_hal = 0;
ischeduler_st* nbfi_scheduler = 0;

_Bool switched_to_custom_settings = 0;

#ifdef NBFI_LOG
char nbfi_log_string[256];
#endif


void NBFI_Init(nbfi_HAL_st* ptr, ischeduler_st* scheduler, nbfi_dev_info_t* info)
{
    nbfi_hal = ptr;
    nbfi_scheduler = scheduler;
    if((nbfi_hal == 0) || (nbfi_scheduler == 0)) while(1); //HAL and scheduler pointers must be provided 
    NBFi_set_Device_Info(info); 
    NBFi_Config_Set_Default();	

    if(nbfi.master_key != 0)
    {
      NBFi_MAC_Get_Iterator();
      NBFi_Crypto_Set_KEY(nbfi.master_key, &nbfi_iter.ul, &nbfi_iter.dl);
    }
    
    NBFI_Transport_Init();
    
    
}

uint8_t NBFi_get_Received_Packet(uint8_t * payload)
{
  nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_LOCK);
  uint8_t length = NBFi_Next_Ready_DL(payload);
  nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_UNLOCK);
  return length;
}

//call this function in main loop 
void  NBFI_Main_Level_Loop()
{

    if(nbfi_hal->__nbfi_rx_handler)
    {
      uint8_t payload[256];
      while(1)
      {
        uint8_t length = NBFi_get_Received_Packet(payload);
        if(length) nbfi_hal->__nbfi_rx_handler(payload, length);
        else break;
      }
      
    }
    
    if(nbfi_hal->__nbfi_send_status_handler)
    {
      nbfi_ul_sent_status_t* ul;
      while(1)
      {
        nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_LOCK);
        ul = NBFi_Get_Next_Unreported_UL(DELIVERED);
        if(ul == 0) ul = NBFi_Get_Next_Unreported_UL(LOST);
        nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_UNLOCK);
        if(ul) nbfi_hal->__nbfi_send_status_handler(*ul);
        else break;
      }
    }
    
   if(rtc_synchronised && nbfi_hal->__nbfi_rtc_synchronized) 
   {
     rtc_synchronised = 0;
     nbfi_hal->__nbfi_rtc_synchronized(nbfi_rtc);
   }
   
     if(nbfi_settings_need_to_save_to_flash && (nbfi_hal->__nbfi_write_flash_settings != 0)) 
     {
        nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_LOCK);
        nbfi_phy_channel_t tmp_tx_phy = nbfi.tx_phy_channel;
        nbfi_phy_channel_t tmp_rx_phy = nbfi.rx_phy_channel;
        
        if(!nbfi.additional_flags&NBFI_FLG_FIXED_BAUD_RATE) //if auto bitrates
        {
          nbfi_settings_t default_nbfi_settings;
          nbfi_hal->__nbfi_read_default_settings(&default_nbfi_settings);
          nbfi.rx_phy_channel = default_nbfi_settings.rx_phy_channel;
          nbfi.tx_phy_channel = default_nbfi_settings.tx_phy_channel;
        }
        nbfi_hal->__nbfi_write_flash_settings(&nbfi);
        nbfi.tx_phy_channel = tmp_tx_phy;
        nbfi.rx_phy_channel = tmp_rx_phy;      
        nbfi_settings_need_to_save_to_flash = 0;
        nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_UNLOCK);
     }
   
}


nbfi_status_t NBFi_go_to_Sleep(_Bool sleep)
{
    static _Bool old_state = 1;
    nbfi_status_t res = OK;
    if(sleep)
    {
        nbfi.mode = OFF;
        NBFi_Clear_TX_Buffer();
        res = NBFi_RF_Deinit();
    }
    else
    {
        if(old_state != sleep)
        {
            nbfi_settings_t settings;
            NBFi_ReadConfig(&settings);
            nbfi.mode = settings.mode;
            NBFi_Config_Send_Sync(0);
            NBFi_Send_Clear_Cmd(0);
            NBFi_Force_process();
        }
    }
    old_state = sleep;
    return res;
}

uint8_t NBFi_can_sleep()
{
  nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_LOCK);
  uint8_t can = (!rf_busy) && (rf_state == STATE_OFF) && (NBFi_Packets_To_Send() == 0) && NBFi_RF_can_Sleep();
  //can = can && !(last_ack_send_ts &&  ((nbfi_scheduler->__scheduler_curr_time() - last_ack_send_ts) < WAITALITTLEBIT));
  nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_UNLOCK);
  return can;
}

uint8_t NBFi_get_Packets_to_Send()
{
    uint8_t n;
    nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_LOCK);
    n = NBFi_Packets_To_Send();
    nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_UNLOCK);
    return n;
}

nbfi_ul_sent_status_t  NBFi_get_UL_status(uint16_t id)
{
  nbfi_ul_sent_status_t ret;
  nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_LOCK);
  nbfi_ul_sent_status_t *status = NBFi_Get_UL_status(id, 0);
  if(status)
  {
      ret = *status;
  }else   ret.id = 0;
  nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_UNLOCK);  
  return ret;
}

_Bool  NBFi_is_Idle()
{
  _Bool idle = 1;
  nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_LOCK);
  if(NBFi_Packets_To_Send()) idle = 0;
  else if(last_ack_send_ts &&  ((nbfi_scheduler->__scheduler_curr_time() - last_ack_send_ts) < WAITALITTLEBIT)) idle = 0;
  else if(nbfi_active_pkt->state == PACKET_WAIT_FOR_EXTRA_PACKETS) idle = 0;
  nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_UNLOCK);
  return idle;
}

void NBFi_get_state(nbfi_state_t * state)
{
  nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_LOCK);
    memcpy(state, &nbfi_state , sizeof(nbfi_state_t));
  nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_UNLOCK);
}

uint32_t NBFi_get_RTC()
{
  nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_LOCK);    
    NBFi_update_RTC();
    uint32_t rtc = nbfi_rtc;
  nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_UNLOCK);    
    return rtc;
}

void NBFi_set_RTC(uint32_t time)
{
    nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_LOCK);    
    NBFi_set_RTC_irq(time);
    nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_UNLOCK);    
}

void NBFi_set_Device_Info(nbfi_dev_info_t *info)
{
    nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_LOCK);  
    dev_info = *info;
    info_timer = dev_info.send_info_interval - 300 - rand()%600;
    nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_UNLOCK); 
}

void NBFi_get_Settings(nbfi_settings_t* settings)
{
    nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_LOCK);
    memcpy(settings, &nbfi , sizeof(nbfi_settings_t));
    nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_UNLOCK);
}

void NBFi_set_Settings(nbfi_settings_t* settings, _Bool persistent)
{
    _Bool need_to_send_sync = 0;
    _Bool need_to_send_ul_freq_base = 0;
    _Bool need_to_send_dl_freq_base = 0;
    
    nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_LOCK);
    
    if(persistent)
    {
      if(nbfi.rx_phy_channel != settings->rx_phy_channel) need_to_send_sync = 1;
      if(nbfi.ul_freq_base != settings->ul_freq_base) need_to_send_ul_freq_base = 1;
      if(nbfi.dl_freq_base != settings->dl_freq_base) need_to_send_dl_freq_base = 1;    
      if(nbfi.mode != settings->mode) 
      {
        NBFi_Clear_TX_Buffer();
        need_to_send_sync = 1;
      }
      memcpy(&nbfi, settings , sizeof(nbfi_settings_t));
      nbfi_settings_need_to_save_to_flash = 1;
      rf_state = STATE_CHANGED;
      if(need_to_send_sync) {NBFi_Config_Send_Sync(0); NBFi_Force_process();}
      if(need_to_send_ul_freq_base)  NBFi_Config_Send_Mode(HANDSHAKE_NONE, NBFI_PARAM_UL_BASE_FREQ);
      if(need_to_send_dl_freq_base)  NBFi_Config_Send_Mode(HANDSHAKE_NONE, NBFI_PARAM_DL_BASE_FREQ);
    
    }
    else memcpy(&nbfi, settings , sizeof(nbfi_settings_t));
   
    nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_UNLOCK);
}

_Bool NBFi_send_Packet_to_Config_Parser(uint8_t* buf)
{
    nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_LOCK);
    _Bool has_reply = NBFi_Config_Parser(buf);
    nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_UNLOCK);
    return has_reply;
}

void NBFi_clear_Saved_Configuration()
{
	if(nbfi_hal->__nbfi_write_flash_settings == 0) 
		return;
	nbfi_settings_t empty;
        memset(&empty, 0, sizeof(nbfi_settings_t));
	nbfi_hal->__nbfi_write_flash_settings(&empty);
}
        
void NBFi_switch_to_another_settings(nbfi_settings_t* settings, nbfi_crypto_iterator_t* it, _Bool to_or_from)
{
    static nbfi_settings_t old_settings;
    static nbfi_crypto_iterator_t old_iter;
    static nbfi_state_t state; 
    nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_LOCK);
   
    if(to_or_from)
    {
      memcpy(&old_settings, &nbfi, sizeof(nbfi_settings_t));
      memcpy(&state, &nbfi_state, sizeof(nbfi_state_t));
      old_iter = nbfi_iter;
      NBFi_Clear_TX_Buffer();
           
      memcpy(&nbfi, settings, sizeof(nbfi_settings_t));
      nbfi_iter = *it;
    }
    else
    {
        NBFi_Clear_TX_Buffer();
        memcpy(&nbfi_state, &state, sizeof(nbfi_state_t));
        nbfi_iter = old_iter;
        memcpy(&nbfi, &old_settings, sizeof(nbfi_settings_t));
        NBFi_Config_Set_TX_Chan(old_settings.tx_phy_channel);
        NBFi_Config_Set_RX_Chan(old_settings.rx_phy_channel);
        rf_state = STATE_CHANGED;
    }

    if(nbfi.master_key != 0)
    {
      NBFi_Crypto_Set_KEY(nbfi.master_key, &nbfi_iter.ul, &nbfi_iter.dl);
    }
    
    if(rf_state == STATE_RX) NBFi_MAC_RX();
    
    switched_to_custom_settings = to_or_from;
    
    nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_UNLOCK);
}

_Bool   NBFi_is_Switched_to_Custom_Settings()
{
  return switched_to_custom_settings;
}

void NBFi_CPU_Reset()
{
  if(nbfi_hal->__nbfi_reset) nbfi_hal->__nbfi_reset();
}

float NBFi_get_rssi()
{
  float rssi;
  nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_LOCK);
  rssi = NBFi_RF_get_rssi();
  nbfi_hal->__nbfi_lock_unlock_loop_irq(NBFI_UNLOCK);
  return rssi;
}