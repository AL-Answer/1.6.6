#include <stdio.h>
#include "hal/soc/flash.h"
#include "hal/soc/gpio.h"
#include "hal/soc/uart.h"
#include "hal/wifi.h"
#include "hal/ota.h"
#include "hal/soc/pwm.h"
#include "netmgr.h"
#include <stdlib.h>
#include <aos/kernel.h>
#include <k_api.h>
#include "types.h"

#include "hfilop.h"
#include "hfilop_config.h"
#include "smart_outlet.h"
#include "iot_export_linkkit.h"
#include "cJSON.h"
#include "buf.h"
#include <bluetooth.h>
extern void do_awss_reset(void);
extern void start_netmgr(void *p);
extern bool mcu_ota_start_flag;
aos_sem_t sg_get_ntp_time;
extern void do_awss_dev_ap();
extern int awss_config_timeout_check(void);

bool cloud_conn_status=false;
gpio_dev_t GPIO_Link;
gpio_dev_t GPIO_Ready;


static hfble_scan_callback_t	p_ble_scan_callback = NULL;

static void ble_scan_callback(const bt_addr_le_t *addr, s8_t rssi, u8_t evtype,
			 struct net_buf_simple *buf)
{
	BLE_SCAN_RESULT_ITEM item;
	
	item.addr_type = addr->type;
	item.addr[0] = addr->a.val[5];
	item.addr[1] = addr->a.val[4];
	item.addr[2] = addr->a.val[3];
	item.addr[3] = addr->a.val[2];
	item.addr[4] = addr->a.val[1];
	item.addr[5] = addr->a.val[0];
	item.rssi = rssi;
	item.evtype = evtype;
	item.len = buf->len;
	item.data = buf->data;
	
	if(p_ble_scan_callback)
		p_ble_scan_callback(&item);
}

u16_t g_interval = 30*8/5*2;
u16_t g_window = 30*8/5;

int hfble_start_scan(hfble_scan_callback_t p_callback)
{
	struct bt_le_scan_param scan_param = {
		.type       = 0x00/*BT_HCI_LE_SCAN_PASSIVE*/,
		.filter_dup = 0x00/*BT_HCI_LE_SCAN_FILTER_DUP_DISABLE*/,
		.interval   = g_interval,
		.window     = g_window };
	
	p_ble_scan_callback = p_callback;
	printf("hfble_start_scan: g_interval = %d,g_window = %d\r\n",g_interval,g_window);
	if(bt_le_scan_start(&scan_param, ble_scan_callback))
		return -1;
	else
		return 0;
}

int hfble_stop_scan(void)
{
	p_ble_scan_callback = NULL;
	if(bt_le_scan_stop()) 
		return -1;
	else
		return 0;
}


static int hf_atcmd_entm(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	extern char m2m_app_state;
	m2m_app_state = M2M_STATE_RUN_THROUGH;
	return 0;
}


static int hf_atcmd_uart(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc == 0)
	{
		char s_parity[10];
		char s_fc[10];
		switch(g_hfilop_config.uart_parity)
		{
			case NO_PARITY:
				strcpy(s_parity, "NONE");
				break;
			case ODD_PARITY:
				strcpy(s_parity, "ODD");
				break;
			case EVEN_PARITY:
				strcpy(s_parity, "EVEN");
				break;

			default:
				strcpy(s_parity, "NONE");
				break;
		}

		switch(g_hfilop_config.uart_fc)
		{
			case FLOW_CONTROL_DISABLED:
				strcpy(s_fc, "NFC");
				break;
			case FLOW_CONTROL_CTS_RTS:
				strcpy(s_fc, "FC");
				break;

			default:
				strcpy(s_fc, "NFC");
				break;
		}
			
		sprintf(rsp,"=%d,%d,%d,%s,%s", g_hfilop_config.uart_baud, g_hfilop_config.uart_databit+5, g_hfilop_config.uart_stopbit+1, s_parity, s_fc);
		return 0;
	}
	else if(argc==1||argc==5)
	{
		int baud = 115200;
		int databit = DATA_WIDTH_8BIT;
		int parity = NO_PARITY;
		int stopbit = STOP_BITS_1;
		int fc = FLOW_CONTROL_DISABLED;
	
		baud = atoi(argv[0]);
		switch (baud)
		{
			case 1200:
			case 1800:
			case 2400:
			case 4800:
			case 9600:
			case 19200:
			case 38400:
			case 57600:
			case 115200:
			case 230400:
			case 380400:
			case 460800:
			case 921600:
				break;
				
			default:
				return -4;
		}
		if(argc == 1)
		{
			g_hfilop_config.uart_baud = baud;
			g_hfilop_config.uart_databit = databit;
			g_hfilop_config.uart_parity = parity;
			g_hfilop_config.uart_stopbit = stopbit;
			g_hfilop_config.uart_fc = fc;
			hfilop_config_save();
			return 0;
		}

		switch (atoi(argv[1]))
		{
			case 8:
				databit = DATA_WIDTH_8BIT;
				break;
			case 7:
				databit = DATA_WIDTH_7BIT;
				break;
			case 6:
				databit = DATA_WIDTH_6BIT;
				break;
			case 5:
				databit = DATA_WIDTH_5BIT;
				break;
			default:
				return -4;
		}

		if(strncasecmp("none", argv[3], 4)==0)
		{
			parity= NO_PARITY;
		}
		else if(strncasecmp("odd", argv[3], 3)==0)
		{
			parity= ODD_PARITY;
		}
		else if(strncasecmp("even", argv[3], 4)==0)
		{
			parity= EVEN_PARITY;
		}
		else
			return -4;

		switch (atoi(argv[2]))
		{
			case 2:
				stopbit = STOP_BITS_2;
				break;
			case 1:
				stopbit = STOP_BITS_1;
				break;
			default:
				return -4;
		}

		if(strncasecmp("nfc", argv[4], 3)==0)
		{
			fc = FLOW_CONTROL_DISABLED;
		}
		else if(strncasecmp("fc", argv[4], 2)==0)
		{
			fc = FLOW_CONTROL_CTS_RTS;
		}
		else
			return -4;

		g_hfilop_config.uart_baud = baud;
		g_hfilop_config.uart_databit = databit;
		g_hfilop_config.uart_parity = parity;
		g_hfilop_config.uart_stopbit = stopbit;
		g_hfilop_config.uart_fc = fc;
		hfilop_config_save();
		return 0;
	}
	
	return -3;
}

static int hf_atcmd_mid(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{
		sprintf(rsp,"=%s", g_hfilop_config.mid);
		return 0;
	}
	else
		return -3;
}

static int hf_atcmd_wrmid(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc!=1)
		return -3;

	if(strlen(argv[0])>20)
		return -4;
	
	if(strlen(argv[0])==1&&*argv[0]==' ')
	{
		memset(g_hfilop_config.mid, 0, sizeof(g_hfilop_config.mid));
	}
	else
	{
		memset(g_hfilop_config.mid, 0, sizeof(g_hfilop_config.mid));
		strcpy((char*)g_hfilop_config.mid,argv[0]);
	}
	
	hfilop_config_save();
	return 0;
}

static int hf_atcmd_mtype(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{
		switch(HF_MODULE_ID)
		{
			case HFILOP_MODULE_LPT270:
				sprintf(rsp, "=%s", "HF-LPT270");
				break;
			case HFILOP_MODULE_LPT170:
				sprintf(rsp, "=%s", "HF-LPT170");
				break;
			case HFILOP_MODULE_LPT570:
				sprintf(rsp, "=%s", "HF-LPT570");
				break;
			case HFILOP_MODULE_LPT271:
				sprintf(rsp, "=%s", "HF-LPT271");
				break;	
			case HFILOP_MODULE_LPB170:
				sprintf(rsp, "=%s", "HF-LPB170");
				break;	
			default:
				return -5;
		}
		
		return 0;
	}
	else
		return -3;
}

static int hf_atcmd_wsssid(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	netmgr_ap_config_t config;
	if(argc==0)
	{
		netmgr_get_ap_config(&config);
		sprintf(rsp, "=%s", config.ssid);
		return 0;
	}

	uint8_t ssid_len;
	ssid_len=(uint8_t)strlen((const char *)argv[0]);
	if(ssid_len > 32)
		return -4;

	if( (argc==2) && (strlen((const char *)argv[1]) != 12))
		return -4;

	netmgr_get_ap_config(&config);
	memset(config.ssid, 0, sizeof(config.ssid));
	if(strlen(argv[0])==1&&*argv[0]==' ')
		;
	else
		memcpy(config.ssid, (const char *)argv[0], ssid_len);

	netmgr_set_ap_config(&config);
	hf_set_wifi_ssid();
	
	return 0;
}

static int hf_atcmd_wskey(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	netmgr_ap_config_t config;

	if(argc == 0)
	{
		#if 0
		int len=sizeof(netmgr_ap_config_t);
	//	aos_kv_get(NETMGR_WIFI_KEY,&config,&len);

	    memset(&config,0,sizeof(netmgr_ap_config_t));
		netmgr_get_ap_config(&config);
		HFILOP_PRINTF("config.ssid=%s",config.ssid);
		HFILOP_PRINTF("config.pwd=%s",config.pwd);
		if(strlen(config.pwd) <= 0)
			sprintf(rsp, "=OPEN,NONE");
		else
			sprintf(rsp, "=WPA2PSK,AES,%s", config.pwd);
		#endif
		
		return -1;
	}
	else if(2 == argc)
	{
		if (strcasecmp(argv[0], "open") || strcasecmp(argv[1], "none"))
			return -4;

		netmgr_get_ap_config(&config);
		memset(config.pwd, 0, sizeof(config.pwd));
		netmgr_set_ap_config(&config);
		//aos_kv_set(NETMGR_WIFI_KEY, (unsigned char *)&config, sizeof(netmgr_ap_config_t), 1);
		return 0;
	}
	else if(3 == argc)
	{
		if (strcasecmp(argv[0], "wpa2psk") || strcasecmp(argv[1], "aes"))
			return -4;

		netmgr_get_ap_config(&config);
		memset(config.pwd, 0, sizeof(config.pwd));
		strcpy(config.pwd, (const char *)argv[2]);
		HFILOP_PRINTF("sg config.pwd=%s",config.pwd);
		netmgr_set_ap_config(&config);
		HFILOP_PRINTF("sg netmgr_set_ap_config");

		hf_set_wifi_ssid();

		return 0;
	}

	return -3;
}

static int hf_atcmd_wslk(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{
		hal_wifi_link_stat_t out_stat;
		memset(&out_stat, 0, sizeof(hal_wifi_link_stat_t));
		hal_wifi_get_link_stat(NULL, &out_stat);
		
		if(out_stat.is_connected == 0)
		{
			strcpy(rsp, "=Disconnected");
		}
		else
		{
			uint8_t bssid[6];
			memcpy(bssid, out_stat.bssid, 6);
			sprintf(rsp, "=%s(%02X:%02X:%02X:%02X:%02X:%02X)", out_stat.ssid, bssid[0],bssid[1],bssid[2],bssid[3],bssid[4],bssid[5]);
		}
		return 0;
	}
	else 
		return -3;
}

static int hfwifi_transform_rssi(int rssi_dbm)
{
	int ret;
	ret = (rssi_dbm+95)*2;	

	if (ret < 70) 
		ret = ret -(15 - ret/5);

	if(ret < 0)	
		ret = 0;
	else if(ret >100)	
		ret = 100;
	
	return ret;
}
char CONN_SSID[64]={0};
int conn_sigal=0;
static int hf_scan_ssid_success_flag=0;

static int wifi_scan_cb_updata_rssi(ap_list_adv_t *info)
{
	if(info!=NULL)
	{
	//	HFILOP_PRINTF("%d,%s,%d----------\n\r", info->channel, info->ssid,info->ap_power);
		 if(strcmp(info->ssid,CONN_SSID)==0)
	       {	
	       	HFILOP_PRINTF("%d,%s,%d----------\n\r", info->channel, info->ssid,info->ap_power);
			conn_sigal=info->ap_power;
			 hf_scan_ssid_success_flag=1;

	       }
	}
	return 0;
}

