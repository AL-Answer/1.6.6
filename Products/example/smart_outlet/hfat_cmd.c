
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
#include "hfilop/hfilop.h"


#include "hfat_cmd.h"
#include <hal/soc/flash.h>
#include <time.h>
#include "ymodem.h"
#include "mcu_ota.h"
#include "board.h"
#include "app_entry.h"
#include "hfilop/hfilop_config.h"

#define get_ntp_time_timeout		1000 // unit: ms
extern void do_awss_ble_start(void);

static int hf_cmd_get_version(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
	if(argc==0)
	{ 
		sprintf(rsp,"=%s%s",APPVERSION,HFILOP_VERSION);
		return 0;
	}
	return -3;
}

const hfat_cmd_t user_define_at_cmds_table[]=
{
	{"APPVER",hf_cmd_get_version,"   AT+APPVER: get version\r\n", NULL},
	{NULL,NULL,NULL,NULL}
};

