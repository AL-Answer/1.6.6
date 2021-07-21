#include "ilife_uart.h"


UartBuffer g_struUartBuffer;
aos_queue_t g_uart_recv_queue;
aos_queue_t g_wait_wake_queue;
aos_queue_t g_ota_data_queue;
char uart_recv_queue_buf[UART_QUEUE_BUF_SIZE];
char wait_wake_queue_buf[WAIT_QUEUE_BUF_SIZE];
char ota_recv_queue_buf[OTA_QUEUE_BUF_SIZE];
uint8_t g_otaAckStatus = OTA_WAIT_ACK;

char ILIFE_CONN_SSID[64]={0};
char ILIFE_CONN_BSSID[6]={0};
int ilife_conn_sigal=0;
static int ilife_scan_ssid_success_flag=0;
uint8_t mcu_ota_config = 0;
uint8_t mcu_ota_state = 0;

ilife_ota_info_t ilife_ota_info;
uint8_t mcu_update_state = 0;

//extern user_example_ctx_t user_example_ctx;

/*************************************************
*************************************************/
static void ilife_uart_progess(void *p);
static void ilife_upload_wifi_info(void *arg);

uint8_t ilife_calc_sum(uint8_t *pu8Src, uint16_t u16Len)
{
    uint8_t u8Sum = 0;
    uint32_t u32i;
    for(u32i = 0; u32i < u16Len; u32i++)
    {
        u8Sum += pu8Src[u32i];
    }
    return u8Sum;
}
/*************************************************
*************************************************/
uint8_t ilife_check_sum(uint8_t * Buffer,uint32_t len)
{
    uint8_t u8Sum = ilife_calc_sum(Buffer, len-1);
    if (Buffer[len-1] == u8Sum)
    {
        return(ILIFE_RET_OK);//check correct
    }
    else
    {
        return(ILIFE_RET_ERROR);//check error
    }
}

void ilife_uart_recive_data(uint8_t *data, uint32_t len)
{
    uint32_t u32_index = 0;
    uint8_t tmp_data = 0;
    static uint8_t cur_receive_type = 0;//0 NO Receive, 1 Receive data
    static uint64_t timeout_start_ms = 0;
    int ret = 0;
    // printf("ilife_uart_recive_data:(%d)[%s]\n", len, data);

    if (timeout_start_ms == 0)
    {
        timeout_start_ms = HAL_UptimeMs();
    }
    else if ((HAL_UptimeMs() - timeout_start_ms) > UART_REC_TIMEOUT_MS)
    {
        cur_receive_type = PKT_UNKNOWN;
        LOG_TRACE("UART Receive data TimeOut! Reset cur_receive_type = PKT_UNKNOWN");
    }
    for (u32_index = 0; u32_index < len; u32_index++)
    {
        tmp_data = data[u32_index];
        
        // LOG_TRACE("UART Receive data PktLen = %d, RecvLen = %d, data = 0x%02x", g_struUartBuffer.u16PktLen, g_struUartBuffer.u16RecvLen, tmp_data);
        switch(cur_receive_type)
        {
            case PKT_UNKNOWN:
            {
                if (STAND_HEADER == tmp_data)
                {
                    cur_receive_type = PKT_PUREDATA;
                    g_struUartBuffer.u16PktLen = 0;
                    g_struUartBuffer.u16RecvLen = 0;
                    g_struUartBuffer.u8UartBuffer[g_struUartBuffer.u16RecvLen++] = tmp_data;
                }
                break;
            }
            case PKT_PUREDATA:
            {
                g_struUartBuffer.u8UartBuffer[g_struUartBuffer.u16RecvLen++] = tmp_data;
                
                if ((PAYLOADLENOFFSET-1) == g_struUartBuffer.u16RecvLen)
                {
                    g_struUartBuffer.u16PktLen = tmp_data<<8;
                }
                else if(PAYLOADLENOFFSET == g_struUartBuffer.u16RecvLen)
                {
                    g_struUartBuffer.u16PktLen += tmp_data;
                    if (g_struUartBuffer.u16PktLen > MAX_UARTBUF_LEN)
                    {
                        LOG_TRACE("UART Receive data is too long! PktLen = %d, MAX = %d", g_struUartBuffer.u16PktLen, MAX_UARTBUF_LEN);
                        timeout_start_ms = 0;
                        cur_receive_type = PKT_UNKNOWN;
                        g_struUartBuffer.u16PktLen = 0;
                        g_struUartBuffer.u16RecvLen = 0;
                        //g_struUartBuffer.u8UartBuffer[g_struUartBuffer.u16RecvLen++] = tmp_data;
                    }
                }
                if ((g_struUartBuffer.u16RecvLen - 3) == g_struUartBuffer.u16PktLen)
                {
                    //if (STAND_TAIL == tmp_data)
                    {
                        LOG_TRACE("UART Receive data PktLen = %d, data: %02X,%02X,%02X,%02X,%02X,%02X ...",
                                    g_struUartBuffer.u16PktLen,
                                    g_struUartBuffer.u8UartBuffer[0],
                                    g_struUartBuffer.u8UartBuffer[1],
                                    g_struUartBuffer.u8UartBuffer[2],
                                    g_struUartBuffer.u8UartBuffer[3],
                                    g_struUartBuffer.u8UartBuffer[4],
                                    g_struUartBuffer.u8UartBuffer[5]);

                        ret = aos_queue_send(&g_uart_recv_queue, g_struUartBuffer.u8UartBuffer, g_struUartBuffer.u16RecvLen);
                        if (ret !=0 )
                        {
                            LOG_TRACE("g_uart_recv_queue send fail! ret = %d", ret);
                        }
                    }
                    timeout_start_ms = 0;
                    cur_receive_type = PKT_UNKNOWN;
                    g_struUartBuffer.u16PktLen = 0;
                    g_struUartBuffer.u16RecvLen = 0;
                    //g_struUartBuffer.u8UartBuffer[g_struUartBuffer.u16RecvLen++] = tmp_data;
                }
                break;
            }
            default:
                LOG_TRACE("UART Receive data ERR! Type = %d", cur_receive_type);
        }
    }
    timeout_start_ms = HAL_UptimeMs();
}

