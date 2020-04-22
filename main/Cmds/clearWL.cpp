#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern int sendReplyToHost(const int cualm,const cJSON* cj,const int son,const char* cmdI, ...);
extern void write_to_flash();

int cmd_clearwl(parg *argument)
{
	int response;

	char numa[10],numb[10];

	cJSON *posi=cJSON_GetObjectItem((cJSON*)argument->pMessage,"POS");
	if(!posi)
		return ESP_FAIL;

	int pos=posi->valueint;
	if (pos<0) // all
	{
		bzero(&theConf.reservedMacs,sizeof(theConf.reservedMacs));
		itoa(theConf.reservedCnt,numa,10);
		theConf.reservedCnt=0;
		sendReplyToHost(0,(cJSON*)argument->pMessage,2,(char*)"ERASE","","CNT",numa);
	}
	else
	{
		if(pos>theConf.reservedCnt-1)
		{
			itoa(pos,numa,10);
			itoa(theConf.reservedCnt-1,numb,10);
			sendReplyToHost(0,(cJSON*)argument->pMessage,2,(char*)"OB",numa,(char*)"MAX",numb);
			return ESP_OK;
		}

		int son=(MAXSTA-(pos+1))*sizeof(uint32_t);
		if(pos<MAXSTA-1)
			memmove(&theConf.reservedMacs[pos],&theConf.reservedMacs[pos+1],son);
		memset(&theConf.reservedMacs[MAXSTA-1],0,sizeof(theConf.reservedMacs[0]));

		theConf.reservedCnt--;
		itoa(pos,numa,10);
		itoa(theConf.reservedCnt,numb,10);
		sendReplyToHost(0,(cJSON*)argument->pMessage,2,(char*)"DEL",numa,(char*)"MAX",numb);
	}

	write_to_flash();

	return ESP_OK;
}
