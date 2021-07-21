#ifndef __HFBLE_H__
#define __HFBLE_H__

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#define HF_SUCCESS 0
#define BLE_SEND_MTU_MAX_SIZE (247)

#define GAP_MAX_ADV_DATA_LEN              31u

typedef struct
{
	/*GAP Advertisement Parameters which includes Flags, Service UUIDs and short name*/
	uint8_t      advData[GAP_MAX_ADV_DATA_LEN]; 
	/*length of the advertising data. This should be made zero if there is no data */
	uint8_t      advDataLen;
	/*GAP Advertisement Parameters which includes Flags, Service UUIDs and short name*/
	uint8_t      advrspData[GAP_MAX_ADV_DATA_LEN]; 
	/*length of the advertising data. This should be made zero if there is no data */
	uint8_t      advrspDataLen;	
	
} GAPP_DISC_DATA_T;






































#endif