void ilife_uart_send_data(uint8_t *data, uint32_t len)
{
    if (ilife_deal_robot_wakeup((uint8_t*)data, len))
    {
        hfilop_uart_send_data((unsigned char*)data, len);
    }
    else
    {
        LOG_TRACE("ilife_uart_send_data Robot Not Wake!");
    }
}

void ilife_send_data_to_cloud(SendData_t *data)
{
    int ret = 0;
    uint8_t buf[50];
    uint16_t send_len = 0;

    buf[0] = STAND_HEADER;
    buf[1] = (data->len & 0xff00) >> 8;
    buf[2] = data->len & 0x00ff;
    buf[3] = data->ver;
    buf[4] = data->cmd;
    buf[5] = data->method;
    buf[6] = (data->id & 0xff000000) >> 24;
    buf[7] = (data->id & 0x00ff0000) >> 16;
    buf[8] = (data->id & 0x0000ff00) >> 8;
    buf[9] = (data->id & 0x000000ff);

    memcpy(&buf[10], data->payload, data->len-7);

    buf[data->len+2] = ilife_calc_sum(buf, data->len+2);
    //send_len = data->len+3;
    ret = aos_queue_send(&g_uart_recv_queue, buf, data->len+3);
    //ret = IOT_Linkkit_Report(g_living_platform_ctx.master_devid, ITM_MSG_POST_RAW_DATA, buf, send_len);
    if (ret != 0)
    {
        LOG_TRACE("Post WiFi Info to Cloud Raw Data ret = %d", ret);
    }
    //ilife_uart_send_data(buf, data->len+3);
}

void ilife_send_data_to_mcu(SendData_t *data)
{
    uint8_t buf[50];
    uint16_t send_len = 0;

    buf[0] = STAND_HEADER;
    buf[1] = (data->len & 0xff00) >> 8;
    buf[2] = data->len & 0x00ff;
    buf[3] = data->ver;
    buf[4] = data->cmd;
    buf[5] = data->method;
    buf[6] = (data->id & 0xff000000) >> 24;
    buf[7] = (data->id & 0x00ff0000) >> 16;
    buf[8] = (data->id & 0x0000ff00) >> 8;
    buf[9] = (data->id & 0x000000ff);

    memcpy(&buf[10], data->payload, data->len-7);

    buf[data->len+2] = ilife_calc_sum(buf, data->len+2);

    ilife_uart_send_data(buf, data->len+3);
}

