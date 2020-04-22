#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern int sendReplyToHost(const int cualm,const cJSON* cj,const int son,const char* cmdI, ...);
extern int findMID(const char *mid);
extern void pprintf(const char * format, ...);

int cmd_sendmonth(parg *argument)
{
	uint16_t theDay;
	char numa[10],numb[10];
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


	cJSON *param=cJSON_GetObjectItem((cJSON*)argument->pMessage,"MONTH");
	if(param)
	{
		fram.read_month(cualm,(uint16_t)param->valueint,(uint8_t*)&theDay);
	//	printf("Day[%d]=%d\n",param->valueint,theDay);
	}
	else
		return -1;

	itoa(param->valueint,numa,10);
	itoa(theDay,numb,10);
	sendReplyToHost(cualm,(cJSON*)argument->pMessage,2,(char*)"Month",numa,"KwH",numb);

	return ESP_OK;
}
