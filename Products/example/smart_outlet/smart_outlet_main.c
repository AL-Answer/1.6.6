/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */
#include "stdio.h"
#include "iot_export_linkkit.h"
#include "cJSON.h"
#include "app_entry.h"
#include "aos/kv.h"
#if defined(AOS_TIMER_SERVICE)||defined(AIOT_DEVICE_TIMER_ENABLE)
#include "iot_export_timer.h"
#endif
#include "smart_outlet.h"
#include "vendor.h"
#include "device_state_manger.h"
#include "msg_process_center.h"
#include "property_report.h"
#include "hfilop/hfilop.h"
#include "board.h"

#include "hfat_cmd.h"
#include <hal/soc/flash.h>
#include <netmgr.h>
#include "app_entry.h"

#include "hfilop/hfilop.h"
#include "hfilop/hfilop_config.h"

#include "ilife_uart.h"

#ifdef EN_COMBO_NET
#include "combo_net.h"
#endif

static user_example_ctx_t g_user_example_ctx;
int send_flag=1;
#ifdef CERTIFICATION_TEST_MODE
#include "certification/ct_ut.h"
extern bool mcu_ota_start_flag;
extern void print_heap();
#endif

#define USER_EXAMPLE_YIELD_TIMEOUT_MS (30)

#define RESPONE_BUFFER_SIZE   128


#define MAX_USER    5

struct Seq_Info {
    char userId[8];
    uint64_t timeStampe;
    int isValid;
};

static struct Seq_Info seqence_info[MAX_USER];

#ifdef AIOT_DEVICE_TIMER_ENABLE
    //#define MULTI_ELEMENT_TEST
    #ifndef MULTI_ELEMENT_TEST
        #define NUM_OF_TIMER_PROPERTYS 3 /*  */
        const char *propertys_list[NUM_OF_TIMER_PROPERTYS] = { "PowerSwitch", "powerstate", "allPowerstate" };    
    #else // only for test
        #define NUM_OF_TIMER_PROPERTYS 14 /*  */
        // const char *propertys_list[NUM_OF_TIMER_PROPERTYS] = { "testEnum", "testFloat", "testInt", "powerstate", "allPowerstate" };
        const char *propertys_list[NUM_OF_TIMER_PROPERTYS] = { "powerstate", "allPowerstate", "mode", "powerstate_1", "brightness", "PowerSwitch", "powerstate_2", 
                    "powerstate_3", "heaterPower", "windspeed", "angleLR", "testEnum", "testFloat", "testInt" };
        typedef enum {
            T_INT = 1,
            T_FLOAT,
            T_STRING,
            T_STRUCT,
            T_ARRAY,
        }  data_type_t;
        const data_type_t propertys_type[NUM_OF_TIMER_PROPERTYS] = { T_INT,T_INT,T_INT,T_INT,T_INT,T_INT,T_INT,T_INT,T_FLOAT,T_INT,T_INT,T_INT,T_FLOAT,T_INT };

        static int propertys_handle(cJSON *root) {
            cJSON *item = NULL;
            int ret = -1, i = 0;

            for (i = 0; i < NUM_OF_TIMER_PROPERTYS; i++) {
                if (propertys_type[i] == T_STRUCT && (item = cJSON_GetObjectItem(root, propertys_list[i])) != NULL && cJSON_IsObject(item)) { //structs
                    printf(" %s\r\n", propertys_list[i]);
                    ret = 0;
                } else if (propertys_type[i] == T_FLOAT && (item = cJSON_GetObjectItem(root, propertys_list[i])) != NULL && cJSON_IsNumber(item)){ // float
                    printf(" %s %f\r\n", propertys_list[i], item->valuedouble);
                    ret = 0;
                } else if (propertys_type[i] == T_INT &&(item = cJSON_GetObjectItem(root, propertys_list[i])) != NULL && cJSON_IsNumber(item)){ // int
                    printf(" %s %d\r\n", propertys_list[i], item->valueint);
                    ret = 0;
                } else if (propertys_type[i] == T_STRING &&(item = cJSON_GetObjectItem(root, propertys_list[i])) != NULL && cJSON_IsString(item)){ // string
                    printf(" %s %s\r\n", propertys_list[i], item->valuestring);
                    ret = 0;
                } else if (propertys_type[i] == T_ARRAY &&(item = cJSON_GetObjectItem(root, propertys_list[i])) != NULL && cJSON_IsArray(item)){ // array
                    printf(" %s \r\n", propertys_list[i]);
                    ret = 0;
                }
            }

            return ret;
        }
    #endif
static void timer_service_cb(const char *report_data, const char *property_name, const char *data)
{
    uint8_t value = 0; 
    char property_payload[128] = {0};

    // if (report_data != NULL)	/* post property to cloud */
    //     user_post_property_json(report_data);
    if (property_name != NULL) {	/* set value to device */
        LOG_TRACE("timer event callback=%s val=%s", property_name, data);
        #ifdef MULTI_ELEMENT_TEST
        user_example_ctx_t *user_example_ctx = user_example_get_ctx();
        if (strcmp(propertys_list[0], property_name) != 0 && strcmp(propertys_list[1], property_name) != 0) {
            snprintf(property_payload, sizeof(property_payload), "{\"%s\":%s}", property_name, data);
            IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_PROPERTY,
                    property_payload, strlen(property_payload));
            return;
        }
        else 
        #endif
        { 
            // data is int; convert it.
            value = (uint8_t)atoi(data);
        }
        recv_msg_t msg;
        msg.powerswitch = value;
        msg.all_powerstate = value;
        msg.flag = 0x00;
        strcpy(msg.seq, SPEC_SEQ);
        send_msg_to_queue(&msg);
    }

    return;
}
#endif

#ifdef AOS_TIMER_SERVICE
    #define NUM_OF_PROPERTYS 3 /* <=30 dont add timer property */
    const char *control_targets_list[NUM_OF_PROPERTYS] = { "PowerSwitch", "powerstate","allPowerstate" };
    static int num_of_tsl_type[NUM_OF_TSL_TYPES] = { 3, 0, 0 }; /* 1:int/enum/bool; 2:float/double; 3:text/date */

    #define NUM_OF_COUNTDOWN_LIST_TARGET 3 /* <=10 */
    const char *countdownlist_target_list[NUM_OF_COUNTDOWN_LIST_TARGET] = { "PowerSwitch", "powerstate","allPowerstate" };

    #define NUM_OF_LOCAL_TIMER_TARGET 1 /* <=5 */
    const char *localtimer_target_list[NUM_OF_LOCAL_TIMER_TARGET] = { "powerstate" };
#endif



void user_post_property_json(const char *property);
int32_t app_post_property_powerstate(uint8_t value)
{
    int32_t res = -0x100;
    char property_payload[64] = {0};
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    res = HAL_Snprintf(property_payload, sizeof(property_payload), "{\"powerstate\": %d}", value);
    if (res < 0)
    {
        return -0x10E;
    }

    res = IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_PROPERTY,
                             (uint8_t *)property_payload, strlen(property_payload));
    return res;
}

gpio_dev_t gpio_wake_up;

uint8_t g_wifi_state = 0;
uint8_t g_wifi_config = 0;

uint8_t g_wifi_sleep_state = WIFI_STATE_SLEEP;
uint8_t g_ilife_wakeup_robot_state = 0;
uint32_t g_robot_type = 0;

//ilife_ota_info_t ilife_ota_info;
//uint8_t mcu_update_state = 0;

static void ilife_wifi_notify_mcu(void * arg)
{
   if (g_wifi_config == 0)
    {
        ilife_send_wifi_state(ilife_wifi_get_state());
    }
}

