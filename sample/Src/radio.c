#include "main.h"
#include "wa1205.h"
#include "radio.h"
#include "wtimer.h"
#include "string.h"
#include "nbfi.h"
#include "rf.h"
#include "nbfi_config.h"
#include "stm32l0xx_hal_conf.h"

#define MODEM_ID  *((const uint32_t*)0x0801ff80)  
#define KEY  ((const uint32_t*)0x0801ff84)            

#define HW_ID     255
#define HW_REV    0
#define TX_MAX_POWER 15
#define TX_MIN_POWER 0
#define SEND_INFO_PERIOD	2592000  //one time per month
#define BAND         UL868800_DL864000


#if BAND == UL868800_DL446000
#define NBFI_UL_FREQ_BASE       (868800000 - 25000)
#define NBFI_DL_FREQ_BASE       446000000
#elif BAND == UL868800_DL864000
#define NBFI_UL_FREQ_BASE       (868800000 - 25000)
#define NBFI_DL_FREQ_BASE       864000000
#elif BAND == UL868800_DL446000_DL864000
#define NBFI_UL_FREQ_BASE       (868800000 - 25000)
#define NBFI_DL_FREQ_BASE       864000000
#elif BAND == UL867950_DL446000
#define NBFI_UL_FREQ_BASE       (867950000 - 25000)
#define NBFI_DL_FREQ_BASE       446000000
#elif BAND == UL868500_DL446000
#define NBFI_UL_FREQ_BASE       (868500000 - 25000)
#define NBFI_DL_FREQ_BASE       446000000
#elif BAND == UL868100_DL446000
#define NBFI_UL_FREQ_BASE       (868100000 - 25000)
#define NBFI_DL_FREQ_BASE       446000000
#elif BAND == UL864000_DL446000
#define NBFI_UL_FREQ_BASE       (864000000 - 25000)
#define NBFI_DL_FREQ_BASE       446000000
#elif BAND == UL863175_DL446000
#define NBFI_UL_FREQ_BASE       (863175000 - 25000)
#define NBFI_DL_FREQ_BASE       446000000
#elif BAND == UL864000_DL875000
#define NBFI_UL_FREQ_BASE       (864000000 - 25000)
#define NBFI_DL_FREQ_BASE       875000000
#endif 


const nbfi_settings_t nbfi_set_default =
{
    CRX,//mode;
    UL_DBPSK_50_PROT_D, // tx_phy_channel;
    DL_PSK_200,         // rx_phy_channel;
    HANDSHAKE_SIMPLE,
    MACK_1,             //mack_mode
    2,                  //num_of_retries;
    8,                  //max_payload_len;
    {0},                //dl_ID[3];
    {0},                //temp_ID[3];
    {0xFF,0,0},         //broadcast_ID[3];
    {0},                //full_ID[6];
    0,                  //tx_freq;
    0,                  //rx_freq;
    PCB,                //tx_antenna;
    PCB,                //rx_antenna;
    TX_MAX_POWER,       //tx_pwr;
    3600*6,             //heartbeat_interval
    255,                //heartbeat_num
    0,                  //additional_flags
    NBFI_UL_FREQ_BASE,
    NBFI_DL_FREQ_BASE
};


static SPI_HandleTypeDef hspi;

static LPTIM_HandleTypeDef hlptim;

#define SPI_TIMEOUT	1000

void RADIO_SPI_Init(void)
{

  hspi.Instance = SPI2;
  hspi.Init.Mode = SPI_MODE_MASTER;
  hspi.Init.Direction = SPI_DIRECTION_2LINES;
  hspi.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi.Init.NSS = SPI_NSS_SOFT;
  hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi.Init.CRCPolynomial = 7;
  if (HAL_SPI_Init(&hspi) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }
  __HAL_SPI_ENABLE(&hspi);

}

