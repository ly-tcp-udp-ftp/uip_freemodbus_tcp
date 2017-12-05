#include "stm32f10x.h"
#include <stdio.h>

#include "uip.h"
#include "uip_arp.h"

#include "mb.h"
#include "mbutils.h"

#include "tapdev.h"
#include "enc28j60.h"	

#include "bsp_spi1.h"
#include "bsp_usart.h"
#include "timer.h"

#define LED1_ON()   GPIO_SetBits(GPIOB,GPIO_Pin_5)
#define LED1_OFF()  GPIO_ResetBits(GPIOB,GPIO_Pin_5)

#define LED2_ON()   GPIO_SetBits(GPIOD,GPIO_Pin_6)
#define LED2_OFF()  GPIO_ResetBits(GPIOD,GPIO_Pin_6)

#define LED3_ON()   GPIO_SetBits(GPIOD,GPIO_Pin_3)
#define LED3_OFF()  GPIO_ResetBits(GPIOD,GPIO_Pin_3)

#define REG_INPUT_START       0x0000                // ����Ĵ�����ʼ��ַ
#define REG_INPUT_NREGS       16                    // ����Ĵ�������

#define REG_HOLDING_START     0x0000                // ���ּĴ�����ʼ��ַ
#define REG_HOLDING_NREGS     16                    // ���ּĴ�������

#define REG_COILS_START       0x0000                // ��Ȧ��ʼ��ַ
#define REG_COILS_SIZE        16                    // ��Ȧ����

#define REG_DISCRETE_START    0x0000                // ���ؼĴ�����ʼ��ַ
#define REG_DISCRETE_SIZE     16                    // ���ؼĴ�������

// ����Ĵ�������
uint16_t usRegInputBuf[REG_INPUT_NREGS] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
// �Ĵ�����ʼ��ַ
uint16_t usRegInputStart = REG_INPUT_START;
// ���ּĴ�������
uint16_t usRegHoldingBuf[REG_HOLDING_NREGS] = {16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1};
// ���ּĴ�����ʼ��ַ
uint16_t usRegHoldingStart = REG_HOLDING_START;
// ��Ȧ״̬
uint8_t ucRegCoilsBuf[REG_COILS_SIZE / 8] = {0xFF, 0x00};
// ����״̬
uint8_t ucRegDiscreteBuf[REG_DISCRETE_SIZE / 8] = {0x00,0xFF};

#define BUF ((struct uip_eth_hdr *)&uip_buf[0])	

void GPIO_Config(void);
void led_poll(void);

