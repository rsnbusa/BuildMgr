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
static esp_err_t conn_setup_get_handler(httpd_req_t *req);
static esp_err_t conn_sendsetup_handler(httpd_req_t *req);

extern void delay(uint32_t a);
extern void write_to_flash();

static const httpd_uri_t setup = {
    .uri       = "/cmsetup",
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
        }
        free(buf);
    }

    if(setupHost[cualm].bpkwh>0 && setupHost[cualm].startKwh>0 && strlen(setupHost[cualm].meterid)>0)
    {
		time(&now);
		localtime_r(&now, &timeinfo);
		strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    	setupHost[cualm].valid=true;
    	theConf.configured[cualm]=1; //in transit mode
    	sprintf(tempb,"[%s]Meter:%d Id=%s kWh=%d BPK=%d ",strftime_buf,cualm,setupHost[cualm].meterid,setupHost[cualm].startKwh,setupHost[cualm].bpkwh);
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
    .uri       = "/cmchallenge",
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
    bool rebf=false;

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
							strcpy(theConf.medidor_id[a],setupHost[a].meterid);
							time((time_t*)&theConf.bornDate[a]);
							theConf.beatsPerKw[a]=setupHost[a].bpkwh;
							theConf.bornKwh[a]=setupHost[a].startKwh;
							theConf.configured[a]=3;						//final status configured
							fram.formatMeter(a);
							fram.write_lifekwh(a,setupHost[a].startKwh);	// write to Fram. Its beginning of life KWH
							printf("Writing life Meter %d= %d\n",a,setupHost[a].startKwh);
						}
						else
							theConf.configured[a]=0; //reset it
					}
					theConf.active=1;				// theConf is now ACTIVE and Certified
					write_to_flash();
					memset(&setupHost,0,sizeof(setupHost));
					sprintf(tempb,"[%s]Meters were saved permanently...rebooting in 3 secs",strftime_buf);
					rebf=true;
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

	if(rebf)
	{
		delay(3000);
		esp_restart();
	}

    return ESP_OK;
}

static const httpd_uri_t setupend = {
    .uri       = "/cmsetupend",
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

static const httpd_uri_t csetup = {
    .uri       = "/csetup",
    .method    = HTTP_GET,
    .handler   = conn_setup_get_handler,
	.user_ctx	= NULL
};

static esp_err_t conn_setup_get_handler(httpd_req_t *req)
{
		char*  buf;
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
	            if (httpd_query_key_value(buf, "conn", param, sizeof(param)) == ESP_OK)
	            	strcpy(theConf.meterConnName,param);

	            if (httpd_query_key_value(buf, "slot", param, sizeof(param)) == ESP_OK)
	            	theConf.slot_Server.slot_time=atoi(param);

	            if (httpd_query_key_value(buf, "server", param, sizeof(param)) == ESP_OK)
	            	theConf.slot_Server.server_num=atoi(param);

	            if (httpd_query_key_value(buf, "AltDay", param, sizeof(param)) == ESP_OK)
	            	theConf.connId.altDay=atoi(param);

	            if (httpd_query_key_value(buf, "DDay", param, sizeof(param)) == ESP_OK)
	            	theConf.connId.dDay=atoi(param);

	            if (httpd_query_key_value(buf, "SlotSlice", param, sizeof(param)) == ESP_OK)
	            	theConf.connId.connSlot=atoi(param);
	        }
	        free(buf);
	    }

	    if(1)
	    {
			time(&now);
			localtime_r(&now, &timeinfo);
			strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	    	sprintf(tempb,"[%s]Conn:%s SlotDuration=%d MqttServer=%d AltDay=%d DDAY %d Slice=%d",strftime_buf,theConf.meterConnName,theConf.slot_Server.slot_time,
	    			theConf.slot_Server.server_num,theConf.connId.altDay,theConf.connId.dDay,theConf.connId.connSlot);
	    	write_to_flash();
	    }
	    else
	    {

	        sprintf(tempb,"[%s]Invalid parameters",strftime_buf);
	    }

		httpd_resp_send(req, tempb, strlen(tempb));

	    return ESP_OK;
}

static const httpd_uri_t rsetup = {
    .uri       = "/cgetsetup",
    .method    = HTTP_GET,
    .handler   = conn_sendsetup_handler,
	.user_ctx	= NULL
};

static esp_err_t conn_sendsetup_handler(httpd_req_t *req)
{
	    time_t now;
	    struct tm timeinfo;
	    char strftime_buf[64];


		time(&now);
		localtime_r(&now, &timeinfo);
		strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
		sprintf(tempb,"[%s]Setup is Conn:%s SlotDuration=%d MqttServer=%d AltDay=%d DDAY %d Slice=%d",strftime_buf,theConf.meterConnName,theConf.slot_Server.slot_time,
				theConf.slot_Server.server_num,theConf.connId.altDay,theConf.connId.dDay,theConf.connId.connSlot);

		httpd_resp_send(req, tempb, strlen(tempb));

	    return ESP_OK;
}


void start_webserver(void *pArg)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if(theConf.traceflag & (1<<WEBD))
    	printf("%sStarting server on port:%d\n",WEBDT, config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        httpd_register_uri_handler(server, &setup); 		//setup up to 5 meters
        httpd_register_uri_handler(server, &setupend);		//end setup and send challenge
        httpd_register_uri_handler(server, &csetup);		//connmgr setup
        httpd_register_uri_handler(server, &rsetup);		//send connmgr setup
        httpd_register_uri_handler(server, &challenge);		//confirm challenge and store in flash
    }
    if(theConf.traceflag & (1<<WEBD))
    	printf("WebServer Started\n");
    vTaskDelete(NULL);
}


