#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <hal/soc/gpio.h>
#include <aos/aos.h>

#include "hfilop/hfilop.h"
#include "hfilop/hfilop_config.h"

gpio_dev_t GPIO_key;
gpio_dev_t GPIO_led;
gpio_dev_t GPIO_relay;
extern int hf_connect_cloud;
#define SMTLK_CONFIG 250
extern int32_t app_post_property_powerstate(uint8_t value);
void set_led_off(void)
{
	hal_gpio_output_high(&GPIO_relay);
}

void set_led_on(void)
{
	hal_gpio_output_low(&GPIO_relay);
}
void set_rely_switch(uint8_t value)
{
	if(value)
		hal_gpio_output_low(&GPIO_relay);
}

int hf_gpio_state=0;
void set_relay_fun(int value)
{
	if(value == 1)
	{
		set_led_on();
		hf_gpio_state=1;
		//hal_gpio_output_high(&GPIO_relay);
	}
	else
	{
		set_led_off();
		hf_gpio_state=0;
		//hal_gpio_output_low(&GPIO_relay);
	}
}

void delixi_gpio_init(void)
{
	GPIO_key.port=3;
	GPIO_led.port=5;
	GPIO_relay.port=4;
	
	GPIO_key.config=INPUT_PULL_UP;
	GPIO_led.config=OUTPUT_PUSH_PULL;
	GPIO_relay.config=OUTPUT_PUSH_PULL;
	
	hal_gpio_init(&GPIO_key);
	hal_gpio_init(&GPIO_relay);
	hal_gpio_init(&GPIO_led);
	set_led_off();
}
static void key_timer_thread(void *p)
{
	uint32_t val_12=0;
    int key_high_count=0;
	int key_low_count=0;
	while(1)
	{
		hal_gpio_input_get(&GPIO_key,&val_12);
		if(val_12 == 0)
		{
			key_high_count++;
			if(key_high_count >= SMTLK_CONFIG)
			{
				key_high_count=0;
				//printf("enter config net\r\n");
				//living_platform_awss_reset();
			}
		}
		else 
		{
			key_low_count++;
			if(key_low_count>=2 && key_high_count > 2)
			{
				if(hf_gpio_state)
				{
					hal_gpio_output_high(&GPIO_relay);
					//hal_gpio_output_high(&GPIO_led);
					hf_gpio_state=0;
					if(hf_connect_cloud == 1)
						app_post_property_powerstate(0);
				}
				else
				{
					hal_gpio_output_low(&GPIO_relay);
					//hal_gpio_output_low(&GPIO_led);
					hf_gpio_state=1;
					if(hf_connect_cloud == 1)
						app_post_property_powerstate(1);
				}
				key_low_count=0;
			}
			if(key_low_count > 100000)
				key_low_count=0;
			key_high_count=0;
		}
		
		aos_msleep(15);
	}
}

extern char hf_awss_start_flag;
#define HF_AWSS_TIMEOUT 3*60*10
static void socket_thread_fun(void *p)
{
	int reset_config_count=0;
	int val_0=0;
	unsigned int awss_timoout=0;
	while(1)
	{
		if(hf_awss_start_flag == 1)
		{
			if(g_hfilop_config.awss_mode != ILOP_AWSS_DEV_BLE_MODE)
			{
				awss_timoout++;
				if(awss_timoout >= HF_AWSS_TIMEOUT)
				{
					//uint32_t gpio_led_value=0;
					//uint32_t gpio_realy_value=0;
					hf_awss_start_flag=0;
					hal_gpio_output_high(&GPIO_led);
					extern void stop_netmgr(void *p);
					aos_task_new("netmgr_stop", stop_netmgr, NULL, 4096);
					break;
				}
			}
			reset_config_count++;
			if(0 <= reset_config_count && reset_config_count < 10)
				hal_gpio_output_high(&GPIO_led);
			else if(reset_config_count >= 10 && reset_config_count < 20)
				hal_gpio_output_low(&GPIO_led);
			else
				reset_config_count=0;
		}
		else if(hf_awss_start_flag == 2)
		{
			hf_awss_start_flag=0;
			hal_gpio_output_low(&GPIO_led);
		}
		aos_msleep(100);
	}
}

void start_delixi_socket_thread(void)
{	
	aos_task_new("key_timer",key_timer_thread,NULL,2048);
	aos_task_new("socket_thread",socket_thread_fun,NULL,1024);
}

