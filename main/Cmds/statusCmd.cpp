/*
 * statusCmd.cpp
 *
 *  Created on: Jan 26, 2020
 *      Author: rsn
 */


#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern uint32_t millis();

 void monitorCallback( TimerHandle_t xTimer )
 {
	 u32 cualf = ( uint32_t ) pvTimerGetTimerID( xTimer );
	 printf("Timer for Meter %d triggered\n",cualf); //should report to ControlQueue advicing posible problem with this MeterId

 }


void statusCmd(parg *argument)
{
	loginT loginData;
	bool answer=false;

		cJSON *lpos= 	cJSON_GetObjectItem((cJSON*)argument->pMessage,"Pos");
		cJSON *rep= 	cJSON_GetObjectItem((cJSON*)argument->pMessage,"reply");
		cJSON *kwh= 	cJSON_GetObjectItem((cJSON*)argument->pMessage,"KwH");
		cJSON *mid= 	cJSON_GetObjectItem((cJSON*)argument->pMessage,"mid");
		cJSON *beats= 	cJSON_GetObjectItem((cJSON*)argument->pMessage,"beats");

		if(rep)
			answer=rep->valueint;

		if((argument->macn!=0) && lpos)
		{
			int pos=lpos->valueint;						// this is the Meter# from the MtM

			losMacs[argument->pos].dState=MSGSTATE;
			losMacs[argument->pos].stateChangeTS[MSGSTATE]=millis();

			tallies[argument->pos][pos]++;
			if(mid)
				strcpy(losMacs[argument->pos].meterSerial[pos],mid->valuestring);
			if(kwh)
				losMacs[argument->pos].controlLastKwH[pos]=kwh->valueint;
			losMacs[argument->pos].trans[pos]++;
			if(beats)
				losMacs[argument->pos].controlLastBeats[pos]=beats->valueint;


			if(!losMacs[argument->pos].timerH)
			{
#ifdef DEBUGX
				if(theConf.traceflag & (1<<CMDD))
					printf("%sStarting timer for meterController %d MAC %x\n",CMDDT,argument->pos,losMacs[argument->pos].macAdd);
#endif
				int cual=argument->pos;
				losMacs[argument->pos].timerH=xTimerCreate("Monitor",TIMEWAITMAC /portTICK_PERIOD_MS,pdTRUE,( void * )cual,&monitorCallback);
				if(losMacs[argument->pos].timerH==NULL)
					printf("Failed to create HourChange timer\n");
				xTimerStart(losMacs[argument->pos].timerH,0); //Start it
				//kill timer and start a new one
			}
			else
			{
#ifdef DEBUGX
				if(theConf.traceflag & (1<<CMDD))
					printf("%sreseting timer for meterController %d MAC %x\n",CMDDT,argument->pos,losMacs[argument->pos].macAdd);
#endif
				xTimerReset(losMacs[argument->pos].timerH,0); //Start it with new

				//start a new one
			}
#ifdef DEBUGX
			if(theConf.traceflag & (1<<CMDD))
				printf("%sMeter[%d][%d]=%d\n",CMDDT,argument->pos,pos,tallies[argument->pos][pos]);
#endif


		}//lmac && lpos

		if(answer)
		{
			time(&loginData.thedate);
			loginData.theTariff=100;
			send(argument->pComm, &loginData, sizeof(loginData), 0);
		}
}