int main(void)
{
    timer_typedef periodic_timer, arp_timer;
    uip_ipaddr_t ipaddr;
    
    /* TCP��ʱ���޸�Ϊ100ms */
    timer_set(&periodic_timer, CLOCK_SECOND / 10);
    timer_set(&arp_timer, CLOCK_SECOND * 10);
    
    /* IO�ڳ�ʼ�� ��Ҫ��Ϊ�˱���SPI�����ϵ������豸 */
	GPIO_Config();                     
    
	/* ����systic��Ϊ1ms�ж� */
    timer_config(); 
    /* ��ʼ��SPI1 */
    BSP_ConfigSPI1();
	
    /* ENC28J60��ʼ�� */
	tapdev_init();                     		 
	/* UIPЭ��ջ��ʼ�� */
	uip_init();		
    
    /* ����IP��ַ */
	uip_ipaddr(ipaddr, 192,168,1,15);	
	uip_sethostaddr(ipaddr);
    /* ����Ĭ��·����IP��ַ */
	uip_ipaddr(ipaddr, 192,168,1,1);		 
	uip_setdraddr(ipaddr);
    /* ������������ */
	uip_ipaddr(ipaddr, 255,255,255,0);		 
	uip_setnetmask(ipaddr);	
    
    // MODBUS TCP����Ĭ�϶˿� 502
    eMBTCPInit(MB_TCP_PORT_USE_DEFAULT);      
    eMBEnable();	
    
    BSP_ConfigUSART1();
    printf("\r\nuip start!\r\n");
    printf("ipaddr:192.168.1.15\r\n");
    
	while (1)
	{	
        eMBPoll();
        led_poll();
        
        /* �������豸��ȡһ��IP��,�������ݳ��� */
        uip_len = tapdev_read();
        /* �յ�����	*/
		if(uip_len > 0)			    
		{
			/* ����IP���ݰ� */
			if(BUF->type == htons(UIP_ETHTYPE_IP))
			{
				uip_arp_ipin();
				uip_input();
                
				if (uip_len > 0)
				{
					uip_arp_out();
					tapdev_send();
				}
			}
			/* ����ARP���� */
			else if (BUF->type == htons(UIP_ETHTYPE_ARP))
			{
				uip_arp_arpin();
				if (uip_len > 0)
				{
					tapdev_send();
				}
			}
		}
        
        /* 0.5�붨ʱ����ʱ */
        if(timer_expired(&periodic_timer))			
        {
            timer_reset(&periodic_timer);
            
            // GPIOD->ODR ^= GPIO_Pin_3;
            
            /* ����TCP����, UIP_CONNSȱʡ��10�� */
            for(uint8_t i = 0; i < UIP_CONNS; i++)
            {
                /* ����TCPͨ���¼� */
                uip_periodic(i);		
                if(uip_len > 0)
                {
                    uip_arp_out();
                    tapdev_send();
                }
            }
            
#if UIP_UDP
            /* ��������ÿ��UDP����, UIP_UDP_CONNSȱʡ��10�� */
            for(uint8_t i = 0; i < UIP_UDP_CONNS; i++)
            {
                uip_udp_periodic(i);	/*����UDPͨ���¼� */
                /* �������ĺ������õ�������Ӧ�ñ����ͳ�ȥ��ȫ�ֱ���uip_len�趨ֵ> 0 */
                if(uip_len > 0)
                {
                    uip_arp_out();
                    tapdev_send();
                }
            }
#endif /* UIP_UDP */
            
            /* ����ARP���� */
            if (timer_expired(&arp_timer))
            {
                timer_reset(&arp_timer);
                uip_arp_timer();
            }
        }
	}
}

/****************************************************************************
* ��    �ƣ�void GPIO_Configuration(void)
* ��    �ܣ�ͨ��IO������
* ��ڲ�������
* ���ڲ�������
* ˵    ����
* ���÷�����
****************************************************************************/  
void GPIO_Config(void)
{
    
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_USART1 | 
                            RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                            RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD |
                            RCC_APB2Periph_GPIOE, ENABLE);
    
    // ������������ݿ������޸ģ���ע��SPI�����ϵ������豸
    // LED1����
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;				     
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);					 
    
    // LED2, LED3����
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_3;		 
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    
    // ����������SPI1�����ϵ��豸
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;					 
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_7;		 
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // ��ֹSPI1�����ϵ������豸 �ǳ���Ҫ
    GPIO_SetBits(GPIOB, GPIO_Pin_7);    // ������оƬXPT2046 SPI Ƭѡ��ֹ  
    GPIO_SetBits(GPIOB, GPIO_Pin_12);   // VS1003 SPIƬѡ��ֹ 
    GPIO_SetBits(GPIOC, GPIO_Pin_4);    // SST25VF016B SPIƬѡ��ֹ  
    
    // ENC28J60��������ж����ţ�����δʹ��
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;	         	 	
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);		 
}

