/**
  ******************************************************************************
  * @file    STM32F0xx_IAP/src/ymodem.c 
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    29-May-2012
  * @brief   Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "common.h"
#include "ymodem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <aos/aos.h>
#include <aos/yloop.h>
#include "netmgr.h"
#include "iot_export.h"
#include "iot_import.h"
#include "app_entry.h"
#include "hfilop/hfilop.h"


#include "hfat_cmd.h"
#include <hal/soc/flash.h>
#include <k_api.h>
#include "app_entry.h"

unsigned char *ymodem_transmit_buf=NULL;
aos_sem_t mcu_ota_sem;

MCU_IMAGE_HEADER mcu_file_info;//MCU 升级文件信息
unsigned int mcu_file_flash_offset=0;//MCU 升级文件在FLASH中的偏移地址

/**
  * @brief  Send a byte
  * @param  c: Character
  * @retval 0: Byte sent
  */
static uint32_t Send_Byte (uint8_t c)
{
    hfilop_uart_send_data(&c,1);
    return 0;
}

/**
  * @brief  Update CRC16 for input byte
  * @param  CRC input value 
  * @param  input byte
  * @retval Updated CRC value
  */
static uint16_t UpdateCRC16(uint16_t crcIn, uint8_t byte)
{
  uint32_t crc = crcIn;
  uint32_t in = byte|0x100;

  do
  {
    crc <<= 1;
    in <<= 1;

    if(in&0x100)
    {
      ++crc;
    }
    
    if(crc&0x10000)
    {
      crc ^= 0x1021;
    }
 } while(!(in&0x10000));

 return (crc&0xffffu);
}

/**
  * @brief  Cal CRC16 for YModem Packet
  * @param  data
  * @param  length
  * @retval CRC value
  */
static uint16_t Cal_CRC16(const uint8_t* data, uint32_t size)
{
  uint32_t crc = 0;
  const uint8_t* dataEnd = data+size;
  
  while(data<dataEnd)
  {
    crc = UpdateCRC16(crc,*data++);
  }
  crc = UpdateCRC16(crc,0);
  crc = UpdateCRC16(crc,0);

  return (crc&0xffffu);
}

/**
  * @brief  Prepare the first block
  * @param  timeout
  * @retval None
  */
void Ymodem_PrepareIntialPacket(uint8_t *data, const uint8_t* fileName, uint32_t *length)
{
    uint16_t tempCRC;
    int index =0;
    data[index++] = SOH;
    data[index++] = 0x00;
    data[index++] = 0xff;
    strcpy(data+index,fileName);
    index+=strlen(fileName);
    data[index++]=0;//文件名结束符

    char file_size_str[20];
    memset(file_size_str,0,sizeof(file_size_str));
    sprintf(file_size_str,"%d",*length);
    strcpy(data+index,file_size_str);
    index+=strlen(file_size_str);
    data[index++]=0;//文件长度对应16进制字符串结束符

    tempCRC = Cal_CRC16(data+PACKET_HEADER, PACKET_SIZE);
    data[PACKET_SIZE+3] = (tempCRC >> 8);
    data[PACKET_SIZE+4] = (tempCRC & 0xFF);
}

/**
  * @brief
  * @param
  * @retval package len
  */
int Ymodem_PreparePacket(uint8_t *SourceBuf, uint8_t *data, uint8_t pktNo, uint32_t sizeBlk)
{
    int index=0;
    if (sizeBlk >= PACKET_SIZE)
    {
        data[index++] = STX;
    }
    else
    {
        data[index++] = SOH;;
    }
    data[index++] = pktNo;
    data[index++] = (~pktNo);
    memcpy(data+index,SourceBuf,sizeBlk);

    int ota_data_size;
    if(sizeBlk >= PACKET_SIZE)
        ota_data_size=PACKET_1K_SIZE;
    else
        ota_data_size=PACKET_SIZE;
    unsigned short tempCRC = Cal_CRC16(&ymodem_transmit_buf[3], ota_data_size);
    
    ymodem_transmit_buf[ota_data_size+PACKET_HEADER]=(tempCRC >> 8);
    ymodem_transmit_buf[ota_data_size+PACKET_HEADER+1]=(tempCRC & 0xFF);
    return ota_data_size+PACKET_OVERHEAD;
}

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
int current_transmit_step=YMODEM_TRANSMIT_STEP_WAIT_C_SIGN;
bool ymodem_transmit_start_flag=true;