void ilife_send_wifi_state(WIFI_STATE state)
{
    SendData_t send_data;
    uint8_t buf[50];
    uint16_t send_len = 0;

    send_data.ver = 1;
    send_data.cmd = CMD_TO_MCU;
    send_data.method = METHOD_CLOUD_SET;
    send_data.id = 0;
    send_data.payload[0] = ID_TYPE_WIFISTATE;
    send_data.payload[1] = ID_PROP_WIFISTATE;
    send_data.payload[2] = state;
    send_data.len = 8+3;
    
    // ilife_send_data_to_mcu(&send_data);

    buf[0] = STAND_HEADER;
    buf[1] = (send_data.len & 0xff00) >> 8;
    buf[2] = send_data.len & 0x00ff;
    buf[3] = send_data.ver;
    buf[4] = send_data.cmd;
    buf[5] = send_data.method;
    buf[6] = (send_data.id & 0xff000000) >> 24;
    buf[7] = (send_data.id & 0x00ff0000) >> 16;
    buf[8] = (send_data.id & 0x0000ff00) >> 8;
    buf[9] = (send_data.id & 0x000000ff);

    memcpy(&buf[10], send_data.payload, send_data.len-7);

    buf[send_data.len+2] = ilife_calc_sum(buf, send_data.len+2);

    hfilop_uart_send_data((unsigned char*)buf, send_data.len+3);
}

static int wifi_scan_cb_updata_rssi(ap_list_adv_t *info)
{
    static int i = 0;
	if(info!=NULL)
	{
        for (i = 0; i < 6; i++)
        {
            if (info->bssid[i] == ILIFE_CONN_BSSID[i])
            {
                if (i == 5)
                {
                    HFILOP_PRINTF("Comp BSSID: %d,%s,%d,%02X:%02X:%02X:%02X:%02X:%02X----------\n\r", info->channel, info->ssid, info->ap_power,
                    info->bssid[0], info->bssid[1], info->bssid[2], info->bssid[3], info->bssid[4], info->bssid[5]);
                    ilife_conn_sigal = info->ap_power;
                    ilife_scan_ssid_success_flag = 1;
                }
                continue;
            }
            else
            {
                break;
            }            
        }
        /*
		if(strcmp(info->ssid,ILIFE_CONN_SSID)==0)
        {
            HFILOP_PRINTF("%d,%s,%d,%02X:%02X:%02X:%02X:%02X:%02X----------\n\r", info->channel, info->ssid, info->ap_power,
            info->bssid[0], info->bssid[1], info->bssid[2], info->bssid[3], info->bssid[4], info->bssid[5]);
            ilife_conn_sigal = info->ap_power;
            ilife_scan_ssid_success_flag = 1;
        }
        */
	}
	return 0;
}