void RADIO_LPTIM_Init(void)
{

  hlptim.Instance = LPTIM1;
  hlptim.Init.Clock.Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
  hlptim.Init.Clock.Prescaler = LPTIM_PRESCALER_DIV32;
  hlptim.Init.Trigger.Source = LPTIM_TRIGSOURCE_SOFTWARE;
  hlptim.Init.OutputPolarity = LPTIM_OUTPUTPOLARITY_HIGH;
  hlptim.Init.UpdateMode = LPTIM_UPDATE_IMMEDIATE;
  hlptim.Init.CounterSource = LPTIM_COUNTERSOURCE_INTERNAL;
  
  if (HAL_LPTIM_Init(&hlptim) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}


void LPTIM1_IRQHandler(void)
{
  if (__HAL_LPTIM_GET_FLAG(&hlptim, LPTIM_FLAG_CMPM) != RESET) {
		__HAL_LPTIM_CLEAR_FLAG(&hlptim, LPTIM_FLAG_CMPM);
		wtimer_cc0_irq();
	}
}

void SPI_RX(uint8_t* byte, uint16_t len) {
	volatile uint16_t timeout;
	
	for (uint16_t i = 0; i < len; i++)
	{
		hspi.Instance->DR = 0x00;
		timeout = SPI_TIMEOUT;
		while(__HAL_SPI_GET_FLAG(&hspi, SPI_FLAG_RXNE) == RESET && timeout--);
		timeout = SPI_TIMEOUT;
		while(__HAL_SPI_GET_FLAG(&hspi, SPI_FLAG_BSY) == SET && timeout--);
		byte[i] = hspi.Instance->DR;
	}
}

void SPI_TX(uint8_t *byte, uint16_t len) {
	volatile uint16_t timeout;

	for (uint16_t i = 0; i < len; i++)
	{
		hspi.Instance->DR = byte[i];
		timeout = SPI_TIMEOUT;
		while(__HAL_SPI_GET_FLAG(&hspi, SPI_FLAG_TXE) == RESET && timeout--);
		timeout = SPI_TIMEOUT;
		while(__HAL_SPI_GET_FLAG(&hspi, SPI_FLAG_BSY) == SET && timeout--);
		__HAL_SPI_CLEAR_OVRFLAG(&hspi);
	}
}

void SPI_RX_TX(uint8_t *byteTx, uint8_t *byteRx, uint16_t len) {
	volatile uint16_t timeout;

	for (uint16_t i = 0; i < len; i++)
	{
		hspi.Instance->DR = byteTx[i];
		timeout = SPI_TIMEOUT;
		while(__HAL_SPI_GET_FLAG(&hspi, SPI_FLAG_RXNE) == RESET && timeout--);
		timeout = SPI_TIMEOUT;
		while(__HAL_SPI_GET_FLAG(&hspi, SPI_FLAG_BSY) == SET && timeout--);
		byteRx[i] = hspi.Instance->DR;
	}
}

void wa1205_enable_global_irq(void)
{
  __enable_irq();
}

void wa1205_disable_global_irq(void)
{
  __disable_irq();
}

void wa1205_enable_pin_irq(void)
{
  HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);
}

void wa1205_disable_pin_irq(void)
{
  HAL_NVIC_DisableIRQ(EXTI0_1_IRQn);
}

uint8_t wa1205_get_irq_pin_state(void)
{
  return HAL_GPIO_ReadPin(WA_IRQ_PORT, WA_IRQ_PIN);
}

void wa1205_spi_rx(uint8_t *pData, uint16_t Size)
{
  SPI_RX(pData, Size);
  
}

void wa1205_spi_tx(uint8_t *pData, uint16_t Size)
{
  SPI_TX(pData, Size);
}

void wa1205_spi_tx_rx(uint8_t *pTxData, uint8_t *pRxData, uint16_t Size)
{
  SPI_RX_TX(pTxData, pRxData, Size);
}

void wa1205_spi_write_cs(uint8_t state)
{
  if (state)
	HAL_GPIO_WritePin(SPI2_CS_PORT, SPI2_CS_PIN, GPIO_PIN_SET);
  else
	HAL_GPIO_WritePin(SPI2_CS_PORT, SPI2_CS_PIN, GPIO_PIN_RESET);
}

void wtimer_cc_irq_enable(uint8_t chan)
{
	__HAL_LPTIM_ENABLE_IT(&hlptim, LPTIM_IT_CMPM);
}

void wtimer_cc_irq_disable(uint8_t chan)
{
	__HAL_LPTIM_DISABLE_IT(&hlptim, LPTIM_IT_CMPM);
}

void wtimer_cc_set(uint8_t chan, uint16_t data)
{
	hlptim.Instance->CMP = data;
}

uint16_t wtimer_cc_get(uint8_t chan)
{
  return (uint16_t) hlptim.Instance->CMP;
}

uint16_t wtimer_cnt_get(uint8_t chan)
{
  static uint16_t prev = 0; 
  uint16_t timer = (uint16_t) hlptim.Instance->CNT;
  if((timer < prev) && ((prev - timer) < 10000))
  {
    return prev;
  }
  prev = timer;
  return timer;
}

uint8_t wtimer_check_cc_irq(uint8_t chan)
{
	return __HAL_LPTIM_GET_FLAG(&hlptim, LPTIM_IT_CMPM);
}


void nbfi_before_tx(NBFi_wa1205_pins_s * pins)
{
    pins->sysclk = 1; // TX Led ON
}

void nbfi_before_rx(NBFi_wa1205_pins_s * pins)
{
    pins->sysclk = 0; // TX Led OFF
    
}

void nbfi_before_off(NBFi_wa1205_pins_s * pins)
{
    pins->sysclk = 0; // TX Led OFF
}


void nbfi_read_default_settings(nbfi_settings_t* settings)
{
  for(uint8_t i = 0; i != sizeof(nbfi_settings_t); i++)
  {
    ((uint8_t *)settings)[i] = ((uint8_t *)&nbfi_set_default)[i];
  }
}


#define EEPROM_INT_nbfi_data (DATA_EEPROM_BASE + 1024*5)

void  nbfi_read_flash_settings(nbfi_settings_t* settings) 
{
  memcpy((void*)settings, ((const void*)EEPROM_INT_nbfi_data), sizeof(nbfi_settings_t));
}

