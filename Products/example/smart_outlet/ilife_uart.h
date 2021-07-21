#ifndef __ILIFE_UART_H__
#define __ILIFE_UART_H__

#include <aos/aos.h>
#include "stdio.h"
#include "iot_export_linkkit.h"
#include "cJSON.h"
#include "app_entry.h"
#include "hfilop/hfilop.h"
//#include "utils_sysinfo.h"
#include "iot_import_product.h"
#include "smart_outlet.h"
//#include "rda59xx_wifi_include.h"
#include "hal/wifi.h"
#include <hal/soc/flash.h>

#define ILIFE_RET_OK            1
#define ILIFE_RET_ERROR         0

#define STAND_HEADER            0xAA
#define STAND_TAIL              0x5B
#define PAYLOADLENOFFSET        3
#define UART_REC_TIMEOUT_MS     1000
#define MAX_UARTBUF_LEN         (1024)
#define UART_QUEUE_BUF_SIZE     (1024)
#define WAIT_QUEUE_BUF_SIZE     (512)
#define OTA_QUEUE_BUF_SIZE      (700)

#define UART_VER                1

#define OTA_WAIT_ACK            1
#define OTA_RECV_ACK            2
#define OTA_FAIL_ACK            3

#define MCU_OTA_DATA_OFFSET     200
#define MCU_OTA_DATA_MAX_SIZE   256*1024

#define MCU_OTA_CONFIG_NORMAL    0
#define MCU_OTA_CONFIG_NEED_WIFI 1

#define MCU_OTA_STATE_NULL      0
#define MCU_OTA_STATE_DOWNING   1
#define MCU_OTA_STATE_ALREADY   2
#define MCU_OTA_STATE_UPDATING  3

/* #define LOG_TRACE(...)                               \
    do {                                                     \
        HAL_Printf("\033[1;32;40m%s.%d: ", __func__, __LINE__);  \
        HAL_Printf(__VA_ARGS__);                                 \
        HAL_Printf("\033[0m\r\n");                                   \
    } while (0) */

#define LOG_TRACE_RED(...)                               \
    do {                                                     \
        HAL_Printf("\033[1;31;40m%s.%d: ", __func__, __LINE__);  \
        HAL_Printf(__VA_ARGS__);                                 \
        HAL_Printf("\033[0m\r\n");                                   \
    } while (0)

typedef struct
{
    uint16_t u16PktLen;
    uint16_t u16RecvLen;
    uint8_t  u8UartBuffer[MAX_UARTBUF_LEN];
}UartBuffer;

typedef enum {
    PKT_UNKNOWN = 0,
    PKT_PUREDATA,
} PKT_TYPE;

typedef enum {
    CMD_TO_CLOUD = 0x00,
    CMD_TO_MCU = 0x80
}CMD_TYPE;

typedef enum {
    ACK_OK = 200,
    ACK_ERR = 0
}ACK_TYPE;

typedef enum {
    METHOD_CLOUD_GET = 0x00,
    METHOD_CLOUD_SET = 0x01,
    METHOD_CLOUD_SERVICE = 0x02,
    METHOD_REPORT_ATTR = 0x80,
    METHOD_REPORT_EVENT = 0x81,
    METHOD_GET_WIFI_ATTR = 0xF0,
    METHOD_SET_WIFI_ATTR = 0xF1,
    METHOD_OTA_ACK_ATTR = 0xF2,
    METHOD_ACK = 0xFF
}METHOD_TYPE;

typedef enum {
    WIFI_STATE_SMART = 1,
    WIFI_STATE_AP = 2,
    WIFI_STATE_HOST = 3,
    WIFI_STATE_CON_ROUTER = 4,
    WIFI_STATE_DISCON_ROUTER = 5,
    WIFI_STATE_PSW_ERROR = 6,
    WIFI_STATE_CON_CLOUD = 7,
    WIFI_STATE_DISCON_CLOUD = 8,
    WIFI_STATE_CON_AP = 9,
    WIFI_STATE_POWER_OFF = 10,
    WIFI_STATE_POWER_ON = 11,
    WIFI_STATE_UNKNOW = 12
}WIFI_STATE;