static int hf_atcmd_wslq(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{
		hal_wifi_link_stat_t out_stat;
		memset(&out_stat, 0, sizeof(hal_wifi_link_stat_t));
		hal_wifi_get_link_stat(NULL, &out_stat);

		if(out_stat.is_connected == 0)
		{
			strcpy(rsp, "=Disconnected");
			return 0;
		}
		hf_scan_ssid_success_flag=0;
		memset(CONN_SSID,0,64);
		strcpy(CONN_SSID,out_stat.ssid);
		conn_sigal=0;
		int i;
		hfilop_wifi_scan(wifi_scan_cb_updata_rssi);
		HFILOP_PRINTF("wslq end -----------\r\n");
		if(hf_scan_ssid_success_flag==1)
		{
			if(conn_sigal>70)
			{
				sprintf(rsp, "=Good, %d%%", conn_sigal);
			}else if(conn_sigal>40)
			{
				sprintf(rsp, "=Normal, %d%%", conn_sigal);
			}else 
			{
				sprintf(rsp, "=Weak, %d%%", conn_sigal);
			}
			return 0;
		}else
		{
	 		int rssi = hfwifi_transform_rssi(out_stat.wifi_strength);
			if(out_stat.is_connected == 0)
				strcpy(rsp, "=Disconnected");
			else if(rssi>70)
				sprintf(rsp, "=Good, %d%%", rssi);
			else if(rssi>40)
				sprintf(rsp, "=Normal, %d%%", rssi);
			else
				sprintf(rsp, "=Weak, %d%%", rssi);
				
				return 0;
		}
	}
	else
		return -3;
}

static int wifi_scan_cb(ap_list_adv_t *info)
{
	unsigned char mac[6];
	memcpy(mac, info->bssid, 6);
	
	char rsp[128];
	sprintf(rsp, "%d,%s,%02X:%02X:%02X:%02X:%02X:%02X,%d\n\r", info->channel, info->ssid, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], info->ap_power);
	hfilop_uart_send_data(rsp, strlen(rsp));
	return 0;
}

static int hf_atcmd_wscan(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{
		hfilop_wifi_scan(wifi_scan_cb);
		return 0;
	}
	else
		return -3;
}

static int hf_atcmd_wann(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{
		hal_wifi_link_stat_t out_stat;
		memset(&out_stat, 0, sizeof(hal_wifi_link_stat_t));
		hal_wifi_get_link_stat(NULL, &out_stat);

		hal_wifi_ip_stat_t out_net_para;
		memset(&out_net_para, 0, sizeof(hal_wifi_ip_stat_t));
		hal_wifi_get_ip_stat(NULL, &out_net_para, STATION);

		if(out_stat.is_connected == 0)	
			sprintf(rsp,"=DHCP,0.0.0.0,0.0.0.0,0.0.0.0");
		else
			sprintf(rsp,"=DHCP,%s,%s,%s", out_net_para.ip, out_net_para.mask, out_net_para.gate);

		return 0;
	}
	
	return -3;
}

void hfsys_get_wifi_mac(uint8_t *mac)
{
	memcpy(mac,g_hfilop_config.mac,6);
}

static int hf_atcmd_wsmac(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	uint8_t mac[6];
	char temp[3];
	int i,value;
	
	if(argc==0)
	{
		memcpy(mac, g_hfilop_config.mac, 6);	
		sprintf(rsp,"=%02X%02X%02X%02X%02X%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
		return 0;
	}
	else if(argc==3)
	{
	
		if(strcmp(argv[0],"8888")!=0)
			return -4;
		if(strlen(argv[1])!=12)
			return -4;

		for(i=0;i<12;i++)
		{
			if (((argv[1][i] >= 'a')&&(argv[1][i] <= 'f'))
				|| ((argv[1][i] >= 'A')&&(argv[1][i] <= 'F')) 
				|| ((argv[1][i] >= '0')&&(argv[1][i] <= '9')))
				;
			else
				return -4;				
		}
		
		for(i=0;i<12;i++)
		{
			if ((argv[1][i] >= 'a')&&(argv[1][i] <= 'z'))
				argv[1][i] = (argv[1][i] - 'a')+'A';
		}
		
		for(i=0;i<12;)
		{
			temp[0]=argv[1][i];
			temp[1]=argv[1][i+1];
			temp[2]=0;
			if((value=strtol(temp, NULL, 16))<0)
				return -4;
			mac[i/2]=(uint8_t)value;
			i+=2;
		}

		if(strlen(argv[2]) != 8)
			return -4;
		
		extern int check_mac_key_valid(char *mac, char *key);
		if(!check_mac_key_valid(argv[1], argv[2]))
			return -4;
		
		memcpy(g_hfilop_config.mac, mac, 6);
        		bl_wifi_mac_addr_set(mac);
		bl_wifi_sta_mac_addr_set(mac);

		strcpy(g_hfilop_config.key_valid, argv[2]);

		hfilop_config_save();
		return 0;
	}
	else
		return -3;
}

static int hf_atcmd_pwm(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc == 3)
	{
		int pin = atoi(argv[0]);
		int fre = atoi(argv[1]);
		int duty = atoi(argv[2]);

		if(pin < 0 || pin > 4)
			return -4;
		if(fre < 200 || pin > 1000000)
			return -4;
		if(duty < 0 || pin > 100)
			return -4;
		
		pwm_dev_t pwm;
		pwm.port = pin;
		pwm.config.freq = fre;
		pwm.config.duty_cycle = duty*1.0/100;
		if(hal_pwm_init(&pwm) != 0)
			return -5;
		if(hal_pwm_start(&pwm) != 0)
			return -5;

		return 0;
	}

	return -3;
}

static int hf_atcmd_ver(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{
		sprintf(rsp, "=%s, %s, HFSDK:%s", aos_version_get(), aos_get_app_version(), HFILOP_VERSION);
		return 0;
	}

	return -3;
}

static int hf_atcmd_ota(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc == 0)
	{
		hfilop_ota_task_start(0);
		return 0;
	}

	return -3;	
}

static int hf_atcmd_reld(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	hfilop_config_reld();
	netmgr_ap_config_t config;
	netmgr_get_ap_config(&config);
	memset(config.ssid, 0, sizeof(config.ssid));
	memset(config.pwd, 0, sizeof(config.pwd));

	netmgr_set_ap_config(&config);
	aos_kv_set(NETMGR_WIFI_KEY, (unsigned char *)&config, sizeof(netmgr_ap_config_t), 1);
	extern void cmd_reply(const char *reply, int reply_len);
	cmd_reply("+ok=rebooting...", strlen("+ok=rebooting..."));

	aos_msleep(100);
	aos_reboot();
	return 0;
}

static int hf_atcmd_clr(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc == 0)
	{
		hal_flash_erase(HAL_PARTITION_PARAMETER_1, 0, 4096);
		hal_flash_erase(HAL_PARTITION_PARAMETER_2, 0, 4096*2);
		hal_flash_erase(HAL_PARTITION_PARAMETER_3, 0, 4096);
		hal_flash_erase(HAL_PARTITION_PARAMETER_4, 0, 4096);
		return 0;
	}
	else
		return -3;
}

static int hf_atcmd_reset(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	extern void cmd_reply(const char *reply, int reply_len);
	cmd_reply("+ok", strlen("+ok"));

	aos_msleep(100);
	aos_reboot();
	return 0;
}

static uint8_t testpin_num=17;
static uint8_t testpin[20]={4,5,6,7,11,12,13,14,15,16,19,21,26,28,29,30,31};
static int find_test_pin(int test_pin)
{
	int i;
	
	for(i=0;i<testpin_num;i++)
	{
		if(testpin[i]==test_pin)
			return 1;
	}

	return 0;
}

const int hfilop_gpio_pin_table[]=
{
	0,0,0,0,21,5,4,6,0,0,//10
	0,3,7,13,12,0,9,0,0,0,//20
	0,0,0,0,0,0,23,0,22,8,//30
	24,25,0,0,0,0,0,0,0,0,//40
};

static int hfilop_gpio_get_pin(int pin_no)
{
	if(pin_no>(int)(sizeof(hfilop_gpio_pin_table)/sizeof(int))||pin_no<=0)
		return -1;
	else
		return hfilop_gpio_pin_table[pin_no];
}

static int hf_atcmd_testpin(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	int spin,dstpin,output;
	if(argc!=3||testpin_num<2)
		return -3;

	spin = atoi(argv[0]);
	dstpin = atoi(argv[1]);
	output = atoi(argv[2]);

	if(!find_test_pin(spin)||!find_test_pin(dstpin))
	{
		return -4;
	}

	int index_spin = hfilop_gpio_get_pin(spin);
	int index_dstpin = hfilop_gpio_get_pin(dstpin);
	if(index_spin < 0 || index_dstpin < 0)
		return -4;

	gpio_dev_t gpio_spin, gpio_dstpi;
	gpio_spin.port = index_spin;
	gpio_spin.config = ANALOG_MODE;
	gpio_dstpi.port = index_dstpin;
	gpio_dstpi.config = ANALOG_MODE;
	hal_gpio_init(&gpio_spin);
	hal_gpio_init(&gpio_dstpi);
	
	if(output)
	{
		hal_gpio_output_high(&gpio_spin);
	}
	else
	{
		hal_gpio_output_low(&gpio_spin);
	}

	uint32_t value;
	if(hal_gpio_input_get(&gpio_dstpi, &value))
	{
		strcpy(rsp,"=1");
	}
	else
		strcpy(rsp,"=0");
		
	return 0;
}

static int hf_atcmd_product(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{
		sprintf(rsp, "=%s,%s", g_hfilop_config.product_key, g_hfilop_config.product_secret);
		return 0;
	}
	else if(argc==2)
	{
		if(strlen(argv[0]) >= sizeof(g_hfilop_config.product_key))
			return -4;
		if(strlen(argv[1]) >= sizeof(g_hfilop_config.product_secret))
			return -4;

		strcpy(g_hfilop_config.product_key, argv[0]);
		strcpy(g_hfilop_config.product_secret, argv[1]);
		
		if(strcasecmp(argv[0],"none")==0)
			memset(g_hfilop_config.product_key, 0, sizeof(g_hfilop_config.product_key));
		if(strcasecmp(argv[1],"none")==0)
			memset(g_hfilop_config.product_secret, 0,sizeof(g_hfilop_config.product_secret));
		
		hfilop_config_save();
		return 0;
	}

	return -3;
}

static int hf_atcmd_device(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{
		sprintf(rsp, "=%s,%s", g_hfilop_config.device_name, g_hfilop_config.device_secret);
		return 0;
	}
	else if(argc==1)
	{
		if(strlen(argv[0]) >= sizeof(g_hfilop_config.device_name))
			return -4;

		strcpy(g_hfilop_config.device_name, argv[0]);
		strcpy(g_hfilop_config.device_secret, "");
		hfilop_config_save();
		return 0;
	}
	else if(argc==2)
	{
		if(strlen(argv[0]) >= sizeof(g_hfilop_config.device_name))
			return -4;
		if(strlen(argv[1]) >= sizeof(g_hfilop_config.device_secret))
			return -4;

		strcpy(g_hfilop_config.device_name, argv[0]);
		if((strcasecmp(argv[1], "NULL")==0) || (strcasecmp(argv[1], "NONE")==0))
			strcpy(g_hfilop_config.device_secret, "");
		else
			strcpy(g_hfilop_config.device_secret, argv[1]);
		hfilop_config_save();
		return 0;
	}

	return -3;
}

