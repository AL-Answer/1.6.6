/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#ifndef __APP_ENTRY_H__
#define __APP_ENTRY_H__

#include <aos/aos.h>
#include "smart_outlet.h"
#include "ilife_uart.h"

#define ROBOT_TYPE "robot_type"
#define WIFI_CONFIG_STATE "wifi_config"
#define WIFI_CMD_ID "wifi_cmdid"
#define MCU_OTA_CONFIG "ota_config"
#define MCU_OTA_STATE "ota_state"

typedef struct {
    int argc;
    char **argv;
}app_main_paras_t;

int linkkit_main(void *paras);
void do_awss_reset(void);
void do_awss_reboot(void);
void do_awss(void);
void do_ble_awss(void);
void do_awss_dev_ap(void);
void stop_netmgr(void *p);
void start_netmgr(void *p);
void do_awss_ble_start(void);
#endif