int ilife_get_wifi_power(void)
{
    hal_wifi_link_stat_t out_stat;
    memset(&out_stat, 0, sizeof(hal_wifi_link_stat_t));
    hal_wifi_get_link_stat(NULL, &out_stat);

    if(ilife_wifi_get_state() != WIFI_STATE_CON_CLOUD )
    {
        return 0;          
    }  

    if(out_stat.is_connected == 0)
    {
        LOG_TRACE("out_stat.is_connected = %d\n", out_stat.is_connected);
        return 0;
    }
    // LOG_TRACE("out_stat.is_connected bssid = %02X:%02X:%02X:%02X:%02X:%02X\n", 
    // out_stat.bssid[0], out_stat.bssid[1], out_stat.bssid[2], out_stat.bssid[3], out_stat.bssid[4], out_stat.bssid[5]);

    ilife_scan_ssid_success_flag = 0;
    memset(ILIFE_CONN_SSID, 0, 64);
    strcpy(ILIFE_CONN_SSID, out_stat.ssid);

    memset(ILIFE_CONN_BSSID, 0, 6);
    memcpy(&ILIFE_CONN_BSSID, out_stat.bssid, sizeof(out_stat.bssid));
    LOG_TRACE("ILIFE_CONN_BSSID = %02X:%02X:%02X:%02X:%02X:%02X\n", 
    ILIFE_CONN_BSSID[0], ILIFE_CONN_BSSID[1], ILIFE_CONN_BSSID[2], ILIFE_CONN_BSSID[3], ILIFE_CONN_BSSID[4], ILIFE_CONN_BSSID[5]);

    ilife_conn_sigal = 0;
    hfilop_wifi_scan(wifi_scan_cb_updata_rssi);
    if(ilife_scan_ssid_success_flag == 1)
    {
        LOG_TRACE("ilife_scan_ssid_success_flag is 1, ilife_conn_sigal = %d\n", ilife_conn_sigal);
        return ilife_conn_sigal;
    }
    else
    {
        int rssi = 0;//hfwifi_transform_rssi(out_stat.wifi_strength);
        if ((out_stat.wifi_strength < 0) && (out_stat.wifi_strength >= -100))
        {
            rssi = -out_stat.wifi_strength;
        }
        else
        {
            rssi = 0;
        }
        
        if(out_stat.is_connected == 0)
        {
            rssi = 0;
        }
        LOG_TRACE("ilife_scan_ssid_success_flag is 0, connected = %d, power = %d, rssi = %d\n", out_stat.is_connected, out_stat.wifi_strength, rssi);
        return rssi;
    }
}

void ilife_upload_wifi_info_to_cloud(void)
{
    int ret = 0;
    LOG_TRACE("ilife_upload_wifi_info_to_cloud\n");
    //ret = aos_post_delayed_action(20, ilife_upload_wifi_info, NULL);
    //LOG_TRACE("aos_post_delayed_action ret = %d\n", ret);
    aos_task_new("ilife_upload_wifi_info", ilife_upload_wifi_info, NULL, 1024);
}

static void ilife_upload_wifi_info(void *arg)
{
     SendData_t send_data;
     unsigned char mac_addr[MAC_ADDRESS_SIZE];
     char ver[FIRMWARE_VERSION_MAXLEN];
     uint32_t ip = 0,gw = 0, mask =0;
/*      hal_wifi_ip_stat_t wifi_info;
     hal_wifi_type_t wifi_info_type;
     hal_wifi_module_t wifi_info_name; */
     uint8_t sleep_status = 0;

     aos_get_mac_hex(&mac_addr);
     //hal_wifi_get_ip_stat(&wifi_info_name.get_ip_stat,&wifi_info,wifi_info_type);
     memset(ver, 0x0, FIRMWARE_VERSION_MAXLEN);
     strncpy(ver, aos_get_app_version(), FIRMWARE_VERSION_MAXLEN - 1);


    if(ilife_wifi_get_state() == WIFI_STATE_CON_CLOUD )
    {
        wifi_mgmr_sta_ip_get(&ip, &gw, &mask);
    }

     LOG_TRACE("Get Mac = %02x:%02x:%02x:%02x:%02x:%02x", 
                 mac_addr[0],
                 mac_addr[1],
                 mac_addr[2],
                 mac_addr[3],
                 mac_addr[4],
                 mac_addr[5]);

     LOG_TRACE("Get Ver = %s", ver);

     send_data.ver = 1;
     send_data.cmd = CMD_TO_CLOUD;
     send_data.method = METHOD_REPORT_ATTR;
     send_data.id = 0;
     send_data.payload[0] = ID_TYPE_WIFIINFO;
     send_data.payload[1] = ID_PROP_WIFIINFO_CLOUD;
     send_data.payload[2] = mac_addr[0];
     send_data.payload[3] = mac_addr[1];
     send_data.payload[4] = mac_addr[2];
     send_data.payload[5] = mac_addr[3];
     send_data.payload[6] = mac_addr[4];
     send_data.payload[7] = mac_addr[5];
     send_data.payload[8] = (ip & 0x000000ff);
     send_data.payload[9] = (ip & 0x0000ff00) >> 8;
     send_data.payload[10] = (ip & 0x00ff0000) >> 16;
     send_data.payload[11] = (ip & 0xff000000) >> 24;
     send_data.payload[12] = ilife_get_wifi_power() & 0xff;
     send_data.payload[13] = ver[4] - '0';
     send_data.payload[14] = ver[6] - '0';
     send_data.payload[15] = ver[8] - '0';
     send_data.payload[16] = ((ver[10] - '0') << 8) + (ver[11] - '0');

     send_data.len = 8+17;
    
     LOG_TRACE("Get ip = %d.%d.%d.%d, rssi = -%d", 
                 send_data.payload[8],
                 send_data.payload[9],
                 send_data.payload[10],
                 send_data.payload[11],
                 send_data.payload[12]);

     //sleep_status = g_wifi_sleep_state;
     ilife_send_data_to_cloud(&send_data);
     aos_msleep(50);
     //g_wifi_sleep_state = sleep_status;
     LOG_TRACE("g_wifi_sleep_state = %d", g_wifi_sleep_state);
    aos_task_exit(0);
}