static int hf_atcmd_prodevice(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{
		sprintf(rsp, "=%s,%s,%s",g_hfilop_config.product_key,g_hfilop_config.device_name, g_hfilop_config.device_secret);
		return 0;
	}
	else if(argc>0 && argc<=3)
	{
		if(strlen(argv[0]) >= sizeof(g_hfilop_config.product_key))
			return -4;
		if(argc > 1)
		{
			if(strlen(argv[1]) >= sizeof(g_hfilop_config.device_name))
				return -4;
		}
		if(argc > 2)
		{
			if(strlen(argv[2]) >= sizeof(g_hfilop_config.device_secret))
				return -4;
		}
		strcpy(g_hfilop_config.product_key, argv[0]);
		sprintf(g_hfilop_config.device_name, "%02X%02X%02X%02X%02X%02X", 
				g_hfilop_config.mac[0],g_hfilop_config.mac[1],g_hfilop_config.mac[2],g_hfilop_config.mac[3],g_hfilop_config.mac[4],g_hfilop_config.mac[5]);
		strcpy(g_hfilop_config.device_secret, "");
		
		if(argc > 1 && strcasecmp(argv[1], "NULL"))
		{
			strcpy(g_hfilop_config.device_name, argv[1]);
		}
		if(argc > 2 && strcasecmp(argv[2], "NULL"))
		{
			strcpy(g_hfilop_config.device_secret, argv[2]);
		}

		hfilop_config_save();
		return 0;
	}

	return -3;
}

static int hf_atcmd_prosecret(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{
		sprintf(rsp, "=%s",g_hfilop_config.product_secret);
		return 0;
	}
	else if(argc==1)
	{
		
		if(strlen(argv[0]) >= sizeof(g_hfilop_config.product_secret))
			return -4;
		strcpy(g_hfilop_config.product_secret, argv[0]);
		hfilop_config_save();
		return 0;
	}

	return -3;
}

static int hf_atcmd_proall(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{
		sprintf(rsp, "=%s,%s,%s,%s,%d",g_hfilop_config.product_key,g_hfilop_config.device_name, g_hfilop_config.device_secret,g_hfilop_config.product_secret,g_hfilop_config.product_id);
		return 0;
	}

	return -3;
}

static int hf_atcmd_wificonn(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	hal_wifi_init_type_t type;
	memset(&type, 0, sizeof(type));
	type.wifi_mode = STATION;
	type.dhcp_mode = DHCP_CLIENT;
	type.reserved[0] = 1;//not wait wifi connected

	if(argc==1)
	{
		if(strlen(argv[0]) > 32)
			return -4;

		strncpy(type.wifi_ssid, argv[0], sizeof(type.wifi_ssid) - 1);
	}
	else if(argc==2)
	{
		if(strlen(argv[0]) > 32)
			return -4;
		if(strlen(argv[1]) > 64)
			return -4;

		strncpy(type.wifi_ssid, argv[0], sizeof(type.wifi_ssid) - 1);
		strncpy(type.wifi_key, argv[1], sizeof(type.wifi_key) - 1);
	}
	else
		return -3;

	hal_wifi_start(NULL, &type);
	return 0;
}

static int hf_atcmd_aliserver(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc == 0)
	{
		switch(g_hfilop_config.server)
		{
			case 0:
				sprintf(rsp,"=%s", "domestic");
				break;
			case 1:
				sprintf(rsp,"=%s", "abroad");
				break;
			 default:
			 	sprintf(rsp,"=%s", "error");
		}
		return 0;
	}
	else if(argc == 1)
	{
		if(strncasecmp("domestic", argv[0], 8)==0)
		{
			g_hfilop_config.server=0;
			hfilop_config_save();
			return 0;
		}else if( strncasecmp("abroad", argv[0], 6)==0)
		{
			g_hfilop_config.server=1;
			hfilop_config_save();
			return 0;
		}
		else
		{
			return -4;
		}
	}
	
	return -3;
}

static int hf_atcmd_alikey(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc == 0)
	{
		if(g_hfilop_config.key_type == 0)
			sprintf(rsp,"=%s", "many");
		else
			sprintf(rsp,"=%s", "one");
		return 0;
	}
	else if(argc == 1)
	{
		if(strcasecmp("many", argv[0])==0)
		{
			g_hfilop_config.key_type=0;
			hfilop_config_save();
			return 0;
		}else if( strcasecmp("one", argv[0])==0)
		{
			g_hfilop_config.key_type=1;
			hfilop_config_save();
			return 0;
		}
		else
		{
			return -4;
		}
	}
	
	return -3;
}

#define get_ntp_time_timeout		1000 // unit: ms
extern void do_awss_ble_start(void);

void do_print_hex(char *head, unsigned char *data, int len, char *tail)
{
	int i;
	int line;

	printf("%s::len=%d\r\n", head,len);
	line=0;
	for (i=0;i<len;i++)
	{
		printf("%02X ", *(data+i));
		line++;
		if (line>=10)
		{
			printf("\r\n");
			line=0;
		}
	}
	printf("\r\n%s\r\n", tail);
}



static int hf_cmd_debug_level(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
    if(argc==0)
    {
        unsigned int debug_level=aos_get_log_level();
        sprintf(rsp,"=%d",debug_level);
		return 0;
    }
	else if(argc==1)
	{ 
        aos_set_log_level(atoi(argv[0]));
        return 0;
    }
	return -3;
}

static int hf_cmd_awss_active(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
    if(argc==0)
    {
    	g_hfilop_config.awss_mode=ILOP_AWSS_MODE; 
		
        hfilop_uart_send_data("+ok\r\n\r\n",strlen("+ok\r\n\r\n"));
        hfilop_config_save();
		aos_msleep(200);
		aos_reboot();
		
		return 0;
    }
	return -3;
}

unsigned char hfsys_get_awss_state()
{
	return g_hfilop_config.awss_flag;
}

void hfsys_set_awss_state(unsigned char state)
{
	g_hfilop_config.awss_flag = state;
	hfilop_config_save();
}

void hfsys_set_dms()
{
	hfsys_set_awss_state(AWSS_STATE_OPEN);
        aos_schedule_call(do_awss_reset, NULL);
}
static int hf_cmd_awss_reset(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
    if(argc==0)
    {
    	hfsys_set_dms();
		return 0;
    }
	return -3;
}
#if 0
static int hf_cmd_awss_smtlkble(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
    if(argc==0)
    {
    	aos_schedule_call(do_awss_ble_start, NULL);
		return 0;
    }
	return -3;
}
#endif

static int hf_cmd_send_mode(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
    if(argc==0)
    {
    	if(g_hfilop_config.ilop_mode==ILOP_MODE_PASS_THROUGH)
       	{ 
       		sprintf(rsp,"=raw");
		}else if(g_hfilop_config.ilop_mode==ILOP_MODE_ICA)
		{	
			sprintf(rsp,"=ICA");
		}else if(g_hfilop_config.ilop_mode==ILOP_MODE_JSON)
		{
			sprintf(rsp,"=JSON");
		}
		else
		{
			return -4;
		}
		return 0;
    }
	else if(argc==1)
	{ 
        uint32_t off_set = 0;
		if(strcasecmp(argv[0],"raw")==0)
			{
				 g_hfilop_config.ilop_mode=ILOP_MODE_PASS_THROUGH;   
			}else if(strcasecmp(argv[0],"ica")==0)
			{
				g_hfilop_config.ilop_mode=ILOP_MODE_ICA; 
			}else if(strcasecmp(argv[0],"json")==0)
			{
				g_hfilop_config.ilop_mode=ILOP_MODE_JSON; 
			}else
			{
				return -3;
			}
       
        hfilop_config_save();
        return 0;
    }
	return -3;
}

ILOP_CONNECT_STATUS ilop_connect_status=WIFI_DISCONNECT;

static int hf_cmd_ilop_connect_status(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
    if(argc==0)
    {
        if(ilop_connect_status==WIFI_CONNECT)
            sprintf(rsp,"=%s","WIFI_CONNECT");
        else if(ilop_connect_status==WIFI_DISCONNECT)
            sprintf(rsp,"=%s","WIFI_DISCONNECT");
        else if(ilop_connect_status==SERVER_CONNECT)
            sprintf(rsp,"=%s","SERVER_CONNECT");
        else if(ilop_connect_status==SERVER_DISCONNECT)
            sprintf(rsp,"=%s","SERVER_DISCONNECT");
		return 0;
    }
	return -3;
}


static int hf_cmd_echo(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
    extern char cmd_mode_echo;
    if(argc==0)
    { 
        if(cmd_mode_echo==0)
            cmd_mode_echo=1;
        else
            cmd_mode_echo=0;
		return 0;
    }
    else if(argc==1)
    {
        uint32_t off_set = 0;
        if(strcasecmp(argv[0],"on")==0)
        {
            g_hfilop_config.echo_mode=1;
            cmd_mode_echo=1;
			LOG("UserIlopSysConfig.echo_mode=%d--",g_hfilop_config.echo_mode);
            hfilop_config_save();
            return 0;
        }
        else if(strcasecmp(argv[0],"off")==0)
        {
            g_hfilop_config.echo_mode=0;
            cmd_mode_echo=0;
			LOG("UserIlopSysConfig.echo_mode=%d--",g_hfilop_config.echo_mode);
            hfilop_config_save();
            return 0;
        }
        else
            return -4;
    }
	return -3;
}

static void week_to_string(int week, char *string)
{
	switch(week)
	{
		case 0:
			strcpy(string, "Sun");
			break;
		case 1:
			strcpy(string, "Mon");
			break;
		case 2:
			strcpy(string, "Tues");
			break;
		case 3:
			strcpy(string, "Wed");
			break;
		case 4:
			strcpy(string, "Thur");
			break;
		case 5:
			strcpy(string, "Fri");
			break;
		case 6:
			strcpy(string, "Sat");
			break;
		default:return;
	}
}
extern void do_ble_awss();


void hfsys_start_dms()
{
	unsigned char buf[100];
    memset(buf,0,sizeof(buf));
		
	awss_config_press_start_flag=true;
	
	if(g_hfilop_config.awss_mode==ILOP_AWSS_MODE)
	{	
		strcpy(buf,"+ILOPCONNECT=AWSS_START\r\n\r\n");
		printf("hf start wifi sta config ... \r\n");
	}
	else if(g_hfilop_config.awss_mode==ILOP_AWSS_DEV_AP_MODE)
	{
		printf("hf start ap config ... \r\n");
		

		strcpy(buf,"+ILOPCONNECT=DEV_AP_START\r\n\r\n");
		aos_schedule_call((aos_call_t)do_awss_dev_ap, NULL);
	}
	else if(g_hfilop_config.awss_mode==ILOP_AWSS_DEV_BLE_MODE)
	{
	   	printf("start ble config\r\n");

		strcpy(buf,"+ILOPCONNECT=BLE_START\r\n\r\n");
		
	   	do_ble_awss();
	}

	
        
    if(!mcu_ota_start_flag && !g_hfilop_config.tmod)//--20190108
        hfilop_uart_send_data(buf,strlen(buf));

	hfiolp_check_awss();
	
}

void hfiolp_check_awss()
{
    netmgr_ap_config_t config1;
    memset(&config1, 0, sizeof(netmgr_ap_config_t));
    int ret=netmgr_get_ap_config(&config1);
    int32_t len = strlen(config1.ssid);
    if(len<=0)
    {
        LOG("no have wifi ssid can active_awss\n");
        extern int awss_config_press();
        awss_config_press();
        
        awss_config_timeout_check();
    }else
    {
        #ifdef SUPPORT_MCU_OTA
        //aos_post_delayed_action(2000, send_mcu_upgrade_file_ver, NULL);
        #endif
    }
}

void connect_succeed_ssid_passwd_copy(void)
{
    netmgr_ap_config_t config;
    netmgr_get_ap_config(&config);

    bool save_flag=false;
    if(strcmp(config.ssid,g_hfilop_config.last_connect_ssid)!=0)
    {
        memset(g_hfilop_config.last_connect_ssid,0,sizeof(g_hfilop_config.last_connect_ssid));
        strcpy(g_hfilop_config.last_connect_ssid,config.ssid);
        LOG("router connect_succeed_ssid copy:%s\r\n",g_hfilop_config.last_connect_ssid);
        save_flag=true;
    }
    if(strcmp(config.pwd,g_hfilop_config.last_connect_key)!=0)
    {
        memset(g_hfilop_config.last_connect_key,0,sizeof(g_hfilop_config.last_connect_key));
        strcpy(g_hfilop_config.last_connect_key,config.pwd);
        LOG("router connect_succeed_passwd copy:%s\r\n",g_hfilop_config.last_connect_key);
        save_flag=true;
    }

    if(save_flag)
    {
        uint32_t off_set = 0;
        hfilop_config_save();
    }
}

