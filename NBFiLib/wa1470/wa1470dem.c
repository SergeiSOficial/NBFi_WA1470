#include "wa1470.h"
//#include "wa1470dem.h"
#include "wtimer.h"
#include  <string.h>
#include <stdio.h>
#include <log.h>
#include <math.h>
#include "nbfi.h"



#include "stm32l0xx_hal_conf.h"

#define DEM_MAS_SIZE	8

dem_packet_st dem_mas[DEM_MAS_SIZE];

dem_packet_info_st dem_info_mas[DEM_MAS_SIZE];

uint32_t dem_noise_table[32];
uint32_t dem_noise_table_temp[32];

uint8_t	dem_mess_received;

struct wtimer_desc dem_processMessages_desc;

extern void (*__wa1470_enable_pin_irq)(void);
extern void (*__wa1470_disable_pin_irq)(void);
extern void (*__wa1470_data_received)(dem_packet_st *pkt, dem_packet_info_st * info);


dem_packet_st tmp_dem_mas[DEM_MAS_SIZE];
dem_packet_info_st tmp_dem_info_mas[DEM_MAS_SIZE];
uint8_t	tmp_dem_mess_received;

dem_bitrate_s current_rx_phy = DBPSK_100H_PROT_D;

extern uint8_t (*__wa1470_get_irq_pin_state)(void);

uint8_t current_hop_table[8] = {DEM_PLUS90000,DEM_PLUS65000,DEM_PLUS40000,DEM_PLUS15000,DEM_MINUS15000,DEM_MINUS40000,DEM_MINUS65000,DEM_MINUS90000};

const int32_t DEM_FREQ_OFFSETS[8] = {90000,65000,40000,15000,-15000,-40000,-65000,-90000};

uint8_t mas[128];

uint32_t dem_noise_sum[32];
uint32_t dem_noise_min[32];
float dem_noise[32];
uint8_t dem_noise_cntr = 0;
uint8_t dem_noise_min_cntr = 10;


void wa1470dem_init()
{
  
    uint8_t preambule[4] = {0x97,0x15,0x7a,0x6f};
    uint8_t NB_FI_RX_CRC_POLY[4] = {0xb7, 0x1d, 0xc1, 0x04};
  
    wa1470_spi_read(0x20, mas, 128);
    wa1470_spi_write8(DEM_CONTROL, DEM_CONTROL_RESET);
    wa1470dem_set_bitrate(DBPSK_50_PROT_D);
    wa1470dem_set_threshold(1024);
    wa1470dem_set_alpha(128, 5);
    wa1470dem_set_crc_poly(NB_FI_RX_CRC_POLY);
    wa1470dem_set_hop_table(current_hop_table);
    wa1470dem_set_hop_len(36);
    //wa1470dem_set_gain(23);
    wa1470_spi_write8(DEM_FFT_MSB, 0x80 + 23);
    //wa1470_spi_write8(DEM_DET_TRESHOLD, 0x58); // wtite 600 to DEM_DET_TRESHOLD
    //wa1470_spi_write8(DEM_DET_TRESHOLD + 1, 0x02);
    //wa1470dem_reset();
    
    wa1470_spi_write8(DEM_CONTROL, 0);  
     
    wa1470_spi_read(0x20, mas, 128);
    memset(dem_noise_table_temp, 0xff, sizeof(dem_noise_table_temp));
   // for(uint8_t i = 0; i != 32; i++) dem_noise_min[i] = -150;
    //log_send_str_len("Demodulator inited\n\r", sizeof("Demodulator inited\n\r"));
}

void wa1470dem_reset(void)
{
  wa1470_spi_write8(DEM_CONTROL, DEM_CONTROL_RESET);
  wa1470_spi_write8(DEM_CONTROL, 0);  
}

