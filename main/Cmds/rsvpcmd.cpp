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
extern cJSON *answerMsg(const char* toWhom, int err,int tariff,const char *cmds);
extern int aes_encrypt(const char* src, size_t son, char *dst,const char *cualKey);
extern int reserveSlot(parg* argument);
extern cJSON *answerMsg(const char* toWhom, int err,int tariff,const char *cmds);

#ifdef HEAPSAMPLE
extern void heapSample(char *subr);
#endif


int cmd_rsvp(parg *argumentin)
{
	cJSON 		*theAnswer=NULL;
    parg 		argument;

    int mtmPos=argumentin->pos;
    printf("RSVP in n");
	cJSON *macnn=cJSON_GetObjectItem((cJSON*)argumentin->pMessage,"macn"); //get mac.
	if(macnn)
	{
		argument.macn=macnn->valuedouble;
	//	argument.pComm=argumentin->pComm;
		if(theConf.traceflag & (1<<WIFID))
			printf("%sRSVP Command in %u\n",WIFIDT,(uint32_t)argument.macn);
		reserveSlot(&argument);

		theAnswer=answerMsg(losMacs[mtmPos].mtmName,ESP_OK,100,NULL);	//should send currentHour tariff etc. Whatever logic

		char *res=cJSON_Print(theAnswer);
		if(res)
		{
			if(theConf.traceflag & (1<<MSGD))
				if( theConf.macTrace & (1<<argumentin->pos))
					pprintf("%sRSVP %u %s\n",MSGDT,(uint32_t)argument.macn,res);
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
					size_t num=send(argumentin->pComm, output,sendcount, 0);
					if(num!=sendcount)
						pprintf("RSVP Answer sendcount err %d %d\n",sendcount,num);
				}
				else
					pprintf("RSVP Failed to encrypt %d\n",sendcount);
				FREEANDNULL(output)
			}
			else
				pprintf("RSVP No Ram for Status answer\n");
			FREEANDNULL(res)
		}
		cJSON_Delete(theAnswer);
	}
	return ESP_OK;

}