static void ilife_wifi_reconnect_reboot(void * arg)
{
    LOG_TRACE("ilife_wifi_reconnect_reboot: current is %d", ilife_wifi_get_state());
    if (ilife_wifi_get_state() != WIFI_STATE_CON_CLOUD)
    {
        LOG_TRACE("ilife_wifi_reconnect_reboot: reboot now~~~~~~~~~~~~~~~~~~~~~~~");
        HAL_Reboot();
    }
}

void ilife_wifi_set_state(uint8_t state)
{
    static uint8_t last_state = 0;
    uint8_t need_reboot = 0;

    LOG_TRACE("ilife_wifi_set_state: current is %d, want to %d", g_wifi_state, state);

    if ((g_wifi_state == WIFI_STATE_CON_CLOUD) && ((state == WIFI_STATE_DISCON_CLOUD) || (state == WIFI_STATE_DISCON_ROUTER)))
    {
        LOG_TRACE("ilife_wifi_set_state: need_reboot~~~~~~~~~~~~~~~~~~~~~~~");
        need_reboot = 1;
        //HAL_Reboot();
        //aos_post_delayed_action(2000, ilife_wifi_reconnect_reboot, NULL);
    }

    g_wifi_state = state;

    // if (g_wifi_config == 1)
    // {
    //     return;
    // }
    //if (g_wifi_state != last_state)
    {
        aos_post_delayed_action(10, ilife_wifi_notify_mcu, NULL);
    }
    if (need_reboot == 1)
    {
        HAL_SleepMs(500);
        HAL_Reboot();
    }    
}

uint8_t ilife_wifi_get_state(void)
{
    return g_wifi_state;
}

static void ilife_wakeup_robot(void *p)
{
    int ret = 0;
    uint8_t recv[512] = {0};
    uint16_t recv_len = 0;
    uint8_t cnt = 0;

    LOG_TRACE("ilife_wakeup_robot gpio_wake_up high");
    ilife_wake_up_gpio_init_output();
    hal_gpio_output_high(&gpio_wake_up);

    if ((g_robot_type == 0x45) || (g_robot_type == 0x50))
    {
        ilife_send_wifi_state(WIFI_STATE_POWER_ON);
    }

    while (g_wifi_sleep_state != WIFI_STATE_WAKEUP)
    {
        cnt++;
        aos_msleep(50);
        if (cnt == 10)//500ms
        {
            LOG_TRACE("ilife_wakeup_robot gpio_wake_up low1");
            ilife_wake_up_gpio_init_input();
        }
        else if (cnt == 100)//5s
        {
            //g_wifi_sleep_state = WIFI_STATE_WAKEUP;
            LOG_TRACE("ilife_wakeup_robot gpio_wake_up high22");
            ilife_wake_up_gpio_init_output();
            hal_gpio_output_high(&gpio_wake_up);
            aos_msleep(200);
            LOG_TRACE("ilife_wakeup_robot gpio_wake_up low22");
            ilife_wake_up_gpio_init_input();
            //break;
        }
        else if (cnt > 120)//6s
        {
            g_wifi_sleep_state = WIFI_STATE_SLEEP;
            break;
        }
    }
    aos_msleep(50);
    LOG_TRACE("ilife_wakeup_robot gpio_wake_up low2");
    ilife_wake_up_gpio_init_input();
//    hal_gpio_output_low(&gpio_wake_up);
//    aos_msleep(2000);
    while (1)
    {
        ret = aos_queue_recv(&g_wait_wake_queue, 100, recv, &recv_len);
        if (ret == 0)
        {
            LOG_TRACE("g_wait_wake_queue send data to MCU!");
            hfilop_uart_send_data(recv, recv_len);
        }
        else
        {
            LOG_TRACE("g_wait_wake_queue Receive data ERR! ret = %d", ret);
            //g_wifi_sleep_state = WIFI_STATE_WAKEUP;
            break;
        }
    }
    g_ilife_wakeup_robot_state = 0;
    aos_task_exit(0);
}

uint8_t ilife_deal_robot_wakeup(uint8_t *data, uint32_t len)
{
    int ret = 0;
    uint8_t recv[10] = {0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00};
    uint16_t recv_len = 10;

//    if ((g_robot_type == 0x66) || (g_robot_type == 0x80) || (g_robot_type == 0x84))
    
    if ((g_robot_type == 0x66) || (g_robot_type == 0x80) || (g_robot_type == 0x84) || (g_robot_type == 0x45) || (g_robot_type == 0x50))
    {
        if ((g_wifi_sleep_state == WIFI_STATE_SLEEP) && (g_ilife_wakeup_robot_state == 0))
        {
            LOG_TRACE("g_robot_type = 0x%04X, data 5 : 0x%02x", g_robot_type, data[5]);
            if (data[5] == METHOD_ACK)
            {
                LOG_TRACE("ilife_deal_robot_wakeup METHOD_ACK do not send to mcu");
                return 0;
            }
            ret = aos_queue_send(&g_wait_wake_queue, data, len);
            if (ret != 0)
            {
                LOG_TRACE("g_wait_wake_queue send fail! ret = %d", ret);
            }
            g_wifi_sleep_state = WIFI_STATE_WAKEING;
            g_ilife_wakeup_robot_state = 1;
            LOG_TRACE("start ilife_wakeup_robot...");
            aos_task_new("ilife_wakeup_robot", ilife_wakeup_robot, NULL, 2048);
            return 0;
        }
        else if ((g_wifi_sleep_state == WIFI_STATE_WAKEING) || (g_ilife_wakeup_robot_state == 1))
        {
            LOG_TRACE("g_robot_type = 0x%04X, g_wifi_sleep_state = %d", g_robot_type, g_wifi_sleep_state);
            ret = aos_queue_send(&g_wait_wake_queue, data, len);
            if (ret !=0 )
            {
                LOG_TRACE("g_wait_wake_queue send fail! ret = %d", ret);
            }
            return 0;
        }
        else
        {
            LOG_TRACE("g_robot_type = 0x%04X, g_wifi_sleep_state = %d", g_robot_type, g_wifi_sleep_state);
            return 1;
        }
    }
    else if (g_wifi_sleep_state == WIFI_STATE_SLEEP)
    {
        LOG_TRACE("g_robot_type = 0x%04X, g_wifi_sleep_state = %d?? data 5 : 0x%02x", g_robot_type, g_wifi_sleep_state, data[5]);
        if (data[5] == METHOD_ACK)
        {
            LOG_TRACE("ilife_deal_robot_wakeup METHOD_ACK do not send to mcu");
            return 0;
        }
        g_wifi_sleep_state = WIFI_STATE_WAKEUP;
        hal_gpio_output_high(&gpio_wake_up);
        hfilop_uart_send_data(recv, recv_len);
        aos_msleep(50);
        hal_gpio_output_low(&gpio_wake_up);
        return 1;
    }
    return 1;
}

uint8_t ilife_covChar2value(char c)
{
    uint8_t value = 0;
    if ((c >= 'A') && (c <= 'F'))
    {
        value = c - 'A' + 10;
    }
    else if ((c >= 'a') && (c <= 'f'))
    {
        value = c - 'a' + 10;
    }
    else
    {
        value = c - '0';
    }
//    LOG_TRACE("ilife_covChar2value c = %c, value = 0x%02x", c, value);
    return value;
}

uint32_t ilife_getMcuOtaVer(char *str)
{
    uint32_t ver = 0;
    
    ver = ilife_covChar2value(str[4]);
    ver = ver << 4;
    ver += ilife_covChar2value(str[5]);
    ver = ver << 4;
    ver += ilife_covChar2value(str[7]);
    ver = ver << 4;
    ver += ilife_covChar2value(str[8]);
    ver = ver << 4;
    ver += ilife_covChar2value(str[10]);
    ver = ver << 4;
    ver += ilife_covChar2value(str[11]);
    ver = ver << 4;
    ver += ilife_covChar2value(str[13]);
    ver = ver << 4;
    ver += ilife_covChar2value(str[14]);

    LOG_TRACE("ilife_getMcuOtaVer ver = 0x%02x", ver);
    return ver;
}