static void  wa1470dem_process_messages(struct wtimer_desc *desc)
{
      
        if(__wa1470_disable_pin_irq) __wa1470_disable_pin_irq();
	
        tmp_dem_mess_received = dem_mess_received;
        memcpy(tmp_dem_mas, dem_mas, sizeof(tmp_dem_mas)); 
        memcpy(tmp_dem_info_mas, dem_info_mas, sizeof(tmp_dem_info_mas)); 
        dem_mess_received = 0;
        memset(dem_mas, 0 , sizeof(dem_mas));
	memset(dem_info_mas, 0 , sizeof(dem_info_mas));

        if(__wa1470_enable_pin_irq) __wa1470_enable_pin_irq();      
        
      /*  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
        delay_ms(1000);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);*/
        
	if(__wa1470_data_received)
		for(uint8_t i = 0; i != tmp_dem_mess_received; i++) 
                {                  
                  sprintf(log_string, "%05u: ID=%02X%02X%02X PLD: ", (uint16_t)(NBFi_get_RTC()&0xffff), tmp_dem_mas[i].packet.id_0, tmp_dem_mas[i].packet.id_1, tmp_dem_mas[i].packet.id_2);
                  for(uint8_t k = 0; k != 8; k++) sprintf(log_string + strlen(log_string), "%02X", tmp_dem_mas[i].packet.payload[k]);
                  sprintf(log_string + strlen(log_string), " IT=%2d COP=%2d(%2d) FREQ=%2d", tmp_dem_mas[i].packet.flags&0x1f, tmp_dem_info_mas[i].num_of_crc + tmp_dem_info_mas[i].num_of_zigzag, tmp_dem_info_mas[i].num_of_zigzag, tmp_dem_mas[i].freq&0x1f);
                  uint64_t rssi64 = tmp_dem_mas[i].rssi_39_32;
                  rssi64 <<= 32;
                  rssi64 += tmp_dem_mas[i].rssi;
                  //int16_t rssi = 0;//omsplog10(rssi64);// - 52 - 192;//log10f(rssi64)*20 - 48 - 192;//fxlog(packet->rssi);
                  float rssi = log10f(rssi64)*20 - 48 - rfe_logoffset;
                  float dsnr = log10f(((float)rssi64)/tmp_dem_mas[i].noise/4)*20;
                  tmp_dem_info_mas[i].rssi = (int16_t)rssi;
                  float snr = rssi - dem_noise[tmp_dem_mas[i].freq&0x1f];
                  if(snr < 0) snr = 0;
                  tmp_dem_info_mas[i].snr = (uint8_t)snr;
                  
                  
                  
                  //sprintf(log_string + strlen(log_string), " RSSI39_32=%d", tmp_dem_mas[i].rssi_39_32);
                  sprintf(log_string + strlen(log_string), " RSSI=%ld", tmp_dem_mas[i].rssi);
                  sprintf(log_string + strlen(log_string), " LRSSI=%f", rssi);
                  sprintf(log_string + strlen(log_string), " SNR=%f", rssi - dem_noise[tmp_dem_mas[i].freq&0x1f] + (float)((current_rx_phy == DBPSK_25600_PROT_D)?24:0));
                  sprintf(log_string + strlen(log_string), " DSNR=%f", dsnr);
                 
                  
                  switch(current_rx_phy)
                  {
                  case DBPSK_50_PROT_D:
                    sprintf(log_string + strlen(log_string), " 50BPS");
                    break;
                  case DBPSK_400_PROT_D:
                    sprintf(log_string + strlen(log_string), " 400BPS");
                    break;
                  case DBPSK_3200_PROT_D:
                    sprintf(log_string + strlen(log_string), " 3200BPS");
                    break;
                  case DBPSK_25600_PROT_D:
                    sprintf(log_string + strlen(log_string), " 25600BPS");
                    tmp_dem_info_mas[i].snr += 24;
                    break;
                  case DBPSK_100H_PROT_D:
                    sprintf(log_string + strlen(log_string), " 100HBPS");
                    break;
                    
                  }
                  log_send_str(log_string);
                  __wa1470_data_received(&tmp_dem_mas[i], &tmp_dem_info_mas[i]);
                }
	
}


void wa1470dem_isr(void) 
{
   
        uint8_t status;
 
        wa1470_spi_read(DEM_CONTROL, &status, 1);
      
        if(!(status&DEM_CONTROL_IRQ_FLAG)) return;
  
        dem_packet_st new_packet;
	          
        do
        {
          
          wa1470_spi_read(DEM_RECEIVED_MES_BUF, (uint8_t*)(&new_packet), 32);
          
          wa1470_spi_write8(DEM_RECEIVED_MES_BUF, 0);
          
                    
          dem_mas[dem_mess_received] = new_packet;
          

          if(dem_mess_received == (DEM_MAS_SIZE - 1))  return;

          
          ScheduleTask(&dem_processMessages_desc,  wa1470dem_process_messages, RELATIVE, MILLISECONDS(50));
          
          
          uint16_t id = (*((uint16_t*)(&dem_mas[dem_mess_received])));
          uint8_t i;
          for(i = 0; i < dem_mess_received; i++)
          {
                  if( (*((uint16_t*)&(dem_mas[i]))) == id)
                          if(dem_mas[i].packet.flags == dem_mas[dem_mess_received].packet.flags)
                          {
                                  break;
                          }
          }

          if(new_packet.crc_or_zigzag)
          {
                  dem_info_mas[i].num_of_zigzag++;
          }
          else dem_info_mas[i].num_of_crc++;
          
          if(i == dem_mess_received)
          {
                  dem_mess_received++;
          }
          else
          {
                  if((dem_mas[i].rssi_39_32 < new_packet.rssi_39_32))
                  {
                          dem_mas[i] = dem_mas[dem_mess_received];
                  }
                  else if((dem_mas[i].rssi_39_32 == new_packet.rssi_39_32) && (dem_mas[i].rssi < new_packet.rssi))
                  {
                          dem_mas[i] = dem_mas[dem_mess_received];
                  }
          }
        }
        while (__wa1470_get_irq_pin_state && __wa1470_get_irq_pin_state());

}