void ilife_send_wifi_info(void)
{
     SendData_t send_data;
     unsigned char mac_addr[MAC_ADDRESS_SIZE];
     char ver[FIRMWARE_VERSION_MAXLEN];
     uint32_t ip = 0,gw = 0, mask =0;

/*      hal_wifi_ip_stat_t wifi_info;
     hal_wifi_type_t wifi_info_type;
     hal_wifi_module_t wifi_info_name; */
     aos_get_mac_hex(&mac_addr);
     //hal_wifi_get_ip_stat(&wifi_info_name.get_ip_stat,&wifi_info,wifi_info_type);
     memset(ver, 0x0, FIRMWARE_VERSION_MAXLEN);
     strncpy(ver, aos_get_app_version(), FIRMWARE_VERSION_MAXLEN - 1);

     wifi_mgmr_sta_ip_get(&ip, &gw, &mask);
     LOG_TRACE("Get Mac = %02x:%02x:%02x:%02x:%02x:%02x", 
                 mac_addr[0],
                 mac_addr[1],
                 mac_addr[2],
                 mac_addr[3],
                 mac_addr[4],
                 mac_addr[5]);

     LOG_TRACE("Get Ver = %s", ver);

     send_data.ver = 1;
     send_data.cmd = CMD_TO_MCU;
     send_data.method = METHOD_CLOUD_SET;
     send_data.id = 0;
     send_data.payload[0] = ID_TYPE_WIFIINFO;
     send_data.payload[1] = ID_PROP_WIFIINFO_MCU;
     send_data.payload[2] = mac_addr[0];
     send_data.payload[3] = mac_addr[1];
     send_data.payload[4] = mac_addr[2];
     send_data.payload[5] = mac_addr[3];
     send_data.payload[6] = mac_addr[4];
     send_data.payload[7] = mac_addr[5];
     send_data.payload[8] = (ip & 0x000000ff);
     send_data.payload[9] = (ip & 0x0000ff00) >> 8;
     send_data.payload[10] = (ip & 0x00ff0000) >> 16;
     send_data.payload[11] = (ip & 0xff000000) >> 24;
     send_data.payload[12] = ilife_get_wifi_power() & 0xff;
     send_data.payload[13] = ver[4] - '0';
     send_data.payload[14] = ver[6] - '0';
     send_data.payload[15] = ver[8] - '0';
     send_data.payload[16] = ((ver[10] - '0') << 8) + (ver[11] - '0');

     send_data.len = 8+17;
    
     LOG_TRACE("Get ip = %d.%d.%d.%d, rssi = -%d", 
                 send_data.payload[8],
                 send_data.payload[9],
                 send_data.payload[10],
                 send_data.payload[11],
                 send_data.payload[12]);

     ilife_send_data_to_mcu(&send_data);
}

void ilife_send_ack(uint32_t id, ACK_TYPE ack)
{
    SendData_t send_data;
    
    send_data.ver = 1;
    send_data.cmd = CMD_TO_MCU;
    send_data.method = METHOD_ACK;
    send_data.id = id;
    // send_data.payload[0] = (ack & 0xff00) >> 8;
    // send_data.payload[1] = ack & 0x00ff;
    send_data.payload[0] = ack;
    send_data.len = 8+1;
    
    ilife_send_data_to_mcu(&send_data);
}