int ilife_StringToHex(char *str, uint8_t *out, unsigned int *outlen)
{
    char *p = str;
    char high = 0, low = 0;
    int tmplen = strlen(p), cnt = 0;
    tmplen = strlen(p);
    //LOG_TRACE("ilife_StringToHex len = %d", tmplen);
    while(cnt < (tmplen / 2))
    {
        //high = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
        if ((*p >= 'A') && (*p <= 'F'))
        {
            high = *p - 'A' + 10;
        }
        else if ((*p >= 'a') && (*p <= 'f'))
        {
            high = *p - 'a' + 10;
        }
        else
        {
            high = *p - '0';
        }
        //LOG_TRACE("ilife_StringToHex str = %s, high = 0x%02x", *p, high);
        p ++;
		//low = (*(++ p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *(p) - 48 - 7 : *(p) - 48;
        if ((*p >= 'A') && (*p <= 'F'))
        {
            low = *p - 'A' + 10;
        }
        else if ((*p >= 'a') && (*p <= 'f'))
        {
            low = *p - 'a' + 10;
        }
        else
        {
            low = *p - '0';
        }
        out[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));

        //LOG_TRACE("ilife_StringToHex str = %s, low = 0x%02x", *p, low);

        p ++;
        cnt ++;
    }
    if(tmplen % 2 != 0)
    {
        if ((*p >= 'A') && (*p <= 'F'))
        {
            out[cnt] = *p - 'A' + 10;
        }
        else if ((*p >= 'a') && (*p <= 'f'))
        {
            out[cnt] = *p - 'a' + 10;
        }
        else
        {
            out[cnt] = *p - '0';
        }
    }
    
    if(outlen != NULL) *outlen = tmplen / 2 + tmplen % 2;
    return tmplen / 2 + tmplen % 2;
}



void ilife_get_time(uint32_t time_stamp, ilife_time_t *data)
{
    uint8_t const year_a[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint8_t const year_b[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint16_t const year_c[4] = {366, 365, 365, 365};

    uint8_t i;

    data->sec = time_stamp % 60;
    data->min = time_stamp % 3600 / 60;
    data->hour = time_stamp / 3600 % 24;

    data->year = 1970;
    data->mon = 0;
    data->day = 0;

    time_stamp = (time_stamp / 86400) + 1;

    data->wday = (time_stamp + 3) % 7;//1970.1.1 ????

    i = data->year % 4;

    while (time_stamp > year_c[i])
    {
        time_stamp -= year_c[i];
        data->year++;
        i = data->year % 4;
    }

    i = 0;
    if (data->year % 4 == 0)
    {
        while (time_stamp > year_b[i])
        {
            time_stamp -= year_b[i];
            data->mon++;
            i = data->mon % 12;
        }
    }
    else
    {
        while (time_stamp > year_a[i])
        {
            time_stamp -= year_a[i];
            data->mon++;
            i = data->mon % 12;
        }
    }
    data->mon++;
    data->day = time_stamp;
}

user_example_ctx_t *user_example_get_ctx(void)
{
    return &g_user_example_ctx;
}

void *example_malloc(size_t size)
{
    return HAL_Malloc(size);
}

void example_free(void *ptr)
{
    HAL_Free(ptr);
}

void update_power_state(int state)
{
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();

    user_example_ctx->status.powerswitch = state;
    user_example_ctx->status.all_powerstate = state;
}

static void user_deviceinfo_update(void)
{
    int res = 0;
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    char *device_info_update = "[{\"attrKey\":\"OutletFWVersion\",\"attrValue\":\"smo_1.5.1\"}]";

    res = IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_DEVICEINFO_UPDATE,
            (unsigned char *)device_info_update, strlen(device_info_update));
    LOG_TRACE("Device Info Update Message ID: %d", res);
}

static int user_connected_event_handler(void)
{
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();

    LOG_TRACE("Cloud Connected");
    awss_config_press_start_flag=0;
    user_example_ctx->cloud_connected = 1;
	aos_post_event(EV_YUNIO, CODE_YUNIO_ON_CONNECTED, 0);
    set_net_state(CONNECT_CLOUD_SUCCESS);
    //user_post_property_after_connected();
    hfsys_set_link_low();
    ilife_wifi_set_state(WIFI_STATE_CON_CLOUD);
    user_deviceinfo_update();
    return 0;
}

static int user_disconnected_event_handler(void)
{
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();

    LOG_TRACE("Cloud Disconnected");

    set_net_state(CONNECT_CLOUD_FAILED);

    ilife_wifi_set_state(WIFI_STATE_DISCON_CLOUD);
    user_example_ctx->cloud_connected = 0;

    return 0;
}



static int property_seq_handle(const char *seqenceId, recv_msg_t * msg)
{
    char userId[8], timeStr[16], *ptr;
    int isDiscard = 1, i;
    static int repalce_index = 0;

    if (strlen(seqenceId) > sizeof(msg->seq)) {
        LOG_TRACE("seq to long !!!");
        return -1;
    }
    if (NULL != (ptr = strchr(seqenceId, '@'))) {
        int len = ptr - seqenceId;

        if (len > (sizeof(userId) -1))
            len = sizeof(userId) -1;
        memset(userId, 0, sizeof(userId));
        memcpy(userId, seqenceId, len);

        len = strlen(seqenceId) - strlen(userId) - 1;
        if (len > (sizeof(timeStr) -1))
            len = sizeof(timeStr) -1;
        memset(timeStr, 0, sizeof(timeStr));
        memcpy(timeStr, ptr + 1, len);

        uint64_t time = atoll(timeStr);
        for (i = 0; i < MAX_USER; i++) {
            if (!strcmp(userId, seqence_info[i].userId)) {
                if (time >= seqence_info[i].timeStampe) {
                    isDiscard = 0;
                    seqence_info[i].timeStampe = time;
                }
                break;
            }
        }
        if (i == MAX_USER) {
            for (i = 0; i < MAX_USER; i++) {
                if (!seqence_info[i].isValid) {
                    strcpy(seqence_info[i].userId, userId);
                    seqence_info[i].timeStampe = time;
                    isDiscard = 0;
                    seqence_info[i].isValid = 1;
                    LOG_TRACE("new user %s", userId);
                    break;
                }
            }
            if (i == MAX_USER) {
                strcpy(seqence_info[repalce_index].userId, userId);
                seqence_info[repalce_index].timeStampe = time;
                isDiscard = 0;
                seqence_info[repalce_index].isValid = 1;
                repalce_index = (++repalce_index) % MAX_USER;
                LOG_TRACE("replace new user %s index %d", userId, repalce_index);
            }
        }
    }
    if (isDiscard == 1) {
        LOG_TRACE("Discard msg !!!");
        return -1;
    }
    if (NULL != seqenceId)
        strncpy(msg->seq, seqenceId, sizeof(msg->seq) - 1);

    return 0;
}

static int property_setting_handle(const char *request, const int request_len, recv_msg_t * msg)
{
    cJSON *root = NULL, *item = NULL;
    int ret = -1;

    if ((root = cJSON_Parse(request)) == NULL) {
        LOG_TRACE("property set payload is not JSON format");
        return -1;
    }

    if ((item = cJSON_GetObjectItem(root, "setPropsExtends")) != NULL && cJSON_IsObject(item)) {
        int isDiscard = 0;
        cJSON *seq = NULL, *flag = NULL;

        if ((seq = cJSON_GetObjectItem(item, "seq")) != NULL && cJSON_IsString(seq)) {
            if (property_seq_handle(seq->valuestring, msg)) {
                isDiscard = 1;
            }
        }
        if (isDiscard == 0) {
            if ((flag = cJSON_GetObjectItem(item, "flag")) != NULL && cJSON_IsNumber(flag)) {
                msg->flag = flag->valueint;
            } else {
                msg->flag = 0;
            }
        } else {
            cJSON_Delete(root);
            return 0;
        }
    }
    if ((item = cJSON_GetObjectItem(root, "powerstate")) != NULL && cJSON_IsNumber(item)) {
        msg->powerswitch = item->valueint;
        msg->all_powerstate = msg->powerswitch;
        ret = 0;
    }
#ifdef TSL_FY_SUPPORT /* support old feiyan TSL */
    else if ((item = cJSON_GetObjectItem(root, "PowerSwitch")) != NULL && cJSON_IsNumber(item)) {
        msg->powerswitch = item->valueint;
        ret = 0;
    }
#endif
    else if ((item = cJSON_GetObjectItem(root, "allPowerstate")) != NULL && cJSON_IsNumber(item)) {
        msg->powerswitch = item->valueint;
        msg->all_powerstate = msg->powerswitch;
        ret = 0;
    }
#ifdef AOS_TIMER_SERVICE
    else if (((item = cJSON_GetObjectItem(root, "LocalTimer")) != NULL && cJSON_IsArray(item)) || \
        ((item = cJSON_GetObjectItem(root, "CountDownList")) != NULL && cJSON_IsObject(item)) || \
        ((item = cJSON_GetObjectItem(root, "PeriodTimer")) != NULL && cJSON_IsObject(item)) || \
        ((item = cJSON_GetObjectItem(root, "RandomTimer")) != NULL && cJSON_IsObject(item)))
    {
        // Before LocalTimer Handle, Free Memory
        cJSON_Delete(root);
        timer_service_property_set(request);
        user_example_ctx_t *user_example_ctx = user_example_get_ctx();
        IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_PROPERTY,
                (unsigned char *)request, request_len);
        return 0;
    }
#endif
#ifdef AIOT_DEVICE_TIMER_ENABLE
    else if ((item = cJSON_GetObjectItem(root, DEVICETIMER)) != NULL && cJSON_IsArray(item))
    {
        // Before LocalTimer Handle, Free Memory
        cJSON_Delete(root);
        ret = deviceTimerParse(request, 0, 1);
        user_example_ctx_t *user_example_ctx = user_example_get_ctx();
        if (ret == 0) {
            IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_PROPERTY,
                (unsigned char *)request, request_len);
        } else {
            char *report_fail = "{\"DeviceTimer\":[]}";
            IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_PROPERTY,
                    (unsigned char *)report_fail, strlen(report_fail));
            ret = -1;
        }
        // char *property = device_timer_post(1);
        // if (property != NULL)
        //     HAL_Free(property);
        return 0;
    }
    #ifdef MULTI_ELEMENT_TEST
    else if (propertys_handle(root) >= 0) {
        user_example_ctx_t *user_example_ctx = user_example_get_ctx();
        cJSON_Delete(root);
        IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_PROPERTY,
                (unsigned char *)request, request_len);
        return 0;
    }
    #endif