static bool send_ota_file_ack_received=false;
void Ymodem_Transmit_uart_data(char *data,int len)
{
    if(!ymodem_transmit_start_flag)
        return;

    static char recv_first_package_ack=0;
    static char recv_EOT_ack=0;
    unsigned char ca_sign=CA;
    
    if(current_transmit_step==YMODEM_TRANSMIT_STEP_WAIT_C_SIGN)
    {
        recv_first_package_ack=0;
        recv_EOT_ack=0;
        
        unsigned char wait_c_sign=CRC16;
        unsigned char recv_sign=(unsigned char)data[0];
        if(wait_c_sign==recv_sign)
        {
            current_transmit_step=YMODEM_TRANSMIT_STEP_SEND_FIRST_PACKAGE;
            aos_sem_signal(&mcu_ota_sem);
            return;
        }
        else if(ca_sign==recv_sign)
        {
            current_transmit_step=YMODEM_TRANSMIT_STEP_TERMINATION;
            aos_sem_signal(&mcu_ota_sem);
            return;
        }
    }
    else if(current_transmit_step==YMODEM_TRANSMIT_STEP_SEND_FIRST_PACKAGE)
    {
        int i;
        for(i=0;i<len;i++)
        {
            unsigned char wait_response_sign=ACK;
            unsigned char wait_response_sign1=CRC16;
            unsigned char recv_sign=(unsigned char)data[i];
            
            if(wait_response_sign == recv_sign)
            {
                recv_first_package_ack=1;
            }
            else if(wait_response_sign1 == recv_sign)
            {
                if(recv_first_package_ack==1)
                {
                    recv_first_package_ack=0;
                    current_transmit_step=YMODEM_TRANSMIT_STEP_SEND_OTA_FILE;
                    aos_sem_signal(&mcu_ota_sem);
                    return;
                }
            }
            else if(ca_sign==recv_sign)
            {
                current_transmit_step=YMODEM_TRANSMIT_STEP_TERMINATION;
                aos_sem_signal(&mcu_ota_sem);
                return;
            }
            
        }
    }
    else if(current_transmit_step==YMODEM_TRANSMIT_STEP_SEND_OTA_FILE)
    {
        unsigned char nak_sign=NAK;
        unsigned char wait_response_sign=ACK;
        unsigned char recv_sign=(unsigned char)data[0];
        if (wait_response_sign== recv_sign)
        {
            send_ota_file_ack_received=true;
            aos_sem_signal(&mcu_ota_sem);
            return;
        }
        else if(nak_sign==recv_sign)
        {
            aos_sem_signal(&mcu_ota_sem);
            return;
        }
        else if(ca_sign==recv_sign)
        {
            current_transmit_step=YMODEM_TRANSMIT_STEP_TERMINATION;
            aos_sem_signal(&mcu_ota_sem);
            return;
        }
    }
    else if(current_transmit_step==YMODEM_TRANSMIT_STEP_SEND_OTA_FILE_OVER)
    {
        unsigned char wait_response_sign=NAK;
        unsigned char recv_sign=(unsigned char)data[0];
        if (wait_response_sign== recv_sign)
        {
            current_transmit_step=YMODEM_TRANSMIT_STEP_SEND_OVER_CONFIRM;
            aos_sem_signal(&mcu_ota_sem);
            return;
        }
        else if(ca_sign==recv_sign)
        {
            current_transmit_step=YMODEM_TRANSMIT_STEP_TERMINATION;
            aos_sem_signal(&mcu_ota_sem);
            return;
        }
    }
    else if(current_transmit_step==YMODEM_TRANSMIT_STEP_SEND_OVER_CONFIRM)
    {

        int i;
        for(i=0;i<len;i++)
        {
            unsigned char wait_response_sign=ACK;
            unsigned char wait_response_sign1=CRC16;
            unsigned char wait_response_sign2=NAK;
            unsigned char recv_sign=(unsigned char)data[i];
            
            if(wait_response_sign == recv_sign)
            {
                recv_EOT_ack=1;
            }
            else if(wait_response_sign1 == recv_sign)
            {
                if(recv_EOT_ack==1)
                {
                    recv_EOT_ack=0;
                    current_transmit_step=YMODEM_TRANSMIT_STEP_SEND_FINISH_PACKAGE;
                    aos_sem_signal(&mcu_ota_sem);
                    return;
                }
            }
            else if(wait_response_sign2==recv_sign)
            {
                aos_sem_signal(&mcu_ota_sem);
                return;
            }
            else if(ca_sign==recv_sign)
            {
                current_transmit_step=YMODEM_TRANSMIT_STEP_TERMINATION;
                aos_sem_signal(&mcu_ota_sem);
                return;
            }
        }
    }
    else if(current_transmit_step==YMODEM_TRANSMIT_STEP_SEND_FINISH_PACKAGE)
    {
        unsigned char wait_sign=ACK;
        unsigned char wait_sign1=NAK;
        unsigned char recv_sign=(unsigned char)data[0];
        if(wait_sign==recv_sign)
        {
            current_transmit_step=YMODEM_TRANSMIT_STEP_OVER;
            aos_sem_signal(&mcu_ota_sem);
            return;
        }
        else if(wait_sign1==recv_sign)
        {
            aos_sem_signal(&mcu_ota_sem);
            return;
        }
        else if(ca_sign==recv_sign)
        {
            current_transmit_step=YMODEM_TRANSMIT_STEP_TERMINATION;
            aos_sem_signal(&mcu_ota_sem);
            return;
        }
    }
}

