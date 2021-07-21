/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

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
#include "aos/kv.h"
#include "vendor.h"
#include "device_state_manger.h"
#include "smart_outlet.h"
#include "msg_process_center.h"
#include "property_report.h"
#include "hfilop/hfilop.h"
#include "hfat_cmd.h"
#include "ymodem.h"
#include "hal/wifi.h"
#include <hal/soc/flash.h>
#include <hal/soc/gpio.h>
#include "board.h"
#include "hfilop/hfilop.h"
#include "hfilop/hfilop_config.h"

#include "ilife_uart.h"

#ifdef SUPPORT_MCU_OTA
#include "hfat_cmd.h"
#include "mcu_ota.h"
#else
extern bool mcu_ota_start_flag;

#endif

#ifdef AOS_TIMER_SERVICE
#include "iot_export_timer.h"
#endif
#ifdef CSP_LINUXHOST
#include <signal.h>
#endif

#include <k_api.h>

#if defined(OTA_ENABLED) && defined(BUILD_AOS)
#include "ota_service.h"
#endif

#ifdef EN_COMBO_NET
#include "breeze_export.h"
#include "combo_net.h"
#endif

#include <hfilop/hfilop_ble.h>

static aos_task_t task_key_detect;
static aos_task_t task_msg_process;
static aos_task_t task_property_report;
static aos_task_t task_linkkit_reset;
static aos_task_t task_reboot_device;
char linkkit_started = 0;
aos_timer_t awss_config_timeout_timer;
static char awss_dev_ap_started = 0;

iotx_vendor_dev_reset_type_t ilife_reset_type = IOTX_VENDOR_DEV_RESET_TYPE_UNBIND_ONLY;


extern int init_awss_flag(void);
extern int HAL_Awss_Get_Timeout_Interval_Ms(void);


extern const hfat_cmd_t user_define_at_cmds_table[];



void print_heap()
{
    extern k_mm_head *g_kmm_head;
    int               free = g_kmm_head->free_size;
    LOG("============free heap size =%d==========", free);
}

#ifdef CONFIG_PRINT_HEAP
void print_heap()
{
    extern k_mm_head *g_kmm_head;
    int               free = g_kmm_head->free_size;
    LOG("============free heap size =%d==========", free);
}
#endif

static void wifi_service_event(input_event_t *event, void *priv_data)
{
    if (event->type != EV_WIFI) {
        return;
    }

    LOG("wifi_service_event(), event->code=%d", event->code);
    if (event->code == CODE_WIFI_ON_CONNECTED) {
        LOG("CODE_WIFI_ON_CONNECTED");
    } else if (event->code == CODE_WIFI_ON_DISCONNECT) {
        LOG("CODE_WIFI_ON_DISCONNECT");
#ifdef EN_COMBO_NET
        combo_set_ap_state(COMBO_AP_DISCONNECTED);
#endif
    } else if (event->code == CODE_WIFI_ON_CONNECT_FAILED) {
        LOG("CODE_WIFI_ON_CONNECT_FAILED");
    } else if (event->code == CODE_WIFI_ON_GOT_IP) {
        LOG("CODE_WIFI_ON_GOT_IP");
#ifdef EN_COMBO_NET
        combo_set_ap_state(COMBO_AP_CONNECTED);
#endif
    }

    if (event->code != CODE_WIFI_ON_GOT_IP) {
        return;
    }

    netmgr_ap_config_t config;
    memset(&config, 0, sizeof(netmgr_ap_config_t));
    netmgr_get_ap_config(&config);
    LOG("wifi_service_event config.ssid %s", config.ssid);
    if (strcmp(config.ssid, "adha") == 0 || strcmp(config.ssid, "aha") == 0) {
        // clear_wifi_ssid();
        return;
    }
    set_net_state(GOT_AP_SSID);
#ifdef EN_COMBO_NET
    combo_ap_conn_notify();
#endif

    if (!linkkit_started) {
#ifdef CONFIG_PRINT_HEAP
        print_heap();
#endif
#if (defined (TG7100CEVB))
        aos_task_new("linkkit", (void (*)(void *))linkkit_main, NULL, 1024 * 8);
#else
        aos_task_new("linkkit", (void (*)(void *))linkkit_main, NULL, 1024 * 6);
#endif
        linkkit_started = 1;
    }
}

static void cloud_service_event(input_event_t *event, void *priv_data)
{
    if (event->type != EV_YUNIO) {
        return;
    }

    LOG("cloud_service_event %d", event->code);

    if (event->code == CODE_YUNIO_ON_CONNECTED) {
        LOG("user sub and pub here");
		cloud_conn_status=true;//--20181029
        return;
    }

    if (event->code == CODE_YUNIO_ON_DISCONNECTED) {
        cloud_conn_status=false;//--20181029
    }
}

static void awss_config_net_timeout_event(void)
{
    static bool not_send_flag=true;
    if(not_send_flag)
    {
        not_send_flag=false;
        unsigned char buf[100];
        memset(buf,0,sizeof(buf));
        strcpy(buf,"+ILOPCONNECT=AWSS_TIMEOUT\r\n\r\n");
        if(!mcu_ota_start_flag && !g_hfilop_config.tmod)//--20190108
            hfilop_uart_send_data(buf,strlen(buf));
    }
}

static void awss_config_timeout_check_function(void *timer,void *args)
{
    if(awss_config_press_start_flag)//awss config timeout
    {
        if(strlen(g_hfilop_config.last_connect_ssid)>0)//Has the SSID that connected the router successfully
        {
            netmgr_ap_config_t config;
            memset(config.ssid,0,sizeof(config.ssid));
            memset(config.pwd,0,sizeof(config.pwd));
            strcpy(config.ssid,g_hfilop_config.last_connect_ssid);
            strcpy(config.pwd,g_hfilop_config.last_connect_key);
            netmgr_set_ap_config(&config);
	        aos_kv_set(NETMGR_WIFI_KEY, (unsigned char *)&config, sizeof(netmgr_ap_config_t), 1);
            
            unsigned char buf[100];
            memset(buf,0,sizeof(buf));
            strcpy(buf,"+ILOPCONNECT=AWSS_TIMEOUT\r\n\r\n");
            if(!mcu_ota_start_flag && !g_hfilop_config.tmod)//--20190108
                hfilop_uart_send_data(buf,strlen(buf));
            LOG("awss config timeout,connect last ssid(%s) key(%s).",g_hfilop_config.last_connect_ssid,g_hfilop_config.last_connect_key);
            aos_msleep(1000);
            aos_reboot();
        }
        else
        {
            unsigned char buf[100];
            memset(buf,0,sizeof(buf));
            strcpy(buf,"+ILOPCONNECT=AWSS_TIMEOUT\r\n\r\n");
            if(!mcu_ota_start_flag && !g_hfilop_config.tmod)//--20190108
                hfilop_uart_send_data(buf,strlen(buf));
            LOG("awss config timeout,linkkit_reset.");
            aos_msleep(1000);
            linkkit_reset(NULL);
            
        }
    }
    else
    {
        aos_timer_stop(&awss_config_timeout_timer);
        aos_timer_free(&awss_config_timeout_timer);
    }
}


int awss_config_timeout_check(void)
{
    int outtime_time_ms=HAL_Awss_Get_Timeout_Interval_Ms()-(10*1000);
    return aos_timer_new(&awss_config_timeout_timer,awss_config_timeout_check_function,NULL,outtime_time_ms,1);
}


/*
 * Note:
 * the linkkit_event_monitor must not block and should run to complete fast
 * if user wants to do complex operation with much time,
 * user should post one task to do this, not implement complex operation in
 * linkkit_event_monitor
 */