#endif
    else {
        LOG_TRACE("property set payload is not JSON format");
        ret = -1;
    }

    cJSON_Delete(root);
    if (ret != -1)
        send_msg_to_queue(msg);

    return ret;
}

static int user_service_request_event_handler(const int devid, const char *serviceid, const int serviceid_len,
        const char *request, const int request_len,
        char **response, int *response_len)
{
    cJSON *root = NULL;

#ifdef CERTIFICATION_TEST_MODE
    return ct_main_service_request_event_handler(devid, serviceid, serviceid_len, request, request_len, response, response_len);
#endif

    LOG_TRACE("Service Request Received, Devid: %d, Service ID: %.*s, Payload: %s", devid, serviceid_len, serviceid,
            request);

    HFILOP_PRINTF("devid=%d serviceid=%s serviceid_len=%d request=%s request_len=%d--------\r\n",devid,serviceid,serviceid_len,request,request_len);
	if(g_hfilop_config.ilop_mode==ILOP_MODE_JSON)//20190527
	{
		ilop_server_jsdata_down(serviceid,serviceid_len,request);
		print_heap();

	}
	else
	{
	    /* Parse Root */
	    root = cJSON_Parse(request);
	    if (root == NULL || !cJSON_IsObject(root))
		{
	        LOG("JSON Parse Error");
	        return -1;
	    }
		char  property_buf[64] = { 0 };

		char server_property[32]={0};
		strncpy(server_property,serviceid,serviceid_len);
		HFILOP_PRINTF("server_property=%s---------\r\n",server_property);
		char * off =request;
		char *first_sign=NULL;
		char *second_sige=NULL;
		char get_property[50]={0};
		char value_str[20] ={0};
		int data=0;
		while(strlen(off)>=1)
		{
			first_sign=NULL;
			first_sign=strstr(off,"\"");
			if(first_sign !=NULL)
			{
				HFILOP_PRINTF("find first_sign------%s----\r\n",first_sign);
				second_sige=NULL;
				off=first_sign+1;
				second_sige=strstr(first_sign+1,"\"");
				if(second_sige !=NULL)
				{
					HFILOP_PRINTF("find second_sige------%s----\r\n",second_sige);
					strncpy(get_property,first_sign+1,second_sige-first_sign-1);
					get_property[second_sige-first_sign-1]='\0';
					HFILOP_PRINTF("sg get_property=%s------\r\n",get_property);
					off =get_one_data_from_str(get_property,second_sige);//20190514

				}else
				{
					HFILOP_PRINTF("----------no find second_sige   \r\n");
					break;
				}
			}
			else
			{
				HFILOP_PRINTF("----------no find first_sign \r\n");
				break;
			}
		}
	}

	const char *response_fmt=NULL;
	response_fmt = (char *)HAL_Malloc(request_len);
	
	/* Send Service Response To Cloud */
        *response_len = request_len+11;
        *response = (char *)HAL_Malloc(*response_len);
        if (*response == NULL) {
            LOG_TRACE("Memory Not Enough");
            cJSON_Delete(root);
            return -1;
        }
        memset(*response, 0, RESPONE_BUFFER_SIZE);
        memcpy(*response, response_fmt, strlen(response_fmt));
	 HFILOP_PRINTF("response=%s ---------",*response);
        *response_len = strlen(*response);
    
    cJSON_Delete(root);
    return 0;
}

#ifdef EN_COMBO_NET
#ifdef EN_COMBO_BLE_CONTROL
int user_ble_serv_request_event_handler(const int devid, const char *serviceid, const int serviceid_len,
        const char *request, const int request_len)
{
    LOG_TRACE("BLE Service Request Received, Devid: %d, Service ID: %.*s, Payload: %s", devid, serviceid_len, serviceid,
            request);
    cJSON *root = cJSON_Parse(request);
    if (root == NULL || !cJSON_IsObject(root)) {
        LOG_TRACE("JSON Parse Error");
        return -1;
    }

    if (strlen("CommonService") == serviceid_len && memcmp("CommonService", serviceid, serviceid_len) == 0) {
        cJSON *item;
        recv_msg_t msg;
        int isDiscard = 0;

        strcpy(msg.seq, SPEC_SEQ);
        if ((item = cJSON_GetObjectItem(root, "seq")) != NULL && cJSON_IsString(item)) {
            if (property_seq_handle(item->valuestring, &msg)) {
                isDiscard = 1;
            }
        }
        if (isDiscard == 0) {
            if ((item = cJSON_GetObjectItem(root, "flag")) != NULL && cJSON_IsNumber(item)) {
                msg.flag = item->valueint;
            } else {
                msg.flag = 0;
            }

            if ((item = cJSON_GetObjectItem(root, "method")) != NULL && cJSON_IsNumber(item)) {
                msg.method = item->valueint;
            } else {
                msg.method = 0;
            }

            if ((item = cJSON_GetObjectItem(root, "params")) != NULL && cJSON_IsString(item)) {
                if (msg.method == 0) {
                    msg.from = FROM_SERVICE_SET;
                    property_setting_handle(item->valuestring, strlen(item->valuestring), &msg);
                } else
                    LOG_TRACE("todo!!");
            }
        }
    }
    cJSON_Delete(root);

    return 0;
}