int get_tsl_from_cloud_success_flag=0;
static bool link_gpio_is_high=true;
static aos_timer_t device_status_light_timer;
void device_status_light_timer_callback(void *timer,void *args)
{
    //Ready
    static char *PK=NULL;
    static char *DM=NULL;
    static long long last_check_ready_status_systime=0;
    long long now=aos_now_ms();
    if(now-last_check_ready_status_systime >= 2000)
    {
        last_check_ready_status_systime=now;
        PK=hfilop_layer_get_product_key();
        DM=hfilop_layer_get_device_name();
        if((PK && DM) && (strlen(PK)>0 && strlen(DM)>0))
            hal_gpio_output_low(&GPIO_Ready);
        else
            hal_gpio_output_high(&GPIO_Ready);
    }       

    //Link
    if(awss_config_press_start_flag)
    {
        if(link_gpio_is_high)
        {
            hal_gpio_output_low(&GPIO_Link);
            link_gpio_is_high=false;
        }
        else
        {
            hal_gpio_output_high(&GPIO_Link);
            link_gpio_is_high=true;
        }
    }
    else if(ilop_connect_status == WIFI_DISCONNECT)
    {
        if(!link_gpio_is_high)
        {
            hal_gpio_output_high(&GPIO_Link);
            link_gpio_is_high=true;
        }
    }
    else if(ilop_connect_status >= WIFI_CONNECT)
    {
        if(link_gpio_is_high)
        {
            hal_gpio_output_low(&GPIO_Link);
            link_gpio_is_high=false;
        }
    }
    
}

void hfsys_ready_link_gpio_init(void)
{
    GPIO_Link.port=5;
    GPIO_Ready.port=4;
    GPIO_Link.config=OUTPUT_PUSH_PULL;
    GPIO_Ready.config=OUTPUT_PUSH_PULL;

    hal_gpio_init(&GPIO_Link);
    hal_gpio_init(&GPIO_Ready);
}

int hfsys_device_status_light_timer_create(void)
{
    int ret=-1;
    static bool create_flag=false;
    if(create_flag)
    {
        LOG("device_status_light_timer have created.\r\n");
        return ret;
    }
    create_flag=true;

    hal_gpio_output_high(&GPIO_Link);
    hal_gpio_output_high(&GPIO_Ready);
    link_gpio_is_high=false;
        
    #define TIMER_TIMEOUT_MS (200)
    ret=aos_timer_new(&device_status_light_timer, device_status_light_timer_callback,NULL, TIMER_TIMEOUT_MS, 1);
    if(ret!=0)
        LOG("Warning: hfsys device_status_light_timer_create failed\r\n");
    return ret;
}

const int8_t wifi_rf_table_lpt270[48] = 
{
	16, 16, 16, 16, 00, 00, 00, 00, //power dbm for 11b 1Mbps/2Mbps/5Mbps/11Mbps
	14, 14, 14, 14, 14, 14, 14, 14, //power dbm for 11g 6,9,12,18,24,36,48,54 Mbps
	13, 13, 13, 13, 13, 13, 13, 13, //power dbm for 11n MCS0~MCS7
	00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, //power re-cal for channel 1~14
	34 //capcode
};

const int8_t wifi_rf_table_lpt170[48] = 
{
	16, 16, 16, 16, 00, 00, 00, 00, //power dbm for 11b 1Mbps/2Mbps/5Mbps/11Mbps
	14, 14, 14, 14, 14, 14, 14, 14, //power dbm for 11g 6,9,12,18,24,36,48,54 Mbps
	13, 13, 13, 13, 13, 13, 13, 13, //power dbm for 11n MCS0~MCS7
	01, 01, 01, 01, 01, 00, 00, 00, 00, 00, 00, 00, 00, 00, //power re-cal for channel 1~14
	30 //capcode
};

const int8_t wifi_rf_table_lpt570[48] = 
{
	16, 16, 16, 16, 00, 00, 00, 00, //power dbm for 11b 1Mbps/2Mbps/5Mbps/11Mbps
	14, 14, 14, 14, 14, 14, 14, 14, //power dbm for 11g 6,9,12,18,24,36,48,54 Mbps
	13, 13, 13, 13, 13, 13, 13, 13, //power dbm for 11n MCS0~MCS7
	01, 01, 01, 01, 01, 00, 00, 00, 00, 00, 00, 00, 00, 00, //power re-cal for channel 1~14
	38 //capcode
};

const int8_t wifi_rf_table_lpt271[48] = 
{
	15, 15, 15, 15, 00, 00, 00, 00, //power dbm for 11b 1Mbps/2Mbps/5Mbps/11Mbps
	14, 14, 14, 14, 14, 14, 14, 14, //power dbm for 11g 6,9,12,18,24,36,48,54 Mbps
	13, 13, 13, 13, 13, 13, 13, 13, //power dbm for 11n MCS0~MCS7
	01, 01, 01, 01, 01, 01, 01, 00, 00, 00, 00, 00, 00, 00, //power re-cal for channel 1~14
	37 //capcode
};

const int8_t wifi_rf_table_lpb170[48] = 
{
	18, 18, 18, 18, 00, 00, 00, 00, //power dbm for 11b 1Mbps/2Mbps/5Mbps/11Mbps
	16, 16, 16, 16, 16, 16, 16, 16, //power dbm for 11g 6,9,12,18,24,36,48,54 Mbps
	15, 15, 15, 15, 15, 15, 15, 15, //power dbm for 11n MCS0~MCS7
	00, 00, -1, -1, -2, -2, -2, -2, -2, -2, -2, -2, -1, -1, //power re-cal for channel 1~14
	34 //capcode
};

void  hfwifi_rf_init(void)
{
	static int rf_init_flag = 0;
	static int8_t table[48] = {0};
	printf("hfwifi_rf_init !\r\n");
	
	if(rf_init_flag == 0)
	{
		rf_init_flag = 1;
		
		if(HF_MODULE_ID == HFILOP_MODULE_LPT270)
		{
			memcpy(table, wifi_rf_table_lpt270, 48);
			rf_init_flag = 2;
		}
		else if(HF_MODULE_ID == HFILOP_MODULE_LPT170)
		{
			memcpy(table, wifi_rf_table_lpt170, 48);
			rf_init_flag = 2;
		}
		else if(HF_MODULE_ID == HFILOP_MODULE_LPT570)
		{
			memcpy(table, wifi_rf_table_lpt570, 48);
			rf_init_flag = 2;
		}
		else if(HF_MODULE_ID == HFILOP_MODULE_LPT271)
		{
			memcpy(table, wifi_rf_table_lpt271, 48);
			rf_init_flag = 2;
		}
		else if(HF_MODULE_ID == HFILOP_MODULE_LPB170)
		{
			memcpy(table, wifi_rf_table_lpb170, 48);
			rf_init_flag = 2;
		}
		
		if(rf_init_flag == 2)
		{
			uint8_t capcode_slot1 = EF_Ctrl_Is_CapCode_Slot_Empty(1, 1);
			uint8_t capcode_slot2 = EF_Ctrl_Is_CapCode_Slot_Empty(2, 1);
			if(!capcode_slot2 || !capcode_slot1)
			{
				int8_t pf[14];
				uint8_t capcode;
				if(bl_efuse_read_pwroft(pf) == 0)
					memcpy(table+24, pf, 14);
				if(bl_efuse_read_capcode(&capcode) == 0)
					table[38] = (int8_t)capcode;
			}
			
			bl_tpc_update_power_table(table);
			hal_sys_capcode_update((uint8_t)table[38], (uint8_t)table[38]);
		}

		printf("[WIFI] ------init--------rf table:\r\n");
		printf( "  11b: %d %d %d %d %d %d %d %d\r\n", table[0], table[1], table[2], table[3], table[4], table[5], table[6], table[7]);
		printf( "  11g: %d %d %d %d %d %d %d %d\r\n", table[8], table[9], table[10], table[11], table[12], table[13], table[14], table[15]);
		printf( "  11n: %d %d %d %d %d %d %d %d\r\n", table[16], table[17], table[18], table[19], table[20], table[21], table[22], table[23]);
		printf( "  ch: %d %d %d %d %d %d %d %d %d %d %d %d %d %d\r\n", 
			table[24], table[25], table[26], table[27], table[28], table[29], table[30], table[31], table[32], table[33], table[34], table[35], table[36], table[37]);
		printf( "  capcode: %d\r\n", table[38]);
		
	}
	else if(rf_init_flag == 2)
	{
		printf("[WIFI] rf table:\r\n");
		printf( "  11b: %d %d %d %d %d %d %d %d\r\n", table[0], table[1], table[2], table[3], table[4], table[5], table[6], table[7]);
		printf( "  11g: %d %d %d %d %d %d %d %d\r\n", table[8], table[9], table[10], table[11], table[12], table[13], table[14], table[15]);
		printf( "  11n: %d %d %d %d %d %d %d %d\r\n", table[16], table[17], table[18], table[19], table[20], table[21], table[22], table[23]);
		printf( "  ch: %d %d %d %d %d %d %d %d %d %d %d %d %d %d\r\n", 
			table[24], table[25], table[26], table[27], table[28], table[29], table[30], table[31], table[32], table[33], table[34], table[35], table[36], table[37]);
		printf( "  capcode: %d\r\n", table[38]);
	}
}


void network_connection_status_check_process(void)
{
    hal_wifi_link_stat_t wifi_state;
    int last_query_wifi_status=0;
    int last_query_cloud_status=0;
    static bool first_connect_cloud_flag=true;

    hal_wifi_ip_stat_t out_net_para;//--20181123
    int test_count=0;//--20181123

    #define PING_GETEWAY_FAILED_RESET_TIME_MS (3*60*1000)
    static char ping_gateway_success_systime=0;
    ping_gateway_success_systime=aos_now_ms();
    
    aos_msleep(1000);//Waiting uart init
    while(1)
    {
		memset(&wifi_state, 0, sizeof(hal_wifi_link_stat_t));
		hal_wifi_get_link_stat(NULL, &wifi_state);

        memset(&out_net_para, 0, sizeof(hal_wifi_ip_stat_t));//--20181123
		hal_wifi_get_ip_stat(NULL, &out_net_para, STATION);//--20181123
		
		if(wifi_state.is_connected==0)
		{
            if(last_query_wifi_status!=0)
            {
                last_query_wifi_status=0;
                ilop_connect_status_down(WIFI_DISCONNECT);
                ilop_connect_status=WIFI_DISCONNECT;
				cloud_conn_status = 0;
            }
            
            if(!cloud_conn_status)//cloud disconnect
			{
			    if(last_query_cloud_status==1)
				{
                    last_query_cloud_status=0;
                    ilop_connect_status_down(SERVER_DISCONNECT);
                    ilop_connect_status=SERVER_DISCONNECT;
                }
			}
		}
        else
		{
            if(last_query_wifi_status==0)
            {
                last_query_wifi_status=1;
                ilop_connect_status_down(WIFI_CONNECT);
                ilop_connect_status=WIFI_CONNECT;
                //connect_succeed_ssid_passwd_copy();
            }
            
			if(cloud_conn_status)//cloud connect
			{
               
				if(last_query_cloud_status!=1)
				{
                    if(first_connect_cloud_flag)
                    {
                        first_connect_cloud_flag=false;
                    }
                    last_query_cloud_status=1;
                    ilop_connect_status_down(SERVER_CONNECT);
                    ilop_connect_status=SERVER_CONNECT;
					extern unsigned char hfsys_get_awss_state();
					if(hfsys_get_awss_state() == AWSS_STATE_OPEN)
					{
						hfsys_set_awss_state(AWSS_STATE_CLOSE);
					}
                }
			}
			else//cloud disconnect
			{
			    if(last_query_cloud_status==1)
				{
                    last_query_cloud_status=0;
                    ilop_connect_status_down(SERVER_DISCONNECT);
                    ilop_connect_status=SERVER_DISCONNECT;
                }
			}

		}   
		aos_msleep(2000);
	}
}

