/*
 *copyright (C) 2019-2022 Alibaba Group Holding Limited
 */

#ifndef __SMART_OUTLET_METER_H__
#define __SMART_OUTLET_METER_H__

#include "aos/aos.h"
#include <hal/soc/gpio.h>
#define DEBUG_OUT   1

#ifdef DEBUG_OUT
#define LOG_TRACE(...)                               \
    do {                                                     \
        HAL_Printf("%s.%d: ", __func__, __LINE__);  \
        HAL_Printf(__VA_ARGS__);                                 \
        HAL_Printf("\r\n");                                   \
    } while (0)
#else
#define LOG_TRACE(...)
#endif

typedef struct {
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t wday;
    uint8_t day;
    uint8_t mon;
    uint16_t year;
} ilife_time_t;



extern gpio_dev_t gpio_wake_up;
//extern ilife_ota_info_t ilife_ota_info;
extern uint8_t mcu_update_state;

#define MAC_ADDRESS_SIZE    8
#define WIFI_STATE_WAKEUP   0   //
#define WIFI_STATE_SLEEP    1   //
#define WIFI_STATE_WAKEING  2   //

#define GPIO_WAKEUP_IO  14   //

extern uint8_t g_wifi_sleep_state;
extern uint8_t g_wifi_config;
extern uint32_t g_robot_type;


uint8_t ilife_covChar2value(char c);
uint32_t ilife_getMcuOtaVer(char *str);
int ilife_StringToHex(char *str, uint8_t *out, unsigned int *outlen);
uint8_t ilife_deal_robot_wakeup(uint8_t *data, uint32_t len);
void ilife_wifi_set_state(uint8_t state);
uint8_t ilife_wifi_get_state(void);
void ilife_get_time(uint32_t time_stamp, ilife_time_t *data);
//int ilife_send_ota_data(uint8_t attr, uint32_t id, ilife_ota_info_t ota_info, uint8_t *buf, uint16_t len);

typedef struct {
    uint8_t powerswitch;
    uint8_t all_powerstate;
} device_status_t;

typedef struct {
    int master_devid;
    int cloud_connected;
    int master_initialized;
    int bind_notified;
    device_status_t status;
} user_example_ctx_t;

user_example_ctx_t *user_example_get_ctx(void);
void user_post_powerstate(int powerstate);
void update_power_state(int state);
void example_free(void *ptr);

#endif
