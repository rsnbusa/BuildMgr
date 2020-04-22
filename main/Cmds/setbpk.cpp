#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern int sendReplyToHost(const int cualm,const cJSON* cj,const int son,const char* cmdI, ...);
extern void write_to_flash();
extern int findMID(const char *mid);
extern void pprintf(const char * format, ...);

int cmd_setbpk(parg *argument)
{
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

	cJSON *bpk=cJSON_GetObjectItem((cJSON*)argument->pMessage,"BPK");
	if(bpk)
	{//need to calculate first and last day
		cJSON *born=cJSON_GetObjectItem((cJSON*)argument->pMessage,"BORN");
		if(born)
		{
			theConf.beatsPerKw[cualm]=bpk->valueint;	// beat per KW. MAGIC number.
			theConf.bornKwh[cualm]=born->valueint;		// Initial KW in meter
			time(&theConf.bornDate[cualm]);				// save current date a birthday
			write_to_flash();

			//update FRAM values
			fram.formatMeter(cualm);
			fram.write_lifekwh(cualm,born->valueint);

			theMeters[cualm].curLife=born->valueint;	//for display

			theMeters[cualm].beatsPerkW=theMeters[cualm].curMonth=theMeters[cualm].curMonthRaw=theMeters[cualm].curDay=theMeters[cualm].curDayRaw=0;
			theMeters[cualm].curCycle=theMeters[cualm].currentBeat=theMeters[cualm].beatSave;
			theMeters[cualm].curHour=theMeters[cualm].curHourRaw=0;

			// send confirmation of cmd received

			itoa(bpk->valueint,numa,10);
			itoa(born->valueint,numb,10);
			sendReplyToHost(cualm,(cJSON*)argument->pMessage,2,(char*)"BPK",numa,"BORN",numb);
			return ESP_OK;
		}
		else
			return ESP_FAIL;
	}
	else
		return ESP_FAIL;
}