void wa1470dem_set_bitrate(dem_bitrate_s bitrate)
{
       // if(current_rx_phy == bitrate) return;
        if(__wa1470_disable_pin_irq) __wa1470_disable_pin_irq();

	switch(bitrate)
	{
	case DBPSK_50_PROT_D:
                wa1470_spi_write8(DEM_RX_MODE, 0);
                //wa1470dem_set_gain(23);
                //wa1470dem_set_alpha(128, 5);
                log_send_str_len("Switched to DBPSK_50_PROT_D\n\r", sizeof("Switched to DBPSK_50_PROT_D\n\r"));
		break;
	case DBPSK_400_PROT_D:
                wa1470_spi_write8(DEM_RX_MODE, 1);
                //wa1470dem_set_gain(23);
                //wa1470dem_set_alpha(128, 5);
                log_send_str_len("Switched to DBPSK_400_PROT_D\n\r", sizeof("Switched to DBPSK_400_PROT_D\n\r"));
		break;
	case DBPSK_3200_PROT_D:
                wa1470_spi_write8(DEM_RX_MODE, 2);
                //wa1470dem_set_gain(23);
                //wa1470dem_set_alpha(128, 5);
                log_send_str_len("Switched to DBPSK_3200_PROT_D\n\r", sizeof("Switched to DBPSK_3200_PROT_D\n\r"));
		break;
	case DBPSK_25600_PROT_D:
                wa1470_spi_write8(DEM_RX_MODE, 3);
                //wa1470dem_set_gain(15);
                //wa1470dem_set_alpha(128, 5);
                log_send_str_len("Switched to DBPSK_25600_PROT_D\n\r", sizeof("Switched to DBPSK_25600_PROT_D\n\r"));               
		break;
        case DBPSK_100H_PROT_D:
                wa1470_spi_write8(DEM_RX_MODE, 4);
                //wa1470dem_set_alpha(128, 5);
                log_send_str_len("Switched to DBPSK_100H_PROT_D\n\r", sizeof("Switched to DBPSK_100H_PROT_D\n\r"));
		break;
	}
        current_rx_phy = bitrate;
       
       /* if(bitrate == DBPSK_100H_PROT_D) wa1470dem_set_hop_table(0x12345678);
        else wa1470dem_set_hop_table(0x00000000);*/
    
        wa1470dem_reset();
        if(__wa1470_enable_pin_irq) __wa1470_enable_pin_irq();      
}

void wa1470dem_set_alpha(uint8_t noise_start_bit, uint8_t shift)
{
        if(__wa1470_disable_pin_irq) __wa1470_disable_pin_irq();
        wa1470_spi_write8(DEM_NOSE_START_BIT, noise_start_bit);
        wa1470_spi_write8(DEM_ALPHA_SHIFT, shift);
        if(__wa1470_enable_pin_irq) __wa1470_enable_pin_irq(); 
}

void wa1470dem_set_threshold(uint16_t SOFT_DETECT_THR)
{
  uint8_t  SOFT_DETECT_THR_ARR[2] = {SOFT_DETECT_THR & 255, SOFT_DETECT_THR >> 8};
  if(__wa1470_disable_pin_irq) __wa1470_disable_pin_irq();
  wa1470_spi_write(DEM_DET_TRESHOLD, SOFT_DETECT_THR_ARR, 2);
  if(__wa1470_enable_pin_irq) __wa1470_enable_pin_irq();
}

/*void wa1470dem_set_gain(uint8_t gain)
{
        if(__wa1470_disable_pin_irq) __wa1470_disable_pin_irq();
        wa1470_spi_write8(DEM_GAIN, gain);
        if(__wa1470_enable_pin_irq) __wa1470_enable_pin_irq(); 
}*/

void wa1470dem_set_crc_poly(uint8_t* crc)
{
        if(__wa1470_disable_pin_irq) __wa1470_disable_pin_irq();
        wa1470_spi_write(DEM_CRC_POLY, crc, 4);
        if(__wa1470_enable_pin_irq) __wa1470_enable_pin_irq();
}

void wa1470dem_set_hop_len(uint8_t hop_len)
{
	if(__wa1470_disable_pin_irq) __wa1470_disable_pin_irq();
        wa1470_spi_write8(DEM_HOP_LENGTH, hop_len);
	if(__wa1470_enable_pin_irq) __wa1470_enable_pin_irq();
}

