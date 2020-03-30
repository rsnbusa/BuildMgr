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
extern void pprintf(const char * format, ...);
extern cJSON *answerMsg(char* toWhom, int err,int tariff,char *cmds);
extern int aes_encrypt(char* src, size_t son, char *dst,char *cualKey);
#ifdef HEAPSAMPLE
extern void heapSample(char *subr);
#endif

 void monitorCallback( TimerHandle_t xTimer )
 {
	 u32 cualf = ( uint32_t ) pvTimerGetTimerID( xTimer );
	 if(theConf.traceflag & (1<<MSGD))
		 pprintf("%sTimer for Meter(%d) %s triggered\n",MSGDT,cualf,losMacs[cualf].mtmName); //should report to ControlQueue advicing posible problem with this MeterId
	 losMacs[cualf].toCount++;
	 if(losMacs[cualf].toCount>MAXTIMEOUTS)
	 {
		mqttMsg_t mqttMsgHandle;
		memset(&mqttMsgHandle,0,sizeof(mqttMsgHandle));
		char *mensaje=(char*)malloc(100);
		sprintf(mensaje,"Meter(%d) %s triggered\n",cualf,losMacs[cualf].mtmName);
		mqttMsgHandle.msgLen=strlen(mensaje);
		mqttMsgHandle.message=(uint8_t*)mensaje;
		mqttMsgHandle.maxTime=1000;
		mqttMsgHandle.queueName=(char*)"MeterIoT/EEQ/SOS";
		if(mqttQ)
			xQueueSend( mqttQ,&mqttMsgHandle,0 );			//submode will free Mensaje
		losMacs[cualf].toCount=0; 							//sends every MAXTIMEOUTS minutes an alarm. That serious
		 //send report to SOS
		 if(theConf.traceflag & (1<<MSGD))
			 pprintf("%sSOS Timeout sent for Meter(%d) %s\n",MSGDT,cualf,losMacs[cualf].mtmName);
	 }
		xTimerReset(xTimer,0); //Start it again

 }

char * hostMsg(uint8_t theMtM)
{
	hostCmd cmd;
	uint8_t van=0;
	char *lmsg=NULL;

	if(mtm[theMtM])
	{
		int son=uxQueueMessagesWaiting(mtm[theMtM]);
		if(son>0)
		{
			cJSON *batch=cJSON_CreateObject();
			if(!batch)
				return NULL;
			cJSON *ar = cJSON_CreateArray();
			if(!ar)
			{
				cJSON_Delete(batch);
				return NULL;
			}

			for(int a=0;a<son;a++)
			{
				bzero(&cmd,sizeof(cmd));

				if( xQueueReceive( mtm[theMtM], &cmd, 100/  portTICK_RATE_MS ))
				{
					if(cmd.msg)
					{
						cJSON *cmdJ=cJSON_CreateObject();
						if(cmdJ)
						{
							van++;
							cJSON_AddNumberToObject(cmdJ,"MID",cmd.meter);
							cJSON_AddStringToObject(cmdJ,"cmd",cmd.msg);
							cJSON_AddItemToArray(ar, cmdJ);
						}
					}
					else
						printf("No Msg\n");
				}
				else
				{
					//weird
				}
			}
			cJSON_AddItemToObject(batch, "hostCmd",ar);
			lmsg=cJSON_PrintUnformatted(batch);
			//NOW we free the Message received in Mqtt Msg Handler.
			if(cmd.msg)
			{
				if(theConf.traceflag & (1<<MSGD))
					pprintf("%sFreeing MtM Msg %d %d bytes long\n",MSGDT,theMtM,strlen(cmd.msg));
				free(cmd.msg);			//if it was NULL it will crahs tellin something wrong logic
			}
			cJSON_Delete(batch);
		}
		return lmsg;
	}
	else
		return NULL;			//no queue means no MAC reserved. No Whitelist
}