static void linkkit_event_monitor(int event)
{
    switch (event) {
        case IOTX_AWSS_START: // AWSS start without enbale, just supports device discover
            // operate led to indicate user
			do_awss_active();
            LOG("IOTX_AWSS_START");
            break;
        case IOTX_AWSS_ENABLE: // AWSS enable, AWSS doesn't parse awss packet until AWSS is enabled.
            LOG("IOTX_AWSS_ENABLE");
            // operate led to indicate user
            break;
        case IOTX_AWSS_LOCK_CHAN: // AWSS lock channel(Got AWSS sync packet)
            LOG("IOTX_AWSS_LOCK_CHAN");
            // operate led to indicate user
            break;
        case IOTX_AWSS_PASSWD_ERR: // AWSS decrypt passwd error
            LOG("IOTX_AWSS_PASSWD_ERR");
            // operate led to indicate user
            break;
        case IOTX_AWSS_GOT_SSID_PASSWD:
            LOG("IOTX_AWSS_GOT_SSID_PASSWD");
        if(awss_config_press_start_flag)//awss config success check 
        {
            awss_config_sucess_event_down();
            awss_config_press_start_flag=false;
			#ifdef SUPPORT_MCU_OTA
            aos_post_delayed_action(2000, send_mcu_upgrade_file_ver, NULL);
			#endif
        }
            // operate led to indicate user
            set_net_state(GOT_AP_SSID);
            break;
        case IOTX_AWSS_CONNECT_ADHA: // AWSS try to connnect adha (device
            // discover, router solution)
            LOG("IOTX_AWSS_CONNECT_ADHA");
            // operate led to indicate user
            break;
        case IOTX_AWSS_CONNECT_ADHA_FAIL: // AWSS fails to connect adha
            LOG("IOTX_AWSS_CONNECT_ADHA_FAIL");
            // operate led to indicate user
            break;
        case IOTX_AWSS_CONNECT_AHA: // AWSS try to connect aha (AP solution)
            LOG("IOTX_AWSS_CONNECT_AHA");
            // operate led to indicate user
            break;
        case IOTX_AWSS_CONNECT_AHA_FAIL: // AWSS fails to connect aha
            LOG("IOTX_AWSS_CONNECT_AHA_FAIL");
            // operate led to indicate user
            break;
        case IOTX_AWSS_SETUP_NOTIFY: // AWSS sends out device setup information
            // (AP and router solution)
            LOG("IOTX_AWSS_SETUP_NOTIFY");
            // operate led to indicate user
            break;
        case IOTX_AWSS_CONNECT_ROUTER: // AWSS try to connect destination router
            LOG("IOTX_AWSS_CONNECT_ROUTER");
            // operate led to indicate user
            break;
        case IOTX_AWSS_CONNECT_ROUTER_FAIL: // AWSS fails to connect destination
            // router.
            LOG("IOTX_AWSS_CONNECT_ROUTER_FAIL");
            set_net_state(CONNECT_AP_FAILED);
            ilop_connect_status_down(WIFI_DISCONNECT);
            ilop_connect_status=WIFI_DISCONNECT;
            // operate led to indicate user
            break;
        case IOTX_AWSS_GOT_IP: // AWSS connects destination successfully and got
            // ip address
            LOG("IOTX_AWSS_GOT_IP");
            // operate led to indicate user
            break;
        case IOTX_AWSS_SUC_NOTIFY: // AWSS sends out success notify (AWSS
            // sucess)
            LOG("IOTX_AWSS_SUC_NOTIFY");
            // operate led to indicate user
            break;
        case IOTX_AWSS_BIND_NOTIFY: // AWSS sends out bind notify information to
            // support bind between user and device
            LOG("IOTX_AWSS_BIND_NOTIFY");
            // operate led to indicate user
            user_example_ctx_t *user_example_ctx = user_example_get_ctx();
            user_example_ctx->bind_notified = 1;
            break;
        case IOTX_AWSS_ENABLE_TIMEOUT: // AWSS enable timeout
            // user needs to enable awss again to support get ssid & passwd of router
            LOG("IOTX_AWSS_ENALBE_TIMEOUT");
            // operate led to indicate user
            break;
        case IOTX_CONN_CLOUD: // Device try to connect cloud
            LOG("IOTX_CONN_CLOUD");
            // operate led to indicate user
            break;
        case IOTX_CONN_CLOUD_FAIL: // Device fails to connect cloud, refer to
            // net_sockets.h for error code
            LOG("IOTX_CONN_CLOUD_FAIL");
#ifdef EN_COMBO_NET
            combo_set_cloud_state(0);
#endif
            set_net_state(CONNECT_CLOUD_FAILED);
            // operate led to indicate user
            break;
        case IOTX_CONN_CLOUD_SUC: // Device connects cloud successfully
            LOG("IOTX_CONN_CLOUD_SUC");
#ifdef EN_COMBO_NET
            combo_set_cloud_state(1);
#endif
			hal_gpio_output_low(&GPIO_Link);
            set_net_state(CONNECT_CLOUD_SUCCESS);
            // operate led to indicate user
            break;
        case IOTX_RESET: // Linkkit reset success (just got reset response from
            // cloud without any other operation)
            LOG("IOTX_RESET");
            break;
        case IOTX_CONN_REPORT_TOKEN_SUC:
#ifdef EN_COMBO_NET
            combo_token_report_notify();
#endif
            LOG("---- report token success ----");
            break;
        default:
            break;
    }
}

#ifdef AWSS_BATCH_DEVAP_ENABLE
#define DEV_AP_ZCONFIG_TIMEOUT_MS  120000 // (ms)
extern void awss_set_config_press(uint8_t press);
extern uint8_t awss_get_config_press(void);
extern void zconfig_80211_frame_filter_set(uint8_t filter, uint8_t fix_channel);
void do_awss_dev_ap();

static aos_timer_t dev_ap_zconfig_timeout_timer;
static uint8_t g_dev_ap_zconfig_timer = 0; // this timer create once and can restart
static uint8_t g_dev_ap_zconfig_run = 0;

static void timer_func_devap_zconfig_timeout(void *arg1, void *arg2)
{
    LOG("%s run\n", __func__);

    if (awss_get_config_press()) {
        // still in zero wifi provision stage, should stop and switch to dev ap
        do_awss_dev_ap();
    } else {
        // zero wifi provision finished
    }

    awss_set_config_press(0);
    zconfig_80211_frame_filter_set(0xFF, 0xFF);
    g_dev_ap_zconfig_run = 0;
    aos_timer_stop(&dev_ap_zconfig_timeout_timer);
}

static void awss_dev_ap_switch_to_zeroconfig(void *p)
{
    LOG("%s run\n", __func__);
    // Stop dev ap wifi provision
    awss_dev_ap_stop();
    // Start and enable zero wifi provision
    iotx_event_regist_cb(linkkit_event_monitor);
    awss_set_config_press(1);

    // Start timer to count duration time of zero provision timeout
    if (!g_dev_ap_zconfig_timer) {
        aos_timer_new(&dev_ap_zconfig_timeout_timer, timer_func_devap_zconfig_timeout, NULL, DEV_AP_ZCONFIG_TIMEOUT_MS, 0);
        g_dev_ap_zconfig_timer = 1;
    }
    aos_timer_start(&dev_ap_zconfig_timeout_timer);

    // This will hold thread, when awss is going
    netmgr_start(true);

    LOG("%s exit\n", __func__);
    aos_task_exit(0);
}

int awss_dev_ap_modeswitch_cb(uint8_t awss_new_mode, uint8_t new_mode_timeout, uint8_t fix_channel)
{
    if ((awss_new_mode == 0) && !g_dev_ap_zconfig_run) {
        g_dev_ap_zconfig_run = 1;
        // Only receive zero provision packets
        zconfig_80211_frame_filter_set(0x00, fix_channel);
        LOG("switch to awssmode %d, mode_timeout %d, chan %d\n", 0x00, new_mode_timeout, fix_channel);
        // switch to zero config
        aos_task_new("devap_to_zeroconfig", awss_dev_ap_switch_to_zeroconfig, NULL, 2048);
    }
}
#endif

static void awss_close_dev_ap(void *p)
{
    awss_dev_ap_stop();
    awss_dev_ap_started = 0;
    aos_task_exit(0);
}

static int uart_recv_callback(void *data,int len)
{
    printf("uart_recv_callback\r\n");
	printf("recv uart buff len:%d,buffer:%s\n",len,data);
    ilife_uart_recive_data((uint8_t *)data, len);
	if(hfilop_uart_in_cmd_mode()!=0)
        return len;

	//to do post data to cloud
	/*if(post_uart_data_cloud(data,len))
		printf("post ali iot data success\n");
	else
		printf("post ali iot data fail !!!\n");*/
#ifdef  SUPPORT_MCU_OTA
		if(mcu_ota_start_flag)
		{
			Ymodem_Transmit_uart_data(data,len);
		}
#endif
	return 0;
}


