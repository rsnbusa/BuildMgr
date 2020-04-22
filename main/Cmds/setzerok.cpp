#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern int sendReplyToHost(const int cualm,const cJSON* cj,const int son,const char* cmdI, ...);
extern void write_to_flash();

int cmd_zerokeys(parg *argument)
{
	bzero(theConf.lkey,sizeof(theConf.lkey));
	write_to_flash();

	sendReplyToHost(0,(cJSON*)argument->pMessage,1,(char*)"ZeroKeys","");
	return ESP_OK;
}
