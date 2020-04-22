#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern int sendReplyToHost(const int cualm,const cJSON* cj,const int son,const char* cmdI, ...);
extern int findMID(const char *mid);
extern void pprintf(const char * format, ...);

int cmd_sendbeats(parg *argument)
{
	uint32_t beats;
	char numa[10];
	int cualm;

	cJSON *mid=cJSON_GetObjectItem((cJSON*)argument->pMessage,"MID");
	if(!mid)
	{
		pprintf("No MID given\n");
		return ESP_FAIL;
	}

	cualm=findMID(mid->valuestring);
	if(cualm<0)
	{
		pprintf("Invalid MID given %s\n",mid->valuestring);
		return ESP_FAIL;
	}

	fram.read_beat(cualm,(uint8_t*)&beats);

	itoa(beats,numa,10);
	sendReplyToHost(cualm,(cJSON*)argument->pMessage,1,(char*)"Beats",numa);
	return ESP_OK;
}
