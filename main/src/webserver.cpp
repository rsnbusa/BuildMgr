/*
 * webserver.cpp
 *
 *  Created on: Dec 29, 2019
 *      Author: rsn
 */
#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern void pprintf(const char * format, ...);

static esp_err_t challenge_get_handler(httpd_req_t *req);
static esp_err_t conn_login(httpd_req_t *req);
static esp_err_t conn_firmware(httpd_req_t *req);
static esp_err_t conn_tariff(httpd_req_t *req);
static esp_err_t conn_base(httpd_req_t *req);
static esp_err_t sendok(httpd_req_t *req);
static esp_err_t sendnak(httpd_req_t *req);
static esp_err_t sendlogo(httpd_req_t *req);
static esp_err_t conn_config_handler(httpd_req_t *req);

extern const unsigned char answer_start[] 	asm("_binary_challenge_html_start");
extern const unsigned char setup_start[] 	asm("_binary_connSetup_html_start");
extern const unsigned char login_start[] 	asm("_binary_login_html_start");
extern const unsigned char final_start[] 	asm("_binary_ok_html_start");

extern const unsigned char ok_start[] 		asm("_binary_ok_png_start");
extern const unsigned char ok_end[] 		asm("_binary_ok_png_end");
extern const unsigned char nak_start[] 		asm("_binary_nak_png_start");
extern const unsigned char nak_end[] 		asm("_binary_nak_png_end");
extern const unsigned char logo_start[] 	asm("_binary_meter_png_start");
extern const unsigned char logo_end[] 		asm("_binary_meter_png_end");

int ok_bytes=ok_end-ok_start;
int nak_bytes=nak_end-nak_start;
int logo_bytes=logo_end-logo_start;

extern void delay(uint32_t a);
extern void write_to_flash();
extern void firmUpdate(void* pArg);
extern uint32_t millis();
void shaMake(const char * key,uint8_t klen,uint8_t* shaResult);


static const httpd_uri_t connsetup = {
    .uri       = "/cmconfig",
    .method    = HTTP_GET,
    .handler   = conn_config_handler,
	.user_ctx	= NULL
};

static const httpd_uri_t okpng = {
    .uri       = "/ok.png",
    .method    = HTTP_GET,
    .handler   = sendok,
	.user_ctx	= NULL
};

static const httpd_uri_t nakpng = {
    .uri       = "/nak.png",
    .method    = HTTP_GET,
    .handler   = sendnak,
	.user_ctx	= NULL
};

static const httpd_uri_t logopng = {
    .uri       = "/meter.png",
    .method    = HTTP_GET,
    .handler   = sendlogo,
	.user_ctx	= NULL
};

static const httpd_uri_t challenge = {
    .uri       = "/cmchallenge",
    .method    = HTTP_GET,
    .handler   = challenge_get_handler,
	.user_ctx	= NULL
};

static const httpd_uri_t cmdFW = {
    .uri       = "/cfirmware",
    .method    = HTTP_GET,
    .handler   = conn_firmware,
	.user_ctx	= NULL
};

static const httpd_uri_t cmdTariff = {
    .uri       = "/ctariff",
    .method    = HTTP_GET,
    .handler   = conn_tariff,
	.user_ctx	= NULL
};

static const httpd_uri_t login = {
    .uri       = "/cmlogin",
    .method    = HTTP_GET,
    .handler   = conn_login,
	.user_ctx	= NULL
};

static const httpd_uri_t baseHtml = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = conn_base,
	.user_ctx	= NULL
};

esp_err_t sendnak(httpd_req_t *req)
{
	httpd_resp_set_type(req,"image/png");
	httpd_resp_send(req,(char*)nak_start,nak_bytes);
	return ESP_OK;
}

esp_err_t sendok(httpd_req_t *req)
{
	httpd_resp_set_type(req,"image/png");
	httpd_resp_send(req,(char*)ok_start,ok_bytes);
	return ESP_OK;
}

esp_err_t sendlogo(httpd_req_t *req)
{
	httpd_resp_set_type(req,"image/png");
	httpd_resp_send(req,(char*)logo_start,logo_bytes);
	return ESP_OK;
}