void ilife_ntp_time_reply(const char *offset_time)
{
    uint8_t tmp = 0;
    SendData_t send_data;
    uint32_t timeMs = 0;
    ilife_time_t time_str;
    int i = 0;
    int len = 0;
    LOG_TRACE("ntp time = %s", offset_time);

    len = strlen(offset_time);
    //while(*offset_time != 0)
    for (i = 0; i < len - 3; i++)
    {
        if ((*offset_time < '0') || (*offset_time > '9'))
        {
            break;
        }
        tmp = (*offset_time - '0');
        timeMs = timeMs * 10 + tmp;
        offset_time++;
    }
    LOG_TRACE("timeMs = 0x%lx\r\n", timeMs);

    ilife_get_time(timeMs, &time_str);

    LOG_TRACE("Get Current Time: %04d-%02d-%02d %d %02d:%02d:%02d\r\n",
              time_str.year,
              time_str.mon,
              time_str.day,
              time_str.wday,
              time_str.hour,
              time_str.min,
              time_str.sec);

    send_data.ver = 1;
    send_data.cmd = CMD_TO_MCU;
    send_data.method = METHOD_CLOUD_SET;
    send_data.id = 0;
    send_data.payload[0] = ID_TYPE_UTC;
    send_data.payload[1] = ID_PROP_UTC;
    send_data.payload[2] = 0;
    send_data.payload[3] = 0;
    send_data.payload[4] = 0;
    send_data.payload[5] = 0;
    send_data.payload[6] = (timeMs & 0xff000000) >> 24;
    send_data.payload[7] = (timeMs & 0x00ff0000) >> 16;
    send_data.payload[8] = (timeMs & 0x0000ff00) >> 8;
    send_data.payload[9] = (timeMs & 0x000000ff);
    send_data.payload[10] = (time_str.year & 0xff00) >> 8;
    send_data.payload[11] = (time_str.year & 0x00ff);
    send_data.payload[12] = time_str.mon;
    send_data.payload[13] = time_str.day;
    send_data.payload[14] = time_str.wday;
    send_data.payload[15] = time_str.hour;
    send_data.payload[16] = time_str.min;
    send_data.payload[17] = time_str.sec;

    send_data.len = 8 + 18;

    ilife_send_data_to_mcu(&send_data);
}

uint8_t FactoryTestRes = 0;
wifi_scan_callback_t scan_call_back(ap_list_adv_t *list)
{
    //LOG_TRACE ("scan_call_back : ssid : %s, power : %d", list->ssid, list->ap_power);

    if ((strcmp(list->ssid, "ACPRODUCT_TEST_1") == 0) || (strcmp(list->ssid, "ACPRODUCT_TEST_2") == 0) && (FactoryTestRes == 0))
    {
        LOG_TRACE("scan_call_back : ssid : %s, power : %d", list->ssid, list->ap_power);
        if (list->ap_power > 40)
        {
            FactoryTestRes = 1;
        }
    }
}

void ilife_factory_test(uint32_t id)
{
    int ret = 0;
    uint8_t testRetry = 0;
    FactoryTestRes = 0;
    
    g_wifi_sleep_state = WIFI_STATE_WAKEUP;
    while (testRetry < 3)
    {
        ret = hfilop_wifi_scan(&scan_call_back);
        printf("hfilop_wifi_scan1 ret = %d, test res = %d\r\n", ret, FactoryTestRes);
        if (FactoryTestRes == 1)
        {
            ilife_send_ack(id, ACK_OK);
            break;
        }
        else
        {
            testRetry++;
        }
    }
    if (FactoryTestRes == 0)
    {
        ilife_send_ack(id, ACK_ERR);
    }
}