eMBErrorCode
eMBRegInputCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs )
{
    eMBErrorCode    eStatus = MB_ENOERR;
    int             iRegIndex;
    
    // ��ѯ�Ƿ��ڼĴ�����Χ��
    // Ϊ�˱��⾯�棬�޸�Ϊ�з�������
    if( ( (int16_t) usAddress >= REG_INPUT_START ) \
        && ( usAddress + usNRegs <= REG_INPUT_START + REG_INPUT_NREGS ) )
    {
        iRegIndex = ( int )( usAddress - usRegInputStart );
        while( usNRegs > 0 )
        {
            *pucRegBuffer++ = ( unsigned char )( usRegInputBuf[iRegIndex] >> 8 );
            *pucRegBuffer++ = ( unsigned char )( usRegInputBuf[iRegIndex] & 0xFF );
            iRegIndex++;
            usNRegs--;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }
    
    return eStatus;
}

eMBErrorCode
eMBRegHoldingCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs,
                eMBRegisterMode eMode )
{
    eMBErrorCode    eStatus = MB_ENOERR;
    int             iRegIndex;
    
    if( ( (int16_t)usAddress >= REG_HOLDING_START ) \
        && ( usAddress + usNRegs <= REG_HOLDING_START + REG_HOLDING_NREGS ) )
    {
        iRegIndex = ( int )( usAddress - usRegHoldingStart );
        switch ( eMode )
        {
        case MB_REG_READ:            
            while( usNRegs > 0 )
            {
                *pucRegBuffer++ = ( unsigned char )( usRegHoldingBuf[iRegIndex] >> 8 );
                *pucRegBuffer++ = ( unsigned char )( usRegHoldingBuf[iRegIndex] & 0xFF );
                iRegIndex++;
                usNRegs--;
            }
            break;
            
        case MB_REG_WRITE:
            while( usNRegs > 0 )
            {
                usRegHoldingBuf[iRegIndex] = *pucRegBuffer++ << 8;
                usRegHoldingBuf[iRegIndex] |= *pucRegBuffer++;
                iRegIndex++;
                usNRegs--;
            }
            break;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }
    
    return eStatus;
}


eMBErrorCode
eMBRegCoilsCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNCoils,
              eMBRegisterMode eMode )
{
    eMBErrorCode    eStatus = MB_ENOERR;
    short           iNCoils = ( short )usNCoils;
    unsigned short  usBitOffset;
    
    if( ( (int16_t)usAddress >= REG_COILS_START ) &&
       ( usAddress + usNCoils <= REG_COILS_START + REG_COILS_SIZE ) )
    {
        usBitOffset = ( unsigned short )( usAddress - REG_COILS_START );
        switch ( eMode )
        {
            
        case MB_REG_READ:
            while( iNCoils > 0 )
            {
                *pucRegBuffer++ = xMBUtilGetBits( ucRegCoilsBuf, usBitOffset,
                                                 ( unsigned char )( iNCoils > 8 ? 8 : iNCoils ) );
                iNCoils -= 8;
                usBitOffset += 8;
            }
            break;
            
        case MB_REG_WRITE:
            while( iNCoils > 0 )
            {
                xMBUtilSetBits( ucRegCoilsBuf, usBitOffset,
                               ( unsigned char )( iNCoils > 8 ? 8 : iNCoils ),
                               *pucRegBuffer++ );
                iNCoils -= 8;
                usBitOffset += 8;
            }
            break;
        }
        
    }
    else
    {
        eStatus = MB_ENOREG;
    }
    return eStatus;
}

eMBErrorCode
eMBRegDiscreteCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNDiscrete )
{
    eMBErrorCode    eStatus = MB_ENOERR;
    short           iNDiscrete = ( short )usNDiscrete;
    unsigned short  usBitOffset;
    
    if( ( (int16_t)usAddress >= REG_DISCRETE_START ) &&
       ( usAddress + usNDiscrete <= REG_DISCRETE_START + REG_DISCRETE_SIZE ) )
    {
        usBitOffset = ( unsigned short )( usAddress - REG_DISCRETE_START );
        
        while( iNDiscrete > 0 )
        {
            *pucRegBuffer++ = xMBUtilGetBits( ucRegDiscreteBuf, usBitOffset,
                                             ( unsigned char)( iNDiscrete > 8 ? 8 : iNDiscrete ) );
            iNDiscrete -= 8;
            usBitOffset += 8;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }
    return eStatus;
}

// ����LED����
void led_poll(void)
{
//    uint8_t led_state = ucRegCoilsBuf[0];
    
//    led_state & 0x01 ? LED1_ON():LED1_OFF();
//    led_state & 0x02 ? LED2_ON():LED2_OFF();
//    led_state & 0x04 ? LED3_ON():LED3_OFF();
}






