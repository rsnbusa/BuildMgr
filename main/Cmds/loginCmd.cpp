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
extern bool decryptLogin(char* b64, uint16_t blen, char *decryp, size_t * fueron);


void loginCmd(parg* argument)
{
	loginT loginData;
	char *aca=(char*)malloc(500);
	char *kk=(char*)malloc(500);
	size_t fueron,klen=0;

	if(losMacs[argument->pos].dState>LOGINSTATE)
		printf("Forcing login state MAC %x state %s seen %s",(u32)argument->macn,stateName[losMacs[argument->pos].dState],ctime(&losMacs[argument->pos].lastUpdate));

	cJSON *cmdIteml =(cJSON*)argument->pMessage;
	cJSON *kkey= cJSON_GetObjectItem(cmdIteml,"key");
	if(kkey)
	{
		memcpy(kk,kkey->valuestring,strlen(kkey->valuestring));
		klen=strlen(kkey->valuestring);
		if(decryptLogin(kk,klen,aca, &fueron))
			memcpy(losMacs[argument->pos].theKey,aca,fueron);		//key saved as sent by the MtM
		else
			printf("Invalid decrypt in JSON!!!!!\n"); //stay with the same default aes key and send a warning message as being under attack
	}
	else
		printf("No key\n"); //keep original key. Weird but who knows. It would be a Programmers error not to change the key...but

	losMacs[argument->pos].dState=LOGINSTATE;
	losMacs[argument->pos].stateChangeTS[LOGINSTATE]=millis();

	time(&loginData.thedate);
	loginData.theTariff=100;
	send(argument->pComm, &loginData, sizeof(loginData), 0);
	if(aca)
		free(aca);
	if(kk)
		free(kk);
}