static void ilife_uart_progess(void *p)
{
    uint8_t recv[600] = {0};
    uint16_t recv_len = 0;
    int ret = 0;
    int res = 0;
    uint8_t wifi_config_state = 0;
    uint8_t cmd;
    uint8_t method;
    uint8_t attrid;
    uint32_t id;
    uint8_t data;
    uint8_t index = 0;
    uint8_t i = 0;
    uint8_t data_type = 0;
    uint8_t sleep_state = 0;
    uint32_t robot_type1 = 0;
    uint32_t robot_type2 = 0;
    uint32_t robot_type3 = 0;
    uint32_t robot_type4 = 0;
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    // #define PRO_KEY_LEN         11
    // #define PRO_SECRET_LEN      16
    // #define DEV_NAME_LEN        12
    // #define DEV_SECRET_LEN      32

    char pro_key[PRODUCT_KEY_LEN] = {0};
    char pro_sec[PRODUCT_SECRET_LEN] = {0};
    char dev_name[DEVICE_NAME_LEN] = {0};
    char dev_sec[DEVICE_SECRET_LEN] = {0};
    char mac_addr[8];
    char mcu_ver[16] = {0};

    //user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    char *event_id = "Error";
    char event_payload[32] = {};
    char *property_payload = "NULL";

    while (1)
    {
        ret = aos_queue_recv(&g_uart_recv_queue, AOS_WAIT_FOREVER, recv, &recv_len);
        if (ret == 0)
        {
            LOG_TRACE("g_uart_recv_queue recv data len = %d data = %02X,%02X,%02X,%02X,%02X,%02X ...\n",
                      recv_len,
                      recv[0],  //0xAA
                      recv[1],  //LEN_H
                      recv[2],  //LEN_L
                      recv[3],  //VER
                      recv[4],  //CMD
                      recv[5]); //METHOD
            recv[0] = STAND_HEADER;
            if (ilife_check_sum(recv, recv_len) == 0)
            {
                LOG_TRACE("Check Sum ERR! ret = %d", ret);
            }
            else
            {
                sleep_state = g_wifi_sleep_state;
                g_wifi_sleep_state = WIFI_STATE_WAKEUP;
                cmd = recv[4];
                method = recv[5];
                id = (recv[6] << 24) + (recv[7] << 16) + (recv[8] << 8) + recv[9];
                if ((cmd == CMD_TO_CLOUD) && ((method == METHOD_GET_WIFI_ATTR) || (method == METHOD_SET_WIFI_ATTR) || (method == METHOD_OTA_ACK_ATTR)))
                {
                    if (method == METHOD_GET_WIFI_ATTR) // 
                    {
                        attrid = recv[10];
                        switch (attrid)
                        {
                        case ID_PROP_WIFISTATE:
                        {
                            ilife_send_wifi_state(ilife_wifi_get_state());
                            break;
                        }
                        case ID_PROP_UTC:
                        {
                            ret = linkkit_ntp_time_request(ilife_ntp_time_reply);
                            if (ret != 0)
                            {
                                ilife_send_ack(id, ACK_ERR);
                            }
                            printf("get ntp time...ret =  %d \r\n", ret);
                            break;
                        }
                        case ID_PROP_WIFIINFO_MCU:
                        {
                            ilife_send_wifi_info();
                            break;
                        }
                        default:
                        {
                            ilife_send_ack(id, ACK_ERR);
                        }
                        }
                    }
                    else if (method == METHOD_OTA_ACK_ATTR) // OTA Ack
                    {
                        attrid = recv[11];
                        data = recv[12];
                        LOG_TRACE("METHOD_OTA_ACK_ATTR attrId = %d, data = %d", attrid, data);
                        switch (attrid)
                        {
                        case ID_PROP_OTA_OK_CODE: //
                        {
                            g_otaAckStatus = OTA_RECV_ACK;
                            break;
                        }
                        case ID_PROP_OTA_ERROR_CODE: //
                        {
                            g_otaAckStatus = OTA_FAIL_ACK;
                            break;
                        }
                        case ID_PROP_OTA_REQ_CODE://
                        {
                            if (mcu_ota_state == MCU_OTA_STATE_ALREADY)//
                            {
                                aos_task_new("ilife_mcu_ota_task", ilife_mcu_ota_task, NULL, 2048);
                            }
                        }
                        }
                    }
                    else //
                    {
                        attrid = recv[11];
                        data = recv[12];
                        switch (attrid)
                        {
                        case ID_PROP_WIFISTATE:
                        {
                            if (data == WIFI_STATE_SMART)
                            {
                                g_wifi_config = 1;
                                ilife_send_ack(id, ACK_OK);
                                wifi_config_state = 1;
                                ret = aos_kv_set(WIFI_CONFIG_STATE, &wifi_config_state, 1, 1);
                                printf("wifi_config_state = %d, ret = %d\r\n", wifi_config_state, ret);
                                g_hfilop_config.awss_mode=ILOP_AWSS_MODE;
                                hfilop_config_save();
                                hfsys_set_awss_state(AWSS_STATE_OPEN);
                                do_awss_reset();
                                //awss_report_reset(&ilife_reset_type);
                                // HAL_SleepMs(1000);
                                // HAL_Reboot();
                                // aos_task_exit(0);
                            }
                            else if (data == WIFI_STATE_AP)
                            {
                                g_wifi_config = 1;
                                ilife_send_ack(id, ACK_OK);
                                wifi_config_state = 2;
                                ret = aos_kv_set(WIFI_CONFIG_STATE, &wifi_config_state, 1, 1);
                                printf("wifi_config_state = %d, ret = %d\r\n", wifi_config_state, ret);
                                g_hfilop_config.awss_mode=ILOP_AWSS_DEV_AP_MODE;
                                hfilop_config_save();
                                hfsys_set_awss_state(AWSS_STATE_OPEN);
                                do_awss_reset();
                                //awss_report_reset(&ilife_reset_type);
                                // HAL_SleepMs(1000);
                                // HAL_Reboot();
                                // aos_task_exit(0);
                            }
                            else if (data == WIFI_STATE_HOST)
                            {
                                g_wifi_config = 1;
                                ilife_send_ack(id, ACK_OK);
                            }
                            else
                            {
                                ilife_send_ack(id, ACK_ERR);
                            }
                            break;
                        }
                        case ID_PROP_UNBANDING:
                        {
                            if (data == 1)
                            {
                                ilife_send_ack(id, ACK_OK);
                                // do_awss_reset();
                                awss_report_reset(&ilife_reset_type);
                                HAL_SleepMs(1000);
                            }
                            else
                            {
                                ilife_send_ack(id, ACK_ERR);
                            }
                            break;
                        }
                        case ID_PROP_FACTORYTEST: //
                        {
                            if (data == 1)
                            {
                                g_wifi_config = 1;
                                wifi_config_state = 3;
                                ret = aos_kv_set(WIFI_CONFIG_STATE, &wifi_config_state, 1, 1);
                                printf("wifi_config_state = %d, ret = %d\r\n", wifi_config_state, ret);
                                ret = aos_kv_set(WIFI_CMD_ID, &id, 4, 1);
                                printf("wifi_cmd_id = %d, ret = %d\r\n", id, ret);
                                HAL_Reboot();
                            }
                            else
                            {
                                //ilife_send_ack(id, ACK_ERR);
                            }
                            break;
                        }
                        case ID_PROP_WIFI_ACTIVE_CODE: //
                        {
                            i = 0;
                            for (index = 12; index < (recv_len - 1); index++)
                            {
                                if (recv[index] == 0x2C)
                                {
                                    data_type++;
                                    i = 0;
                                    continue;
                                }
                                switch (data_type)
                                {
                                case 0:
                                {
                                    pro_key[i++] = recv[index];
                                    break;
                                }
                                case 1:
                                {
                                    pro_sec[i++] = recv[index];
                                    break;
                                }
                                case 2:
                                {
                                    dev_name[i++] = recv[index];
                                    break;
                                }
                                case 3:
                                {
                                    dev_sec[i++] = recv[index];
                                    break;
                                }
                                default:
                                {
                                    break;
                                }
                                }
                            }

                            aos_get_mac_hex(&mac_addr);
                            i = 0;
                            for (index = 0; index < 6; index++)
                            {
                                if (((mac_addr[index] & 0xf0) >> 4) > 9)
                                {
                                    dev_name[i++] = ((mac_addr[index] & 0xf0) >> 4) + 0x37;
                                }
                                else
                                {
                                    dev_name[i++] = ((mac_addr[index] & 0xf0) >> 4) + 0x30;
                                }

                                if ((mac_addr[index] & 0x0f) > 9)
                                {
                                    dev_name[i++] = (mac_addr[index] & 0x0f) + 0x37;
                                }
                                else
                                {
                                    dev_name[i++] = (mac_addr[index] & 0x0f) + 0x30;
                                }
                            }
                            dev_name[i++] = 0;

                            LOG_TRACE("Recv ActiveCode: Pro_Key len = %d , %s", PRODUCT_KEY_LEN, pro_key);
                            LOG_TRACE("Recv ActiveCode: Pro_Sec len = %d , %s", PRODUCT_SECRET_LEN, pro_sec);
                            LOG_TRACE("Recv ActiveCode: Dev_Name len = %d , %s", DEVICE_NAME_LEN, dev_name);
                            LOG_TRACE("Recv ActiveCode: Dev_Sec len = %d , %s", DEVICE_SECRET_LEN, dev_sec);

                            hfilop_layer_set_product_key(&pro_key);
                            hfilop_layer_set_product_secret(&pro_sec);
                            hfilop_layer_set_device_name(&dev_name);
                            hfilop_layer_set_device_secret(&dev_sec);

                            ilife_send_ack(id, ACK_OK);
                            break;
                        }
                        default:
                        {
                        }
                        }
                    }
                }
                else if (method == METHOD_REPORT_EVENT)
                {
                    data = recv[11];
                    sprintf(event_payload, "{\"ErrorCode\":%d}", data);
                    ret = IOT_Linkkit_TriggerEvent(user_example_ctx->master_devid, event_id, strlen(event_id), event_payload, strlen(event_payload));
                    LOG_TRACE("Report Event: ret = %d , %s", ret, event_payload);
                }
                else if (method == METHOD_ACK)
                {
                    LOG_TRACE("Receive Ack: id = %d", id);
                }
                else
                {
                    if ((cmd == CMD_TO_CLOUD) && (method == METHOD_REPORT_ATTR))
                    {
                        attrid = recv[11];
                        data = recv[12];
                        if ((attrid == ID_PROP_WORKMODE) && ((data == 1) || (data == 9) || (data == 11) || (data == 16) || (data == 17)))
                        {
                            g_wifi_sleep_state = WIFI_STATE_SLEEP;
                            LOG_TRACE("Set g_wifi_sleep_state: WIFI_STATE_SLEEP");
                        }
                        else
                        {
                            g_wifi_sleep_state = WIFI_STATE_WAKEUP;
                        }

                        if (attrid == ID_PROP_WIFIINFO_CLOUD)
                        {
                            LOG_TRACE("report wifi info, reset sleep state: %d", sleep_state);
                            g_wifi_sleep_state = sleep_state;
                        }
                        else if (attrid == ID_PROP_ROBOTINFO)
                        {
                            g_robot_type = (recv[12] << 24) + (recv[13] << 16) + (recv[14] << 8) + recv[15];
                            LOG_TRACE("Set g_robot_type: %08X", g_robot_type);

                            uint32_t robot_type_save = 0;
                            int len_tmp = sizeof(robot_type_save);
                            ret = aos_kv_get(ROBOT_TYPE, &robot_type_save, &len_tmp);
                            LOG_TRACE("Get robot_type_save: %08X, ret = %d", robot_type_save, ret);
                            if (robot_type_save != g_robot_type)
                            {
                                LOG_TRACE("update save robot type:0x%04X", g_robot_type);
                                ret = aos_kv_set(ROBOT_TYPE, &g_robot_type, sizeof(g_robot_type), 1);
                                LOG_TRACE("Set g_robot_type: %08X, ret = %d", g_robot_type, ret);
                            }

                            uint32_t hardware_ver = 0;
                            hardware_ver = (recv[16] << 24) + (recv[17] << 16) + (recv[18] << 8) + recv[19];
                            if ((hardware_ver & (0x00000001 << 12)))//bit12=1 ota config
                            {
                                mcu_ota_config = MCU_OTA_CONFIG_NEED_WIFI;
                                ret = aos_kv_set(MCU_OTA_CONFIG, &mcu_ota_config, sizeof(mcu_ota_config), 1);
                                LOG_TRACE("Set mcu_ota_config: %08X, ret = %d", mcu_ota_config, ret);
                            }

                            uint32_t software_ver = 0;
                            software_ver = (recv[20] << 24) + (recv[21] << 16) + (recv[22] << 8) + recv[23];
                            sprintf(mcu_ver, "mcu-%02x.%02x.%02x.%02x", recv[20], recv[21], recv[22], recv[23]);
                            LOG_TRACE("software_ver: %08X", software_ver);

                            char mcu_ver_save[16] = {0};
                            len_tmp = sizeof(mcu_ver_save);
                            aos_kv_get("mcu_version", mcu_ver_save, &len_tmp);
                            if (0 != strlen(mcu_ver_save))
                            {
                                if (strncmp(mcu_ver_save, mcu_ver, strlen(mcu_ver)) != 0) //
                                {
                                    //LOG_TRACE("update save new MCU ver:%s, Reboot After 60s!!!!!!!!!", mcu_ver);
                                     res = aos_kv_set("mcu_version", (void *)mcu_ver, sizeof(mcu_ver), 1);
                                    if (res == 0)
                                    {
                                        LOG_TRACE("update save new MCU ver:%s, Reboot Now!!!!!!!!!", mcu_ver);
                                        aos_post_delayed_action(2000, aos_reboot, NULL);
                                    }
                                    else
                                    {
                                        LOG_TRACE("res = %d update save new MCU ver:%s, Reboot After 60s!!!!!!!!!", res, mcu_ver);
                                        aos_post_delayed_action(60000, aos_reboot, NULL);
                                    }
                                    //aos_kv_set("mcu_version", (void *)mcu_ver, sizeof(mcu_ver), 1);
                                    //aos_post_delayed_action(60000, aos_reboot, NULL);
                                    // HAL_SleepMs(100);
                                    // HAL_Reboot();
                                }
                                else if (mcu_ota_config == MCU_OTA_CONFIG_NEED_WIFI)
                                {
                                    ilife_ota_info_t ota_info_tmp;
                                    unsigned int offset = 0;
                                    uint8_t report_buf[23] = {0xaa, 0x00, 0x14, 0x01, 0x00, 0x80, 0x00, 0x01, 0x02, 0x03, 0x2e, 0x24,
                                                            0x01, 0x00, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x05, 0x00};
                                    
                                    report_buf[14] = recv[20];
                                    report_buf[15] = recv[21];
                                    report_buf[16] = recv[22];
                                    report_buf[17] = recv[23];
                                    if (mcu_ota_state == MCU_OTA_STATE_ALREADY)//
                                    {
                                        res = hal_flash_read(HAL_PARTITION_OTA_TEMP, &offset, &ota_info_tmp, sizeof(ilife_ota_info_t));
                                        LOG_TRACE("hal_flash_read ota_info_tmp res=%d, file_size=%d", res, ota_info_tmp.file_size);
                                        
                                        LOG_TRACE("software_ver: 0x%08X??? ota target_ver: 0x%08X", software_ver, ota_info_tmp.target_ver);
                                        if (ota_info_tmp.target_ver <= software_ver)//
                                        {
                                            report_buf[12] = 0;
                                        }
                                        report_buf[18] = (ota_info_tmp.target_ver & 0xff000000) >> 24;
                                        report_buf[19] = (ota_info_tmp.target_ver & 0x00ff0000) >> 16;
                                        report_buf[20] = (ota_info_tmp.target_ver & 0x0000ff00) >> 8;
                                        report_buf[21] = (ota_info_tmp.target_ver & 0x000000ff);

                                    }
                                    else
                                    {
                                        report_buf[12] = 0;
                                        report_buf[18] = recv[20];
                                        report_buf[19] = recv[21];
                                        report_buf[20] = recv[22];
                                        report_buf[21] = recv[23];
                                    }
                                    report_buf[22] = ilife_calc_sum(report_buf, 23-1);
                                    res = IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_RAW_DATA, report_buf, 23);
                                    LOG_TRACE("Post OTA Info Raw Data Message ID: %d, cmd = %d, method = 0x%02X", res, cmd, method);
                                } 

                            }
                            else //
                            {
                                aos_kv_set("mcu_version", (void *)mcu_ver, sizeof(mcu_ver), 1);
                            }
                            LOG_TRACE("update save MCU ver:%s", mcu_ver);
                        }

                        else if(attrid == ID_PROP_MAXMODE)
                        {
                            LOG_TRACE("\r\nANSWER!!!!!\r\n");
                            char *name=hfilop_layer_get_device_name();
	                        char *key=hfilop_layer_get_product_key();
                            char *ps=hfilop_layer_get_product_secret();
                            char *ds=hfilop_layer_get_device_secret();
	                        LOG_TRACE("device_name:%s,product_key:%s,ds:%s,ps:%s\r\n", name,key,ds,ps);

                            iotx_linkkit_dev_meta_info_t    master_meta_info;
                            memset(&master_meta_info, 0, sizeof(iotx_linkkit_dev_meta_info_t));
                            HAL_GetProductKey(master_meta_info.product_key);
                            HAL_GetDeviceName(master_meta_info.device_name);
                            HAL_GetDeviceSecret(master_meta_info.device_secret);
                            HAL_GetProductSecret(master_meta_info.product_secret);
                            LOG_TRACE("Device_name:%s,Product_key:%s,DS:%s,PS:%s\r\n", master_meta_info.device_name,master_meta_info.product_key,master_meta_info.device_secret,master_meta_info.product_secret);
                        }
                    }
                    res = IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_RAW_DATA, recv, recv_len);
                    LOG_TRACE("Post Raw Data Message ID: %d, cmd = %d, method = 0x%02X attrid = %d", res, cmd, method,attrid);
                }
            }
        }
        else
        {
            LOG_TRACE("g_uart_recv_queue Receive data ERR! ret = %d", ret);
        }
    }
}

