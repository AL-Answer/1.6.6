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
#include "mcu_ota.h"
#include "hfat_cmd.h"
#include "board.h"
#include "app_entry.h"

bool mcu_ota_start_flag=false;

#ifdef  SUPPORT_MCU_OTA
int get_wifi_upgrade_file_header(FIRMWARE_INFO *wifi_file_header)
{
	
    int offset=0;
    int ret=hal_flash_read(13,&offset,(unsigned char*)wifi_file_header,sizeof(FIRMWARE_INFO));
    if(ret==0)
    {
        //复用wifi_file_header->version来保存WIFI file size
        LOG("wifi_upgrade_file_header:-->\r\nmagic::%X\r\ncrc32:%X\r\npackage_size:%X\r\nwifi_size:%s\r\n",\
        wifi_file_header->magic,wifi_file_header->crc32,wifi_file_header->size,wifi_file_header->version);
    }
    else
        LOG("get_wifi_upgrade_file_header failed(ret=%d).\r\n",ret);

    return ret;
}

int get_mcu_upgrade_file_header(MCU_IMAGE_HEADER *mcu_file_info)
{
    int ret=-1;
    FIRMWARE_INFO wifi_file_header;
    memset(&wifi_file_header,0,sizeof(FIRMWARE_INFO));
    if(get_wifi_upgrade_file_header(&wifi_file_header)!=0)
        return ret;

    if(wifi_file_header.magic != FIRMWARE_MAGIC)
    {
        LOG("wifi_file_header.magic(%X) error\r\n",wifi_file_header.magic);
        return ret;
    }
    
    //复用wifi_file_header->version来保存WIFI file size
    unsigned int offset=atoi(wifi_file_header.version);
    ret=hal_flash_read(HAL_PARTITION_APPLICATION,&offset,(unsigned char*)mcu_file_info,sizeof(MCU_IMAGE_HEADER));
    if(ret==0)
    {
        // mcu_file_flash_offset=offset+sizeof(MCU_IMAGE_HEADER);
        mcu_file_flash_offset=offset;
		HFILOP_PRINTF("111--offset=%d-MCU_IMAGE_HEADER=%d--\r\n",offset,sizeof(MCU_IMAGE_HEADER));
        LOG("mcu_upgrade_file_header:---->\r\nmagic::%08X\r\ncrc32:%08X\r\nsize:%08X\r\nmcu_ver:%d\r\n",\
        mcu_file_info->mcu_magic,mcu_file_info->crc32,mcu_file_info->size,mcu_file_info->mcu_ver);
		HFILOP_PRINTF("mcu_file_flash_offset=%d---\r\n",mcu_file_flash_offset);
    }
    else
        LOG("get_mcu_upgrade_file_header failed(ret=%d).\r\n",ret);
    
    return ret;
}

int send_mcu_upgrade_file_ver(void)
{
   /* memset(&mcu_file_info,0,sizeof(MCU_IMAGE_HEADER));
    if(get_mcu_upgrade_file_header(&mcu_file_info)!=0)
        return -1;
    
    if(mcu_file_info.mcu_magic!=MCU_IMAGE_MAGIC)
    {
        LOG("mcu_file_info.mcu_magic(%X) error\r\n",mcu_file_info.mcu_magic);
        return -1;
    }
    unsigned char buf[100];
    memset(buf,0,sizeof(buf));
    sprintf(buf,"AT+MCUOTA=%d\r\n\r\n",mcu_file_info.mcu_ver);
    hfilop_uart_send_data(buf,strlen(buf));
    return 0;*/
}


static void mcu_ota_thread(void)
{
    ymodem_transmit_buf=(unsigned char*)aos_malloc(YMODEM_TRANSMIT_BUF_SIZE);
    if(ymodem_transmit_buf==NULL)
    {
        LOG("ymodem_transmit_buf aos_malloc failed ,mcu_ota_thread exit\r\n");
        goto loop;
    }
    
    extern char m2m_app_state;
    m2m_app_state=0x00;
    Ymodem_Transmit_process();

    loop:  
    mcu_ota_start_flag=false;
    current_transmit_step=YMODEM_TRANSMIT_STEP_WAIT_C_SIGN;
    //m2m_app_state=M2M_STATE_RUN_CMD;
    aos_sem_free(&mcu_ota_sem);
    if(ymodem_transmit_buf!=NULL)
        aos_free(ymodem_transmit_buf);
    
    aos_task_exit(0);
}

void mcu_ota_thread_start(void)
{
  /*  if(mcu_ota_start_flag)
    {
        LOG("mcu_ota_thread have start.\r\n");
        return;
    }
    mcu_ota_start_flag=true;
    aos_sem_new(&mcu_ota_sem,1);
    aos_task_new("mcu_ota_thread", mcu_ota_thread, NULL, 2048);//--20180911*/
}

#endif

