#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern int sendReplyToHost(const int cualm,const cJSON* cj,const int son,const char* cmdI, ...);
extern int findMID(const char *mid);
extern void pprintf(const char * format, ...);

int cmd_senddm(parg *argument)
{
	int 	desde=0,hasta,len;
	char	temp[10];
	int 	cualm;

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
			char *todos=(char*)malloc(500);
			bzero(todos,500);
			char *ctodos=todos;

			if(todos==NULL)
				return ESP_FAIL;
			for (int a=0;a<param->valueint;a++)
				desde+=daysInMonth[a];
			hasta=desde+daysInMonth[param->valueint];
			for(int a=desde;a<hasta;a++)
			{
				fram.read_day(cualm,a,(uint8_t*)&theMonth);
				sprintf(temp,"%d|",theMonth);
				len=strlen(temp);
				memcpy(todos,temp,len);
				todos+=len;
			}

		sendReplyToHost(cualm,(cJSON*)argument->pMessage,1,(char*)"DM",ctodos);
		FREEANDNULL(ctodos)
		return ESP_OK;
	}
	else
		return ESP_FAIL;
}
