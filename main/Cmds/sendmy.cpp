#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern int sendReplyToHost(const int cualm,const cJSON* cj,const int son,const char* cmdI, ...);
extern int findMID(const char *mid);
extern void pprintf(const char * format, ...);

int cmd_sendmy(parg *argument)
{
	uint16_t theMonth[12];
	char temp[10];
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

	char *todos=(char*)malloc(100);
	bzero(todos,100);
	char *ctodos=todos;
	int len;

	for(int a=0;a<12;a++)
	{
		fram.read_month(cualm,a,(uint8_t*)&theMonth[a]);
		sprintf(temp,"%d|",theMonth[a]);
		strcpy(todos,temp);
		len=strlen(temp);
		todos+=len;
	}

	sendReplyToHost(cualm,(cJSON*)argument->pMessage,1,(char*)"MonthsKWH",ctodos);
	FREEANDNULL(ctodos)
	return ESP_OK;
}
