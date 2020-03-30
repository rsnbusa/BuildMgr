#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern int sendReplyToHost(int cualm,int response,int son,char* cmdI, ...);
extern void write_to_flash();

int cmd_zerokeys(parg *argument)
{
	int response;

	cJSON *req=cJSON_GetObjectItem((cJSON*)argument->pMessage,"REQ");
	if(req)
		response=req->valueint;
	else
		response=0;
	bzero(theConf.lkey,sizeof(theConf.lkey));
	write_to_flash();

	sendReplyToHost(0,response,1,(char*)"ZeroKeys","");
	return ESP_OK;
}