static int hf_atcmd_time(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{
		int ret;
        extern char nto_time_ms_str[50];
		
        linkkit_ntp_time_test(NULL);
   //     aos_msleep(100);
     ret= aos_sem_wait(&sg_get_ntp_time,get_ntp_time_timeout);
     if(ret==RHINO_SUCCESS)
     	{
	        if(strlen(nto_time_ms_str) >= 13)//US ->  MS
	            nto_time_ms_str[strlen(nto_time_ms_str)-3]=0;
			
	        HFILOP_PRINTF("nto_time_ms_str=%s-----------\r\n",nto_time_ms_str);
			
	        time_t now=atoi(nto_time_ms_str);
			
	        HFILOP_PRINTF("now=%d-----------\r\n",now);
	        int timezone_s=8 * 3600;
	        if(g_hfilop_config.time_zone.time_zone_flag == TIME_ZONE_FLAG)
	            timezone_s=g_hfilop_config.time_zone.time_zone_value * 3600;
	        now+=timezone_s;

	        struct tm *nowtm;
	        nowtm = localtime(&now);

	        char week[10];
	        memset(week,0,sizeof(week));
	        week_to_string(nowtm->tm_wday,week);
	        
			sprintf(rsp,"=%u,%04d-%02d-%02d %02d:%02d:%02d %s",(unsigned int)now,\
				nowtm->tm_year+1900,nowtm->tm_mon+1,\
				nowtm->tm_mday,\
				nowtm->tm_hour,\
				nowtm->tm_min,\
				nowtm->tm_sec,\
				week);
			return 0;
     	}else
     	{
     			sprintf(rsp,"get ntp time fail");
     	}
	}
	else
	{
		return -3;
	}
}

static int hf_atcmd_timezone(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{
        if(g_hfilop_config.time_zone.time_zone_flag == TIME_ZONE_FLAG)
            sprintf(rsp,"=%d",g_hfilop_config.time_zone.time_zone_value);
        else
            sprintf(rsp,"=8");
		return 0;
	}
    else if(argc==1)
	{
        char timezone=atoi(argv[0]);
        if(timezone>=-12 && timezone<=14)
        {
            g_hfilop_config.time_zone.time_zone_flag = TIME_ZONE_FLAG;
            g_hfilop_config.time_zone.time_zone_value=timezone;
            uint32_t off_set = 0;
            hfilop_config_save();
            return 0;
        }
        else
            return -4;
		
	}
	else
	{
		return -3;
	}
}
#if 1
static int hf_cmd_mcu_ota(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
#ifdef  SUPPORT_MCU_OTA
//     if(argc==0)
//     { 
//         if(mcu_file_info.mcu_magic==MCU_IMAGE_MAGIC)
//             sprintf(rsp,"=%d",mcu_file_info.mcu_ver);
//         else
//             sprintf(rsp,"=%s","get fail");
// 		return 0;
//     }
//     else if(argc==1)
//     {
//         if(memcmp("start",argv[0],strlen("start"))==0)
//         {
//             if(mcu_file_info.mcu_magic==MCU_IMAGE_MAGIC)
//                 mcu_ota_thread_start();
//             else
//                 sprintf(rsp,"=%s","error");
            
//         }
//         else 
//         {
//             int mcu_ota_response_ver=atoi(argv[0]);
//             if(mcu_ota_response_ver==mcu_file_info.mcu_ver)
//                 LOG("---------------mcu ota success--------------\r\n");
//             else
//                 LOG("mcu_ota_response_ver:%d.\r\n",mcu_ota_response_ver);
//         }
//         return 0;
//     }
// 	return -3;
// #else
//     return -1;
#endif
}
#endif

char HFTEST_SSID[50]={0};
int HFTEST_sigal=0;
static int scan_ssid_success_flag=0;
static int wifi_scan_cb_test(ap_list_adv_t *info)
{
//	unsigned char mac[6];
//	memcpy(mac, info->bssid, 6);
//	char rsp[128];
//	sprintf(rsp, "%d,%s,%02X:%02X:%02X:%02X:%02X:%02X,%d\n\r", info->channel, info->ssid, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], info->ap_power);
//	hfilop_uart_send_data(rsp, strlen(rsp));
	if(info!=NULL)
	{
		HFILOP_PRINTF("%d,%s,%d----------\n\r", info->channel, info->ssid,info->ap_power);
		 if(strcmp(info->ssid,HFTEST_SSID)==0)
	       {	
	       	HFILOP_PRINTF("%d,%s,%d----------\n\r", info->channel, info->ssid,info->ap_power);
			  if(info->ap_power >=HFTEST_sigal)
			  	{
			  		scan_ssid_success_flag=1;
			  	}
			  else
			  	{
			  		scan_ssid_success_flag=0;
			  	}
	       }
	}
	return 0;
}

static int hf_atcmd_hftest(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==2)
	{
		memset(HFTEST_SSID,0,49);
		strcpy(HFTEST_SSID,argv[0]);
		HFTEST_sigal=atoi(argv[1]);
		HFILOP_PRINTF("HFTEST_SSID=%s HFTEST_sigal=%d---\r\n ",HFTEST_SSID,HFTEST_sigal);
		hfilop_wifi_scan(wifi_scan_cb_test);

		 if(scan_ssid_success_flag==1)
		 	{
		 		sprintf(rsp,"=%s","OK");
		 	}
		 else
		 	{
		 		sprintf(rsp,"=%s","NG");
		 	}
		 return 0;
	}else
		return -3;
	
}

static int hf_atcmd_awssmode(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{
		if(g_hfilop_config.awss_mode==ILOP_AWSS_DEV_AP_MODE)
		{
		 	sprintf(rsp,"=DEV_AP");
		}
		else if(g_hfilop_config.awss_mode==ILOP_AWSS_MODE)
		{
			sprintf(rsp,"=AWSS");
		}
		else 
		{
			sprintf(rsp,"=BLE");
		}
	return 0;
	}
	else if(argc==1)
	{ 
		uint32_t off_set = 0;
		if(strcasecmp(argv[0],"AWSS")==0)
		{
    		g_hfilop_config.awss_mode=ILOP_AWSS_MODE;  
		}
		else if(strcasecmp(argv[0],"DEV_AP")==0)
		{
			g_hfilop_config.awss_mode=ILOP_AWSS_DEV_AP_MODE;
		}
		else if(strcasecmp(argv[0],"BLE")==0)
		{
			g_hfilop_config.awss_mode=ILOP_AWSS_DEV_BLE_MODE;
		}
		else
			return -3;
        hfilop_config_save();
        return 0;
    }
	return -3;
}
static int hf_atcmd_proid(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{
		
		sprintf(rsp,"=%d",g_hfilop_config.product_id);
		return 0;
	}
	else if(argc==1)
	{ 
		g_hfilop_config.product_id=atoi(argv[0]);
		printf("product_id=%d\r\n",g_hfilop_config.product_id);
        hfilop_config_save();
        return 0;
    }
	return -3;
}

static int hf_atcmd_blescanstart(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0 || argc==2)
	{
		if(argc == 2)
		{
			g_interval = atoi(argv[0]);
			g_window = atoi(argv[1]);
		}
		extern void hfble_scan_callback(BLE_SCAN_RESULT_ITEM *item);
		hfble_start_scan(hfble_scan_callback);
		return 0;
	}
	return -3;
}

static int hf_atcmd_blescanstop(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{	
		hfble_stop_scan();
		return 0;
	}
	return -3;
}

static int hf_atcmd_blestartadv(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{	
		extern void hf_start_ble_send_adv();
		hf_start_ble_send_adv();
		return 0;
	}
	return -3;
}

static int hf_atcmd_blestopadv(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{	
		extern void hf_ble_stop_adv();
		hf_ble_stop_adv();
		return 0;
	}
	return -3;
}

int hf_get_product_id()
{
	return g_hfilop_config.product_id;
}

static int hf_atcmd_event(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{
		if((g_hfilop_config.tmod == CONFIG_EVENT_ON))
			strcpy(rsp,"=on");
		else if((g_hfilop_config.tmod == CONFIG_EVENT_OFF))
			strcpy(rsp,"=off");
			
		return 0;
	}
	else if(argc==1)
	{ 
		if(strcasecmp(argv[0],"on")==0)
		{
			g_hfilop_config.tmod = CONFIG_EVENT_ON;
		}
		else if(strcasecmp(argv[0],"off")==0)
		{
			g_hfilop_config.tmod = CONFIG_EVENT_OFF;
		}	
		else
			return -4;
			
        hfilop_config_save();
        return 0;
    }

	return -3;
}


char nto_time_ms_str[50];//--20181212
void ntp_time_reply(const char *offset_time)
{
    HFILOP_PRINTF("ntp time:%s\n", offset_time);
    memset(nto_time_ms_str,0,sizeof(nto_time_ms_str));
    strcpy(nto_time_ms_str,offset_time);
	aos_sem_signal(&sg_get_ntp_time);
//	 nto_time_ms_str[strlen(nto_time_ms_str)-3]='\0';
//	 HFILOP_PRINTF("sg nto_time_ms_str=%s-----------\r\n",nto_time_ms_str);
//	 memset(nto_time_ms_str,0,sizeof(nto_time_ms_str));
//    strcpy(nto_time_ms_str,offset_time);
}

void linkkit_ntp_time_test(void *param)
{
    linkkit_ntp_time_request(ntp_time_reply);
}
bool awss_config_press_start_flag=false;

int ilop_user_sys_config_init(void)
{
    LOG("APPVER :>> %s\r\n",APPVERSION);
    unsigned int off_set=0;
    if(g_hfilop_config.user_ilop_config_init_flag != USER_ILOP_SYS_CONFIG_INIT_FLAG)
    {
    	HFILOP_PRINTF("ilop_user_sys_config_init----------\r\n");
        g_hfilop_config.user_ilop_config_init_flag=USER_ILOP_SYS_CONFIG_INIT_FLAG;
        g_hfilop_config.ilop_mode=ILOP_MODE_ICA;
        g_hfilop_config.echo_mode=1;
		g_hfilop_config.awss_mode=ILOP_AWSS_DEV_BLE_MODE;

      
        off_set=0;
		hfilop_config_save();

    }
	
	aos_sem_new(&sg_get_ntp_time,0);
	LOG("UserIlopSysConfig.echo_mode: %d",g_hfilop_config.echo_mode);
    extern char cmd_mode_echo;
    cmd_mode_echo=g_hfilop_config.echo_mode;
}

static int ica_data_publish(char *property_identifier,int data)
{
	print_heap();

    static int example_index = 0;
    int res = 0;
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    char *property_payload = "NULL";
	cJSON *response_root = NULL;
	response_root = cJSON_CreateObject();
	if ((NULL == response_root) )
		{
			cJSON_Delete(response_root);
			LOG("*************Json object create error");
			return ;
		}
	cJSON_AddNumberToObject(response_root,property_identifier,data);
	
	property_payload = cJSON_PrintUnformatted(response_root);
	cJSON_Delete(response_root);

    res = IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_PROPERTY,
                             (unsigned char *)property_payload, strlen(property_payload));
	example_free(property_payload);
    LOG("Post Property Message ID: %d", res);
	print_heap();
	return res;

}
static int ica_event_publish(char * event,int argc,char *argv[])
{
  print_heap();
static int example_index = 0;
    int res = 0;
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
  //  char *event_id = "NULL";
   // char *event_payload = "NULL";
    char event_id[50]={0} ;
    char event_payload[500]={0} ;
	char event_identifier[50]={0} ;
	char *p_identifier=NULL;

	strcpy(event_id,event);
	HFILOP_PRINTF("HF post  event_id=%s-------------\r\n",event_id);
	sprintf(event_payload,"{");
		int i;
		for(i=1;i<argc-1;i+=2)
			{
				p_identifier =strstr(argv[i],".");
                if(p_identifier != NULL)
                	{
                		memset(event_identifier,0,50);
						strcpy(event_identifier,p_identifier+1);
						sprintf(event_payload,"%s\"%s\":%s,",event_payload,event_identifier,argv[i+1]);
                	}
				else{
						sprintf(event_payload,"%s\"%s\":%s,",event_payload,argv[i],argv[i+1]);
					}
			}
		int len=strlen(event_payload);
		event_payload[len-1]=0;
		sprintf(event_payload,"%s}",event_payload);
	HFILOP_PRINTF("HF post event_payload=%s-----------\r\n",event_payload);
    res = IOT_Linkkit_TriggerEvent(user_example_ctx->master_devid, event_id, strlen(event_id),
                                   event_payload, strlen(event_payload));
    LOG("Post Event Message ID: %d", res);
	 print_heap();
	return res;
}