void ilife_wake_up_gpio_init_input(void)
{
    // /* gpio port config */
     gpio_wake_up.port = GPIO_WAKEUP_IO;
    // /* set as output mode */
     gpio_wake_up.config = INPUT_PULL_DOWN;
    // /* configure GPIO with the given settings */
     hal_gpio_init(&gpio_wake_up);
     hal_gpio_output_low(&gpio_wake_up);
}

void ilife_wake_up_gpio_init_output(void)
{
     /* gpio port config */
     gpio_wake_up.port = GPIO_WAKEUP_IO;
    // /* set as output mode */
    gpio_wake_up.config = OUTPUT_PUSH_PULL;
    // /* configure GPIO with the given settings */
     hal_gpio_init(&gpio_wake_up);
     hal_gpio_output_low(&gpio_wake_up);
}

void awss_open_dev_ap(void *p)
{
    iotx_event_regist_cb(linkkit_event_monitor);
    /*if (netmgr_start(false) != 0) */{
        awss_dev_ap_started = 1;
        //aos_msleep(2000);
#ifdef AWSS_BATCH_DEVAP_ENABLE
        awss_dev_ap_reg_modeswit_cb(awss_dev_ap_modeswitch_cb);
#endif
        awss_dev_ap_start();
    }
    aos_task_exit(0);
}

void stop_netmgr(void *p)
{

    awss_stop();
    aos_task_exit(0);
}

void start_netmgr(void *p)
{
    /* wait for dev_ap mode stop done */
    do {
        aos_msleep(100);
    } while (awss_dev_ap_started);
    iotx_event_regist_cb(linkkit_event_monitor);
    netmgr_start(true);
    aos_task_exit(0);
}

void do_awss_active(void)
{
    LOG("do_awss_active");
#ifdef WIFI_PROVISION_ENABLED
    extern int awss_config_press();
    awss_config_press();
#endif
}

#ifdef EN_COMBO_NET
#ifdef EN_COMBO_BLE_CONTROL
int user_ble_event_handler(uint16_t event_code) {
    if (event_code == COMBO_EVT_CODE_FULL_REPORT) {
        report_device_property(NULL, 0);
    }
}
extern int user_ble_serv_request_event_handler(const int devid, const char *serviceid, const int serviceid_len,
        const char *request, const int request_len);
extern int user_ble_property_set_event_handler(const int devid, const char *request, const int request_len);
extern int user_ble_property_get_event_handler(const int devid, const char *request, const int request_len,
        char **response, int *response_len);
#endif