static esp_err_t conn_firmware(httpd_req_t *req)
{
    char*  buf=NULL;
    size_t buf_len;
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            char param[32];
            if (httpd_query_key_value(buf, "password", param, sizeof(param)) == ESP_OK)
            {
            	sprintf(tempb,"[%s]Invalid password",strftime_buf);
            	 if(strcmp(param,"ZiPo")!=0)
            		 	goto exit;
            }
        }
    	sprintf(tempb,"[%s]Firmware update in progress...",strftime_buf);
    	xTaskCreate(&firmUpdate,"U571",10240,NULL, 5, NULL);
    }
    else
    	sprintf(tempb,"Invalid parameters");
	exit:
	if(buf)
		free(buf);
	httpd_resp_send(req, tempb, strlen(tempb));

    return ESP_OK;
}

static esp_err_t conn_tariff(httpd_req_t *req)
{
    char*  buf=NULL;
    size_t buf_len;
    int tariff=0;
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    cmdType cmd;
    cJSON * root=NULL;
    char *lmessage=NULL;

	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            char param[32];
            if (httpd_query_key_value(buf, "password", param, sizeof(param)) == ESP_OK)
            {
            	sprintf(tempb,"[%s]Invalid password",strftime_buf);
            	 if(strcmp(param,"ZiPo")!=0)
            		 	goto exit;
            }
        	root=cJSON_CreateObject();
			cJSON *ar = cJSON_CreateArray();

			if(root==NULL || ar==NULL)
			{
				pprintf("cannot create root tariff\n");
				return -1;
			}
       //     if (httpd_query_key_value(buf, "tariff", param, sizeof(param)) == ESP_OK)
         //   	theConf.slot_Server.tariff_id=atoi(param);

			cJSON *cmdJ=cJSON_CreateObject();
			cJSON_AddStringToObject(cmdJ,"cmd","/ga_tariff");
	//		cJSON_AddNumberToObject(cmdJ,"tariff",theConf.slot_Server.tariff_id);
			cJSON_AddNumberToObject(cmdJ,"tariff",1);
			cJSON_AddItemToArray(ar, cmdJ);
			cJSON_AddItemToObject(root,"Batch", ar);
			lmessage=cJSON_Print(root);
			if(lmessage==NULL)
			{
				sprintf(tempb,"Error creating JSON Tariff");
				cJSON_Delete(root);
				goto exit;
			}
            write_to_flash();
        }
    	sprintf(tempb,"[%s]Tariff %d loading process started...",strftime_buf,tariff);

   // 	xTaskCreate(&loadit,"loadT",10240,(void*)tariff, 5, NULL);
    	cmd.mensaje=lmessage;
    	cmd.fd=3;//send from internal
    	cmd.pos=0;
    	xQueueSend( mqttR,&cmd,0 );

    }
    else
    	sprintf(tempb,"Invalid parameters");
	exit:
	if(buf)
		free(buf);
	if(root)
		cJSON_Delete(root);
	//======================
	//lmessage will be freed by the calling routine
	//======================

	httpd_resp_send(req, tempb, strlen(tempb));

    return ESP_OK;
}

void resetlater(void *parg)
{
	delay(3000);
	esp_restart();
}

