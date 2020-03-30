#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern int sendReplyToHost(int cualm,int response,int son,char* cmdI, ...);
extern int findMID(char *mid);
extern void pprintf(const char * format, ...);

int cmd_senddy(parg *argument)
{
	uint16_t theDay[366];
	string todos="";
	char temp[10];
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

	for(int a=0;a<366;a++)
	{
		fram.read_day(cualm,a,(uint8_t*)&theDay[a]);
	//	printf("DY[%d]=%d ",a,theDay[a]);
		sprintf(temp,"%d|",theDay[a]);
		todos+=string(temp);
	}

	sendReplyToHost(cualm,response,1,(char*)"DY",(char*)todos.c_str());
	return ESP_OK;
}
