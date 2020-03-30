/*
 * sendHost.cpp
 *
 *  Created on: Jan 26, 2020
 *      Author: rsn
 */


#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern void pprintf(const char * format, ...);

void sendHostCallback(int res)
{
#ifdef DEBUGX
	if(theConf.traceflag & (1<<HOSTD))
		pprintf("%sSendHostCmd result %d\n",HOSTDT,res);
#endif
	sendH=res;
    xEventGroupSetBits(wifi_event_group, SENDH_BIT);//message sent bit
}

int sendHostCmd(parg *argument)
{
	mqttMsg_t mqttMsgHandle;

	memset(&mqttMsgHandle,0,sizeof(mqttMsgHandle));
	if(!argument)
	{
		pprintf("Not valid Argument sendHost\n");
		return -1;
	}

	char *mensaje=cJSON_Print((cJSON*)argument->pMessage);
	sendH=0;
	//pprintf("SendHost [%s] len %d\n",mensaje,strlen(mensaje));

	if(mensaje)
	{
		//send it to host. No reply required
		mqttMsgHandle.cb=sendHostCallback;
		mqttMsgHandle.maxTime=1000;
		mqttMsgHandle.message=(uint8_t*)mensaje;						//Submode will free the mensaje variable
		mqttMsgHandle.msgLen=strlen(mensaje);
		mqttMsgHandle.queueName=(char*)"MeterIoT/EEQ/RESPONSE";
	    xEventGroupClearBits(wifi_event_group, SENDH_BIT);//message sent bit

		if(mqttQ)
				xQueueSend( mqttQ,&mqttMsgHandle,0 );			//Submode will free the mensaje variable
		if(!xEventGroupWaitBits(wifi_event_group, SENDH_BIT, false, true,  mqttMsgHandle.maxTime+10/  portTICK_RATE_MS))
		{
			pprintf("SendHostCmd result timeout");
			sendH=BITTM;
		}
	}
		return sendH;
}