typedef struct{
    uint8_t ver;
    uint8_t cmd;
    uint8_t method;
    uint16_t len;
    uint32_t id;
    uint8_t payload[25];
}SendData_t;

typedef enum {
    ID_PROP_BATTERYSTATE = 0,
    ID_PROP_WORKMODE = 1,
    ID_PROP_PAUSESWITCH = 2,
    ID_PROP_WIFI_BAND = 3,
    ID_PROP_WIFI_RSSI = 4,
    ID_PROP_WIFI_AP_BSSID = 5,
    ID_PROP_WIFI_CHANNEL = 6,
    ID_PROP_WIFI_SNR = 7,
    ID_PROP_POWERSWITCH = 8,
    ID_PROP_CLEANDIRECTION = 9,
    ID_PROP_SCHEDULE1 = 10,
    ID_PROP_SCHEDULE2 = 11,
    ID_PROP_SCHEDULE3 = 12,
    ID_PROP_SCHEDULE4 = 13,
    ID_PROP_SCHEDULE5 = 14,
    ID_PROP_SCHEDULE6 = 15,
    ID_PROP_SCHEDULE7 = 16,
    ID_PROP_MAXMODE = 17,
    ID_PROP_WATERTANKCONTRL = 18,
    ID_PROP_PARTSSTATUS = 19,
    ID_PROP_BEEPVOLUME = 20,    
    ID_PROP_BEEPTYPE = 21,
    ID_PROP_BEEPNODISTURB = 22,
    ID_PROP_FINDROBOT = 23,
    ID_PROP_CARPETCONTROL = 24,
    ID_PROP_CLEANTYPE = 25,
    ID_PROP_REALTIMEMAPSTART = 26,
    ID_PROP_REALTIMEMAP = 27,
    ID_PROP_CLEANHISTORYSTARTTIME = 28,
    ID_PROP_CLEANHISTORY = 29,
    ID_PROP_CHARGERPIONT = 30,
    ID_PROP_VIRTUALWALLEN = 31,
    ID_PROP_VIRTUALWALLDATA = 32,
    ID_PROP_PARTITIONDATA = 33,
    ID_PROP_WIFIINFO_CLOUD = 34,
    ID_PROP_ROBOTINFO = 35,
    ID_PROP_OTAINFO = 36,
    ID_PROP_TIMEZONE = 37,
    ID_PROP_RESETFACTORY = 38,

    NUM_ID_PROP,

    ID_PROP_WIFISTATE = 200,
    ID_PROP_UTC = 201,
    ID_PROP_UNBANDING = 202,
    ID_PROP_FACTORYTEST = 203,
    ID_PROP_WIFIINFO_MCU = 204,
    ID_PROP_WIFI_ACTIVE_CODE = 205,
    ID_PROP_OTA_START_CODE = 206,
    ID_PROP_OTA_END_CODE = 207,
    ID_PROP_OTA_DATA_CODE = 208,
    ID_PROP_OTA_OK_CODE = 209,
    ID_PROP_OTA_ERROR_CODE = 210,
    ID_PROP_OTA_REQ_CODE = 211
} property_id;

