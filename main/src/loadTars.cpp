/*
 * loadTars.cpp
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

static esp_err_t http_event_handle(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
            	int *suma=(int*)evt->user_data;
            	fram.writeMany(TARIFADIA+ *suma,(u8*)evt->data,evt->data_len);		//save data directly to fram
            	*suma +=  evt->data_len;
            }
            break;
        default:
        	break;
    }
    return ESP_OK;
}

void loadit(parg *pArg)
{
	char	textl[80];
	parg 	*elcmd=(parg*)pArg;
	int 	tariff=0;

	int vanl=0;
	esp_http_client_config_t lconfig;
pprintf("Load tariffs HTTP\n");
	memset(&lconfig,0,sizeof(lconfig));

	if(elcmd!=NULL)
	{
		if(elcmd->pMessage!=NULL)
		{
			cJSON *tar= cJSON_GetObjectItem((cJSON*)elcmd->pMessage,"tariff");
			if(tar)
				tariff=tar->valueint;
		}
	}
	//sprintf(textl,"http://feediot.c1.biz/tarifasPer%d.txt",tariff);
	sprintf(textl,"http://192.168.100.7:8080/tarifasPer%d.txt",tariff);	//localfile
	lconfig.url=textl;
	lconfig.user_data=(void*)&vanl;										//our counter

#ifdef DEBUGX
	if(theConf.traceflag & (1<<HOSTD))
		pprintf("%sLoading tariffs %s\n",HOSTDT,lconfig.url);
#endif

	lconfig.event_handler = http_event_handle;							//in charge of saving received data to Fram directly

	esp_http_client_handle_t client = esp_http_client_init(&lconfig);
	if(client)
	{
		if(framSem)
			if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))	//reserve our Fram just for this operation
			{
				esp_err_t err = esp_http_client_perform(client);			// do the hard work
				if (err == ESP_OK)
				{
	#ifdef DEBUGX
					if(theConf.traceflag & (1<<HOSTD))
						pprintf("%sStatus = %d, content_length = %d\n",HOSTDT,
							esp_http_client_get_status_code(client),
							esp_http_client_get_content_length(client));
	#endif
					if(esp_http_client_get_status_code(client)!=200)
					{
						if(theConf.traceflag & (1<<HOSTD))
							pprintf("%sFailed to download Tariff Err %x\n",HOSTDT,esp_http_client_get_status_code(client));
						esp_http_client_cleanup(client);
						return;
					}
				}
				else
				{
	#ifdef DEBUGX
					if(theConf.traceflag & (1<<HOSTD))
						pprintf("%sFailed to download Tariff Err %x\n",HOSTDT,err);
					esp_http_client_cleanup(client);
					return;
	#endif
				}
				// all is well, cleanup and read back to our working array
				esp_http_client_cleanup(client);

				err=fram.read_tarif_day(yearDay,(u8*)&tarifasDia);
				xSemaphoreGive(framSem);
				if(err!=0)
					pprintf("%sLoadTariff Error %d reading Tariffs day %d...recovering from Host\n",HOSTDT,err,yearDay);
			}
	}
	else
	{
#ifdef DEBUGX
		if(theConf.traceflag & (1<<HOSTD))
			pprintf("%sFailed to start HTTP Client\n",HOSTDT);
#endif
	}

#ifdef DEBUGX
	if(theConf.traceflag & (1<<HOSTD))
		pprintf("%sLoading tariffs successful\n",HOSTDT);
#endif
}


void loadDefaultTariffs()
{
    cmdType cmd;

	cJSON *root=cJSON_CreateObject();
	cJSON *ar = cJSON_CreateArray();

	if(root==NULL || ar==NULL)
	{
		pprintf("cannot create root tariff\n");
		return;
	}

	cJSON *cmdJ=cJSON_CreateObject();
	cJSON_AddStringToObject(cmdJ,"cmd","/ga_tariff");
	cJSON_AddNumberToObject(cmdJ,"tariff",1); //default is 1
	cJSON_AddStringToObject(cmdJ,"connmgr",theConf.meterConnName);
	cJSON_AddItemToArray(ar, cmdJ);

	cJSON_AddItemToObject(root,"Batch", ar);
	cJSON_AddNumberToObject(root,"macn", theMacNum);	//system checks for a MAC

	char *lmessage=cJSON_Print(root);
	if(lmessage==NULL)
	{
		pprintf("Error creating JSON Tariff\n");
		cJSON_Delete(root);
		return;
	}

	cmd.mensaje=lmessage;
	cmd.fd=3;//send from internal
	cmd.pos=0;
	xQueueSend( mqttR,&cmd,0 );
}