int user_ble_property_set_event_handler(const int devid, const char *request, const int request_len)
{
    int ret = 0;
    recv_msg_t msg;

    LOG_TRACE("BLE property set,  Devid: %d, payload: \"%s\"", devid, request);
    msg.from = FROM_PROPERTY_SET;
    strcpy(msg.seq, SPEC_SEQ);
    property_setting_handle(request, request_len, &msg);
    return ret;
}

int user_ble_property_get_event_handler(const int devid, const char *request, const int request_len, char **response,
        int *response_len)
{
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    device_status_t *device_status = &user_example_ctx->status;
    cJSON *request_root = NULL, *item_propertyid = NULL;
    cJSON *response_root = NULL;

    LOG_TRACE("BLE Property Get Received, Devid: %d, Request: %s", devid, request);
    request_root = cJSON_Parse(request);
    if (request_root == NULL || !cJSON_IsArray(request_root)) {
        LOG_TRACE("JSON Parse Error");
        return -1;
    }

    response_root = cJSON_CreateObject();
    if (response_root == NULL) {
        LOG_TRACE("No Enough Memory");
        cJSON_Delete(request_root);
        return -1;
    }

    for (int index = 0; index < cJSON_GetArraySize(request_root); index++) {
        item_propertyid = cJSON_GetArrayItem(request_root, index);
        if (item_propertyid == NULL || !cJSON_IsString(item_propertyid)) {
            LOG_TRACE("JSON Parse Error");
            cJSON_Delete(request_root);
            cJSON_Delete(response_root);
            return -1;
        }

        LOG_TRACE("Property ID, index: %d, Value: %s", index, item_propertyid->valuestring);
#ifdef TSL_FY_SUPPORT
        if (strcmp("PowerSwitch", item_propertyid->valuestring) == 0) {
            cJSON_AddNumberToObject(response_root, "PowerSwitch", device_status->powerswitch);
        }
#endif
        if (strcmp("powerstate", item_propertyid->valuestring) == 0) {
            cJSON_AddNumberToObject(response_root, "powerstate", device_status->powerswitch);
        }
        if (strcmp("allPowerstate", item_propertyid->valuestring) == 0) {
            cJSON_AddNumberToObject(response_root, "allPowerstate", device_status->all_powerstate);
        }
#ifdef AOS_TIMER_SERVICE
        else if (strcmp("LocalTimer", item_propertyid->valuestring) == 0) {
            char *local_timer_str = NULL;

            if (NULL != (local_timer_str = timer_service_property_get("[\"LocalTimer\"]"))) {
                LOG_TRACE("local_timer %s", local_timer_str);
                cJSON *property = NULL, *value = NULL;

                property = cJSON_Parse(local_timer_str);
                if (property == NULL) {
                    LOG_TRACE("No Enough Memory");
                    continue;
                }
                value = cJSON_GetObjectItem(property, "LocalTimer");
                if (value == NULL) {
                    LOG_TRACE("No Enough Memory");
                    cJSON_Delete(property);
                    continue;
                }
                cJSON *dup_value = cJSON_Duplicate(value, 1);

                cJSON_AddItemToObject(response_root, "LocalTimer", dup_value);
                cJSON_Delete(property);
                example_free(local_timer_str);
            } else {
                cJSON *array = cJSON_CreateArray();
                cJSON_AddItemToObject(response_root, "LocalTimer", array);
            }
        } else if (strcmp("CountDownList", item_propertyid->valuestring) == 0) {
            char *count_down_list_str = NULL;

            if (NULL != (count_down_list_str = timer_service_property_get("[\"CountDownList\"]"))) {
                LOG_TRACE("CountDownList %s", count_down_list_str);
                cJSON *property = NULL, *value = NULL;

                property = cJSON_Parse(count_down_list_str);
                if (property == NULL) {
                    LOG_TRACE("No Enough Memory");
                    continue;
                }
                value = cJSON_GetObjectItem(property, "CountDownList");
                if (value == NULL) {
                    LOG_TRACE("No Enough Memory");
                    cJSON_Delete(property);
                    continue;
                }
                cJSON *dup_value = cJSON_Duplicate(value, 1);

                cJSON_AddItemToObject(response_root, "CountDownList", dup_value);
                cJSON_Delete(property);
                example_free(count_down_list_str);
            } else {
                cJSON_AddStringToObject(response_root, "CountDownList", "");
            }
#endif
        }
    }
    cJSON_Delete(request_root);

    *response = cJSON_PrintUnformatted(response_root);
    if (*response == NULL) {
        LOG_TRACE("cJSON_PrintUnformatted Error");
        cJSON_Delete(response_root);
        return -1;
    }
    cJSON_Delete(response_root);
    *response_len = strlen(*response);

    LOG_TRACE("Property Get Response: %s", *response);

    return SUCCESS_RETURN;
}
#endif
#endif
int32_t app_parse_property(const char *request, uint32_t request_len)
{
    cJSON *powerstate = NULL;
    cJSON *req = cJSON_Parse(request);
    if (req == NULL || !cJSON_IsObject(req))
    {
        return -0x911;
    }
    powerstate = cJSON_GetObjectItem(req, "powerstate");
    if (powerstate != NULL && cJSON_IsNumber(powerstate))
    {
        /* process property powerstate here */
		set_relay_fun(powerstate->valueint);
        printf("property id: powerstate, value: %d", powerstate->valueint);
    }
    cJSON_Delete(req);
}