void wa1470dem_set_preambule(uint8_t* preambule)
{
    if(__wa1470_disable_pin_irq) __wa1470_disable_pin_irq();
    wa1470_spi_write(DEM_PREAMBLE_ID, preambule,4);

    if(__wa1470_enable_pin_irq) __wa1470_enable_pin_irq();

}

void wa1470dem_set_hop_table(uint8_t* hop)
{
	if(__wa1470_disable_pin_irq) __wa1470_disable_pin_irq();
	wa1470_spi_write8(DEM_HOP_TABLE ,  (hop[1] << 4) | hop[0]);
	wa1470_spi_write8(DEM_HOP_TABLE+1, (hop[3] << 4) | hop[2]);
	wa1470_spi_write8(DEM_HOP_TABLE+2, (hop[5] << 4) | hop[4]);
	wa1470_spi_write8(DEM_HOP_TABLE+3, (hop[7] << 4) | hop[6]);
	if(__wa1470_enable_pin_irq) __wa1470_enable_pin_irq();
        for(int i = 0; i<8; i++ ) current_hop_table[i] = hop[i];
}

/*void wa1470dem_set_hop_table(uint32_t hop_table)
{
        if(__wa1470_disable_pin_irq) __wa1470_disable_pin_irq();
        wa1470_spi_write(DEM_HOP_TABLE, ((uint8_t*)&hop_table), 4);
        if(__wa1470_enable_pin_irq) __wa1470_enable_pin_irq(); 
}*/

void wa1470dem_set_freq(uint32_t freq)
{
  sprintf(log_string, "dem_set_freq to %ld", freq);
  log_send_str(log_string); 
  switch(current_rx_phy)
  {
    case  DBPSK_50_PROT_D:
    case  DBPSK_400_PROT_D:
    case  DBPSK_3200_PROT_D:
      wa1470rfe_set_freq(freq - DEM_FREQ_OFFSETS[current_hop_table[0]]);     
      //wa1470rfe_set_freq(858000000);
      //wa1470rfe_set_freq(864055540 - 15000);//wa1470rfe_set_freq(freq + 90000 + 137500000); 
      //wa1470rfe_set_freq(868000000);
      break;
    case  DBPSK_25600_PROT_D:
      wa1470rfe_set_freq(freq - DEM_FREQ_OFFSETS[current_hop_table[1]]);   
      //wa1470rfe_set_freq(freq + 90000 + 137500000);
      break;
    case DBPSK_100H_PROT_D:
      wa1470rfe_set_freq(freq);
      break;
  }

}
 /* struct fft_point
  {
    int16_t real;
    int16_t im;    
  };

 struct fft_point fft_tbl[32];*/

//float fft_fmagn[32];

 
void wa1470dem_update_noise()
{
   
#define DEM_NOISE_AVER  16
   if(dem_noise_cntr == DEM_NOISE_AVER)
   {
      dem_noise_cntr = 0;
      
      for(uint8_t i = 0; i != 32; i++)
      {
        uint32_t aver = dem_noise_sum[i]/DEM_NOISE_AVER;
        dem_noise_sum[i] = 0;
        if(aver < dem_noise_min[i]) dem_noise_min[i] = aver; 
        if(dem_noise_min_cntr == 0)
        {
          if(dem_noise_min[i] == 0) dem_noise[i] = aver;
          else dem_noise[i] = dem_noise_min[i];
          dem_noise[i] = log10f(dem_noise[i])*20 - rfe_logoffset;
          dem_noise_min[i] = 0xffffffff;
        }       
      }
      
      if(dem_noise_min_cntr)
      {
        dem_noise_min_cntr--;
      }else  dem_noise_min_cntr = 20;
      
   }
   else
   {
      uint32_t tmp_mas[32];
      if(__wa1470_disable_pin_irq) __wa1470_disable_pin_irq(); 
      wa1470_spi_read(DEM_FFT_READ_BUF, (uint8_t*)(&tmp_mas[0]), 4*32); 
      wa1470_spi_write8(DEM_FFT_READ_BUF + 100, 0);     
      //log_send_to_sfmonitor(tmp_mas, 32);
      //wa1470rfe_set_freq(868000000);
      //static uint8_t ones = 3;
      //if(ones) {wa1470rfe_set_freq(864055540 - 90000); ones--;}
      //wa1470rfe_set_freq(864055540 - 90000);
     // wa1470rfe_set_freq(858000000);
      if(__wa1470_enable_pin_irq) __wa1470_enable_pin_irq();    
      for(uint8_t i = 0; i != 32; i++) dem_noise_sum[i] += tmp_mas[i];
      dem_noise_cntr++;

   }
   
   
}