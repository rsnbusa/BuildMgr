#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern void write_to_flash();

int cmd_setdisplay(parg *argument)
{
	cJSON *param=cJSON_GetObjectItem((cJSON*)argument->pMessage,"MODE");
	if(param)
	{
		theConf.displayMode=param->valueint;
		write_to_flash();
	}
	else
		return -1;

	return ESP_OK;
}
