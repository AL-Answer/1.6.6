
#ifndef HFILOP_HEAD_H
#define HFILOP_HEAD_H

#include <stdlib.h>
#include <stdarg.h>
#include "hal/wifi.h"
#include "hal/soc/uart.h"
#include <hal/soc/gpio.h>

#define HFILOP_VERSION "2M_V1.05-20210625"


#define HFILOP_PRINTF(...)  \
    do {                                                     \
        printf("\e[0;32m%s@line%d:\t", __FUNCTION__, __LINE__);  \
        printf(__VA_ARGS__);                                 \
        printf("\e[0m");                                   \
    } while (0)


enum HFILOP_MODULE_TYPE
{
	HFILOP_MODULE_LPT270=1,
	HFILOP_MODULE_LPT170,
	HFILOP_MODULE_LPT570,
	HFILOP_MODULE_LPT271,
	HFILOP_MODULE_LPB170,
};


typedef int (*data_callback_t)(void *data, uint32_t len);

typedef struct _at_session
{
}at_session_t,*pat_session_t;

typedef struct _at_cmd
{
	const char * name;
	int	(*func)(pat_session_t,int, char** ,char *,int);
	const char * doc;
	int	(*callhook)(pat_session_t,int, char** ,char *,int);
} hfat_cmd_t,*phfat_cmd_t;



/**
 * @brief init RF type.
 *
 * @param[in] type: module type, refer enum HFILOP_MODULE_TYPE
 * @return[out] none
 * @see None.
 * @note None.
 */
//void hfilop_init_rf_type(enum HFILOP_MODULE_TYPE type);

/**
 * @brief start uart task.
 *
 * @param[in] cb: uart data callback
 *			 cmd: cmd table
 * @return[out] none
 * @see None.
 * @note None.
 */
void hfilop_uart_task_start(data_callback_t cb, phfat_cmd_t cmd);

/**
 * @brief send uart data.
 *
 * @param[in] data: send data
 *			 len: the length of data, in bytes
 * @return[out] the data length of send success
 * @see None.
 * @note None.
 */
int hfilop_uart_send_data(unsigned char *data, int len);

/**
 * @brief recv uart data.
 *
 * @param[in] data: a pointer to a buffer to recv data
 *			 len: the length, in bytes, of the data pointed to by the data parameter
 *			 timeouts: recv timeout, in millisecond
 * @return[out] the data length of send success
 * @see None.
 * @note None.
 */
int hfilop_uart_recv_data(unsigned char *data, int len, int timeout);

/**
 * @brief check is in cmd mode.
 *
 * @param[in] none
 * @return[out] 1-in cmd mode, 0-not in cmd mode
 * @see None.
 * @note None.
 */
int hfilop_uart_in_cmd_mode(void);

/**
 * @brief start assis thread.
 *
 * @param[in] none
 * @return[out] none
 * @see None.
 * @note None.
 */
void hfilop_assis_task_start(void);

/**
 * @brief check local OTA state.
 *
 * @param[in] none
 * @return[out] none
 * @see None.
 * @note None.
 */
void hfilop_check_ota_state(void);

/**
 * @brief start local OTA.
 *
 * @param[in] none
 * @return[out] flag
 * @see None.
 * @note None.
 */
void hfilop_ota_task_start(int flag);

/**
 * @brief automatically upgrade after connect to specified router.
 *
 * @param[in] none
 * @return[out] ssid: the ssid of specified router
 *			 key: the key of specified router, "" or NULL means no key
 * @see if ssid==NULL and pwd==NULL, will use function 'hfilop_ota_auto_upgrade_is_valid' 
 *                   and 'hfilop_ota_auto_upgrade_get_ssid' and 'hfilop_ota_auto_upgrade_get_pwd'.
 * @note will block in this function forever.
 */
void hfilop_ota_auto_upgrade(char *ssid, char *pwd);

/**
 * @brief OTA auto upgrade flag.
 *
 * @param[in] none
 * @return[out] 1-need auto upgrade, 0-not need, default is 0
 * @see None.
 * @note if you need this function, please define it yourself.
 */