int statusCmd(parg *argument)
{
	bool answer=false;
	vTaskDelay(100 /  portTICK_RATE_MS);

#ifdef HEAPSAMPLE
	heapSample("StatusCmdSt");
#endif

	int mtmPos=argument->pos;

	// argument->pos is Mac Pos (mtmPos)
	// lpos is MID pos within the MtM (devpos)
	cJSON *lpos= 	cJSON_GetObjectItem((cJSON*)argument->pMessage,"Pos");
	cJSON *rep= 	cJSON_GetObjectItem((cJSON*)argument->pMessage,"reply");
	cJSON *kwh= 	cJSON_GetObjectItem((cJSON*)argument->pMessage,"KwH");
	cJSON *mid= 	cJSON_GetObjectItem((cJSON*)argument->pMessage,"mid");
	cJSON *beats= 	cJSON_GetObjectItem((cJSON*)argument->pMessage,"beats");

	if(rep)
		answer=rep->valueint;

	if((argument->macn!=0) && lpos)						//must have a valid Mac# and a MID pos from MtM
	{
		int devpos=lpos->valueint;						// this is the Meter# from the MtM
		if(devpos>=MAXDEVS)
		{
			printf("Status Unit %d OB\n",devpos);
			return -1;
		}

		losMacs[mtmPos].dState=MSGSTATE;
		losMacs[mtmPos].stateChangeTS[MSGSTATE]=millis();

		tallies[mtmPos][devpos]++;
		if(mid)
			strcpy(losMacs[mtmPos].meterSerial[devpos],mid->valuestring);
		if(kwh)
			losMacs[mtmPos].controlLastKwH[devpos]=kwh->valueint;

		losMacs[mtmPos].trans[devpos]++;
		if(beats)
			losMacs[mtmPos].controlLastBeats[devpos]=beats->valueint;

		if(!(theConf.pause & (1<<PSTAT)))
		{
			if(!losMacs[mtmPos].timerH)
			{
#ifdef DEBUGX
				if(theConf.traceflag & (1<<CMDD))
					pprintf("%sStarting timer for meterController %d MAC %x\n",CMDDT,mtmPos,losMacs[mtmPos].macAdd);
#endif
				int cual=mtmPos;
				losMacs[mtmPos].timerH=xTimerCreate("Monitor",theConf.msgTimeOut /portTICK_PERIOD_MS,pdTRUE,( void * )cual,&monitorCallback);
				if(losMacs[mtmPos].timerH==NULL)
					pprintf("Failed to create HourChange timer\n");
				xTimerStart(losMacs[mtmPos].timerH,0); //Start it
			}
			else
			{
#ifdef DEBUGX
				if(theConf.traceflag & (1<<CMDD))
					pprintf("%s reseting timer for meterController %d MAC %x\n",CMDDT,mtmPos,losMacs[mtmPos].macAdd);
#endif
				xTimerReset(losMacs[mtmPos].timerH,0); //Start it with new
				losMacs[mtmPos].toCount=0;
			}
		}
#ifdef DEBUGX
		if(theConf.traceflag & (1<<CMDD))
			pprintf("%sMeter[%d][%d]=%d\n",CMDDT,mtmPos,devpos,tallies[mtmPos][devpos]);
#endif
	}//lmac && lpos

	if(answer)
	{
		//find if any msg for that mtm
		char *lbuf=hostMsg(mtmPos);			//can be NULL as in No Msg from Host

		cJSON *theAnswer=answerMsg(losMacs[mtmPos].mtmName,ESP_OK,100,lbuf);
		if(theAnswer)
		{
			char *res=cJSON_Print(theAnswer);
			if(res)
			{
				if(theConf.traceflag & (1<<MSGD))
					pprintf("%sSTAnswer %d %s\n",MSGDT,mtmPos,res);
				int len=strlen(res);
				int theSize=len;
				int rem= theSize % 16;
				theSize+=16-rem;

				char * output=(char*)malloc(theSize);
				if(output)
				{
					bzero(output,theSize);
					int sendcount=aes_encrypt(res,len,output,theConf.lkey[mtmPos]);
					if(sendcount>0)
					{
						size_t num=send(argument->pComm, output,sendcount, 0);
						if(num!=sendcount)
							pprintf("Answer sendcount err %d %d\n",sendcount,num);
					}
					else
						pprintf("Failed to encrypt %d\n",sendcount);
					free(output);
				}
				else
					pprintf("No Ram for Status answer\n");
				free(res);
			}
			cJSON_Delete(theAnswer);
		}
		else
			pprintf("Could not cJSON Status Answer\n");
		if(lbuf)
		{
			free(lbuf);
			lbuf=NULL;
		}
	}

#ifdef HEAPSAMPLE
	heapSample("StatusCmdEnd");
#endif
	return 0;

}