static esp_err_t challenge_get_handler(httpd_req_t *req)
{
#define SHAC	16				//best force it to 16 chars, 8 hex values from the SHA challenge
#define SHAL	(SHAC/2)-1

    char*  buf;
    size_t buf_len;
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    bool rebf=false;
	char param[100];
#if SHAC>8
    long long shaint=1,gotsha=0;
#else
    uint32_t shaint=0,gotsha=1;
#endif

	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
		sprintf(tempb,(char*)final_start,"nak","Invalid parameters");
        buf = (char*)malloc(buf_len);
        if(buf)
        {
			if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
			{
				if (httpd_query_key_value(buf, "challenge", param, sizeof(param)) == ESP_OK)
				{
					int ll=strlen(param);

					if(ll<SHAC)
					{
						sprintf(tempb,(char*)final_start,"nak","Invalid Challenge count");
						httpd_resp_send(req, tempb, strlen(tempb));
						return ESP_OK;
					}
					if(ll>SHAC)
						param[SHAC]=0; //cut it at 8 chars

#if SHAC>8
					gotsha=strtoull(param, NULL, 16);
#else
					gotsha=strtoul(param, NULL, 16);
#endif

					uint8_t *p=(uint8_t*)&shaint;
					uint8_t * sh=(uint8_t*)&sharesult[SHAL];

					for (int a=SHAL;a>=0;a--)	//big endian to little endian
						*(p++) = *(sh--);

				//	printf("Shatint %llu gotsha %llu\n",shaint,gotsha);

					if (shaint==gotsha)
					{
						for (int a=0;a<MAXDEVS;a++)
						{
							strcpy(theConf.medidor_id[a],setupHost[a]->meterid);
							time((time_t*)&theConf.bornDate[a]);
							theConf.beatsPerKw[a]			=setupHost[a]->bpkwh;
							theConf.bornKwh[a]				=setupHost[a]->startKwh;
							if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
							{
								fram.formatMeter(a);
								fram.write_lifekwh(a,setupHost[a]->startKwh);	// write to Fram. Its beginning of life KWH
								xSemaphoreGive(framSem);
							}
							pprintf("Writing life Meter %d= %d\n",a,setupHost[a]->startKwh);
						}
						theConf.active=1;				// theConf is now ACTIVE and Certified
						write_to_flash();
						memset(&setupHost,0,sizeof(setupHost));
						bzero(tempb,1000);
						sprintf(tempb,(char*)final_start,"ok","Success...rebooting in 3 secs");
						rebf=true;
					}
					else
						sprintf(tempb,(char*)final_start,"nak","Invalid challenge");
				}
				else
					sprintf(tempb,(char*)final_start,"nak","Missing challenge");
			}
			free(buf);
        }
     }

	httpd_resp_send(req, tempb, strlen(tempb));
	if(rebf)
	{
		xTaskCreate(&resetlater,"web",1024,(void*)1, 4, NULL);	// Messages from the Meters. Controller Section socket manager
	}

    return ESP_OK;
}

bool getParam(char *buf,char *cualp,char *donde)
{
	if( httpd_query_key_value(buf, cualp, donde, 30)==ESP_OK)
		return true;
	else
		return false;
}


static esp_err_t conn_base(httpd_req_t *req)		// the /(base) url
{

	httpd_resp_send(req,(char*)login_start, strlen(login_start));
	return ESP_OK;
}

static esp_err_t conn_login(httpd_req_t *req) //receives user/password and sends Conf Data
{
	int buf_len;
	char username[20];
	char *buf=NULL;
	char param[30];

	buf_len = httpd_req_get_url_query_len(req) + 1;
	if (buf_len > 1)
	{
		buf = (char*)malloc(buf_len);
		if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
		{
			bzero(param,sizeof(param));
			if(getParam(buf,(char*)"user",param))
			{
				if(strlen(param)>=8)
				{
					strcpy(username,param);
					bzero(param,sizeof(param));
					if(getParam(buf,(char*)"password",param))
					{
						if(strlen(param)>=8)
						{
							bzero(tempb,2000);
							// prepare html wiht known parameters
		sprintf(tempb,(const char*)setup_start,
		theConf.meterConnName,theConf.msgTimeOut,
		theConf.medidor_id[0],theConf.beatsPerKw[0]==800?"Selected":"",theConf.beatsPerKw[0]==1000?"Selected":"",theConf.beatsPerKw[0]==1200?"Selected":"",theConf.beatsPerKw[0]==2000?"Selected":"",theConf.bornKwh[0],theConf.configured[0]?"checked":"",
		theConf.medidor_id[1],theConf.beatsPerKw[1]==800?"Selected":"",theConf.beatsPerKw[1]==1000?"Selected":"",theConf.beatsPerKw[1]==1200?"Selected":"",theConf.beatsPerKw[1]==2000?"Selected":"",theConf.bornKwh[1],theConf.configured[1]?"checked":"",
		theConf.medidor_id[2],theConf.beatsPerKw[2]==800?"Selected":"",theConf.beatsPerKw[2]==1000?"Selected":"",theConf.beatsPerKw[2]==1200?"Selected":"",theConf.beatsPerKw[2]==2000?"Selected":"",theConf.bornKwh[2],theConf.configured[2]?"checked":"",
		theConf.medidor_id[3],theConf.beatsPerKw[3]==800?"Selected":"",theConf.beatsPerKw[3]==1000?"Selected":"",theConf.beatsPerKw[3]==1200?"Selected":"",theConf.beatsPerKw[3]==2000?"Selected":"",theConf.bornKwh[3],theConf.configured[3]?"checked":"",
		theConf.medidor_id[4],theConf.beatsPerKw[4]==800?"Selected":"",theConf.beatsPerKw[4]==1000?"Selected":"",theConf.beatsPerKw[4]==1200?"Selected":"",theConf.beatsPerKw[4]==2000?"Selected":"",theConf.bornKwh[4],theConf.configured[4]?"checked":"");

		httpd_resp_send(req, tempb, strlen(tempb));
						}
					}
				}
			}
		}
	}
	return ESP_OK;
}