void nbfi_write_flash_settings(nbfi_settings_t* settings)
{
    if(HAL_FLASHEx_DATAEEPROM_Unlock() != HAL_OK) return;
    for(uint8_t i = 0; i != sizeof(nbfi_settings_t); i++)
    {
      if(HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_BYTE, EEPROM_INT_nbfi_data + i, ((uint8_t *)settings)[i]) != HAL_OK) break;
    }
    HAL_FLASHEx_DATAEEPROM_Lock(); 
}


uint32_t nbfi_measure_valtage_or_temperature(uint8_t val)
{
	return 0;
}

uint32_t nbfi_update_rtc()
{
  //you should use this callback when external RTC used
  //return rtc_counter;  
  return 0;
}

void nbfi_rtc_synchronized(uint32_t time)
{
  //you should use this callback for RTC counter correction when external RTC used
  //rtc_counter = time;
  
}


void nbfi_receive_complete(uint8_t * data, uint16_t length)
{

 // NBFi_Send(data, length); //loopback
  
}

uint8_t nbfi_lock = 0;

void nbfi_lock_unlock_nbfi_irq(uint8_t lock)
{
    nbfi_lock = lock;
}

void wa1205_init(void)
{
        RADIO_LPTIM_Init();       
        
        HAL_LPTIM_Counter_Start(&hlptim, 0xffff);
        
        RADIO_SPI_Init();
        
	wa1205_reg_func(WARADIO_ENABLE_GLOBAL_IRQ, (void*)wa1205_enable_global_irq);
	wa1205_reg_func(WARADIO_DISABLE_GLOBAL_IRQ, (void*)wa1205_disable_global_irq);
	wa1205_reg_func(WARADIO_ENABLE_IRQ_PIN, (void*)wa1205_enable_pin_irq);
	wa1205_reg_func(WARADIO_DISABLE_IRQ_PIN, (void*)wa1205_disable_pin_irq);
	wa1205_reg_func(WARADIO_GET_IRQ_PIN, (void*)wa1205_get_irq_pin_state);
	wa1205_reg_func(WARADIO_SPI_RX, (void*)wa1205_spi_rx);
	wa1205_reg_func(WARADIO_SPI_TX, (void*)wa1205_spi_tx);
	wa1205_reg_func(WARADIO_SPI_TX_RX, (void*)wa1205_spi_tx_rx);
	wa1205_reg_func(WARADIO_SPI_CS_WRITE, (void*)wa1205_spi_write_cs);

	HAL_GPIO_WritePin(SPI2_CS_PORT, SPI2_CS_PIN, GPIO_PIN_SET);

	wtimer_reg_func(WTIMER_GLOBAL_IRQ_ENABLE, (void*)wa1205_enable_global_irq);
	wtimer_reg_func(WTIMER_GLOBAL_IRQ_DISABLE, (void*)wa1205_disable_global_irq);
	wtimer_reg_func(WTIMER_CC_IRQ_ENABLE, (void*)wtimer_cc_irq_enable);
	wtimer_reg_func(WTIMER_CC_IRQ_DISABLE, (void*)wtimer_cc_irq_disable);
	wtimer_reg_func(WTIMER_SET_CC, (void*)wtimer_cc_set);
	wtimer_reg_func(WTIMER_GET_CC, (void*)wtimer_cc_get);
	wtimer_reg_func(WTIMER_GET_CNT, (void*)wtimer_cnt_get);
	wtimer_reg_func(WTIMER_CHECK_CC_IRQ, (void*)wtimer_check_cc_irq);

	wtimer_init();

	NBFI_reg_func(NBFI_BEFORE_TX, (void*)nbfi_before_tx);
	NBFI_reg_func(NBFI_BEFORE_RX, (void*)nbfi_before_rx);
        NBFI_reg_func(NBFI_BEFORE_OFF, (void*)nbfi_before_off);
	NBFI_reg_func(NBFI_RECEIVE_COMLETE, (void*)nbfi_receive_complete);
	NBFI_reg_func(NBFI_READ_FLASH_SETTINGS, (void*)nbfi_read_flash_settings);
	NBFI_reg_func(NBFI_WRITE_FLASH_SETTINGS, (void*)nbfi_write_flash_settings);
        NBFI_reg_func(NBFI_READ_DEFAULT_SETTINGS, (void*)nbfi_read_default_settings);
	NBFI_reg_func(NBFI_MEASURE_VOLTAGE_OR_TEMPERATURE, (void*)nbfi_measure_valtage_or_temperature);
        
        //register callbacks when external RTC used
        //NBFI_reg_func(NBFI_UPDATE_RTC, (void*)nbfi_update_rtc);
        //NBFI_reg_func(NBFI_RTC_SYNCHRONIZED, (void*)nbfi_rtc_synchronized);
        
	nbfi_dev_info_t info = {MODEM_ID, (uint32_t*)KEY, TX_MIN_POWER, TX_MAX_POWER, HW_ID, HW_REV, BAND, SEND_INFO_PERIOD};

	NBFi_Config_Set_Device_Info(&info);
        //NBFi_Clear_Saved_Configuration(); //if you need to clear previously saved nbfi configuration in EEPROM
	NBFI_Init(0);
  
}



