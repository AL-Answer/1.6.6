/**
  ******************************************************************************
  * @file    STM32F0xx_IAP/inc/ymodem.h 
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    29-May-2012
  * @brief   Header for main.c module
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2012 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _YMODEM_H_
#define _YMODEM_H_
#include <aos/aos.h>
/* Includes ------------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
#define PACKET_SEQNO_INDEX      (1)
#define PACKET_SEQNO_COMP_INDEX (2)

#define PACKET_HEADER           (3)
#define PACKET_TRAILER          (2)
#define PACKET_OVERHEAD         (PACKET_HEADER + PACKET_TRAILER)
#define PACKET_SIZE             (128)
#define PACKET_1K_SIZE          (1024)

#define FILE_NAME_LENGTH        (256)
#define FILE_SIZE_LENGTH        (16)

#define SOH                     (0x01)  /* start of 128-byte data packet */
#define STX                     (0x02)  /* start of 1024-byte data packet */
#define EOT                     (0x04)  /* end of transmission */
#define ACK                     (0x06)  /* acknowledge */
#define NAK                     (0x15)  /* negative acknowledge */
#define CA                      (0x18)  /* two of these in succession aborts transfer */
#define CRC16                   (0x43)  /* 'C' == 0x43, request 16-bit CRC */

#define ABORT1                  (0x41)  /* 'A' == 0x41, abort by user */
#define ABORT2                  (0x61)  /* 'a' == 0x61, abort by user */

#define MAX_ERRORS              (5)

#define YMODEM_TRANSMIT_BUF_SIZE (PACKET_1K_SIZE + PACKET_OVERHEAD)

#define MCU_IMAGE_MAGIC 0XACCF
#define FIRMWARE_MAGIC      0xEAEA
#define VERSION_SZ          24

#pragma pack(push)
#pragma pack(1)
typedef struct _mcu_image_header
{
    uint16_t mcu_magic;
    uint32_t mcu_ver;
    uint32_t crc32;
    uint32_t size;
}MCU_IMAGE_HEADER;

//firmware_info结构体必须和Rda5981_bootloader中的一致
typedef struct firmware_info {
    uint32_t magic;
    uint8_t  version[VERSION_SZ];
    uint32_t addr;
    uint32_t size;
    uint32_t crc32;
    uint32_t bootaddr;//add for rf_test
    uint32_t bootmagic;
}FIRMWARE_INFO;
#pragma pack(pop)

typedef enum _YMODEM_TRANSMIT_STEP
{
    YMODEM_TRANSMIT_STEP_WAIT_C_SIGN =0,
    YMODEM_TRANSMIT_STEP_SEND_FIRST_PACKAGE =1,
    YMODEM_TRANSMIT_STEP_SEND_OTA_FILE  =2,
    YMODEM_TRANSMIT_STEP_SEND_OTA_FILE_OVER =3,
    YMODEM_TRANSMIT_STEP_SEND_OVER_CONFIRM =4,
    YMODEM_TRANSMIT_STEP_SEND_FINISH_PACKAGE=5,
    YMODEM_TRANSMIT_STEP_OVER,
    YMODEM_TRANSMIT_STEP_TERMINATION
}YMODEM_TRANSMIT_STEP;


extern MCU_IMAGE_HEADER mcu_file_info;
extern unsigned int mcu_file_flash_offset;
extern int current_transmit_step;

/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

void Ymodem_Transmit_uart_data(char *data,int len);
void Ymodem_Transmit_process(void);
void do_print_hex(char *head, unsigned char *data, int len, char *tail);


#endif  /* _YMODEM_H_ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
