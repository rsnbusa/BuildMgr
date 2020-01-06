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

static esp_err_t challenge_get_handler(httpd_req_t *req);
static esp_err_t setup_get_handler(httpd_req_t *req);
static esp_err_t setupend_get_handler(httpd_req_t *req);

extern void write_to_flash();

static const httpd_uri_t setup = {
    .uri       = "/setup",
    .method    = HTTP_GET,
    .handler   = setup_get_handler,
	.user_ctx	= NULL
};

static esp_err_t setup_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    int cualm=0;
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
            if (httpd_query_key_value(buf, "meter", param, sizeof(param)) == ESP_OK)
            {
            	 cualm=atoi(param);
            	 sprintf(tempb,"[%s]Invalid Meter range %d",strftime_buf,cualm);
            	 if (cualm>=MAXDEVS)
            		 goto exit;
            }
            if (httpd_query_key_value(buf, "mid", param, sizeof(param)) == ESP_OK)
            	 strcpy((char*)&setupHost[cualm].meterid,param);

            if (httpd_query_key_value(buf, "kwh", param, sizeof(param)) == ESP_OK)
            	 setupHost[cualm].startKwh=atoi(param);

            if (httpd_query_key_value(buf, "bpk", param, sizeof(param)) == ESP_OK)
            	 setupHost[cualm].bpkwh=atoi(param);

            if (httpd_query_key_value(buf, "duedate", param, sizeof(param)) == ESP_OK)
            	 setupHost[cualm].diaCorte=atoi(param);

            if (httpd_query_key_value(buf, "tariff", param, sizeof(param)) == ESP_OK)
            	 setupHost[cualm].tariff=atoi(param);
        }
        free(buf);
    }

    if(setupHost[cualm].bpkwh>0 && setupHost[cualm].diaCorte>0 && setupHost[cualm].startKwh>0 && setupHost[cualm].tariff>0 && strlen(setupHost[cualm].meterid)>0)
    {
		time(&now);
		localtime_r(&now, &timeinfo);
		strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    	setupHost[cualm].valid=true;
    	theConf.configured[cualm]=1; //in transit mode
    	sprintf(tempb,"[%s]Meter:%d Id=%s kWh=%d BPK=%d Corte=%d Tariff=%d",strftime_buf,cualm,setupHost[cualm].meterid,setupHost[cualm].startKwh,setupHost[cualm].bpkwh,
    			setupHost[cualm].diaCorte,setupHost[cualm].tariff);
    }
    else
    {
    	exit:
    	setupHost[cualm].valid=false;
        sprintf(tempb,"[%s]Invalid parameters",strftime_buf);
    }

	httpd_resp_send(req, tempb, strlen(tempb));

    return ESP_OK;
}

static const httpd_uri_t challenge = {
    .uri       = "/challenge",
    .method    = HTTP_GET,
    .handler   = challenge_get_handler,
	.user_ctx	= NULL
};

static esp_err_t challenge_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    int cualm,valor;
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
		sprintf(tempb,"[%s]Invalid parameters",strftime_buf);
        buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            char param[32];

			if (httpd_query_key_value(buf, "challenge", param, sizeof(param)) == ESP_OK)
			{
				valor=atoi(param);
				if (valor==654321)
				{
					for (int a=0;a<MAXDEVS;a++)
					{
						if(theConf.configured[a]==2)
						{
							theMeters[a].beatsPerkW=setupHost[a].bpkwh;
							memcpy((void*)&theMeters[a].serialNumber,(void*)&setupHost[a].meterid,sizeof(theMeters[a].serialNumber));
							theMeters[a].curLife=setupHost[a].startKwh;
							time((time_t*)&theMeters[a].ampTime);

							theConf.beatsPerKw[a]=setupHost[a].bpkwh;
							memcpy(theConf.medidor_id[a],(void*)&setupHost[a].meterid,sizeof(theConf.medidor_id[cualm]));
							time((time_t*)&theConf.bornDate[a]);
							theConf.beatsPerKw[a]=setupHost[a].bpkwh;
							theConf.bornKwh[a]=setupHost[a].startKwh;
							theConf.diaDeCorte[a]=setupHost[a].tariff;
							theConf.configured[a]=3;					//final status configured
						}
						else
							theConf.configured[a]=0; //reset it
					}
					theConf.active=1;				// theConf is now ACTIVE and Certified
					write_to_flash();
					memset(&setupHost,0,sizeof(setupHost));


					sprintf(tempb,"[%s]Meters were saved permanently",strftime_buf);
				}
				else
					sprintf(tempb,"[%s]Invalid challenge",strftime_buf);
			}
			else
				sprintf(tempb,"[%s]Missing challenge",strftime_buf);
        }
        free(buf);
       }
	httpd_resp_send(req, tempb, strlen(tempb));

    return ESP_OK;
}

static const httpd_uri_t setupend = {
    .uri       = "/setupend",
    .method    = HTTP_GET,
    .handler   = setupend_get_handler,
	.user_ctx	= NULL
};

static esp_err_t setupend_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    int cuantos;
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
		sprintf(tempb,"[%s]Invalid parameters",strftime_buf);
        buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            char param[32];
            if (httpd_query_key_value(buf, "meter", param, sizeof(param)) == ESP_OK) //number of meters to make permanent
            {
            	 cuantos=atoi(param);
            	 if (cuantos<=MAXDEVS)
            	 {
            		 for (int a=0;a<cuantos;a++)
            		 {
            			 if(theConf.configured[a]!=1)
            			 {
              				sprintf(tempb,"[%s]Meter %d is not configured",strftime_buf,a);
              				goto exit;
            			 }
            			 else
            				 theConf.configured[a]=2;								//waiting for challenge
            		 }
            			 sprintf(tempb,"[%s]Meters 1-%d will be saved. Challenge=123456",strftime_buf,cuantos);
            	 }
            	 else
     				sprintf(tempb,"[%s]Invalid number of meters %d",strftime_buf,cuantos);
            }
        }
        free(buf);
       }
exit:
	httpd_resp_send(req, tempb, strlen(tempb));

    return ESP_OK;
}

void start_webserver(void *pArg)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port=91;

    if(theConf.traceflag & (1<<WEBD))
    	printf("%sStarting server on port:%d\n",WEBDT, config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        httpd_register_uri_handler(server, &setup); 		//setup upto 5 meters
        httpd_register_uri_handler(server, &setupend);		//end setup and send challenge
        httpd_register_uri_handler(server, &challenge);		//confirm challenge and store in flash
    }
    if(theConf.traceflag & (1<<WEBD))
    	printf("WebServer Started\n");
    vTaskDelete(NULL);
}