int hfilop_ota_auto_upgrade_is_valid(void);

/**
 * @brief OTA auto upgrade ssid name.
 *
 * @param[in] none
 * @return[out] ssid name
 * @see None.
 * @note if you need this function, please define it yourself.
 */
char *hfilop_ota_auto_upgrade_get_ssid(void);

/**
 * @brief OTA auto upgrade password.
 *
 * @param[in] none
 * @return[out] password
 * @see None.
 * @note if you need this function, please define it yourself.
 */
char *hfilop_ota_auto_upgrade_get_pwd(void);

/**
 * @brief OTA success callback function.
 *
 * @param[in] none
 * @return[out] none
 * @see None.
 * @note if you need this function, please define it yourself.
 */
void hfilop_ota_success_callback(void);

/**
 * @brief OTA fail callback function.
 *
 * @param[in] none
 * @return[out] none
 * @see None.
 * @note if you need this function, please define it yourself.
 */
void hfilop_ota_fail_callback(void);


typedef int (*wifi_scan_callback_t)(ap_list_adv_t *);

/**
 * @brief wifi scan.
 *
 * @param[in] cb: pass ssid info(scan result) to this callback one by one
 * @return[out] the number of wifi
 * @see None.
 * @note None.
 */
int hfilop_wifi_scan(wifi_scan_callback_t cb);

/**
 * @brief get MAC address in STA mode.
 *
 * @param[in] none
 * @return[out] MAC address
 * @see None.
 * @note None.
 */
unsigned char *hfilop_layer_get_mac(void);

/**
 * @brief get hostname(MID).
 *
 * @param[in] none
 * @return[out] hostname
 * @see None.
 * @note None.
 */
char *hfilop_layer_get_hostname(void);

/**
 * @brief get ILOP product key.
 *
 * @param[in] none
 * @return[out] product key
 * @see None.
 * @note None.
 */
char *hfilop_layer_get_product_key(void);

/**
 * @brief get ILOP product secret.
 *
 * @param[in] none
 * @return[out] product secret
 * @see None.
 * @note None.
 */
char *hfilop_layer_get_product_secret(void);

/**
 * @brief get ILOP device name.
 *
 * @param[in] none
 * @return[out] device name
 * @see None.
 * @note None.
 */
char *hfilop_layer_get_device_name(void);

/**
 * @brief get ILOP device secret.
 *
 * @param[in] none
 * @return[out] device secret
 * @see None.
 * @note None.
 */
char *hfilop_layer_get_device_secret(void);

/**
 * @brief set hostname.
 *
 * @param[in] hostname, maxnum is 20 bytes
 * @return[out] 0-successfully, other value is failed
 * @see None.
 * @note None.
 */
int hfilop_layer_set_hostname(char * hostname);

/**
 * @brief set product key.
 *
 * @param[in] product key, maxnum is 64 bytes
 * @return[out] 0-successfully, other value is failed
 * @see None.
 * @note None.
 */
int hfilop_layer_set_product_key(char * product_key);

/**
 * @brief set product secret.
 *
 * @param[in] product secret, maxnum is 64 bytes
 * @return[out] 0-successfully, other value is failed
 * @see None.
 * @note None.
 */
int hfilop_layer_set_product_secret(char * product_secret);

/**
 * @brief set device name.
 *
 * @param[in] device name, maxnum is 64 bytes
 * @return[out] 0-successfully, other value is failed
 * @see None.
 * @note None.
 */
int hfilop_layer_set_device_name(char * device_name);

/**
 * @brief set device secret.
 *
 * @param[in] device secret, maxnum is 64 bytes
 * @return[out] 0-successfully, other value is failed
 * @see None.
 * @note None.
 */
int hfilop_layer_set_device_secret(char * device_secret);

/**
 * @brief set uart baud.
 *
 * @param[in] baud
 * @return[out] 0-successfully, other value is failed
 * @see None.
 * @note None.
 */
int hf_uart_config(int baud,hal_uart_parity_t parity);

/**
 * @brief set wifi connect wifi when no have product secreta and device name device secret.
 *
 * @param[in] ssid and key
 * @return[out] 0-successfully, other value is failed
 * @see None.
 * @note None.
 */