static esp_err_t conn_config_handler(httpd_req_t *req)
{
	    time_t now;
	    struct tm timeinfo;
	    char strftime_buf[64];
	    int buf_len;
		char param[32],tt[10];
		bool success=false;
	    char challengeSHA[50],aca[20];
	    char *buf=NULL;


		time(&now);
		localtime_r(&now, &timeinfo);
		strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

		buf_len = httpd_req_get_url_query_len(req) + 1;
		if (buf_len > 1)
		{
			buf = (char*)malloc(buf_len);
			if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
			{
				bzero(param,sizeof(param));
				if(getParam(buf,(char*)"conn",param))
				{
					sprintf(tempb,final_start,"nak","Invalid Paramters");
				   	strcpy(theConf.meterConnName,param);
					for (int a=0;a<MAXDEVS;a++)
					{
						sprintf(tt,"m%d",a+1);
						bzero(param,sizeof(param));
						if(getParam(buf,tt,param))
							if(strlen(param)>0)
								strcpy(setupHost[a]->meterid,param);
							else
								continue;
						else
							continue;

						sprintf(tt,"b%d",a+1);
						bzero(param,sizeof(param));
						if(getParam(buf,tt,param))
							if(strlen(param)>0)
								setupHost[a]->bpkwh=atoi(param);
							else
								continue;
						else
							continue;

						sprintf(tt,"k%d",a+1);
						bzero(param,sizeof(param));
						if(getParam(buf,tt,param))
							if(strlen(param)>0)
								setupHost[a]->startKwh=atoi(param);
							else
								continue;
						else
							continue;

						sprintf(tt,"c%d",a+1);
						bzero(param,sizeof(param));
						if(getParam(buf,tt,param))
								theConf.configured[a]=1;
							else
								theConf.configured[a]=0;

						success=true;
					}
				}
			}
		}
		if(success)
		{
		 	 sprintf(challengeSHA,"%s%u%06x",theConf.meterConnName,millis(),theMacNum);
			 shaMake(challengeSHA,strlen(challengeSHA),(uint8_t*)sharesult);
			 memcpy(aca,sharesult,16);
			 aca[16]=0;
			 shaMake(aca,16,(uint8_t*)sharesult);		//sharesult has the SHA to compare against when challenge received
			 size_t writ=0;
			 char newdest[100];
			 bzero(newdest,sizeof(newdest));

			 mbedtls_base64_encode((unsigned char*)newdest,sizeof(newdest),&writ,(unsigned char*)aca,16);
			 sprintf(tempb,(char*)answer_start,newdest);
		}
		httpd_resp_send(req, tempb, strlen(tempb));

	    return ESP_OK;
}



void start_webserver(void *pArg)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
#ifdef DEBUGX
    if(theConf.traceflag & (1<<WEBD))
    	pprintf("%sStarting server on port:%d\n",WEBDT, config.server_port);
#endif
    if (httpd_start(&server, &config) == ESP_OK) {
    	for(int a=0;a<MAXDEVS;a++)
    	{
    		setupHost[a]=(host_t*)malloc(sizeof(host_t));
    		memset(setupHost[a],0,sizeof(host_t));
    	}
        // Set URI handlers
        httpd_register_uri_handler(server, &challenge);		//confirm challenge and store in flash
    //    httpd_register_uri_handler(server, &cmdFW);			//confirm challenge and store in flash
    //    httpd_register_uri_handler(server, &cmdTariff);		//confirm challenge and store in flash
        httpd_register_uri_handler(server, &connsetup);
        httpd_register_uri_handler(server, &login);
        httpd_register_uri_handler(server, &baseHtml);
        httpd_register_uri_handler(server, &okpng);
        httpd_register_uri_handler(server, &nakpng);
        httpd_register_uri_handler(server, &logopng);
    }
#ifdef DEBUGX
    if(theConf.traceflag & (1<<WEBD))
    	pprintf("WebServer Started\n");
#endif
    vTaskDelete(NULL);
}


