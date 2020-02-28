#include "main.h"
#include "stm32_init.h"
#include "string.h"
#include "radio.h"
#include "log.h"
#include "defines.h"

void HAL_SYSTICK_Callback(void)
{

}


//nbfi_ul_sent_status_t last_send_status;


void nbfi_send_complete(nbfi_ul_sent_status_t ul)
{
   
   /*uint8_t string[] = "Hello, we are testing 25600bps receiving stability. This huge packet is sending for giving a numerous quantity of packets";
      
    if(ul.id == last_send_status.id)
    {
      string[0] = (ul.id>>8);
      string[1] = (ul.id & 0xff);
      last_send_status = NBFi_Send((uint8_t*)string, sizeof(string));
    }   */
}

void nbfi_receive_complete(uint8_t * data, uint16_t length)
{
  NBFi_Send(data, length);
}


int main(void)
{
        
  HAL_Init();

  ADC_init();
  
  SystemClock_Config();
  
  MX_GPIO_Init();
 
  radio_init();

  log_init();
  
  //last_send_status = NBFi_Send("Hello!", sizeof("Hello!"));   
  
  while (1) 
  {     
     
      #ifdef PLOT_SPECTRUM
      plot_spectrum();
      #endif
            
      NBFI_Main_Level_Loop();
      
      if (wa1470_cansleep()&& NBFi_can_sleep()) 
      {
          //HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_SLEEPENTRY_WFI);
         HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
      }
      else HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
  }
}