typedef enum {
/* 基本数据类型 */
    TYPE_ID_BOOL            = 0,    /* 布尔型：取值为0或1 */
    TYPE_ID_INT8            = 1,    /* 整数形：取值范围 -128 ~ 127 */
    TYPE_ID_UINT8           = 2,    /* 整数形：取值范围 0 ~ 255 */
    TYPE_ID_INT16           = 3,    /* 整数形：取值范围 -32768 ~ 32767 */
    TYPE_ID_UINT16          = 4,    /* 整数形：取值范围 0 ~ 65535 */
    TYPE_ID_INT32           = 5,    /* 整数型 */
    TYPE_ID_UINT32          = 6,
    TYPE_ID_INT64           = 7,
    TYPE_ID_UINT64          = 8,
    TYPE_ID_FLOAT32         = 9,    /* 浮点型 */
    TYPE_ID_FLOAT64         = 10,
    TYPE_ID_STRING          = 11,   /* 字符串型 */
    TYPE_ID_DATE            = 12,   /* 时间类型 */

/* 扩展数据类型 */
    TYPE_ID_ARRAY_BOOL      = 16,
    TYPE_ID_ARRAY_INT8      = 17,
    TYPE_ID_ARRAY_UINT8     = 18,
    TYPE_ID_ARRAY_INT16     = 19,
    TYPE_ID_ARRAY_UINT16    = 20,
    TYPE_ID_ARRAY_INT32     = 21,
    TYPE_ID_ARRAY_UINT32    = 22,
    TYPE_ID_ARRAY_INT64     = 23,
    TYPE_ID_ARRAY_UINT64    = 24,
    TYPE_ID_ARRAY_FLOAT32   = 25,
    TYPE_ID_ARRAY_FLOAT64   = 26,
    TYPE_ID_ARRAY_STRING    = 27,
/* 用户定义的结构体类型ID */
    TYPE_ID_STRU_SCHEDULE1 = 30,
    TYPE_ID_STRU_SCHEDULE2 = 31,
    TYPE_ID_STRU_SCHEDULE3 = 32,
    TYPE_ID_STRU_SCHEDULE4 = 33,
    TYPE_ID_STRU_SCHEDULE5 = 34,
    TYPE_ID_STRU_SCHEDULE6 = 35,
    TYPE_ID_STRU_SCHEDULE7 = 36,
    TYPE_ID_STRU_PARTSSTATUS = 37,
    TYPE_ID_STRU_REALTIMEMAP = 38,
    TYPE_ID_STRU_CLEANHISTORY = 39,
    TYPE_ID_STRU_VIRTUALWALLDATA = 40,
    TYPE_ID_STRU_PARTITIONDATA = 41,
    TYPE_ID_STRU_BEEPNODISTURB = 42,
    TYPE_ID_STRU_CHARGERPIONT = 43,
    TYPE_ID_STRU_WIFIINFO = 44,
    TYPE_ID_STRU_ROBOTINFO = 45,
    TYPE_ID_STRU_OTAINFO = 46,
    TYPE_ID_STRU_TIMEZONE = 47,

    NUM_TYPE_ID
} data_type_t;

 #define   ID_TYPE_BATTERYSTATE             TYPE_ID_FLOAT64
 #define   ID_TYPE_WORKMODE                 TYPE_ID_UINT8
 #define   ID_TYPE_PAUSESWITCH              TYPE_ID_BOOL
 #define   ID_TYPE_WIFI_BAND                0
 #define   ID_TYPE_WIFI_RSSI                0
 #define   ID_TYPE_WIFI_AP_BSSID            0
 #define   ID_TYPE_WIFI_CHANNEL             0
 #define   ID_TYPE_WIFI_SNR                 0
 #define   ID_TYPE_POWERSWITCH              TYPE_ID_BOOL
 #define   ID_TYPE_CLEANDIRECTION           TYPE_ID_UINT8
 #define   ID_TYPE_SCHEDULE1                TYPE_ID_STRU_SCHEDULE1
 #define   ID_TYPE_SCHEDULE2                TYPE_ID_STRU_SCHEDULE2
 #define   ID_TYPE_SCHEDULE3                TYPE_ID_STRU_SCHEDULE3
 #define   ID_TYPE_SCHEDULE4                TYPE_ID_STRU_SCHEDULE4
 #define   ID_TYPE_SCHEDULE5                TYPE_ID_STRU_SCHEDULE5
 #define   ID_TYPE_SCHEDULE6                TYPE_ID_STRU_SCHEDULE6
 #define   ID_TYPE_SCHEDULE7                TYPE_ID_STRU_SCHEDULE7
 #define   ID_TYPE_MAXMODE                  TYPE_ID_UINT8
 #define   ID_TYPE_WATERTANKCONTRL          TYPE_ID_UINT8
 #define   ID_TYPE_PARTSSTATUS              TYPE_ID_STRU_PARTSSTATUS
 #define   ID_TYPE_BEEPVOLUME               TYPE_ID_UINT8
 #define   ID_TYPE_BEEPTYPE                 TYPE_ID_UINT8
 #define   ID_TYPE_BEEPNODISTURB            TYPE_ID_STRU_BEEPNODISTURB
 #define   ID_TYPE_FINDROBOT                TYPE_ID_BOOL
 #define   ID_TYPE_CARPETCONTROL            TYPE_ID_UINT8
 #define   ID_TYPE_CLEANTYPE                TYPE_ID_UINT8
 #define   ID_TYPE_REALTIMEMAPSTART         TYPE_ID_INT32
 #define   ID_TYPE_REALTIMEMAP              TYPE_ID_STRU_REALTIMEMAP
 #define   ID_TYPE_CLEANHISTORYSTARTTIME    TYPE_ID_INT32
 #define   ID_TYPE_CLEANHISTORY             TYPE_ID_STRU_CLEANHISTORY
 #define   ID_TYPE_CHARGERPIONT             TYPE_ID_STRU_CHARGERPIONT
 #define   ID_TYPE_VIRTUALWALLEN            TYPE_ID_BOOL
 #define   ID_TYPE_VIRTUALWALLDATA          TYPE_ID_STRU_VIRTUALWALLDATA
 #define   ID_TYPE_PARTITIONDATA            TYPE_ID_STRU_PARTITIONDATA
 #define   ID_TYPE_WIFIINFO                 TYPE_ID_STRU_WIFIINFO
 #define   ID_TYPE_ROBOTINFO                TYPE_ID_STRU_ROBOTINFO
 #define   ID_TYPE_OTAINFO                  TYPE_ID_STRU_OTAINFO
 #define   ID_TYPE_TIMEZONE                 TYPE_ID_STRU_TIMEZONE
 #define   ID_TYPE_RESETFACTORY             TYPE_ID_BOOL
 #define   ID_TYPE_WIFISTATE                TYPE_ID_UINT8
 #define   ID_TYPE_UTC                      TYPE_ID_STRING
 #define   ID_TYPE_UNBANDING                TYPE_ID_BOOL
 #define   ID_TYPE_FACTORYTEST              TYPE_ID_UINT8
