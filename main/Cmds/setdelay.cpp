#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern void write_to_flash();

int cmd_setdelay(parg *argument)
{
	cJSON *param=cJSON_GetObjectItem((cJSON*)argument->pMessage,"DELAY");
	if(param)
	{
		theConf.msgTimeOut=param->valueint;
		write_to_flash();
	}
	else
		return -1;

	return ESP_OK;
}