int ilife_send_ota_data(uint8_t attr, uint32_t id, ilife_ota_info_t ota_info, uint8_t *buf, uint16_t len)
{
    uint8_t retrycnt = 0;
    uint16_t waitCnt = 0;
    uint8_t sendBuf[600];
    uint16_t sendLen = 0;
    uint16_t packLen = 0;//data+(head+length+ver+cmd+method+id)+chksum-(head+length)
    static unsigned int offset = 0;
    g_otaAckStatus = OTA_WAIT_ACK;
    int kv_len = 0;
    int ret = 0;    
 
    sendBuf[0] = STAND_HEADER;
    // sendBuf[1] = (packLen & 0xff00) >> 8;
    // sendBuf[2] = packLen & 0x00ff;
    sendBuf[3] = 1;
    sendBuf[4] = 0x80;
    sendBuf[5] = 0xF2;
    sendBuf[6] = (id & 0xff000000) >> 24;
    sendBuf[7] = (id & 0x00ff0000) >> 16;
    sendBuf[8] = (id & 0x0000ff00) >> 8;
    sendBuf[9] = (id & 0x000000ff);
    sendBuf[10] = 0x00;
    sendBuf[11] = attr;
    sendBuf[12] = (ota_info.target_ver & 0xff000000) >> 24;
    sendBuf[13] = (ota_info.target_ver & 0x00ff0000) >> 16;
    sendBuf[14] = (ota_info.target_ver & 0x0000ff00) >> 8;
    sendBuf[15] = (ota_info.target_ver & 0x000000ff);

    switch (attr)
    {
        case ID_PROP_OTA_START_CODE:
        case ID_PROP_OTA_END_CODE:
        {
            packLen = len+36+1-3;//data+(head+length+ver+cmd+method+id)+chksum-(head+length)
            sendBuf[1] = (packLen & 0xff00) >> 8;
            sendBuf[2] = packLen & 0x00ff;
            sendBuf[16] = (ota_info.file_size & 0xff000000) >> 24;
            sendBuf[17] = (ota_info.file_size & 0x00ff0000) >> 16;
            sendBuf[18] = (ota_info.file_size & 0x0000ff00) >> 8;
            sendBuf[19] = (ota_info.file_size & 0x000000ff);
            memcpy(&sendBuf[20], ota_info.file_md5, 16);
            sendBuf[len+36] = ilife_calc_sum(sendBuf, len+36);

            sendLen = len + 37;
            if (mcu_ota_config == MCU_OTA_CONFIG_NEED_WIFI)//wifi need save ota data
            {
                if (ota_info.file_size > MCU_OTA_DATA_MAX_SIZE)
                {
                    LOG_TRACE("ota_info.file_size = %d, MAX=%d!!!!!!!!", ota_info.file_size, MCU_OTA_DATA_MAX_SIZE);
                    return OTA_FAIL_ACK;
                }
                if (mcu_ota_state == MCU_OTA_STATE_UPDATING)
                {
                    printf("ilife_uart_send_data len = %d\r\n", sendLen);
                    ilife_uart_send_data(sendBuf, sendLen);
                }
                //else if ((attr == ID_PROP_OTA_START_CODE) && (mcu_ota_state == MCU_OTA_STATE_NULL))
                else if (attr == ID_PROP_OTA_START_CODE)
                {
                    offset = 0;
                    ret = hal_flash_erase(HAL_PARTITION_OTA_TEMP, offset, ota_info.file_size + MCU_OTA_DATA_OFFSET);
                    LOG_TRACE("hal_flash_erase ret= %d, file_size=%d", ret, ota_info.file_size);
                    //memcpy(&sendBuf[100], ota_info, sizeof(ota_info));
                    ret = hal_flash_write(HAL_PARTITION_OTA_TEMP, &offset, &ota_info, sizeof(ilife_ota_info_t));
                    LOG_TRACE("hal_flash_write ota_info ret= %d, offset=%d", ret, offset);
                    offset = MCU_OTA_DATA_OFFSET;

                    mcu_ota_state = MCU_OTA_STATE_DOWNING;
                    ret = aos_kv_set(MCU_OTA_STATE, &mcu_ota_state, 1, 1);
                    printf("mcu_ota_state = %d, ret = %d\r\n", mcu_ota_state, ret);
                    return OTA_RECV_ACK;
                }
                else if ((attr == ID_PROP_OTA_END_CODE) && (mcu_ota_state <= MCU_OTA_STATE_DOWNING))
                {
                    mcu_ota_state = MCU_OTA_STATE_ALREADY;
                    ret = aos_kv_set(MCU_OTA_STATE, &mcu_ota_state, 1, 1);
                    return OTA_RECV_ACK;
                }
            }
            else
            {
                printf("ilife_uart_send_data len = %d\r\n", sendLen);
                ilife_uart_send_data(sendBuf, sendLen);
            }
            break;
        }
        case ID_PROP_OTA_DATA_CODE:
        {
            packLen = len+24+1-3;//data+(head+length+ver+cmd+method+id)+chksum-(head+length)
            sendBuf[1] = (packLen & 0xff00) >> 8;
            sendBuf[2] = packLen & 0x00ff;
            sendBuf[16] = (ota_info.file_offset & 0xff000000) >> 24;
            sendBuf[17] = (ota_info.file_offset & 0x00ff0000) >> 16;
            sendBuf[18] = (ota_info.file_offset & 0x0000ff00) >> 8;
            sendBuf[19] = (ota_info.file_offset & 0x000000ff);
            sendBuf[20] = (ota_info.file_size & 0xff000000) >> 24;
            sendBuf[21] = (ota_info.file_size & 0x00ff0000) >> 16;
            sendBuf[22] = (ota_info.file_size & 0x0000ff00) >> 8;
            sendBuf[23] = (ota_info.file_size & 0x000000ff);
            memcpy(&sendBuf[24], buf, len);
            sendBuf[len+24] = ilife_calc_sum(sendBuf, len+24);
            //ilife_uart_send_data(sendBuf, len+25);
            sendLen = len + 25;
            
           if (mcu_ota_state == MCU_OTA_STATE_UPDATING)
            {
                printf("MCU_OTA_STATE_UPDATING ilife_uart_send_data len = %d\r\n", sendLen);
                hfilop_uart_send_data(sendBuf, sendLen);
            }
            else if (mcu_ota_config == MCU_OTA_CONFIG_NEED_WIFI)//wifi need save ota data
            {
                hal_flash_write(HAL_PARTITION_OTA_TEMP, &offset, buf, len);
                return OTA_RECV_ACK;
            }
            else
            {
                hfilop_uart_send_data(sendBuf, sendLen);
            }
            break;
        }
    }
    
    waitCnt = 1;
    retrycnt = 0;
    
    while (g_otaAckStatus != OTA_RECV_ACK)
    {
        if (((waitCnt++) % 200) == 0) //2s
        {
            LOG_TRACE_RED("ilife_send_ota_data retry = %d, set wakep up pin.", retrycnt);
            ilife_wake_up_gpio_init_output();
            hal_gpio_output_high(&gpio_wake_up);
            aos_msleep(200);
            ilife_wake_up_gpio_init_input();
            LOG_TRACE_RED("ilife_send_ota_data retry = %d, reset wake up pin.", retrycnt);

            retrycnt++;
            if (retrycnt < 4)
            {
                switch (attr)
                {
                    case ID_PROP_OTA_START_CODE:
                    case ID_PROP_OTA_END_CODE:
                    {
                        ilife_uart_send_data(sendBuf, sendLen);
                        break;
                    }
                    case ID_PROP_OTA_DATA_CODE:
                    {
                        hfilop_uart_send_data(sendBuf, sendLen);
                        break;
                    }
                }
            }
            else
            {
                g_otaAckStatus = OTA_FAIL_ACK;
                break;
            }
        }
        HAL_SleepMs(10);
    }
    #if 0
    #endif
//    LOG_TRACE("ilife_send_ota_data g_otaAckStatus = %d", g_otaAckStatus);
    return g_otaAckStatus;
}