static int ica_Compound_publish(char * Compound,char*property_identifier,char *data)
{
   print_heap();
static int example_index = 0;
   int res = 0;
   user_example_ctx_t *user_example_ctx = user_example_get_ctx();
   char *property_payload = "NULL";
   cJSON *response_root = NULL;
   response_root = cJSON_CreateObject();
   cJSON *item = cJSON_CreateObject();
   if ((NULL == response_root) )
	   {
		   cJSON_Delete(response_root);
		   LOG("*************Json object create error");
		   return ;
	   }
    if (item == NULL)
		{
            cJSON_Delete(item);
            return ;
        }
 //  cJSON_AddNumberToObject(response_root,property_identifier,data);
	cJSON_AddNumberToObject(item, property_identifier, *data);
   cJSON_AddItemToObject(response_root, Compound, item);
   property_payload = cJSON_PrintUnformatted(response_root);
   cJSON_Delete(response_root);
   cJSON_Delete(item);

   res = IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_PROPERTY,
							(unsigned char *)property_payload, strlen(property_payload));
   example_free(property_payload);

   LOG("Post Property Message ID: %d", res);
   print_heap();
   return res;
}
#if 1
char lefttemperdata[10]={0};
char righttemperdata[10]={0};
int user_filter_send_data(char *property,char *data)
{
	if(memcmp(property,"LeftCurrentTemperature",strlen("LeftCurrentTemperature"))==0)
	{
		LOG("is LeftCurrentTemperature \r\n");
		if(memcmp(data,lefttemperdata,strlen(data))==0)
			return 0;
		else
		{
			memset(lefttemperdata,0,sizeof(lefttemperdata));
            memcpy(lefttemperdata,data,strlen(data));
			return 1;
		}	
	}

	
	if(memcmp(property,"RightCurrentTemperature",strlen("RightCurrentTemperature"))==0)
	{
		LOG("is RightCurrentTemperature \r\n");
		if(memcmp(data,righttemperdata,strlen(data))==0)
			return 0;
		else
		{
			memset(righttemperdata,0,sizeof(righttemperdata));
            memcpy(righttemperdata,data,strlen(data));
			return 1;
		}	
	}
	LOG("is other property \r\n");
	return 1;
}
int hf_cmd_send_ica(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
    if(argc < 3)
    {
        return -1;
    }

	static char property_identifier[200]={0};
	//static char data_str[20]={0};
	char *uart_str=NULL;
	char *uart_arg=NULL;
	int data=0; 
	int res;
	 static char identifier[200];
    if(memcmp(argv[0],"property",strlen("property"))==0)
    {
        if(argc==3)
        {
            LOG("hf_cmd_send_ica: send property:{%s:%s}\r\n",argv[1],argv[2]);
                memset(property_identifier,0,sizeof(property_identifier));
                memcpy(property_identifier,argv[1],strlen(argv[1]));
		//		memset(data_str,0,20);
		//		memcpy(data_str,argv[2],strlen(argv[2]));
           /*     strcpy(property_identifier,argv[1]);
				data=atoi(argv[2]);
				LOG("property_identifier=%s data=%d  \r\n",property_identifier,data);
				res =ica_data_publish(property_identifier,data);*/
			if(user_filter_send_data(property_identifier,argv[2])==1)
			{
				res=hf_user_post_property(property_identifier,argv[2]);//20190514
				if(res <0)
				{
					sprintf(rsp,"=send failed");
				}
			}
            return 0;
        }
        else if(argc>3)
        {
            int i;
			res=hf_user_post_many_property(argc,argv);//20190514
			if(res <0)
				{
					sprintf(rsp,"=send failed");
				}
            return 0;
        }
        else
            return -4;
    }
    else if(memcmp(argv[0],"event",strlen("event"))==0)
    {	
    	
        if(argc>=3)
        {
               
				char event_identifier[100]={0};
				char event[50]={0};
                memset(identifier,0,sizeof(identifier));
				HFILOP_PRINTF("updata event--------\r\n");
                strcpy(identifier,argv[1]);
                char *p_identifier=strstr(identifier,".");
                if(p_identifier != NULL)
					strcpy(event_identifier,p_identifier+1);
			
                   strncpy(event,argv[1],strlen(argv[1])-strlen(event_identifier)-1);
               HFILOP_PRINTF("p_identifier=%s event_identifier=%s argv[2]=%s----\r\n",event,event_identifier,argv[2]);
				// res=ica_event_publish(event,event_identifier,argv[2]);
            res=ica_event_publish(event,argc,argv);
           if(res<0)
           	{
           	sprintf(rsp,"=send failed");
           	}

            return 0;
        }
        else
            return -4;
		//AT+SENDICA=Compound,???¡§???¡ì?¡ì???¡ì?o???¡ì?o?????¨¬??¨¬????¡§?o????¡§???¡ì?¡ì???¡ì?o???¡ì?o?????¨¬??¨¬?.2????¡ì?oy???¡§???¡ì?¡ì???¡ì?o???¡ì?o?,?|??¡§??,???¡§???¡ì?¡ì???¡ì?o???¡ì?o?????¨¬??¨¬?.2????¡ì?oy???¡§???¡ì?¡ì???¡ì?o???¡ì?o?,?|??¡§??,???¡ì??????????¡ì????¨¬??¨¬???¡ì????¨¬??¨¬???¡ì?a?
    }else if(memcmp(argv[0],"Compound",strlen("Compound"))==0)//???o?D???¡ì?a???¡ì?oy?Y
       	{
       		 if(argc>=4)
       		 {
       		 	if((argc%2)!=0)
       		 	{
       		 		return -3;
       		 	}
				res=hf_user_post_compound_property(argc,argv);//20190514
				if(res <0)
				{
					sprintf(rsp,"=send failed");
				}
            	return 0;
#if 0
            	static int example_index = 0;

   				user_example_ctx_t *user_example_ctx_c = user_example_get_ctx();
   				char *property_payload_c = "NULL";
   				cJSON *response_root_c = NULL;
   				response_root_c = cJSON_CreateObject();
  				 cJSON *item = cJSON_CreateObject();
   				if ((NULL == response_root_c) )
	  			 {
		 		  cJSON_Delete(response_root_c);
		  			 LOG("*************Json object create error");
		  			 return ;
	  			 }
   				 if (item == NULL)
				{
           		 cJSON_Delete(item);
           		 return ;
        		}
				 char Compound_identifier[100]={0};
				 int i;
				  char *p_identifier_c=NULL ;
				 for(i=2;i<argc-1;i+=2)
				{
					 LOG("hf_cmd_send_ica: set property:{%s:%s}\r\n",argv[i],argv[i+1]);
					 p_identifier_c=strstr(argv[i],".");
					 if(p_identifier_c!=NULL)
					 {
					 	strcpy(Compound_identifier,p_identifier_c+1);
						
					 }else
					 {
					 strcpy(Compound_identifier,argv[i]);
					 }
					cJSON_AddNumberToObject(item, Compound_identifier, atoi(argv[i+1]));	  
				 }
				 cJSON_AddItemToObject(response_root_c, argv[1], item);

				 property_payload_c = cJSON_PrintUnformatted(response_root_c);
				 cJSON_Delete(response_root_c);
				 
				 res = IOT_Linkkit_Report(user_example_ctx_c->master_devid, ITM_MSG_POST_PROPERTY,
										  (unsigned char *)property_payload_c, strlen(property_payload_c));
				 if(res<0)
           		{
           		sprintf(rsp,"=send failed");
           		}
				  example_free(property_payload_c);
				 LOG("Post Property Message ID: %d", res);
				 return 0;
#endif
        	 }
        	else
           	 return -4;
    		}
   		 return -3;
}
#endif
int hf_cmd_send_raw(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
   return 0;
}

void hfsys_set_link_low()
{
	hal_gpio_output_low(&GPIO_Link);
}


int  ilop_server_data_down(ILOP_MODE ilop_mode,char *key,char *value_str)
{
    int ret=-1;
    if((key==NULL)||(value_str==NULL))
        return ret;
    
    int buf_len=strlen(value_str)+100;
    char *ilop_server_data_down_buf=aos_malloc(buf_len);
    if(ilop_server_data_down_buf==NULL)
    {
        LOG("ilop_server_data_down_buf aos_malloc failed.\r\n");
        return ret;
    }
    memset(ilop_server_data_down_buf,0,buf_len);
    if(ilop_mode==ILOP_MODE_ICA)
    {
        sprintf(ilop_server_data_down_buf,"+ILOPDATA=ICA,%s,%s\r\n\r\n",key,value_str);
    }
    else
    {
        sprintf(ilop_server_data_down_buf,"+ILOPDATA=THROUGH,%d,%s\r\n\r\n",strlen(value_str),value_str);
    }
    if(!mcu_ota_start_flag && !g_hfilop_config.tmod)//--20190108
        hfilop_uart_send_data(ilop_server_data_down_buf,strlen(ilop_server_data_down_buf));
    aos_free(ilop_server_data_down_buf);
    ret=0;
    return ret;
}

int ilop_connect_status_down(ILOP_CONNECT_STATUS connect_status)
{
    int ret =0;
    unsigned char buf[100];
    memset(buf,0,sizeof(buf));
    if(connect_status==WIFI_CONNECT)
    {
        strcpy(buf,"+ILOPCONNECT=WIFI_CONNECT\r\n\r\n");
    }
    else if(connect_status==WIFI_DISCONNECT)
    {
        strcpy(buf,"+ILOPCONNECT=WIFI_DISCONNECT\r\n\r\n");
    }
    else if(connect_status==SERVER_CONNECT)
    {
        strcpy(buf,"+ILOPCONNECT=SERVER_CONNECT\r\n\r\n");
    }
    else if(connect_status==SERVER_DISCONNECT)
    {
        strcpy(buf,"+ILOPCONNECT=SERVER_DISCONNECT\r\n\r\n");
    }
    else
    {
        ret=-1;
        return ret;
    }

    if(!mcu_ota_start_flag && !g_hfilop_config.tmod)//--20190108
        hfilop_uart_send_data(buf,strlen(buf));
    return ret;
}

void awss_config_sucess_event_down(void)
{
    unsigned char buf[100];
    memset(buf,0,sizeof(buf));
    strcpy(buf,"+ILOPCONNECT=AWSS_SUCCEED\r\n\r\n");

    if(!mcu_ota_start_flag && !g_hfilop_config.tmod)//--20190108
        hfilop_uart_send_data(buf,strlen(buf));
}

