
#include "usart.h"
#include "stm32f0xx.h"

//#include "stdio.h"

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"


#define USART_GPIO GPIOA
#define BUFFER_LENGTH 40	//length of TX, RX and command buffers
//#define USART_ECHO

//DMA_InitTypeDef DMA_InitStruct;
xQueueHandle usart_txq, usart_rxq;

static void hal_init(void);
static void irq_init(void);
static void tasks_init(void);

static void usart_tx_task(void *pvParameters);
static void usart_rx_task(void *pvParameters);




void USART_Config(void)
{
    hal_init();
    irq_init();
    tasks_init();

}

static void hal_init(void)
{
    //GPIO init: USART1 PA9 as OUT, PA10 as IN
    RCC_AHBPeriphClockCmd( RCC_AHBPeriph_GPIOA, ENABLE );
    GPIO_InitTypeDef port_init;
    port_init.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    port_init.GPIO_Mode = GPIO_Mode_AF;
    port_init.GPIO_Speed = GPIO_Speed_2MHz;
    port_init.GPIO_OType = GPIO_OType_PP;
    port_init.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init( GPIOA, &port_init );
    GPIO_PinAFConfig(GPIOA,  GPIO_PinSource9, GPIO_AF_1);
    GPIO_PinAFConfig(GPIOA,  GPIO_PinSource10, GPIO_AF_1);


    //USART init: USART1 9600 8n1
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    USART_InitTypeDef usart_init;
    usart_init.USART_BaudRate = 9600;
    usart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart_init.USART_Parity = USART_Parity_No;
    usart_init.USART_StopBits = USART_StopBits_1;
    usart_init.USART_WordLength = USART_WordLength_8b;
    usart_init.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
//    usart_init.USART_Mode = USART_Mode_Tx;

    USART_Init(USART1, &usart_init);
    USART_Cmd(USART1, ENABLE);

}

static void irq_init(void)
{
    NVIC_InitTypeDef nvic;

    nvic.NVIC_IRQChannel = USART1_IRQn;
    nvic.NVIC_IRQChannelPriority = 3;
    nvic.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&nvic);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

}

void USART1_IRQHandler(void)
{
    uint8_t data;
    static portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

    if (USART_GetITStatus(USART1, USART_IT_RXNE)==SET) {
	data = USART_ReceiveData(USART1);
	xQueueSendFromISR(usart_rxq, &data, &xHigherPriorityTaskWoken);
//	if (xHigherPriorityTaskWoken==pdTRUE)
//	    portSWITCH_CONTEXT();

	USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}

static void tasks_init(void)
{
    usart_txq = xQueueCreate(BUFFER_LENGTH,sizeof(uint8_t));
    usart_rxq = xQueueCreate(BUFFER_LENGTH,sizeof(uint8_t));

    xTaskCreate(usart_tx_task, (signed char *)"USART_T", configMINIMAL_STACK_SIZE, (void *)NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(usart_rx_task, (signed char *)"USART_R", configMINIMAL_STACK_SIZE, (void *)NULL, tskIDLE_PRIORITY + 1, NULL);
}

static void usart_tx_task(void *pvParameters)
{
    uint8_t data;
    for (;;) {
	if (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {
	    vTaskDelay(1);
	} else {
	    GPIO_ResetBits(GPIOC, GPIO_Pin_8);
//	    if (pdPASS == xQueueReceive(usart_txq, &data, portMAX_DELAY)) {
	    if (pdPASS == xQueueReceive(usart_txq, &data, 100)) {
		GPIO_SetBits(GPIOC, GPIO_Pin_8);
		USART_SendData(USART1, data);
	    }
	}
    }
    vTaskDelete(NULL);
}

static void usart_rx_task(void *pvParameters)
{
    uint8_t i=0;
    char data_buf[BUFFER_LENGTH];
    for (;;) {
	if (pdPASS == xQueueReceive(usart_rxq, &data_buf[i], portMAX_DELAY)) {
#ifdef USART_ECHO
	    USART_SendByte(data_buf[i]);
#endif
	    if (data_buf[i]=='\r') {
		data_buf[i+1]='\0';
		if (i) {
		    USART_SendString("\r\nReceived data: ");
		    USART_SendString(data_buf);
		};
#ifndef USART_ECHO
		USART_SendByte('\r');
#endif
		USART_SendByte('\n');
		i=0;
	    } else if (i<BUFFER_LENGTH-1)
		i++;
	}
    }
    vTaskDelete(NULL);
}



void USART_SendByte(uint8_t data)
{
//    xQueueSend(usart_txq, &data, 100/portTICK_RATE_MS);
    xQueueSend(usart_txq, &data, 100);
//    xQueueSend(usart_txq, &data, portMAX_DELAY);
}

void USART_SendString(char * str)
{
    while(* str) {
	USART_SendByte(*str);
	str++;
    }
}