static int user_property_set_event_handler(const int devid, const char *request, const int request_len)
{
  print_heap();
    int res = 0;
	  char value_str[20] ={0};
	  int data=0;
	  char down_data[500]={0};
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    LOG("Property Set Received, Devid: %d, Request: %s", devid, request);
	char  property_buf[64] = { 0 };
	char  property_head[64]={0};
	char get_property[100]={0};
	int data_type=0;
		char *first_sign=NULL;
		char *second_sige=NULL;
	if(g_hfilop_config.ilop_mode==ILOP_MODE_JSON)//20190527
	{
		ilop_server_jsdata_down(NULL,0,request);
		res = IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_PROPERTY,
                             (unsigned char *)request, request_len);
    LOG("Post Property Message ID: %d", res);
 
	print_heap();

    return 0;
	}
	cJSON * data_JSON = cJSON_Parse(request);
    if(data_JSON == NULL)
    {
        cJSON_Delete(data_JSON);
        return 0;
    }
	char *	off=strstr(request,":{");
	if(off !=NULL )
	{
		data_type=1;
		HFILOP_PRINTF("-------------down data is mary data-- %d----%s-----\r\n",off,off);
		
	}
	if(data_type==0)
	{
		first_sign=strstr(request,"\"");
		if(first_sign!=NULL)
		{
			HFILOP_PRINTF("find first_sign------%s----\r\n",first_sign);
		}
		second_sige=strstr(first_sign+1,"\"");
		if(second_sige !=NULL)
		{
			HFILOP_PRINTF("find second_sige------%s----\r\n",second_sige);
			strncpy(get_property,first_sign+1,second_sige-first_sign-1);
			HFILOP_PRINTF("HF get_property =%s ---------\r\n",get_property);
			if(strcmp(get_property,"LocalTimer")==0)
			{
				localtimer(request);
			}else
			{
			
			 get_one_data_from_str(get_property,second_sige);//20190514
			}
		}
	}else if(data_type==1)
	{
		first_sign=strstr(request,"\"");
		if(first_sign!=NULL)
		{
			HFILOP_PRINTF("find first_sign------%s----\r\n",first_sign);
		}
		second_sige=strstr(first_sign+1,"\"");
		if(second_sige !=NULL)
		{
		HFILOP_PRINTF("find second_sige------%s----\r\n",second_sige);
		strncpy(property_head,first_sign+1,second_sige-first_sign-1);
		HFILOP_PRINTF("get_property head is %s ------\r\n",property_head);
		}
		while(strlen(off)>2)
		{
			first_sign=NULL;
			first_sign=strstr(off,"\"");
			if(first_sign !=NULL)
			{
				HFILOP_PRINTF("find first_sign------%s----\r\n",first_sign);
				second_sige=NULL;
				off=first_sign+1;
				second_sige=strstr(first_sign+1,"\"");
				if(second_sige !=NULL)
				{
					//off=second_sige+3;
					HFILOP_PRINTF("find second_sige------%s----\r\n",second_sige);
					strncpy(get_property,first_sign+1,second_sige-first_sign-1);
					get_property[second_sige-first_sign-1]='\0';
					HFILOP_PRINTF("sg get_property=%s------\r\n",get_property);
					snprintf(property_buf, sizeof(property_buf), "%s.%s", property_head, get_property);
					off=get_data_from_str(get_property,second_sige,property_head);//20190514
					if(off==0)
					{
						HFILOP_PRINTF("----------return error \r\n");
						break;
					}
				}else
				{
					HFILOP_PRINTF("----------no find second_sige   \r\n");
					break;
				}
			}
			else
			{
				HFILOP_PRINTF("----------no find first_sign \r\n");
				break;
			}
	 }
	
	}
    res = IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_PROPERTY,
                             (unsigned char *)request, request_len);
    LOG("Post Property Message ID: %d", res);
    cJSON_Delete(data_JSON);
	print_heap();

}

#ifdef ALCS_ENABLED
static int user_property_get_event_handler(const int devid, const char *request, const int request_len, char **response,
        int *response_len)
{
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    device_status_t *device_status = &user_example_ctx->status;
    cJSON *request_root = NULL, *item_propertyid = NULL;
    cJSON *response_root = NULL;

#ifdef CERTIFICATION_TEST_MODE
    return ct_main_property_get_event_handler(devid, request, request_len, response, response_len);
#endif

    LOG_TRACE("Property Get Received, Devid: %d, Request: %s", devid, request);
    request_root = cJSON_Parse(request);
    if (request_root == NULL || !cJSON_IsArray(request_root)) {
        LOG_TRACE("JSON Parse Error");
        return -1;
    }

    response_root = cJSON_CreateObject();
    if (response_root == NULL) {
        LOG_TRACE("No Enough Memory");
        cJSON_Delete(request_root);
        return -1;
    }
    int index = 0;
    for (index = 0; index < cJSON_GetArraySize(request_root); index++) {
        item_propertyid = cJSON_GetArrayItem(request_root, index);
        if (item_propertyid == NULL || !cJSON_IsString(item_propertyid)) {
            LOG_TRACE("JSON Parse Error");
            cJSON_Delete(request_root);
            cJSON_Delete(response_root);
            return -1;
        }
        LOG_TRACE("Property ID, index: %d, Value: %s", index, item_propertyid->valuestring);
        if (strcmp("powerstate", item_propertyid->valuestring) == 0) {
            cJSON_AddNumberToObject(response_root, "powerstate", device_status->powerswitch);
        }
        else if (strcmp("allPowerstate", item_propertyid->valuestring) == 0) {
            cJSON_AddNumberToObject(response_root, "allPowerstate", device_status->all_powerstate);
        }
#ifdef TSL_FY_SUPPORT /* support old feiyan TSL */
        else if (strcmp("PowerSwitch", item_propertyid->valuestring) == 0) {
            cJSON_AddNumberToObject(response_root, "PowerSwitch", device_status->powerswitch);
        }
#endif
#ifdef AOS_TIMER_SERVICE
        else if (strcmp("LocalTimer", item_propertyid->valuestring) == 0) {
            char *local_timer_str = NULL;

            if (NULL != (local_timer_str = timer_service_property_get("[\"LocalTimer\"]"))) {
                LOG_TRACE("local_timer %s", local_timer_str);
                cJSON *property = NULL, *value = NULL;

                property = cJSON_Parse(local_timer_str);
                if (property == NULL) {
                    LOG_TRACE("No Enough Memory");
                    continue;
                }
                value = cJSON_GetObjectItem(property, "LocalTimer");
                if (value == NULL) {
                    LOG_TRACE("No Enough Memory");
                    cJSON_Delete(property);
                    continue;
                }
                cJSON *dup_value = cJSON_Duplicate(value, 1);

                cJSON_AddItemToObject(response_root, "LocalTimer", dup_value);
                cJSON_Delete(property);
                example_free(local_timer_str);
            } else {
                cJSON *array = cJSON_CreateArray();
                cJSON_AddItemToObject(response_root, "LocalTimer", array);
            }
        } else if (strcmp("CountDownList", item_propertyid->valuestring) == 0) {
            char *count_down_list_str = NULL;

            if (NULL != (count_down_list_str = timer_service_property_get("[\"CountDownList\"]"))) {
                LOG_TRACE("CountDownList %s", count_down_list_str);
                cJSON *property = NULL, *value = NULL;

                property = cJSON_Parse(count_down_list_str);
                if (property == NULL) {
                    LOG_TRACE("No Enough Memory");
                    continue;
                }
                value = cJSON_GetObjectItem(property, "CountDownList");
                if (value == NULL) {
                    LOG_TRACE("No Enough Memory");
                    cJSON_Delete(property);
                    continue;
                }
                cJSON *dup_value = cJSON_Duplicate(value, 1);

                cJSON_AddItemToObject(response_root, "CountDownList", dup_value);
                cJSON_Delete(property);
                example_free(count_down_list_str);
            } else {
                cJSON_AddStringToObject(response_root, "CountDownList", "");
            }
        }
#endif
    }

    cJSON_Delete(request_root);

    *response = cJSON_PrintUnformatted(response_root);
    if (*response == NULL) {
        LOG_TRACE("cJSON_PrintUnformatted Error");
        cJSON_Delete(response_root);
        return -1;
    }
    cJSON_Delete(response_root);
    *response_len = strlen(*response);

    LOG_TRACE("Property Get Response: %s", *response);
    return SUCCESS_RETURN;
}
#endif

static int user_report_reply_event_handler(const int devid, const int msgid, const int code, const char *reply,
        const int reply_len)
{
    const char *reply_value = (reply == NULL) ? ("NULL") : (reply);
    const int reply_value_len = (reply_len == 0) ? (strlen("NULL")) : (reply_len);

    //LOG_TRACE("Message Post Reply Received, Devid: %d, Message ID: %d, Code: %d, Reply: %.*s", devid, msgid, code,
    //        reply_value_len, reply_value);
    return 0;
}

static int user_trigger_event_reply_event_handler(const int devid, const int msgid, const int code, const char *eventid,
        const int eventid_len, const char *message, const int message_len)
{
    LOG_TRACE("Trigger Event Reply Received, Devid: %d, Message ID: %d, Code: %d, EventID: %.*s, Message: %.*s", devid,
            msgid, code, eventid_len, eventid, message_len, message);

    return 0;
}