void get_val_from_CJ_localtimer_arr(cJSON *c_json,char * property,int *data, char* time)
{
	 cJSON *pSub,*pSub1 ;
	int iCount=0;
	cJSON *test_arr = cJSON_GetObjectItem(c_json,"LocalTimer");
	 iCount = cJSON_GetArraySize(test_arr);
	 HFILOP_PRINTF("localtimer array len=%d---\r\n",iCount);
	 cJSON *arr_item = test_arr->child;

	  pSub1 = cJSON_GetObjectItem(arr_item, property);
	
	  if(pSub1 != NULL)
	  {  
	  	  if(strcmp(property,"Timer")==0)
	  		{
				strcpy(time,pSub1->valuestring);
				HFILOP_PRINTF("property=%s  data=%s--\r\n",property,time);  
	 		}else
	 		{
	    	 	*data = pSub1->valueint;
				HFILOP_PRINTF("property=%s  pItem->valueint=%d--\r\n",property,data);  
	 		}
	  }
}
void localtimer(char *payload) //20190506
{
	char  local_buf[64] = { 0 };
	char  local_head[64]={0};
	char get_property[100]={0};
	 char value_str[20] ={0};
	char *first_sign=NULL;
	char *second_sign=NULL;
	int data;
	cJSON * data_JSON1 = cJSON_Parse(payload);
	first_sign=strstr(payload,"\"");
	if(first_sign!=NULL)
	{
		HFILOP_PRINTF("find first_sign------%s----\r\n",first_sign);
	}
	second_sign=strstr(first_sign+1,"\"");
	HFILOP_PRINTF("find second_sige------%s----\r\n",second_sign);
	strncpy(local_head,first_sign+1,second_sign-first_sign-1);
	HFILOP_PRINTF("get_property head is %s ------\r\n",local_head);
	char *off_loacl=strstr(payload,"[{");
	if(off_loacl==NULL)
	{
		return ;
	}
	HFILOP_PRINTF("off_loacl=%d-----------\r\n",off_loacl);
	char *off_end=strstr(payload,"},{");
	if(off_end==NULL)
	{
		return ;
	}
	HFILOP_PRINTF("off_end=%d-----------\r\n",off_end);
	while(off_loacl<=off_end)
	{
		first_sign=NULL;
		first_sign=strstr(off_loacl,"\"");
		if(first_sign !=NULL)
		{
			if(first_sign>off_end)
				{
					break;
				}
			HFILOP_PRINTF("find first_sign------%s----\r\n",first_sign);
			second_sign=NULL;
			off_loacl=first_sign+1;
			second_sign=strstr(first_sign+1,"\"");
			if(second_sign !=NULL)
			{
				
				HFILOP_PRINTF("find second_sige------%s----\r\n",second_sign);
				strncpy(get_property,first_sign+1,second_sign-first_sign-1);
				get_property[second_sign-first_sign-1]='\0';
				HFILOP_PRINTF("sg get_property=%s------\r\n",get_property);
				snprintf(local_buf, sizeof(local_buf), "%s.%s", local_head, get_property);
			//	get_complex_int_value(&data,data_JSON,local_head,get_property);
				
			/*	if(strcmp(get_property,"Timer")==0)
				{
					get_val_from_CJ_localtimer_arr(data_JSON1,get_property,NULL,value_str);
					HFILOP_PRINTF("value_str=%s------\r\n",value_str);
					ilop_server_data_down(ILOP_MODE_ICA,local_buf,value_str);
					char *sign=strstr(second_sign+1,"\"");
					if(sign !=NULL)
					{
						sign=strstr(sign+1,"\"");
						if(sign !=NULL)
						{
							off_loacl=sign+1;
						}
					}
				}else
				{
					get_val_from_CJ_localtimer_arr(data_JSON1,get_property,&data,NULL);
					HFILOP_PRINTF("get value_str=%d -------\r\n ",data);
					itoa(data,value_str,10);
					HFILOP_PRINTF("value_str=%s------\r\n",value_str);
					ilop_server_data_down(ILOP_MODE_ICA,local_buf,value_str);
					off_loacl=second_sign+3;
				}*/
				off_loacl=get_data_from_str(get_property,second_sign,local_head);
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
	
	cJSON_Delete(data_JSON1);

}

void get_data_cjson(cJSON *c_json,char *objectiem,char *quest)//20190514
{
//	char sg_quest[1024]={0};
//memcpy(sg_quest,quest+1,strlen(quest)-2);
//	HFILOP_PRINTF("sg_quest=%s--------\r\n",sg_quest);
//	cJSON * data_JSON = cJSON_Parse(sg_quest);
	char*out=cJSON_Print(c_json);
	HFILOP_PRINTF("c_json=%s---\r\n",out);
        cJSON *param = cJSON_GetObjectItem(c_json, objectiem);
//		HFILOP_PRINTF("param=%s---\r\n",param);
        if (NULL != param) {
            if(cJSON_IsObject(param)){/* param is json object */
                HFILOP_PRINTF( "Json object\r\n");
                char* value_str = cJSON_PrintUnformatted(param);
                if(NULL!=value_str){
				HFILOP_PRINTF("value_str=%s------\r\n",value_str);
				ilop_server_data_down(ILOP_MODE_ICA,(char *)objectiem,(char *)value_str);
                    cJSON_free(value_str);
                    value_str = NULL;
                }
            }else if(cJSON_IsString(param)){/* param is string */
            //    HFILOP_PRINTF "String %s", param->valuestring);
				HFILOP_PRINTF("value_str=%s------\r\n",param->valuestring);
				ilop_server_data_down(ILOP_MODE_ICA,(char *)objectiem,(char *)param->valuestring);
            }else{
				switch(param->type)
					{
						case 0:
							HFILOP_PRINTF("cJSON_Invalid------\r\n");break;
						case 0x01:
							HFILOP_PRINTF("cJSON_False------\r\n");break;
						case 0x02:
							HFILOP_PRINTF("cJSON_True------\r\n");break;
						case 0x04:
							HFILOP_PRINTF("cJSON_NULL------\r\n");break;
						case 0x08:
							HFILOP_PRINTF("cJSON_Number------\r\n");
							HFILOP_PRINTF("valuedouble=%f----int data=%d--\r\n",param->valuedouble,param->valueint);
							break;
						case 0x10:
							HFILOP_PRINTF("cJSON_String------\r\n");break;
						case 0x20:
							HFILOP_PRINTF("cJSON_Array------\r\n");break;
						case 0x40:
							HFILOP_PRINTF("cJSON_Object------\r\n");break;
						case 0x80:
							HFILOP_PRINTF("cJSON_Raw------\r\n");break;
					}

			
                HFILOP_PRINTF("Not support format param->type=%d--\r\n",param->type);
            }
        }
}


int get_one_data_from_str(char *property,char*quest)
{
	char value_str[100]={0};
	if(strncmp(quest+1,":",1)==0)
	{
		HFILOP_PRINTF("quest=%s-\r\n",quest);
		quest++;
	}else
	{
		HFILOP_PRINTF("is error quest=%s-\r\n",quest);
		return 0;
	}
	/****????¡ì??2???¡ì?|???¡ì?oy?Y???¡ì?o?????¨¬??¨¬??a???¡§???¡ì|?????¨¬??¨¬????****/
	char *	off1=strstr(quest+1,"\"");
	if(off1 !=NULL )
	{
		HFILOP_PRINTF("--------data is str----------\r\n");
		if(off1==quest+1)
		{
			char *	off2=strstr(off1+1,"\"");
			if(off2 !=NULL )
			{
				memcpy(value_str,off1+1,off2-off1-1);
				HFILOP_PRINTF("value_str=%s----\r\n",value_str);
				ilop_server_data_down(ILOP_MODE_ICA,(char *)property,(char *)value_str);
				return off2+1;
			}else
			{
				HFILOP_PRINTF("off2 is no found--\r\n");
				return 0;
			}
		}
		
	}
/***************????¡ì??2???¡ì?|?oo?********************************/
	char *	off_comma=strstr(quest+1,",");
	if(off_comma!=NULL)
		{
			memcpy(value_str,quest+1,off_comma-quest-1);
			HFILOP_PRINTF("value_str=%s----\r\n",value_str);
			ilop_server_data_down(ILOP_MODE_ICA,(char *)property,(char *)value_str);
			return off_comma+1;
		}

/***************????¡ì??2???¡ì?|?????¡ì??1???¡ì?2o?}********************************/
	char *	off_coun=strstr(quest+1,"}");
	if(off_coun!=NULL)
		{
			memcpy(value_str,quest+1,off_coun-quest-1);
			HFILOP_PRINTF("value_str=%s----\r\n",value_str);
			ilop_server_data_down(ILOP_MODE_ICA,(char *)property,(char *)value_str);
			return off_coun+1;
		}
	HFILOP_PRINTF("no found data--------\r\n");
	return 0;

}
int get_data_from_str(char *property,char*quest,char*property_head)
{
	char  property_buf[64] = { 0 };
	int len=0;
	snprintf(property_buf, sizeof(property_buf), "%s.%s", property_head, property);

	char value_str[100]={0};
	if(strncmp(quest+1,":",1)==0)
	{
		HFILOP_PRINTF("quest=%s-\r\n",quest);
		quest++;
	}else
	{
		HFILOP_PRINTF("is error quest=%s-\r\n",quest);
		return 0;
	}
	/****????¡ì??2???¡ì?|???¡ì?oy?Y???¡ì?o?????¨¬??¨¬??a???¡§???¡ì|?????¨¬??¨¬????****/
	char *	off1=strstr(quest+1,"\"");
	if(off1 !=NULL )
	{
		
		if(off1==quest+1)
		{
		HFILOP_PRINTF("--------data is str----------\r\n");
			char *	off2=strstr(off1+1,"\"");
			if(off2 !=NULL )
			{
				memcpy(value_str,off1+1,off2-off1-1);
				HFILOP_PRINTF("value_str=%s----\r\n",value_str);
				ilop_server_data_down(ILOP_MODE_ICA,(char *)property_buf,(char *)value_str);
				return off2+1;
			}else
			{
				HFILOP_PRINTF("off2 is no found--\r\n");
				return 0;
			}
		}
		
	}
/***************????¡ì??2???¡ì?|?oo?********************************/
	char *	off_comma=strstr(quest+1,",");
	if(off_comma!=NULL)
		{
			char *	off_cou=strstr(quest+1,"}");
			if(off_comma!=NULL)
			{
				if(off_cou<off_comma)// ???????¡§???¡ì?¡ì?|??¡§????????¨¬????¡ì?o???¡§???¡ì?¡ì???¡§?o????¡ì?oy???¡§???¡ì|???¡ì?|??????¡ì??
					{
						memcpy(value_str,quest+1,off_cou-quest-1);
						HFILOP_PRINTF("value_str=%s----\r\n",value_str);
						ilop_server_data_down(ILOP_MODE_ICA,(char *)property_buf,(char *)value_str);
						return off_comma+1;
					}
			}
			memcpy(value_str,quest+1,off_comma-quest-1);
			HFILOP_PRINTF("value_str=%s----\r\n",value_str);
			ilop_server_data_down(ILOP_MODE_ICA,(char *)property_buf,(char *)value_str);
			return off_comma+1;
		}

/***************????¡ì??2???¡ì?|?????¡ì??1???¡ì?2o?}********************************/
	char *	off_coun=strstr(quest+1,"}");
	if(off_coun!=NULL)
		{
			memcpy(value_str,quest+1,off_coun-quest-1);
			HFILOP_PRINTF("value_str=%s----\r\n",value_str);
			ilop_server_data_down(ILOP_MODE_ICA,(char *)property_buf,(char *)value_str);
			return off_coun;
		}
	HFILOP_PRINTF("no found data--------\r\n");
	return 0;

}
int hf_user_post_property(char* property,char *data)//20190514
{
	print_heap();
//	HFILOP_PRINTF("hf_user_post_property=%s data=%s\r\n",property,data);

    static int example_index = 0;
    int res = 0;
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    char property_payload[1024]={0};
        /* Normal Example */
     //   property_payload = "{\"LightSwitch\":1}";
        /* example_index++; */
   
		sprintf(property_payload,"{\"%s\":%s}",property,data);
		HFILOP_PRINTF("property_payload=%s-----\r\n",property_payload);

    res = IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_PROPERTY,
                             (unsigned char *)property_payload, strlen(property_payload));

    LOG("Post Property Message ID: %d", res);
		print_heap();
	return res;
}
int hf_user_post_many_property(int argc,char *argv[])//20190514
{
	print_heap();
	HFILOP_PRINTF("hf_user_post_many_property\r\n");

	static int example_index = 0;
	int res = 0;
	user_example_ctx_t *user_example_ctx = user_example_get_ctx();
	char property_payload[1024]={0};
		/* Normal Example */
	 //   property_payload = "{\"LightSwitch\":1}";
		/* example_index++; */
   
		sprintf(property_payload,"{");
		int i;
		for(i=1;i<argc-1;i+=2)
			{
				sprintf(property_payload,"%s\"%s\":%s,",property_payload,argv[i],argv[i+1]);
			}
		int len=strlen(property_payload);
		property_payload[len-1]=0;
		sprintf(property_payload,"%s}",property_payload);

	res = IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_PROPERTY,
							 (unsigned char *)property_payload, strlen(property_payload));

	LOG("Post Property Message ID: %d", res);
		print_heap();
	return res;

}
int hf_user_post_compound_property(int argc,char *argv[])//20190514
{
	print_heap();
	char *p_identifier_c=NULL;
	HFILOP_PRINTF("hf_user_post_compound_property\r\n");
	char Compound_identifier[100]={0};

	static int example_index = 0;
	int res = 0;
	user_example_ctx_t *user_example_ctx = user_example_get_ctx();
	char property_payload[1024]={0};
		/* Normal Example */
	 //   property_payload = "{\"LightSwitch\":1}";
		/* example_index++; */
   
		sprintf(property_payload,"{\"%s\":{",argv[1]);
		int i;
		for(i=2;i<argc-1;i+=2)
			{
				memset(Compound_identifier,0,sizeof(Compound_identifier));
				 p_identifier_c=strstr(argv[i],".");
					 if(p_identifier_c!=NULL)
					 {
					 	strcpy(Compound_identifier,p_identifier_c+1);
						
					 }else
					 {
					 strcpy(Compound_identifier,argv[i]);
					 }
				sprintf(property_payload,"%s\"%s\":%s,",property_payload,Compound_identifier,argv[i+1]);
			}
		int len=strlen(property_payload);
		property_payload[len-1]=0;
		sprintf(property_payload,"%s}}",property_payload);

	res = IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_PROPERTY,
							 (unsigned char *)property_payload, strlen(property_payload));

	LOG("Post Property Message ID: %d", res);
		print_heap();
	return res;

}
//20190527
int hf_cmd_send_js(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
    if(argc < 2)
    {
        return -1;
    }

	HFILOP_PRINTF("argc=%d----\r\n",argc);
	int res=0;
	 if(memcmp(argv[0],"property",strlen("property"))==0)
	 	{
	 		res=hf_user_post_json(argc,argv);
			if(res <0)
				{
					sprintf(rsp,"=send failed");
				}
			 return 0;
	 	}  else if(memcmp(argv[0],"event",strlen("event"))==0)
	 		{
	 			res=hf_user_post_json_evevt(argc,argv);
				if(res <0)
				{
					sprintf(rsp,"=send failed");
				}
			 return 0;
	 		}
		 return -3;
}
int hf_user_post_json(int argc,char *argv[])
{
	print_heap();

    static int example_index = 0;
    int res = 0;
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    char property_payload[1024] = {0};
	HFILOP_PRINTF("hf_user_post_json=---------\r\n");
	int i=0;
	for(i=1;i<argc;i++)
	{
		sprintf(property_payload,"%s%s,",property_payload,argv[i]);
	}
	int len=strlen(property_payload);
		property_payload[len-1]=0;
	//	sprintf(property_payload,"%s}",property_payload);
 	 HFILOP_PRINTF("property_payload=%s\r\n",property_payload);
    res = IOT_Linkkit_Report(user_example_ctx->master_devid, ITM_MSG_POST_PROPERTY,
                             (unsigned char *)property_payload, strlen(property_payload));

    LOG("Post Property Message ID: %d", res);
	print_heap();
	return res;
}
void hf_user_post_json_evevt(int argc,char *argv[])
{
	print_heap();

    static int example_index = 0;
    int res = 0;
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();
    char event_payload[1024] = {0};
	char event_id[50]={0};
	strcpy(event_id,argv[1]);
	HFILOP_PRINTF("event_id=%s---\r\n",event_id);

 		int i=0;
	for(i=2;i<argc;i++)
	{
		sprintf(event_payload,"%s%s,",event_payload,argv[i]);
	}
	int len=strlen(event_payload);
		event_payload[len-1]=0;
  HFILOP_PRINTF("event_payload=%s\r\n",event_payload);
    res =  res = IOT_Linkkit_TriggerEvent(user_example_ctx->master_devid, event_id, strlen(event_id),
                                   event_payload, strlen(event_payload));

   LOG("Post Event Message ID: %d", res);
   print_heap();
	return res;
}