extern unsigned int mcu_file_flash_offset;
static void sent_mcu_ota_file(unsigned int off_set,unsigned char blkNumber,int pktSize)
{
    memset(ymodem_transmit_buf,0X1A,YMODEM_TRANSMIT_BUF_SIZE);
    int ota_data_size,index=0;
    if (pktSize >= PACKET_SIZE)
    {
        ymodem_transmit_buf[index++] = STX;
        ota_data_size=PACKET_1K_SIZE;
    }
    else
    {
        ymodem_transmit_buf[index++] = SOH;;
        ota_data_size=PACKET_SIZE;
    }
    ymodem_transmit_buf[index++] = blkNumber;
    ymodem_transmit_buf[index++] = (~blkNumber);

    unsigned int application_flash_offset=mcu_file_flash_offset+off_set;
    int ret=hal_flash_read(HAL_PARTITION_APPLICATION,&application_flash_offset,(unsigned char*)ymodem_transmit_buf+index,pktSize);

    unsigned short tempCRC = Cal_CRC16(&ymodem_transmit_buf[3], ota_data_size);
    
    ymodem_transmit_buf[ota_data_size+PACKET_HEADER]=(tempCRC >> 8);
    ymodem_transmit_buf[ota_data_size+PACKET_HEADER+1]=(tempCRC & 0xFF);

    int sent_len=ota_data_size+PACKET_OVERHEAD;
    LOG("YModem mcu.bin send(size %d), off_set:%d file_size:%d-->\r\n",pktSize,off_set,mcu_file_info.size);
    do_print_hex("sent_mcu_ota_file:",ymodem_transmit_buf,sent_len,"");
    hfilop_uart_send_data(ymodem_transmit_buf, sent_len);
}

