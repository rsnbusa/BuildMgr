#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern int sendReplyToHost(int cualm,int response,int son,char* cmdI, ...);
extern int findMID(char *mid);
extern void pprintf(const char * format, ...);

int cmd_sendmy(parg *argument)
{
	uint16_t theMonth[12];
	char temp[10];
	string todos="";
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

	for(int a=0;a<12;a++)
	{
		fram.read_month(cualm,a,(uint8_t*)&theMonth[a]);
	//	printf("MY[%d]=%d ",a,theMonth[a]);
		sprintf(temp,"%d|",theMonth[a]);
		todos+=string(temp);
	}
//	printf("\n Temp %s\n",todos.c_str());

	sendReplyToHost(cualm,response,1,(char*)"MonthsKWH",(char*)todos.c_str());
	return ESP_OK;
}
