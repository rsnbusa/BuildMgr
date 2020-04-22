#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern int sendReplyToHost(const int cualm,const cJSON* cj,const int son,const char* cmdI, ...);
extern void write_to_flash();

int cmd_setSlice(parg *argument)
{
	int response;

	char numa[10];

	cJSON *posi=cJSON_GetObjectItem((cJSON*)argument->pMessage,"NUM");
	if(!posi)
		return ESP_FAIL;

	theConf.numSlices=posi->valueint;
	itoa(theConf.numSlices,numa,10);
	sendReplyToHost(0,(cJSON*)argument->pMessage,1,(char*)"SLICES",numa);

	write_to_flash();

	return ESP_OK;
}