void combo_open(void)
{
    combo_net_init();
#ifdef EN_COMBO_BLE_CONTROL
    combo_reg_evt_cb(user_ble_event_handler);
    combo_reg_common_serv_cb(user_ble_serv_request_event_handler);
    combo_reg_property_set_cb(user_ble_property_set_event_handler);
    combo_reg_property_get_cb(user_ble_property_get_event_handler);
#endif
}

void ble_awss_open(void *p)
{
    iotx_event_regist_cb(linkkit_event_monitor);
    combo_set_awss_state(1);
    aos_task_exit(0);
}

static void ble_awss_close(void *p)
{
    combo_set_awss_state(0);
    aos_task_exit(0);
}

void do_ble_awss()
{
    aos_task_new("ble_awss_open", ble_awss_open, NULL, 2048);
}
#endif

void do_awss_dev_ap()
{
    // Enter dev_ap awss mode
    aos_task_new("netmgr_stop", stop_netmgr, NULL, 4096);
    aos_task_new("dap_open", awss_open_dev_ap, NULL, 4096);
}

void do_awss()
{
    // Enter smart_config awss mode
    aos_task_new("dap_close", awss_close_dev_ap, NULL, 4096);
    aos_task_new("netmgr_start", start_netmgr, NULL, 5120);
}

void linkkit_reset(void *p)
{
    aos_msleep(2000);
#ifdef AOS_TIMER_SERVICE
    timer_service_clear();
#endif
#ifdef AIOT_DEVICE_TIMER_ENABLE
    aiot_device_timer_clear();
#endif
    aos_kv_del(KV_KEY_SWITCH_STATE);
    iotx_sdk_reset_local();
    netmgr_clear_ap_config();
#ifdef EN_COMBO_NET
    breeze_clear_bind_info();
#endif
    HAL_Reboot();
    aos_task_exit(0);
}

extern int iotx_sdk_reset(iotx_vendor_dev_reset_type_t *reset_type);
iotx_vendor_dev_reset_type_t reset_type = IOTX_VENDOR_DEV_RESET_TYPE_UNBIND_ONLY;
void do_awss_reset(void)
{
#ifdef WIFI_PROVISION_ENABLED
    aos_task_new("reset", (void (*)(void *))iotx_sdk_reset, &reset_type, 6144);  // stack taken by iTLS is more than taken by TLS.
#endif
    aos_task_new_ext(&task_linkkit_reset, "reset task", linkkit_reset, NULL, 1024, 0);
}

void do_awss_ble_start(void)
{
	  g_hfilop_config.awss_mode=ILOP_AWSS_DEV_BLE_MODE;
 	  hfilop_config_save();
	  hfsys_set_dms();

}


void reboot_device(void *p)
{
    aos_msleep(500);
    HAL_Reboot();
    aos_task_exit(0);
}

void do_awss_reboot(void)
{
    int ret;
    unsigned char awss_flag = 1;
    int len = sizeof(awss_flag);

    ret = aos_kv_set("awss_flag", &awss_flag, len, 1);
    if (ret != 0)
        LOG("KV Setting failed");

    aos_task_new_ext(&task_reboot_device, "reboot task", reboot_device, NULL, 1024, 0);
}

void linkkit_key_process(input_event_t *eventinfo, void *priv_data)
{
    if (eventinfo->type != EV_KEY) {
        return;
    }
    LOG("awss config press %d\n", eventinfo->value);

    if (eventinfo->code == CODE_BOOT) {
        if (eventinfo->value == VALUE_KEY_CLICK) {
            do_awss_active();
        } else if (eventinfo->value == VALUE_KEY_LTCLICK) {
            do_awss_reset();
        }
    }
}

#ifdef MANUFACT_AP_FIND_ENABLE
void manufact_ap_find_process(int result)
{
    // Informed manufact ap found or not.
    // If manufact ap found, lower layer will auto connect the manufact ap
    // IF manufact ap not found, lower layer will enter normal awss state
    if (result == 0) {
        LOG("%s ap found.\n", __func__);
    } else {
        LOG("%s ap not found.\n", __func__);
    }
}
#endif

#ifdef CONFIG_AOS_CLI
static void handle_reset_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    aos_schedule_call((aos_call_t)do_awss_reset, NULL);
}

static void handle_active_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    aos_schedule_call((aos_call_t)do_awss_active, NULL);
}

static void handle_dev_ap_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    aos_schedule_call((aos_call_t)do_awss_dev_ap, NULL);
}

#ifdef EN_COMBO_NET
static void handle_ble_awss_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    aos_schedule_call((aos_call_t)do_ble_awss, NULL);
}
#endif

static void handle_linkkey_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    if (argc == 1) {
        int len = 0;
        char product_key[PRODUCT_KEY_LEN + 1] = { 0 };
        char product_secret[PRODUCT_SECRET_LEN + 1] = { 0 };
        char device_name[DEVICE_NAME_LEN + 1] = { 0 };
        char device_secret[DEVICE_SECRET_LEN + 1] = { 0 };
        char pidStr[9] = { 0 };

        len = PRODUCT_KEY_LEN + 1;
        aos_kv_get("linkkit_product_key", product_key, &len);

        len = PRODUCT_SECRET_LEN + 1;
        aos_kv_get("linkkit_product_secret", product_secret, &len);

        len = DEVICE_NAME_LEN + 1;
        aos_kv_get("linkkit_device_name", device_name, &len);

        len = DEVICE_SECRET_LEN + 1;
        aos_kv_get("linkkit_device_secret", device_secret, &len);

        aos_cli_printf("Product Key=%s.\r\n", product_key);
        aos_cli_printf("Device Name=%s.\r\n", device_name);
        aos_cli_printf("Device Secret=%s.\r\n", device_secret);
        aos_cli_printf("Product Secret=%s.\r\n", product_secret);
        len = sizeof(pidStr);
        if (aos_kv_get("linkkit_product_id", pidStr, &len) == 0) {
            aos_cli_printf("Product Id=%d.\r\n", atoi(pidStr));
        }
    } else if (argc == 5 || argc == 6) {
        aos_kv_set("linkkit_product_key", argv[1], strlen(argv[1]) + 1, 1);
        aos_kv_set("linkkit_device_name", argv[2], strlen(argv[2]) + 1, 1);
        aos_kv_set("linkkit_device_secret", argv[3], strlen(argv[3]) + 1, 1);
        aos_kv_set("linkkit_product_secret", argv[4], strlen(argv[4]) + 1, 1);
        if (argc == 6)
            aos_kv_set("linkkit_product_id", argv[5], strlen(argv[5]) + 1, 1);
        aos_cli_printf("Done");
    } else {
        aos_cli_printf("Error: %d\r\n", __LINE__);
        return;
    }
}

static void handle_awss_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    aos_schedule_call((aos_call_t)do_awss, NULL);
}

static struct cli_command resetcmd = {
    .name = "reset",
    .help = "factory reset",
    .function = handle_reset_cmd
};

static struct cli_command awss_enable_cmd = {
    .name = "active_awss",
    .help = "active_awss [start]",
    .function = handle_active_cmd
};

static struct cli_command awss_dev_ap_cmd = {
    .name = "dev_ap",
    .help = "awss_dev_ap [start]",
    .function = handle_dev_ap_cmd
};

static struct cli_command awss_cmd = {
    .name = "awss",
    .help = "awss [start]",
    .function = handle_awss_cmd
};

#ifdef EN_COMBO_NET
static struct cli_command awss_ble_cmd = {
    .name = "ble_awss",
    .help = "ble_awss [start]",
    .function = handle_ble_awss_cmd
};
#endif

static struct cli_command linkkeycmd = {
    .name = "linkkey",
    .help = "set/get linkkit keys. linkkey [<Product Key> <Device Name> <Device Secret> <Product Secret>]",
    .function = handle_linkkey_cmd
};

#endif

#ifdef CONFIG_PRINT_HEAP
static void duration_work(void *p)
{
    print_heap();
    aos_post_delayed_action(5000, duration_work, NULL);
}
#endif