void Ymodem_Transmit_process(void)
{
    #define SEM_WAIT_TIMEOUT_MAX_COUNT 15
    int ret,aos_sem_wait_timeout_count=0;
    unsigned int size=mcu_file_info.size;
    int pktSize=0;
    unsigned char blkNumber=0x01;
    uint16_t tempCRC;
    unsigned int off_set=0;
    int remaining_file_length;
    while(1)
    {
        ret=aos_sem_wait((aos_sem_t*)&mcu_ota_sem, 1000);
        if(ret!=RHINO_SUCCESS)
        {
            aos_sem_wait_timeout_count++;
            if(aos_sem_wait_timeout_count>SEM_WAIT_TIMEOUT_MAX_COUNT)
            {
                LOG("aos_sem_wait timeout more than %d times,stop mcu ota file transmit.\r\n",MAX_ERRORS);
                break;
            }
            continue;
        }
        else
            aos_sem_wait_timeout_count=0;
        
        if(current_transmit_step==YMODEM_TRANSMIT_STEP_WAIT_C_SIGN)
        {
            continue;
        }  
        else if(current_transmit_step==YMODEM_TRANSMIT_STEP_SEND_FIRST_PACKAGE)
        {
            memset(ymodem_transmit_buf,0,YMODEM_TRANSMIT_BUF_SIZE);
            Ymodem_PrepareIntialPacket(ymodem_transmit_buf,"ota.bin",&(mcu_file_info.size));
            do_print_hex("YModem start frame:",ymodem_transmit_buf,PACKET_SIZE + PACKET_OVERHEAD,"");
            hfilop_uart_send_data(ymodem_transmit_buf, PACKET_SIZE + PACKET_OVERHEAD);
            continue;
        }
        else if(current_transmit_step==YMODEM_TRANSMIT_STEP_SEND_OTA_FILE)
        {
            
            if(send_ota_file_ack_received==false)//no response  first ota file package
            {
                off_set-=pktSize;
                if(blkNumber>0x01)
                {
                    blkNumber--;
                }
                
                remaining_file_length=mcu_file_info.size-off_set;
                if (remaining_file_length >= PACKET_1K_SIZE)
                {
                    pktSize = PACKET_1K_SIZE;
                }
                else
                {
                    pktSize = remaining_file_length;
                }
                
                sent_mcu_ota_file(off_set,blkNumber,pktSize);
                off_set+=pktSize;
                blkNumber++;
                continue;
            }
            else
            {
                if(off_set>=mcu_file_info.size)
                {
                    current_transmit_step=YMODEM_TRANSMIT_STEP_SEND_OTA_FILE_OVER;
                    LOG("YModem mcu.bin send over -->\r\n");
                    Send_Byte(EOT);
                }
                else
                {
                    send_ota_file_ack_received=false;
                    remaining_file_length=mcu_file_info.size-off_set;
                    if (remaining_file_length >= PACKET_1K_SIZE)
                    {
                        pktSize = PACKET_1K_SIZE;
                    }
                    else
                    {
                        pktSize = remaining_file_length;
                    }
                    
                    sent_mcu_ota_file(off_set,blkNumber,pktSize);
                    off_set+=pktSize;
                    blkNumber++;
                }
                continue;
            }
        }
        else if(current_transmit_step==YMODEM_TRANSMIT_STEP_SEND_OVER_CONFIRM)
        {
            LOG("YModem mcu.bin send over confirm-->\r\n");
            Send_Byte(EOT);
        }
        else if(current_transmit_step==YMODEM_TRANSMIT_STEP_SEND_FINISH_PACKAGE)
        {
            memset(ymodem_transmit_buf,0,YMODEM_TRANSMIT_BUF_SIZE);
            ymodem_transmit_buf[0] = SOH;
            ymodem_transmit_buf[1] = 0;
            ymodem_transmit_buf [2] = 0xFF;
            tempCRC = Cal_CRC16(&ymodem_transmit_buf[3], PACKET_SIZE);
            ymodem_transmit_buf[PACKET_SIZE+3]=(tempCRC >> 8);
            ymodem_transmit_buf[PACKET_SIZE+4]=(tempCRC & 0xFF);

            LOG("YModem send last packet preparation -->\r\n");
            hfilop_uart_send_data(ymodem_transmit_buf, PACKET_SIZE + PACKET_OVERHEAD);
            continue;
        }
        else if(current_transmit_step==YMODEM_TRANSMIT_STEP_OVER)
        {
            LOG("YModem mcu_ota.bin trasmitted successfully -->\r\n");
            break;
        }
        else if(current_transmit_step==YMODEM_TRANSMIT_STEP_TERMINATION)
        {
            LOG("WarningYModem mcu_ota.bin trasmitted termination -->\r\n");
            break;
        }
    }
}
