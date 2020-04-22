/*
 * loginCmd.cpp
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
extern bool decryptLogin(const char* b64, uint16_t blen, char *decryp, size_t * fueron);
extern void write_to_flash();
extern cJSON *answerMsg(const char* toWhom, int err,int tariff,const char *cmds);
extern int aes_encrypt(const char* src, size_t son, char *dst,const char *cualKey);
#ifdef HEAPSAMPLE
extern void heapSample(char *subr);
#endif
extern void pprintf(const char * format, ...);

int loginCmd(parg* argument)
{
	char *aca=(char*)malloc(500);
	char *kk=(char*)malloc(500);
	char oldk[32];
	//return;

	size_t fueron,klen=0;
	vTaskDelay(400/portTICK_RATE_MS);			//give system a head start

#ifdef HEAPSAMPLE
	 heapSample((char*)"LoginCmdSt");
#endif
	memcpy(oldk,losMacs[argument->pos].theKey,AESL);

	if(losMacs[argument->pos].dState>LOGINSTATE)
		pprintf("Forcing login state MAC %x state %s seen %s",(u32)argument->macn,stateName[losMacs[argument->pos].dState],ctime(&losMacs[argument->pos].lastUpdate));

	cJSON *cmdIteml =(cJSON*)argument->pMessage;
	cJSON *kkey= cJSON_GetObjectItem(cmdIteml,"key");
	if(kkey)
	{
		memcpy(kk,kkey->valuestring,strlen(kkey->valuestring));
		klen=strlen(kkey->valuestring);
		if(decryptLogin(kk,klen,aca, &fueron))
		{
			memcpy(&losMacs[argument->pos].theKey,aca,fueron);		//key saved as sent by the MtM
			memcpy(&theConf.lkey[argument->pos],aca,fueron);
			write_to_flash();
		}
		else
			pprintf("Invalid decrypt in JSON!!!!!\n"); //stay with the same default aes key and send a warning message as being under attack
	}
	else
		pprintf("No key\n"); //keep original key. Weird but who knows. It would be a Programmers error not to change the key...but

	losMacs[argument->pos].dState=LOGINSTATE;
	losMacs[argument->pos].stateChangeTS[LOGINSTATE]=millis();

	free(aca);
	free(kk);

	//make answer to Login

	cJSON *theAnswer=answerMsg(losMacs[argument->pos].mtmName,ESP_OK,100,NULL);
	if(theAnswer)
	{
		char *res=cJSON_Print(theAnswer);
		if(res)
		{
			if(theConf.traceflag & (1<<MSGD))
				if( theConf.macTrace & (1<<argument->pos))
					pprintf("LoginAnswer %s\n",res);
			int len=strlen(res);
			int theSize=len;
			int rem= theSize % 16;
			theSize+=16-rem;

			char * output=(char*)malloc(theSize);
			if(output)
			{
				bzero(output,theSize);
				int sendcount=aes_encrypt(res,len,output,theConf.lkey[argument->pos]);
				if(sendcount>0)
				{
					size_t num=send(argument->pComm, output,sendcount, 0);
					if(num!=sendcount)
						pprintf("Login sendcount err %d %d\n",sendcount,num);
				}
				else
					pprintf("Failed to encrypt %d\n",sendcount);
				free(res);
				free(output);
			}
			else
				pprintf("No Ram for Login answer\n");
		}
		cJSON_Delete(theAnswer);
	}
	else
		pprintf("Could not cJSON Login Answer\n");


	/*
	cJSON *theAnswer=answerMsg(losMacs[argument->pos].mtmName,ESP_OK,100,NULL);
	if(theAnswer)
	{
		res=cJSON_Print(theAnswer);
		cJSON_Delete(theAnswer);
	}
	int len=strlen(res);
	int theSize=len;
	int rem= theSize % 16;
	theSize+=16-rem;
	char * output=(char*)malloc(theSize);
	bzero(output,theSize);
	int sendcount=aes_encrypt(res,len,output,oldk);
	if(sendcount>0)
	{
		send(argument->pComm, output,sendcount, 0);
	}
	else
		printf("Failed to encrypt %d\n",sendcount);

	if(output)
		free(output);
*/
#ifdef HEAPSAMPLE
	 heapSample((char*)"LoginCmdEnd");
#endif
	return 0;
}