#if defined(OTA_ENABLED) && defined(BUILD_AOS)
static int ota_init(void);
static ota_service_t ctx = {0};
static bool ota_service_inited = false;
#ifdef CERTIFICATION_TEST_MODE
void *ct_entry_get_uota_ctx(void)
{
    if (ota_service_inited == true)
    {
        return (void *)&ctx;
    }
    else
    {
        return NULL;
    }
}
#endif
#endif
static int mqtt_connected_event_handler(void)
{
#if defined(OTA_ENABLED) && defined(BUILD_AOS)

    if (ota_service_inited == true) {
        int ret = 0;

        LOG("MQTT reconnected, let's redo OTA upgrade");
        if ((ctx.h_tr) && (ctx.h_tr->upgrade)) {
            LOG("Redoing OTA upgrade");
            ret = ctx.h_tr->upgrade(&ctx);
            if (ret < 0) LOG("Failed to do OTA upgrade");
        }

        return ret;
    }

    LOG("MQTT Construct  OTA start to inform");
#ifdef DEV_OFFLINE_OTA_ENABLE
    ota_service_inform(&ctx);
#else
    ota_init();
#endif

#ifdef OTA_MULTI_MODULE_DEBUG
    extern ota_hal_module_t ota_hal_module1;
    extern ota_hal_module_t ota_hal_module2;
    iotx_ota_module_info_t module;
    char module_name_key[MODULE_NAME_LEN + 1] = {0};
    char module_version_key[MODULE_VERSION_LEN + 1] = {0};
    char module_name_value[MODULE_NAME_LEN + 1] = {0};
    char module_version_value[MODULE_VERSION_LEN + 1] = {0};
    char buffer_len = 0;
    int ret = 0;

    for(int i = 1; i <= 2; i++){
        memset(module_name_key, 0, MODULE_NAME_LEN);
        memset(module_version_key, 0, MODULE_VERSION_LEN);
        memset(module_name_value, 0, MODULE_NAME_LEN);
        memset(module_version_value, 0, MODULE_VERSION_LEN);
        HAL_Snprintf(module_name_key, MODULE_NAME_LEN, "ota_m_name_%d", i);
        HAL_Snprintf(module_version_key, MODULE_VERSION_LEN, "ota_m_version_%d", i);
        HAL_Printf("module_name_key is %s\n",module_name_key);
        HAL_Printf("module_version_key is %s\n",module_version_key);
        buffer_len = MODULE_NAME_LEN;
        ret = HAL_Kv_Get(module_name_key,module_name_value, &buffer_len);
        buffer_len = MODULE_VERSION_LEN;
        ret |= HAL_Kv_Get(module_version_key,module_version_value, &buffer_len);
        memcpy(module.module_name, module_name_value, MODULE_NAME_LEN);
        memcpy(module.module_version, module_version_value, MODULE_VERSION_LEN);
        memcpy(module.product_key, ctx.pk, sizeof(ctx.pk)-1);
        memcpy(module.device_name, ctx.dn, sizeof(ctx.dn)-1);
        if(!ret){
            if(i == 1){
                module.hal = &ota_hal_module1;
            }else{
                module.hal = &ota_hal_module2;
            }
            ota_service_set_module_info(&ctx, &module);
        }
        HAL_Printf("module_name_value is %s\n",module_name_value);
        HAL_Printf("module_version_value is %s\n",module_version_value);
    }

#endif
    ota_service_inited = true;
#endif
    return 0;
}

static int ota_init(void)
{
#if defined(OTA_ENABLED) && defined(BUILD_AOS)
    char product_key[PRODUCT_KEY_LEN + 1] = {0};
    char device_name[DEVICE_NAME_LEN + 1] = {0};
    char device_secret[DEVICE_SECRET_LEN + 1] = {0};
    HAL_GetProductKey(product_key);
    HAL_GetDeviceName(device_name);
    HAL_GetDeviceSecret(device_secret);
    memset(&ctx, 0, sizeof(ota_service_t));
    strncpy(ctx.pk, product_key, sizeof(ctx.pk)-1);
    strncpy(ctx.dn, device_name, sizeof(ctx.dn)-1);
    strncpy(ctx.ds, device_secret, sizeof(ctx.ds)-1);
    ctx.trans_protcol = 0;
    ctx.dl_protcol = 3;
    ota_service_init(&ctx);
#endif
    return 0;
}

static void show_firmware_version(void)
{
    printf("\r\n--------Firmware info--------");
    printf("\r\napp: %s,  board: %s", APP_NAME, PLATFORM);
    printf("\r\nHost: %s", COMPILE_HOST);
    printf("\r\nBranch: %s", GIT_BRANCH);
    printf("\r\nHash: %s", GIT_HASH);
    printf("\r\nDate: %s %s", __DATE__, __TIME__);
    printf("\r\nKernel: %s", aos_get_kernel_version());
    printf("\r\nLinkKit: %s", LINKKIT_VERSION);
    printf("\r\nAPP: %s", aos_get_app_version());

    printf("\r\nRegion env: %s\r\n\r\n", REGION_ENV_STRING);
}
static int uart_data_process(char *data, uint32_t len)
{
    LOG("uart_data_process:(%d)[%s]\n", len,data);
    if(hfilop_uart_in_cmd_mode()!=0)
        return len;
//    hfilop_uart_send_data((unsigned char*)data,len);
#ifdef  SUPPORT_MCU_OTA
		if(mcu_ota_start_flag)
		{
			Ymodem_Transmit_uart_data(data,len);
		}
#endif

    return 0;
}



#if 0//(defined (TG7100CEVB))
void media_to_kv(void)
{
    char product_key[PRODUCT_KEY_LEN + 1] = { 0 };
    char *p_product_key = NULL;
    char product_secret[PRODUCT_SECRET_LEN + 1] = { 0 };
    char *p_product_secret = NULL;
    char device_name[DEVICE_NAME_LEN + 1] = { 0 };
    char *p_device_name = NULL;
    char device_secret[DEVICE_SECRET_LEN + 1] = { 0 };
    char *p_device_secret = NULL;
    char pidStr[9] = { 0 };
    char *p_pidStr = NULL;
    int len;

    int res;

    /* check media valid, and update p */
    res = ali_factory_media_get(
                &p_product_key,
                &p_product_secret,
                &p_device_name,
                &p_device_secret,
                &p_pidStr);
    if (0 != res) {
        printf("ali_factory_media_get res = %d\r\n", res);
        return;
    }

    /* compare kv media */
    len = sizeof(product_key);
    aos_kv_get("linkkit_product_key", product_key, &len);
    len = sizeof(product_secret);
    aos_kv_get("linkkit_product_secret", product_secret, &len);
    len = sizeof(device_name);
    aos_kv_get("linkkit_device_name", device_name, &len);
    len = sizeof(device_secret);
    aos_kv_get("linkkit_device_secret", device_secret, &len);
    len = sizeof(pidStr);
    aos_kv_get("linkkit_product_id", pidStr, &len);

    if (p_product_key) {
        if (0 != memcmp(product_key, p_product_key, strlen(p_product_key))) {
            printf("memcmp p_product_key different. set kv: %s\r\n", p_product_key);
            aos_kv_set("linkkit_product_key", p_product_key, strlen(p_product_key), 1);
        }
    }
    if (p_product_secret) {
        if (0 != memcmp(product_secret, p_product_secret, strlen(p_product_secret))) {
            printf("memcmp p_product_secret different. set kv: %s\r\n", p_product_secret);
            aos_kv_set("linkkit_product_secret", p_product_secret, strlen(p_product_secret), 1);
        }
    }
    if (p_device_name) {
        if (0 != memcmp(device_name, p_device_name, strlen(p_device_name))) {
            printf("memcmp p_device_name different. set kv: %s\r\n", p_device_name);
            aos_kv_set("linkkit_device_name", p_device_name, strlen(p_device_name), 1);
        }
    }
    if (p_device_secret) {
        if (0 != memcmp(device_secret, p_device_secret, strlen(p_device_secret))) {
            printf("memcmp p_device_secret different. set kv: %s\r\n", p_device_secret);
            aos_kv_set("linkkit_device_secret", p_device_secret, strlen(p_device_secret), 1);
        }
    }
    if (p_pidStr) {
        if (0 != memcmp(pidStr, p_pidStr, strlen(p_pidStr))) {
            printf("memcmp p_pidStr different. set kv: %s\r\n", p_pidStr);
            aos_kv_set("linkkit_product_id", p_pidStr, strlen(p_pidStr), 1);
        }
    }
}
#endif