//  #define   ID_TYPE_WIFIINFO                 TYPE_ID_STRU_WIFIINFO


typedef enum {
    ID_EVEN_ERROR = 0,

    NUM_ID_EVEN
} event_id;

typedef enum {

    NUM_ID_SERV
} service_id;

typedef struct {
    uint32_t update_status;
    uint32_t target_ver;
    uint32_t file_offset;
    uint32_t file_size;
    uint8_t file_md5[16];
} ilife_ota_info_t;

extern ilife_ota_info_t ilife_ota_info;

extern aos_queue_t g_uart_recv_queue;
extern char uart_recv_queue_buf[UART_QUEUE_BUF_SIZE];
extern aos_queue_t g_wait_wake_queue;
extern char wait_wake_queue_buf[WAIT_QUEUE_BUF_SIZE];
extern aos_queue_t g_ota_data_queue;
extern char ota_recv_queue_buf[OTA_QUEUE_BUF_SIZE];
extern uint8_t g_otaAckStatus;
extern uint8_t mcu_ota_config;
extern uint8_t mcu_ota_state;

uint8_t ilife_check_sum(uint8_t * Buffer,uint32_t len);
void ilife_uart_recive_data(uint8_t *data, uint32_t len);
void ilife_uart_send_data(uint8_t *data, uint32_t len);
//static void ilife_uart_progess(void *p);
void ilife_send_data_to_mcu(SendData_t *data);
void ilife_send_wifi_state(WIFI_STATE state);
void ilife_upload_wifi_info_to_cloud(void);
//static void ilife_upload_wifi_info(void *arg);
void ilife_send_ack(uint32_t id, ACK_TYPE ack);
int ilife_send_ota_data(uint8_t attr, uint32_t id, ilife_ota_info_t ota_info, uint8_t *buf, uint16_t len);
void ilife_mcu_ota_task(void *p);
#endif
