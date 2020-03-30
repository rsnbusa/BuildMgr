#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern int sendReplyToHost(int cualm,int response,int son,char* cmdI, ...);
extern int findMID(char *mid);
extern void pprintf(const char * format, ...);

int cmd_senddm(parg *argument)
{
	int desde=0,hasta;
	string todos="";
	char	temp[10];
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

	cJSON *param=cJSON_GetObjectItem((cJSON*)argument->pMessage,"MONTH");
	if(param)
	{//need to calculate first and last day
		uint16_t theMonth;

			for (int a=0;a<param->valueint;a++)
				desde+=daysInMonth[a];
			hasta=desde+daysInMonth[param->valueint];
			for(int a=desde;a<hasta;a++)
			{
				fram.read_day(cualm,a,(uint8_t*)&theMonth);
			//	printf("DM[%d]=%d ",a,theMonth);
				sprintf(temp,"%d|",theMonth);
				todos+=string(temp);
			}
	//		printf("\n");

		sendReplyToHost(cualm,response,1,(char*)"DM",todos.c_str());
		return ESP_OK;
	}
	else
		return ESP_FAIL;
}