int  ilop_server_jsdata_down(char * serid,int id_len,char *data)
{
    int ret=-1;
    if((data==NULL))
        return ret;
    
    int buf_len=strlen(data)+100;
    char *ilop_server_jsdata_down_buf=aos_malloc(buf_len);
    if(ilop_server_jsdata_down_buf==NULL)
    {
        LOG("ilop_server_jsdata_down_buf aos_malloc failed.\r\n");
        return ret;
    }
    memset(ilop_server_jsdata_down_buf,0,buf_len);
	if(serid==NULL)
	{
		 sprintf(ilop_server_jsdata_down_buf,"+ILOPDATA=JSON,%s\r\n\r\n",data);
	}else
		{
     sprintf(ilop_server_jsdata_down_buf,"+ILOPDATA=JSON,%.*s,%s\r\n\r\n",id_len,serid,data);
		}
    if(!mcu_ota_start_flag && !g_hfilop_config.tmod)//--20190108
        hfilop_uart_send_data(ilop_server_jsdata_down_buf,strlen(ilop_server_jsdata_down_buf));
    aos_free(ilop_server_jsdata_down_buf);
    ret=0;
    return ret;
}


void hfsys_start_network_status_process()
{
	if(g_hfilop_config.ota_mode == 0)
    {		
        aos_task_new("network_status", network_connection_status_check_process, NULL, 1024);
	}
}


const hfat_cmd_t hfilop_at_cmds_table[]=
{
	{"ENTM",hf_atcmd_entm,"   AT+ENTM: Goto Through MOde.\r\n",NULL},
	{"EVENT",hf_atcmd_event,"   AT+EVENT: 0:General firmware mode 1:Goto through mode.\r\n",NULL},
	{"UART",hf_atcmd_uart,"   AT+UART: Set/Get the UART Parameters.\r\n",NULL},
	{"MID",hf_atcmd_mid,"   AT+MID: Get The Module ID.\r\n",NULL},
	{"MTYPE",hf_atcmd_mtype,"   AT+MTYPE: Get The Module type.\r\n",NULL},
	{"WRMID",hf_atcmd_wrmid,"   AT+WRMID: Write Module ID.\r\n",NULL},
	{"WSSSID", hf_atcmd_wsssid, "   AT+WSSSID: Set/Get the AP's SSID of WIFI STA Mode.\r\n", NULL},
	{"WSKEY", hf_atcmd_wskey, "   AT+WSKEY: Set/Get the Security Parameters of WIFI STA Mode.\r\n", NULL},
	{"WSLK",hf_atcmd_wslk,"   AT+WSLK: Get Link Status of the Module (Only for STA Mode).\r\n"},
	{"WSLQ", hf_atcmd_wslq,"   AT+WSLQ: Get Link Quality of the Module (Only for STA Mode).\r\n",NULL},
	{"WSCAN",hf_atcmd_wscan,"   AT+WSCAN: Get The AP site Survey (only for STA Mode).\r\n",NULL},
	{"WANN",hf_atcmd_wann,"   AT+WANN: Set/Get The WAN setting if in STA mode.\r\n",NULL},
	{"WSMAC", hf_atcmd_wsmac,"   AT+WSMAC: Set/Get Module MAC Address.\r\n",NULL},
	//{"PWM", hf_atcmd_pwm,"   AT+PWM: Set pwm.\r\n",NULL},
	{"VER",hf_atcmd_ver,"   AT+VER: Get application version.\r\n",NULL},
	{"OTA",hf_atcmd_ota,"   AT+OTA:OTA\r\n",NULL},
	{"RELD",hf_atcmd_reld,"   AT+RELD: Reload the default setting and reboot.\r\n",NULL},
	{"CLR",hf_atcmd_clr,"   AT+CLR: Clear ali information.\r\n",NULL},
	{"Z",hf_atcmd_reset,"   AT+Z: Reset the Module.\r\n",NULL},
	{"TESTPIN",hf_atcmd_testpin,"\r\n   AT+TESTPIN: Test Pin.\r\n",NULL},
	//{"PRODUCT",hf_atcmd_product,"   AT+PRODUCT: Set/Get product key&secret.\r\n",NULL},
	{"DEVICE",hf_atcmd_device,"   AT+DEVICE: Set/Get device name&secret.\r\n",NULL},
	{"WIFICONN",hf_atcmd_wificonn,"   AT+WIFICONN: Set connect WiFi parameters.\r\n",NULL},
	{"PRODEVICE",hf_atcmd_prodevice,"   AT+PRODEVICE: SET/GET product key&secret and device name&secret.\r\n", NULL},
	{"PROSECRET",hf_atcmd_prosecret,"   AT+PROPROSECRET: SET/GET product secret.\r\n",NULL},
	{"ALISERVER",hf_atcmd_aliserver,"   AT+ALISERVER: Set/Get server address.\r\n",NULL},
	{"ALIKEY",hf_atcmd_alikey,"   AT+ALIKEY: Set/Get key register type.\r\n",NULL},
	{"NDBGL",hf_cmd_debug_level,"   AT+NDBGL: set/get debug_level\r\n", NULL},
    //{"AWSSACTIVE",hf_cmd_awss_active,"   AT+AWSSACTIVE: awss active\r\n", NULL},
    {"AWSSRESET",hf_cmd_awss_reset,"   AT+AWSSRESET: awss reset\r\n", NULL},
    //{"AWSSBLE",hf_cmd_awss_smtlkble,"   AT+AWSSBLE: ble config\r\n", NULL},
    {"SENDICA",hf_cmd_send_ica,"   AT+SENDICA: ica mode send data to server\r\n", NULL},
    {"SENDJS",hf_cmd_send_js,"   AT+SENDJS: JS mode send data to server\r\n", NULL},//20190527
    {"SENDRAW",hf_cmd_send_raw,"   AT+SENDRAW: through mode send data to server\r\n", NULL},
    {"SENDMODE",hf_cmd_send_mode,"   AT+SENDMODE: set/get send data mode\r\n", NULL},
    {"ILOPCONNECT",hf_cmd_ilop_connect_status,"   AT+ILOPCONNECT: get ilop connect status\r\n", NULL},
    {"E",hf_cmd_echo,"   AT+E: Set echo mode\r\n", NULL},
    {"NTIME",hf_atcmd_time,"\r\n   AT+NTIME:set/get system time\r\n",NULL},
    {"NTIMETZ",hf_atcmd_timezone,"\r\n   AT+NTIMETZ:set/get time zone\r\n",NULL},
    {"MCUOTA",hf_cmd_mcu_ota,"   AT+MCUOTA: mcu ota function\r\n", NULL},
    {"HFTEST",hf_atcmd_hftest,"   AT+HFTEST:set product test .\r\n",NULL},
    {"AWSSMODE",hf_atcmd_awssmode,"   AT+AWSSMODE:set AWSS mode .\r\n",NULL},
    {"PROID",hf_atcmd_proid,"   AT+PROID:set ble id .\r\n",NULL},
    {"BLESCANSTART",hf_atcmd_blescanstart,"   AT+BLESCANSTART:start ble scan .\r\n",NULL},
    {"BLESCANSTOP",hf_atcmd_blescanstop,"   AT+BLESCANSTOP:stop ble scan .\r\n",NULL},
    {"BLESTARTADV",hf_atcmd_blestartadv,"   AT+BLESCANSTOP:stop ble scan .\r\n",NULL},
    {"BLESTOPADV",hf_atcmd_blestopadv,"   AT+BLESCANSTOP:stop ble scan .\r\n",NULL},
    {"PROALL",hf_atcmd_proall,"   AT+PROALL: get five group.\r\n",NULL},
	{NULL,NULL,NULL,NULL} //the last item must be null
};

