
#ifndef HFILOP_CONFIG_H
#define HFILOP_CONFIG_H

#include <stdlib.h>
#include <stdarg.h>


#define HF_G_CONFIG_ADDR 0x1F9000
#define HF_G_CONFIG_BACKUP_ADDR 0x1FA000
#pragma pack(push)
#pragma pack(1)

#define MAGIC_HEAD 0x5A5AA5A5

typedef struct _TIME_ZONE
{
    unsigned char time_zone_flag;
    char time_zone_value;
}TIME_ZONE;

typedef struct
{
	unsigned int magic_head;
	unsigned char mac[6];
	unsigned char key_valid[10];
	char mid[21];
	
	char product_key[65];
	char product_secret[65];
	char device_name[65];
	char device_secret[65];

	unsigned char rf_type;
	unsigned int uart_baud;
	unsigned int uart_databit;
	unsigned int uart_parity;
	unsigned int uart_stopbit;
	unsigned int uart_fc;
	unsigned char ota_mode;
	unsigned int server;
	unsigned char key_type;

	unsigned int user_ilop_config_init_flag;
    unsigned char ilop_mode;
    char last_connect_ssid[32+1];
    char last_connect_key[64+1];
    unsigned char echo_mode;
	unsigned char awss_mode;
    TIME_ZONE time_zone;//--20181212
    char factory_test_ssid[32+1];//--20190108
    char factory_test_key[64+1];//--20190108
    int product_id;
    char tmod;
    unsigned char awss_flag;
	
	unsigned char res[83];
	unsigned char crc;
} hfilop_cfg_t;

#pragma pack(pop)


enum ENWORKTMODE
{
	M2M_STATE_RUN_THROUGH = 0x00,
	M2M_STATE_RUN_CMD
};

enum CONFIG_EVENT
{
	CONFIG_EVENT_ON	= 0x00,
	CONFIG_EVENT_OFF
};

extern hfilop_cfg_t g_hfilop_config;

typedef int (*hfble_scan_callback_t)(PBLE_SCAN_RESULT_ITEM);

typedef struct
{
	uint8_t addr_type;
	uint8_t addr[6];
	int8_t rssi;
	uint8_t evtype;
	uint16_t len;
	uint8_t *data;

}BLE_SCAN_RESULT_ITEM,*PBLE_SCAN_RESULT_ITEM;


#define M2M_MAXUARTRECV 1024


/**
 * @brief init ILOP config with module type.
 *
 * @param[in] none
 * @return[out] 0-successfully, other value is product key&secret invalid
 * @see None.
 * @note None.
 */
int hfilop_config_init(void);

/**
 * @brief save ILOP to flash.
 *
 * @param[in] none
 * @return[out] none
 * @see None.
 * @note None.
 */
void hfilop_config_save(void);

/**
 * @brief default ILOP config.
 *
 * @param[in] none
 * @return[out] none
 * @see None.
 * @note None.
 */
void hfilop_config_default(void);

int flash_enable(int enable);

int flash_check(void);

#define PRODUCT_PARA_ADDR					0xD000


typedef struct _product_cmd
{
	const char * name;
	void	(*callhook)(int, char *);
} hfproduct_cmd_t,*phfproduct_cmd_t;


#endif /* HFILOP_CONFIG_H */