void test_hf_at_cmd()
{
	char rsp[64] = {0};
	
	hfat_send_cmd("AT+UART\r\n",sizeof("AT+UART\r\n"),rsp,sizeof(rsp));

	LOG("-------------rsp = %s \r\n",rsp);
	if(0 != strcmp("+ok=9600,8,1,none,nfc",rsp))
	{
		hfat_send_cmd("AT+UART=9600,8,1,NONE,NFC\r\n",sizeof("AT+UART=9600,8,1,NONE,NFC\r\n"),rsp,sizeof(rsp));
	}
}

void set_general_firmware()
{
	extern char m2m_app_state;

	g_hfilop_config.tmod = CONFIG_EVENT_ON;
	m2m_app_state = M2M_STATE_RUN_CMD;
}

const hfproduct_cmd_t user_define_product_cmds_table[]=
{
	{NULL,			NULL} //the last item must be null
};

void hfble_scan_callback(BLE_SCAN_RESULT_ITEM *item)
{
	printf("---------------------\r\n");
	printf("%02X:%02X:%02X:%02X:%02X:%02X\r\n",item->addr[0],item->addr[1],item->addr[2],item->addr[3],item->addr[4],item->addr[5]);
	printf("addr_type = %d,rssi = %d,evtype = %d,len = %d\r\n",item->addr_type,item->rssi,item->evtype,item->len);

	printf("\r\n");
	int i;
	for(i = 0; i < item->len; i++)
		printf("%02x ",item->data[i]);
	printf("\r\n");
	printf("---------------------\r\n");
}

void app_fill_ble_adv_data(void)
{
	// user fill self ble adv data to Advertisement_Data
	extern GAPP_DISC_DATA_T Advertisement_Data;
}

int application_start(int argc, char **argv)
{
    int len = 0;
    int ret = 0;
    char interval[9] = {0};
    uint32_t id = 0;
    uint8_t wifi_config_state = 0;
    uint8_t state = 0;
    
	LOG("-----------HiFlying App Entry Start-------------\r\n");

#if (defined (TG7100CEVB))
    //media_to_kv();
#endif

#ifdef CONFIG_PRINT_HEAP
    print_heap();
    aos_post_delayed_action(5000, duration_work, NULL);
#endif

#ifdef CSP_LINUXHOST
    signal(SIGPIPE, SIG_IGN);
#endif

#ifdef WITH_SAL
    sal_init();
#endif

#ifdef MDAL_MAL_ICA_TEST
    HAL_MDAL_MAL_Init();
#endif

#ifdef DEFAULT_LOG_LEVEL_DEBUG
    IOT_SetLogLevel(IOT_LOG_DEBUG);
#else
    IOT_SetLogLevel(IOT_LOG_WARNING);
#endif

    
	//set_general_firmware();
	
   	show_firmware_version();

     ret = aos_queue_new(&g_uart_recv_queue, uart_recv_queue_buf, UART_QUEUE_BUF_SIZE, 2048);
     if (ret != 0)
     {
         LOG_TRACE("g_uart_recv_queue init fail! ret = %d", ret);
     }
    ret = aos_queue_new(&g_wait_wake_queue, wait_wake_queue_buf, WAIT_QUEUE_BUF_SIZE, 2048);
    if (ret != 0)
    {
        LOG_TRACE("g_wait_wake_queue init fail! ret = %d", ret);
    }
    ilife_wake_up_gpio_init_input();
     aos_task_new("ilife_uart_progess", ilife_uart_progess, NULL, 1024 * 4);

	hfilop_uart_task_start(uart_recv_callback, &user_define_at_cmds_table); 	   //
	
   if(strlen(hfilop_layer_get_product_key()) <=0 || strlen(hfilop_layer_get_device_name()) <=0)
   {
	   while(1)
		   aos_msleep(1000);
   }
   else
   {
	   char *name=hfilop_layer_get_device_name();
	   char *key=hfilop_layer_get_product_key();

	   printf("device_name:%s,product_key:%s\r\n", name,key);
   }
   
#ifdef HF_ID2
		gpio_dev_t GPIO4;
		GPIO24.port=4;
		GPIO24.config=OUTPUT_PUSH_PULL;
			
		hal_gpio_init(&GPIO4);
		hal_gpio_output_high(&GPIO4);
#endif

//    set_device_meta_info();
    netmgr_init();
//    vendor_product_init();
    dev_diagnosis_module_init();
#ifdef DEV_OFFLINE_OTA_ENABLE
    ota_init();
#endif

    aos_register_event_filter(EV_KEY, linkkit_key_process, NULL);
    aos_register_event_filter(EV_WIFI, wifi_service_event, NULL);
    aos_register_event_filter(EV_YUNIO, cloud_service_event, NULL);
    IOT_RegisterCallback(ITE_MQTT_CONNECT_SUCC,mqtt_connected_event_handler);

#ifdef CONFIG_AOS_CLI
    aos_cli_register_command(&resetcmd);
    aos_cli_register_command(&awss_enable_cmd);
    aos_cli_register_command(&awss_dev_ap_cmd);
    aos_cli_register_command(&awss_cmd);
#ifdef EN_COMBO_NET
    aos_cli_register_command(&awss_ble_cmd);
#endif
    aos_cli_register_command(&linkkeycmd);
#endif

    init_awss_flag();

	extern unsigned char hfsys_get_awss_state();
	if(hfsys_get_awss_state() == AWSS_STATE_OPEN)
	{
		hfsys_start_dms();	
	}

    len = 1;
    ret = aos_kv_get(MCU_OTA_STATE, &mcu_ota_state, &len);
    printf("mcu_ota_state = %d, ret = %d, len = %d\r\n", mcu_ota_state, ret, len);
    
    ret = aos_kv_get(MCU_OTA_CONFIG, &mcu_ota_config, &len);
    printf("mcu_ota_config = %d, ret = %d, len = %d\r\n", mcu_ota_config, ret, len);

    ret = aos_kv_get(WIFI_CONFIG_STATE, &wifi_config_state, &len);
    printf("wifi_config_state = %d, ret = %d, len = %d\r\n", wifi_config_state, ret, len);
    state = wifi_config_state;

    if (wifi_config_state != 0)
    {
        wifi_config_state = 0;
        ret = aos_kv_set(WIFI_CONFIG_STATE, &wifi_config_state, 1, 1);
        printf("reset wifi_config_state = %d, ret = %d\r\n", wifi_config_state, ret);
    }

    len = sizeof(g_robot_type);
    ret = aos_kv_get(ROBOT_TYPE, &g_robot_type, &len);
    printf("robot_type = 0x%04X, ret = %d\r\n", g_robot_type, ret);

    // state = 1;
    if (state == 1)//
    {
        ilife_wifi_set_state(WIFI_STATE_SMART);
        // awss_config_press();
        // awss_start();
        // aos_task_new("netmgr_start", start_netmgr, NULL, 5120);
        // hfiolp_check_awss();
        // aos_task_new("netmgr_start", start_netmgr, NULL, 5120);
		// hfiolp_check_awss();
        printf("start ble config\r\n");
        //do_ble_awss();
        //awss_config_press_start_flag=1;
        //g_hfilop_config.awss_mode=ILOP_AWSS_MODE;
        //hfilop_config_save();
        //unsigned char buf[100];
        //memset(buf,0,sizeof(buf));
        // strcpy(buf,"+ILOPCONNECT=AWSS_START\r\n\r\n");
        // hfilop_uart_send_data(buf,strlen(buf));
    }
    else if (state == 2)//
    {
        ilife_wifi_set_state(WIFI_STATE_AP);
        //awss_dev_ap_start();
        //aos_task_new("dap_open", awss_open_dev_ap, NULL, 4096);
    }
    else if (state == 3)//
    {
        len = 4;
        ret = aos_kv_get(WIFI_CMD_ID, &id, &len);
        printf("wifi_cmd_id = %d, ret = %d, len = %d\r\n", id, ret, len);
        ilife_factory_test(id);
    }
#ifdef EN_COMBO_NET
    combo_open();
#endif
	
    hfsys_start_network_status_process();
	
    check_factory_mode();
	
	hfsys_ready_link_gpio_init();//--20181212

	hfsys_device_status_light_timer_create();//--20181212

	hfilop_assis_task_start();
	
    hfilop_check_ota_state();

	LOG("-----------HiFlying App Entry End-------------\r\n");
	
    aos_loop_run();

    return 0;
}