int hf_wificonn(char *ssid, char *key);

/**
 * @brief set wifi connect wifi when have product secreta and device name device secret.
 *
 * @param[in] ssid and key
 * @return[out] 0-successfully, other value is failed
 * @see None.
 * @note None.
 */
int hf_set_wsssid(char *ssid, char *key);

/**
 * @brief get aliserver type.
 *
 * @param[in] none
 * @return[out] refer to enum linkkit_cloud_domain_type_t
 * @see None.
 * @note None.
 */
int hf_get_aliserver(void);

/**
 * @brief get ali key register type.
 *
 * @param[in] none
 * @return[out] 0: one machine one key, 1:one model one key
 * @see None.
 * @note None.
 */
int hf_get_alikeytype(void);

#define APPVERSION "NW7772_LPT570_Alios_v1.7_20210521"

#define USER_ILOP_SYS_CONFIG_INIT_FLAG 0XACAC1234

typedef enum _ILOP_MODE
{
    ILOP_MODE_PASS_THROUGH=0,
    ILOP_MODE_ICA,
    ILOP_MODE_JSON
}ILOP_MODE;
	
typedef enum _ILOP_AWSS
{
	ILOP_AWSS_MODE=0,
	ILOP_AWSS_DEV_AP_MODE,
	ILOP_AWSS_DEV_BLE_MODE
}ILOP_AWSS;
	
typedef enum _ILOP_CONNECT_STATUS
{
  
    WIFI_DISCONNECT=0,
     WIFI_CONNECT =1,
    SERVER_CONNECT,
    SERVER_DISCONNECT
}ILOP_CONNECT_STATUS;

/*enum ENWORKTMODE
{
	M2M_STATE_RUN_THROUGH = 0x00,
	M2M_STATE_RUN_CMD
};*/


#define TIME_ZONE_FLAG 0XCA
#pragma pack(push)
#pragma pack(1)
#pragma pack(pop)

enum AWSS_STATE
{
	AWSS_STATE_CLOSE,
	AWSS_STATE_OPEN,
};


extern ILOP_CONNECT_STATUS ilop_connect_status;
extern int hf_cmd_send_ica(pat_session_t s,int argc,char *argv[],char *rsp,int len);
extern int hf_cmd_send_js(pat_session_t s,int argc,char *argv[],char *rsp,int len);//20190527
extern int hf_cmd_send_raw(pat_session_t s,int argc,char *argv[],char *rsp,int len);
extern void do_awss_reset();
extern void linkkit_reset(void *p);
extern void do_print_hex(char *head, unsigned char *data, int len, char *tail);
extern int hf_get_product_id();
extern unsigned char hfsys_get_awss_state();
extern void hfsys_set_awss_state(unsigned char state);
extern void hfiolp_check_awss();
extern void hfsys_start_dms();
extern void down_dev_ap_awss_start();
extern void connect_succeed_ssid_passwd_copy(void);
extern void hfsys_ready_link_gpio_init(void);
extern int hfsys_device_status_light_timer_create(void);
extern void device_status_light_timer_callback(void *timer,void *args);
extern void network_connection_status_check_process(void);
extern int ilop_user_sys_config_init(void);
extern int ilop_connect_status_down(ILOP_CONNECT_STATUS connect_status);
extern void awss_config_sucess_event_down(void);
extern void hfsys_start_network_status_process();
extern int  ilop_server_data_down(ILOP_MODE ilop_mode,char *key,char *value_str);
extern void hfsys_set_link_low();
extern int hfat_send_cmd(char *cmd_line,int cmd_len,char *rsp,int len);
extern void hfsys_set_mtype(unsigned char type);




extern bool awss_config_press_start_flag;
extern gpio_dev_t GPIO_Link;
extern gpio_dev_t GPIO_Ready;
extern bool cloud_conn_status;

/*
 * Please define correct module type, refer enum HFILOP_MODULE_LPT270
 */

#define HF_MODULE_ID (HFILOP_MODULE_LPT270)
#endif /* HFILOP_HEAD_H */

