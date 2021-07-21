
#ifndef _MCU_OTA_H_
#define _MCU_OTA_H_

#include "ymodem.h"
// open if support mcu ota
//#define SUPPORT_MCU_OTA



extern bool mcu_ota_start_flag;

extern aos_sem_t mcu_ota_sem;

extern unsigned char *ymodem_transmit_buf;

extern MCU_IMAGE_HEADER mcu_file_info;

int get_mcu_upgrade_file_header(MCU_IMAGE_HEADER *mcu_file_info);

void mcu_ota_thread_start(void);

int send_mcu_upgrade_file_ver(void);

#endif