#ifdef AOS_TIMER_SERVICE
static void timer_service_cb(const char *report_data, const char *property_name, int i_value,
        double d_value, const char *s_value, int prop_idx)
{
    if (prop_idx >= NUM_OF_CONTROL_TARGETS) {
        LOG_TRACE("ERROR: prop_idx=%d is out of limit=%d", prop_idx, NUM_OF_CONTROL_TARGETS);
    }
    if (report_data != NULL)	/* post property to cloud */
        user_post_property_json(report_data);
    if (property_name != NULL) {	/* set value to device */
        LOG_TRACE("timer event callback: property_name=%s prop_idx=%d", property_name, prop_idx);
        if ((prop_idx < num_of_tsl_type[0] && strcmp(control_targets_list[0], property_name) == 0)
            ||(prop_idx < num_of_tsl_type[0] && strcmp(control_targets_list[1], property_name) == 0)) {
            recv_msg_t msg;

            msg.powerswitch = i_value;
            msg.all_powerstate = i_value;
            msg.flag = 0x00;
            strcpy(msg.seq, SPEC_SEQ);
            send_msg_to_queue(&msg);
            LOG_TRACE("timer event callback: int_value=%d", i_value);
            /* set int value */
        } else if (prop_idx < num_of_tsl_type[0] + num_of_tsl_type[1]) {
            LOG_TRACE("timer event callback: double_value=%f", d_value);
            /* set doube value */
        } else {
            if (s_value != NULL)
                LOG_TRACE("timer event callback: test_value=%s", s_value);
            /* set string value */
        }

    }

    return;
}
#endif
static int user_initialized(const int devid)
{
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    LOG_TRACE("Device Initialized, Devid: %d", devid);

    if (user_example_ctx->master_devid == devid) {
        user_example_ctx->master_initialized = 1;
    }

    return 0;
}

/** type:
 *
 * 0 - new firmware exist
 *
 */
// static int user_fota_event_handler(int type, const char *version)
// {
//     char buffer[128] = {0};
//     int buffer_length = 128;
//     user_example_ctx_t *user_example_ctx = user_example_get_ctx();

//     if (type == 0) {
//         LOG_TRACE("New Firmware Version: %s", version);

//         IOT_Linkkit_Query(user_example_ctx->master_devid, ITM_MSG_QUERY_FOTA_DATA, (unsigned char *)buffer, buffer_length);
//     }

//     return 0;
// }

static uint64_t user_update_sec(void)
{
    static uint64_t time_start_ms = 0;

    if (time_start_ms == 0) {
        time_start_ms = HAL_UptimeMs();
    }

    return (HAL_UptimeMs() - time_start_ms) / 1000;
}

void user_post_property_json(const char *property)
{
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    int res = IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_PROPERTY, (unsigned char *)property,
            strlen(property));

    LOG_TRACE("Property post Response: %s", property);
    return;
}

static int notify_msg_handle(const char *request, const int request_len)
{
    cJSON *request_root = NULL;
    cJSON *item = NULL;
    uint8_t sendBuf[14]={0xaa,0x00,0x0b,0x00,0x80,0x01,0x00,0x00,0x00,0x01,0x00,0x26,0x01,0x5e};

    request_root = cJSON_Parse(request);
    if (request_root == NULL) {
        LOG_TRACE("JSON Parse Error");
        return -1;
    }

    item = cJSON_GetObjectItem(request_root, "identifier");
    if (item == NULL || !cJSON_IsString(item)) {
        cJSON_Delete(request_root);
        return -1;
    }
    if (!strcmp(item->valuestring, "awss.BindNotify")) {
        cJSON *value = cJSON_GetObjectItem(request_root, "value");
        if (value == NULL || !cJSON_IsObject(value)) {
            cJSON_Delete(request_root);
            return -1;
        }
        cJSON *op = cJSON_GetObjectItem(value, "Operation");
        if (op != NULL && cJSON_IsString(op)) {
            if (!strcmp(op->valuestring, "Bind")) {
                LOG_TRACE("Device Bind");
                vendor_device_bind();
            } else if (!strcmp(op->valuestring, "Unbind")) {
                LOG_TRACE("Device unBind");
                vendor_device_unbind();
            } else if (!strcmp(op->valuestring, "Reset")) {
                LOG_TRACE("Device reset");
                ilife_uart_send_data(sendBuf,14);
                vendor_device_reset();
            }
        }
    }

    cJSON_Delete(request_root);
    return 0;
}

static int user_event_notify_handler(const int devid, const char *request, const int request_len)
{
    int res = 0;
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    LOG_TRACE("Event notify Received, Devid: %d, Request: %s", devid, request);

    notify_msg_handle(request, request_len);

    res = IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_EVENT_NOTIFY_REPLY,
            (unsigned char *)request, request_len);
    LOG_TRACE("Post Property Message ID: %d", res);
	if(strstr(request,"Unbind"))
	{
		// linkkit_reset(NULLn);
        do_awss_ble_start();
	}
    return 0;
}

#if defined (CLOUD_OFFLINE_RESET)
static int user_offline_reset_handler(void)
{
    LOG_TRACE("callback user_offline_reset_handler called.");
    vendor_device_unbind();
}
#endif

void user_post_powerstate(int powerstate)
{
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    device_status_t *device_status = &user_example_ctx->status;

    device_status->powerswitch = powerstate;
    device_status->all_powerstate = powerstate;
    report_device_property(NULL, 0);
}

void user_post_property_after_connected(void)
{
    int res = 0;
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    device_status_t *device_status = &user_example_ctx->status;
    char property_payload[128];

#ifdef TSL_FY_SUPPORT /* support old feiyan TSL */
    snprintf(property_payload, sizeof(property_payload), \
             "{\"%s\":%d, \"%s\":%d, \"%s\":%d}", "PowerSwitch", device_status->powerswitch, \
             "powerstate", device_status->powerswitch, "allPowerstate", device_status->all_powerstate);
#else
    snprintf(property_payload, sizeof(property_payload), \
             "{\"%s\":%d, \"%s\":%d}", "powerstate", device_status->powerswitch, \
             "allPowerstate", device_status->powerswitch, device_status->all_powerstate);
#endif

	product_set_switch(device_status->powerswitch);
	
    res = IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_PROPERTY,
            property_payload, strlen(property_payload));

    LOG_TRACE("Post Event Message ID: %d payload %s", res, property_payload);
}

static int user_down_raw_data_arrived_event_handler(const int devid, const unsigned char *payload,
                                                    const int payload_len)
{
    LOG_TRACE("Down Raw Message, Devid: %d, Payload Length: %d", devid, payload_len);
    if ((payload_len >= 12) && (payload[0] == 0xaa) && (payload[10] == 0x2c) && (payload[11] == 0x22))//wifi info
    {
        ilife_upload_wifi_info_to_cloud();
        //aos_task_new("ilife_upload_wifi_info", ilife_upload_wifi_info, NULL, 512);
    }
    else
    {
        ilife_uart_send_data(payload, payload_len);
    }

    if ((mcu_ota_config == MCU_OTA_CONFIG_NEED_WIFI) && //
            (payload_len >= 12) && 
            (payload[0] == 0xaa) && 
            (payload[10] == 0x02) && //typeId
            (payload[11] == 0x01) && //attrId
            (payload[12] == 0x0e)) //data
    {
        if (mcu_ota_state == MCU_OTA_STATE_ALREADY)//
        {
            aos_task_new("ilife_mcu_ota_task", ilife_mcu_ota_task, NULL, 2048);
        }
    }
    
    return 0;
}

static int user_master_dev_available(void)
{
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();

    if (user_example_ctx->cloud_connected && user_example_ctx->master_initialized) {
        return 1;
    }

    return 0;
}