void ilife_mcu_ota_task(void *p)
{
    int res = 0;
    uint32_t offset = 0;
    uint8_t step = 0;
    ilife_ota_info_t ota_info_tmp;
    uint8_t data_buf[512];
    uint16_t read_data_len = 0;

    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    uint8_t flag = 0;
    uint8_t Updatebuf[23] = {0xaa, 0x00, 0x14, 0x01, 0x00, 0x80, 0x00, 0x01, 0x02, 0x03, 0x2e, 0x24,
                                                            0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x05, 0x00};


    res = hal_flash_read(HAL_PARTITION_OTA_TEMP, &offset, &ota_info_tmp, sizeof(ilife_ota_info_t));
    LOG_TRACE("hal_flash_read ota_info_tmp res=%d, file_size=%d", res, ota_info_tmp.file_size);
    if (res != 0)
    {
        LOG_TRACE_RED("hal_flash_read res != 0, stop ota!)");
        return;
    }
    mcu_ota_state = MCU_OTA_STATE_UPDATING;

    while(1)
    {
        switch (step)
        {
        case 0://start
            res = ilife_send_ota_data(ID_PROP_OTA_START_CODE, 1, ota_info_tmp, NULL, 0);
            if (res == OTA_RECV_ACK)
            {
                step = 1;
                ota_info_tmp.file_offset = 0;
                offset = MCU_OTA_DATA_OFFSET;

                // char CurrentVer[16] = {0};
                // int len_tmp = sizeof(CurrentVer);
                // aos_kv_get("mcu_version", CurrentVer, &len_tmp);
                
                // Updatebuf[12] = 0x02;

                // Updatebuf[14] = CurrentVer[10];
                // Updatebuf[15] = CurrentVer[11];
                // Updatebuf[16] = CurrentVer[13];
                // Updatebuf[17] = CurrentVer[14];

                //Updatebuf[18] = (ota_info_tmp.target_ver & 0xff000000) >> 24;
                //Updatebuf[19] = (ota_info_tmp.target_ver & 0x00ff0000) >> 16;
                //Updatebuf[20] = (ota_info_tmp.target_ver & 0x0000ff00) >> 8;
                //Updatebuf[21] = (ota_info_tmp.target_ver & 0x000000ff);

                //Updatebuf[22] = ilife_calc_sum(Updatebuf, 23-1);
                //LOG_TRACE_RED("Updating!!!!!");
                //IOT_Linkkit_Report(1, ITM_MSG_POST_RAW_DATA, Updatebuf, 23);
            }
            else
            {
                LOG_TRACE_RED("ilife_send_ota_data res = 0, stop ota!", res);
                step = 2;//ota fail
                flag = 1;
            }
            break;
        case 1://data
            if ((offset - MCU_OTA_DATA_OFFSET + 512) >= ota_info_tmp.file_size)
            {
                read_data_len = ota_info_tmp.file_size - offset + MCU_OTA_DATA_OFFSET;
            }
            else
            {
                read_data_len = 512;
            }

            LOG_TRACE("file_size = %d, offset = %d, read_data_len = %d", ota_info_tmp.file_size, offset, read_data_len);

            res = hal_flash_read(HAL_PARTITION_OTA_TEMP, &offset, data_buf, read_data_len);
            if (res == 0)
            {
                res = ilife_send_ota_data(ID_PROP_OTA_DATA_CODE, read_data_len, ota_info_tmp, data_buf, read_data_len);
                if (res == OTA_FAIL_ACK)
                {
                    LOG_TRACE_RED("ilife_send_ota_data res == OTA_FAIL_ACK)");
                    step = 2;//ota fail
                    flag = 1;
                }
                ota_info_tmp.file_offset += read_data_len;
            }
            else
            {
                LOG_TRACE_RED("hal_flash_read res = %d)", res);
                step = 2;//ota fail
                flag = 1;
            }

            if ((offset - MCU_OTA_DATA_OFFSET) >= ota_info_tmp.file_size)
            {
                LOG_TRACE("offset >= ota_info_tmp.file_size)", res);
                step = 2;//ota end
                flag = 2;
            }

            break;
        case 2://end
            ilife_send_ota_data(ID_PROP_OTA_END_CODE, 1, ota_info_tmp, NULL, 0);
            mcu_ota_state = MCU_OTA_STATE_ALREADY;

            if(flag == 1)
            {
                Updatebuf[12] = 0x03;
                Updatebuf[22] = ilife_calc_sum(Updatebuf, 23-1);
                //IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_RAW_DATA, Updatebuf, 23);
            }

            aos_task_exit(0);
        break;
        default:
            break;
      }        
    }
    aos_task_exit(0);
}
