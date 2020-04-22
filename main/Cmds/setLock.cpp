#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern int sendReplyToHost(const int cualm,const cJSON* cj,const int son,const char* cmdI, ...);
extern void write_to_flash();

int cmd_setLock(parg *argument)
{
	int response;

	char numa[10];

	cJSON *posi=cJSON_GetObjectItem((cJSON*)argument->pMessage,"LOCK");
	if(!posi)
		return ESP_FAIL;

	theConf.closedForRSVP=posi->valueint?1:0;
	sendReplyToHost(0,(cJSON*)argument->pMessage,1,(char*)"SLICES",theConf.closedForRSVP?"LOCK":"UNLOCK");


	write_to_flash();

	return ESP_OK;
}