static int max_running_seconds = 0;
int linkkit_main(void *paras)
{
    uint64_t                        time_prev_sec = 0, time_now_sec = 0;
    uint64_t                        time_begin_sec = 0;
    int                             res = 0;
    iotx_linkkit_dev_meta_info_t    master_meta_info;
    user_example_ctx_t             *user_example_ctx = user_example_get_ctx();
    device_status_t                *device_status = &user_example_ctx->status;
#if defined(__UBUNTU_SDK_DEMO__)
    int                             argc = ((app_main_paras_t *) paras)->argc;
    char                          **argv = ((app_main_paras_t *) paras)->argv;

    if (argc > 1) {
        int tmp = atoi(argv[1]);

        if (tmp >= 60) {
            max_running_seconds = tmp;
            LOG_TRACE("set [max_running_seconds] = %d seconds\n", max_running_seconds);
        }
    }
#endif

#if !defined(WIFI_PROVISION_ENABLED) || !defined(BUILD_AOS)
    set_device_meta_info();
#endif

    memset(user_example_ctx, 0, sizeof(user_example_ctx_t));

    device_status->powerswitch = (int)product_get_switch();
    device_status->all_powerstate = device_status->powerswitch;

    /* Register Callback */
    IOT_RegisterCallback(ITE_CONNECT_SUCC, user_connected_event_handler);
    IOT_RegisterCallback(ITE_DISCONNECTED, user_disconnected_event_handler);
    IOT_RegisterCallback(ITE_RAWDATA_ARRIVED, user_down_raw_data_arrived_event_handler);
    IOT_RegisterCallback(ITE_SERVICE_REQUEST, user_service_request_event_handler);
    IOT_RegisterCallback(ITE_PROPERTY_SET, user_property_set_event_handler);
#ifdef ALCS_ENABLED
    /*Only for local communication service(ALCS) */
    IOT_RegisterCallback(ITE_PROPERTY_GET, user_property_get_event_handler);
#endif
    IOT_RegisterCallback(ITE_REPORT_REPLY, user_report_reply_event_handler);
    IOT_RegisterCallback(ITE_TRIGGER_EVENT_REPLY, user_trigger_event_reply_event_handler);
    // IOT_RegisterCallback(ITE_TIMESTAMP_REPLY, user_timestamp_reply_event_handler);
    IOT_RegisterCallback(ITE_INITIALIZE_COMPLETED, user_initialized);
    // IOT_RegisterCallback(ITE_FOTA, user_fota_event_handler);
    IOT_RegisterCallback(ITE_EVENT_NOTIFY, user_event_notify_handler);
#if defined (CLOUD_OFFLINE_RESET)
    IOT_RegisterCallback(ITE_OFFLINE_RESET, user_offline_reset_handler);
#endif

    memset(&master_meta_info, 0, sizeof(iotx_linkkit_dev_meta_info_t));
    HAL_GetProductKey(master_meta_info.product_key);
    HAL_GetDeviceName(master_meta_info.device_name);
    HAL_GetDeviceSecret(master_meta_info.device_secret);
    HAL_GetProductSecret(master_meta_info.product_secret);

    if ((0 == strlen(master_meta_info.product_key)) || (0 == strlen(master_meta_info.device_name))
            || (0 == strlen(master_meta_info.device_secret)) || (0 == strlen(master_meta_info.product_secret))) {
        LOG_TRACE("No device meta info found...\n");
        while (1) {
            aos_msleep(USER_EXAMPLE_YIELD_TIMEOUT_MS);
        }
    }

    /* Choose Login Method */
    int dynamic_register = 0;

#ifdef CERTIFICATION_TEST_MODE
#ifdef CT_PRODUCT_DYNAMIC_REGISTER_AND_USE_RAWDATA
    dynamic_register = 1;
#endif
#endif

    IOT_Ioctl(IOTX_IOCTL_SET_DYNAMIC_REGISTER, (void *)&dynamic_register);
#ifdef REPORT_UUID_ENABLE
    int uuid_enable = 1;
    IOT_Ioctl(IOTX_IOCTL_SET_UUID_ENABLED, (void *)&uuid_enable);
#endif

    /* Choose Whether You Need Post Property/Event Reply */
    int post_event_reply = 1;
    IOT_Ioctl(IOTX_IOCTL_RECV_EVENT_REPLY, (void *)&post_event_reply);

#ifdef CERTIFICATION_TEST_MODE
    ct_ut_init();
#endif

    /* Create Master Device Resources */
    do {
        user_example_ctx->master_devid = IOT_Linkkit_Open(IOTX_LINKKIT_DEV_TYPE_MASTER, &master_meta_info);
        if (user_example_ctx->master_devid < 0) {
            LOG_TRACE("IOT_Linkkit_Open Failed, retry after 5s...\n");
            HAL_SleepMs(5000);
        }
    } while (user_example_ctx->master_devid < 0);
    /* Start Connect Aliyun Server */
    do {
        res = IOT_Linkkit_Connect(user_example_ctx->master_devid);
        if (res < 0) {
            LOG_TRACE("IOT_Linkkit_Connect Failed, retry after 5s...\n");
            HAL_SleepMs(5000);
        }
    } while (res < 0);

#ifdef AOS_TIMER_SERVICE
    static bool ntp_update = false;
    int ret = timer_service_init(control_targets_list, NUM_OF_PROPERTYS,
            countdownlist_target_list, NUM_OF_COUNTDOWN_LIST_TARGET,
            localtimer_target_list, NUM_OF_LOCAL_TIMER_TARGET,
            timer_service_cb, num_of_tsl_type, NULL);
    if (ret == 0)
        ntp_update = true;
#endif
#ifdef AIOT_DEVICE_TIMER_ENABLE
    aiot_device_timer_init(propertys_list, NUM_OF_TIMER_PROPERTYS, timer_service_cb);
#endif

    time_begin_sec = user_update_sec();
    ntp_server_init();

    while (1) {
        IOT_Linkkit_Yield(USER_EXAMPLE_YIELD_TIMEOUT_MS);

#ifndef REPORT_MULTHREAD
        process_property_report();
#endif
        time_now_sec = user_update_sec();
        if (time_prev_sec == time_now_sec) {
            continue;
        }
        if (max_running_seconds && (time_now_sec - time_begin_sec > max_running_seconds)) {
            LOG_TRACE("Example Run for Over %d Seconds, Break Loop!\n", max_running_seconds);
            break;
        }

        if (user_master_dev_available())
        {
#ifdef AOS_TIMER_SERVICE
            if ((time_now_sec % 11 == 0) && !ntp_update)
            {
                /* init timer service */
                int ret = timer_service_init(control_targets_list, NUM_OF_PROPERTYS,
                                             countdownlist_target_list, NUM_OF_COUNTDOWN_LIST_TARGET,
                                             localtimer_target_list, NUM_OF_LOCAL_TIMER_TARGET,
                                             timer_service_cb, num_of_tsl_type, NULL);
                if (ret == 0)
                    ntp_update = true;
            }
#endif
#ifdef AIOT_DEVICE_TIMER_ENABLE
            if ((time_now_sec % 11 == 0) && !aiot_device_timer_inited())
                aiot_device_timer_init(propertys_list, NUM_OF_TIMER_PROPERTYS, timer_service_cb);
#endif
#ifdef CERTIFICATION_TEST_MODE
            ct_ut_misc_process(time_now_sec);
#endif
        }

        time_prev_sec = time_now_sec;
    }

    IOT_Linkkit_Close(user_example_ctx->master_devid);

    IOT_DumpMemoryStats(IOT_LOG_DEBUG);
    IOT_SetLogLevel(IOT_LOG_NONE);

    return 0;
}
