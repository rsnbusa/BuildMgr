#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern int sendReplyToHost(const int cualm,const cJSON* cj,const int son,const char* cmdI, ...);
extern int findMID(const char *mid);

int cmd_sendday(parg *argument)
{
	uint16_t theDay;
	int cualf=0;
	char numa[10],numb[10];


	cJSON *mid=cJSON_GetObjectItem((cJSON*)argument->pMessage,"MID");
	if(mid)
	{
		cualf=findMID(mid->valuestring);
		if(cualf<0)
		{
			printf("MID %s not found\n",mid->valuestring);
			return ESP_FAIL;
		}
	}

	cJSON *param=cJSON_GetObjectItem((cJSON*)argument->pMessage,"DAY");
	if(param)
		fram.read_day(cualf,(uint16_t)param->valueint,(uint8_t*)&theDay);
	else
		return ESP_FAIL;

	itoa(param->valueint,numa,10);
	itoa(theDay,numb,10);
	sendReplyToHost(cualf,(cJSON*)argument->pMessage,2,(char*)"Day",numa,"KwH",numb);

	return ESP_OK;
}
