#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern int sendReplyToHost(int cualm,int response,int son,char* cmdI, ...);
extern int findMID(char *mid);
extern void pprintf(const char * format, ...);

int cmd_sendkwh(parg *argument)
{
	uint32_t kwh;
	char numa[10];
	int cualm,response;

	cJSON *req=cJSON_GetObjectItem((cJSON*)argument->pMessage,"REQ");
	if(req)
		response=req->valueint;
	else
		response=0;

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
	fram.read_lifekwh(cualm,(uint8_t*)&kwh);
	itoa(kwh,numa,10);
	sendReplyToHost(cualm,response,2,(char*)"MID",mid->valuestring,(char*)"KwH",numa);
	return ESP_OK;
}
