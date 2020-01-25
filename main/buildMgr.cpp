#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

//forward declarations even externals works without extern. Linker will resolve
static void submode(void * pArg);
void kbd(void *pArg);
static void update_mac(u32 newmac);
void write_to_fram(u8 meter,bool addit);
void testMqtt();
static void sendTelemetry();
void start_webserver(void* pArg);
static void reserveSlot(parg* argument);
void write_to_flash();
void watchDog(void* pArg);

#define OTA_URL_SIZE 1024
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
//extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

#ifdef DISPLAY
void displayManager(void * pArg);
void drawString(int x, int y, string que, int fsize, int align,displayType showit,overType erase);
#endif

static const char *TAG = "BDMGR";

static EventGroupHandle_t 	wifi_event_group;
const static int MQTT_BIT 	= BIT0;
const static int WIFI_BIT 	= BIT1;
const static int PUB_BIT 	= BIT2;
const static int DONE_BIT 	= BIT3;
const static int SNTP_BIT 	= BIT4;

static int find_mac(uint32_t esteMac)
{
	for (int a=0;a<theConf.reservedCnt;a++)
	{
		if(theConf.reservedMacs[a]==esteMac)
			return a;
	}
	return -1;
}

static int find_ip(uint32_t esteIp)
{
	for (int a=0;a<theConf.reservedCnt;a++)
	{
		if(losMacs[a].theIp==esteIp)
			return a;
	}
	return -1;
}


static int findCommand(string cual)
{
	for (int a=0;a<MAXCMDS;a++)
	{
		if(cual==string(cmds[a].comando))
			return a;
	}
	return -1;
}

uint32_t  millisISR()
{
	return xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;

}

uint32_t millis()
{
	return xTaskGetTickCount() * portTICK_PERIOD_MS;

}

void delay(uint32_t a)
{
	vTaskDelay(a /  portTICK_RATE_MS);
}


void hourChange()
{
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		printf("%sHour change Old %d New %d\n",CMDDT,oldHorag,horag);
#endif
//	return;


	for (int a=0;a<MAXDEVS;a++)
	{
#ifdef DEBUGX
		if(theConf.traceflag & (1<<CMDD))
			printf("%sHour change meter %d val %d\n",CMDDT,a,theMeters[a].curHour);
#endif
		if(framSem)
		{
			if(xSemaphoreTake(framSem, portMAX_DELAY))
			{
				fram.write_hour(a, oldYearDay,oldHorag, theMeters[a].curHour);//write old one before init new
				fram.write_hourraw(a, oldYearDay,oldHorag, theMeters[a].curHourRaw);//write old one before init new
				xSemaphoreGive(framSem);
			}

			theMeters[a].curHour=0; //init it
			theMeters[a].curHourRaw=0;
			u16 oldt=tarifasDia[oldHorag];

			if (diag !=oldDiag)
			{
				int err=fram.read_tarif_day(yearDay,(u8*)&tarifasDia);
				if(err!=0)
					printf("Error reading tarifadia %d...using same\n",yearDay);
			}

			// calculate remaining Beats to convert to next Tariff IF different
			if(oldt!=tarifasDia[horag])
			{
				u16 oldBeats=theMeters[a].beatsPerkW*tarifasDia[oldHorag]/100;
				float oldTarRemain=(float)(theMeters[a].beatSave-oldBeats);
				float perc=(float)oldTarRemain/(float)oldBeats;
				theMeters[a].beatSave=(int)(perc*(float)(theMeters[a].beatsPerkW*tarifasDia[horag]/100));
			} //else keep counting in the same fashion
			oldHorag=horag;
		}
	}
}

void dayChange()
{
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		printf("%sDay change Old %d New %d\n",CMDDT,oldDiag,diag);
#endif

	if(framSem)
	{
		for (int a=0;a<MAXDEVS;a++)
		{
	#ifdef DEBUGX
			if(theConf.traceflag & (1<<CMDD))
				printf("%sDay change mes %d day %d oldday %d\n",CMDDT,oldMesg,diag,oldDiag);
	#endif
			if(xSemaphoreTake(framSem, portMAX_DELAY))
			{
				fram. write_day(a,oldYearDay, theMeters[a].curDay);
				theMeters[a].curDay=0;
				xSemaphoreGive(framSem);
			}
		}
	}

	oldDiag=diag;
	oldYearDay=yearDay;
}

void monthChange()
{
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		printf("%sMonth change Old %d New %d\n",CMDDT,oldMesg,mesg);
#endif

	if(framSem)
	{
		for (int a=0;a<MAXDEVS;a++)
		{
			if(xSemaphoreTake(framSem, portMAX_DELAY))
			{
				fram.write_month(a, oldMesg, theMeters[a].curMonth);
				xSemaphoreGive(framSem);
				theMeters[a].curMonth=0;
			}
		}
	}
	oldMesg=mesg;
	oldYearDay=yearDay;
}

void check_date_change()
{
	time_t now;
	struct tm timep;

	time(&now);
	localtime_r(&now,&timep);
	mesg=timep.tm_mon;   // Global Month
	diag=timep.tm_mday-1;    // Global Day
	yearg=timep.tm_year;     // Global Year
	horag=timep.tm_hour;     // Global Hour
	yearDay=timep.tm_yday;

#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		printf("%sHour change mes %d- %d day %d- %d hora %d- %d Min %d Sec %d dYear %d\n",CMDDT,mesg,oldMesg,diag,oldDiag,horag,oldHorag,
				timep.tm_min,timep.tm_sec,yearDay);
#endif

	if(horag==oldHorag && diag==oldDiag && mesg==oldMesg)
		return;

	if(horag!=oldHorag) // hour change up or down(day change)
		hourChange();

	if(diag!=oldDiag) // day change up or down(month change). Also hour MUST HAVE CHANGED before
		dayChange();

	if(mesg!=oldMesg) // month change up or down(year change). What to do with prev Year???? MONTH MUST HAVE CHANGED
		monthChange();
}

static void ConfigSystem(void *pArg)
{
	uint32_t del=(uint32_t)pArg;
	while(1)
	{
		gpio_set_level((gpio_num_t)WIFILED, 1);
		delay(del);
		gpio_set_level((gpio_num_t)WIFILED, 0);
		delay(del);
	}
}

void firmUpdate(void *pArg)
{
	esp_http_client_config_t config;
	memset(&config,0,sizeof(config));

//	config.url = OTA_BIN_FILE;
	config.url = "http://192.168.100.7:8080/buildMgrOled.bin";
	config .cert_pem = (char *)server_cert_pem_start;
	config.event_handler = NULL;

	config.skip_cert_common_name_check = true;	//for testing only

	printf("Ota begin\n");

	xTaskCreate(&ConfigSystem, "cfg", 1024, (void*)50, 3, &blinkHandle);// blink to show ota process is active

	esp_err_t ret = esp_https_ota(&config);
	if (ret == ESP_OK) {
		printf("Ota ended OK\n");
		esp_restart();
	}
	else
	{
		printf("Firmware failed\n");
		if(blinkHandle)
			vTaskDelete(blinkHandle);
		gpio_set_level((gpio_num_t)WIFILED, 0);
	}

	vTaskDelete(NULL);
}

static esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            break;
        case HTTP_EVENT_ON_CONNECTED:
            break;
        case HTTP_EVENT_HEADER_SENT:
            break;
        case HTTP_EVENT_ON_HEADER:
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
            	int *suma=(int*)evt->user_data;
            	fram.writeMany(TARIFADIA+ *suma,(u8*)evt->data,evt->data_len);		//save data directly to fram
            	*suma +=  evt->data_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            break;
        case HTTP_EVENT_DISCONNECTED:
            break;
    }
    return ESP_OK;
}

void sendTelemetryCmd(parg *pArg)
{
	meterType dummy;

#ifdef DEBUGX
	if(theConf.traceflag & (1<<HOSTD))
		printf("%sHost requested telemetry readings\n",HOSTDT);
#endif
	dummy.code=sendTelemetry;
	dummy.saveit=true;
	if(mqttQ)
		xQueueSend( mqttQ,&dummy,0 );
}

void loadit(parg *pArg)
{
	char	textl[80];
	parg 	*elcmd=(parg*)pArg;
	int 	tariff=0;

	int vanl=0;
	esp_http_client_config_t lconfig;

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
		printf("%sLoading tariffs %s\n",HOSTDT,lconfig.url);
#endif

	lconfig.event_handler = _http_event_handle;							//in charge of saving received data to Fram directly

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
						printf("%sStatus = %d, content_length = %d\n",HOSTDT,
							esp_http_client_get_status_code(client),
							esp_http_client_get_content_length(client));
	#endif
					if(esp_http_client_get_status_code(client)!=200)
					{
						if(theConf.traceflag & (1<<HOSTD))
							printf("%sFailed to download Tariff Err %x\n",HOSTDT,esp_http_client_get_status_code(client));
						esp_http_client_cleanup(client);
						return;
					}
				}
				else
				{
	#ifdef DEBUGX
					if(theConf.traceflag & (1<<HOSTD))
						printf("%sFailed to download Tariff Err %x\n",HOSTDT,err);
					esp_http_client_cleanup(client);
					return;
	#endif
				}
				// all is well, cleanup and read back to our working array
				esp_http_client_cleanup(client);

				err=fram.read_tarif_day(yearDay,(u8*)&tarifasDia);
				xSemaphoreGive(framSem);
				if(err!=0)
					printf("%sLoadTariff Error %d reading Tariffs day %d...recovering from Host\n",HOSTDT,err,yearDay);
			}
	}
	else
	{
#ifdef DEBUGX
		if(theConf.traceflag & (1<<HOSTD))
			printf("%sFailed to start HTTP Client\n",HOSTDT);
#endif
	}

#ifdef DEBUGX
	if(theConf.traceflag & (1<<HOSTD))
		printf("%sLoading tariffs successful\n",HOSTDT);
#endif
}

#ifdef TEMP
float DS_get_temp(DS18B20_Info * cual)
{
    ds18b20_convert_all(owb);
    ds18b20_wait_for_conversion(ds18b20_info);
    float dtemp;
    DS18B20_ERROR err= ds18b20_read_temp(cual, &dtemp);
    if(err)
    	return -1;
    else
    return dtemp;
}


static void init_temp()
{

	  owb = owb_rmt_initialize(&rmt_driver_info, DSPIN, RMT_CHANNEL_1, RMT_CHANNEL_0);
	  owb_use_crc(owb, true);  // enable CRC check for ROM code

	  numsensors = 0;
	  OneWireBus_ROMCode rom_code;
	  owb_status status = owb_read_rom(owb, &rom_code);
	  if (status == OWB_STATUS_OK)
	  {
		char rom_code_s[OWB_ROM_CODE_STRING_LENGTH];
		owb_string_from_rom_code(rom_code, rom_code_s, sizeof(rom_code_s));
//		printf("Single device %s present\n", rom_code_s);
		ds18b20_info = ds18b20_malloc();
		ds18b20_init_solo(ds18b20_info, owb);
		ds18b20_use_crc(ds18b20_info, true);           // enable CRC check for temperature readings
		ds18b20_set_resolution(ds18b20_info, DS18B20_RESOLUTION_12_BIT);
	  }
	  float tt=DS_get_temp(ds18b20_info);
	  printf("Temp %.1f\n",tt);
	  numsensors=1;


}
#endif


static void pcnt_intr_handler(void *arg)
{
    uint32_t intr_status = PCNT.int_st.val;
    pcnt_evt_t evt;
    portBASE_TYPE HPTaskAwoken = pdFALSE;

    for (int i = 0; i < PCNT_UNIT_MAX+1; i++)
    {
        if (intr_status & (BIT(i)))
        {
            evt.unit = i;
            evt.status = PCNT.status_unit[i].val;
            PCNT.int_clr.val = BIT(i);

            xQueueSendFromISR(pcnt_evt_queue, &evt, &HPTaskAwoken);
            if (HPTaskAwoken == pdTRUE)
                portYIELD_FROM_ISR();
        }
    }
}

static void pcnt_basic_init(uint16_t who, uint16_t th1)
{
	pcnt_config_t pcnt_config;

	memset((void*)&pcnt_config,0,sizeof(pcnt_config));

	pcnt_config.ctrl_gpio_num 	= -1;
	pcnt_config .channel 		= PCNT_CHANNEL_0;
	pcnt_config .pos_mode 		= PCNT_COUNT_INC;   // Count up on the positive edge
	pcnt_config.neg_mode 		= PCNT_COUNT_DIS;   // not used
	pcnt_config.lctrl_mode 		= PCNT_MODE_KEEP; 	// not used
	pcnt_config .hctrl_mode 	= PCNT_MODE_KEEP;	// not used

	pcnt_config.pulse_gpio_num = theMeters[who].pin;
	pcnt_config.unit = (pcnt_unit_t)who;
	pcnt_unit_config(&pcnt_config);

	pcnt_event_disable((pcnt_unit_t)who, PCNT_EVT_H_LIM);
	pcnt_event_disable((pcnt_unit_t)who, PCNT_EVT_L_LIM);
	pcnt_event_disable((pcnt_unit_t)who, PCNT_EVT_ZERO);

	pcnt_set_filter_value((pcnt_unit_t)who, 1000);
	pcnt_filter_enable((pcnt_unit_t)who);

	pcnt_set_event_value((pcnt_unit_t)who, PCNT_EVT_THRES_1, th1);
	pcnt_event_enable((pcnt_unit_t)who, PCNT_EVT_THRES_1);

	pcnt_counter_pause((pcnt_unit_t)who);
	pcnt_counter_clear((pcnt_unit_t)who);
	pcnt_intr_enable((pcnt_unit_t)who);
	pcnt_counter_resume((pcnt_unit_t)who);
}

static void pcnt_init(void)
{
    pcnt_isr_handle_t user_isr_handle = NULL; //user's ISR service handle

	theMeters[0].pin=METER0;
	theMeters[1].pin=METER1;
	theMeters[2].pin=METER2;
	theMeters[3].pin=METER3;
	theMeters[4].pin=METER4;

	theMeters[0].pinB=BREAK0;
	theMeters[1].pinB=BREAK1;
	theMeters[2].pinB=BREAK2;
	theMeters[3].pinB=BREAK3;
	theMeters[4].pinB=BREAK4;

    pcnt_isr_register(pcnt_intr_handler, NULL, 0,&user_isr_handle);
    workingDevs=0;

	for(int a=0;a<MAXDEVS;a++)
	{
		if(theMeters[a].beatsPerkW>0) //only if configured.
		{
			theMeters[a].pos=a;
			pcnt_basic_init(a,10);
			workingDevs++;
		}
	}
	if (!workingDevs)
	{
		if(xSemaphoreTake(I2CSem, portMAX_DELAY))
			{
				drawString(64,32,"NO METERS",16,TEXT_ALIGN_CENTER,DISPLAYIT,NOREP);
				xSemaphoreGive(I2CSem);
			}
	}
}

static void getMessage(void *pArg)
{
    int len;
    cmdType comando;
    task_param *theP=(task_param*)pArg;
    char s1[10];
    parg argument;


    int sock =theP->sock_p;
    int pos=theP->pos_p;
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
	{
		printf("%sStarting GetM Fd %d Pos %d GlobaCount %d Macf %d\n",CMDDT,sock,pos,globalSocks,theP->macf);
		printf("%sGetm%d heap %d\n",CMDDT,sock,esp_get_free_heap_size());
	}
#endif

		while(true)
		{
			do {
				comando.mensaje=(char*)malloc(MAXBUFFER);
				len = recv(sock, comando.mensaje, MAXBUFFER-1, 0);
				if (len < 0) {
#ifdef DEBUGX
					if(theConf.traceflag & (1<<CMDD))
						printf("%sError occurred during receiving: errno %d fd: %d Pos %d\n",CMDDT, errno,sock,pos);
#endif

					goto exit;
				} else if (len == 0) {
#ifdef DEBUGX
					if(theConf.traceflag & (1<<CMDD))
						printf("%sConnection closed: errno %d \n", CMDDT,errno);
#endif
				   goto exit;
				} else {
					// check special reserve msg
					if(theP->macf<0) //no mac found in buildmgr
					{
						if(strstr(comando.mensaje,"rsvp")==NULL)
						{
							if(theConf.traceflag & (1<<CMDD))
								printf("%sTask Killed No MAC \n", CMDDT);
							goto exit;
						}
						else
						{
							*(comando.mensaje+len)=0;
							// reserve cmd rx. Process it without queue
							// msg struct simple = rsvp123456789 rsvp=key numbers are macn in chars
							memcpy(s1,comando.mensaje+4,len-4);
							argument.macn=atof(s1);
							argument.pComm=sock;
							reserveSlot(&argument);
							goto exit; //done
						}
					}

			   		whitelist[pos].dState=MSGSTATE;
			    	whitelist[pos].seen=millis();
			    	time(&whitelist[pos].ddate);

					llevoMsg++;
					comando.mensaje[len] = 0;
					comando.pos=pos;
					comando.fd=sock;
					if(mqttR)
						xQueueSend(mqttR,&comando,0);

					//break; //if break, will not allow for stream of multiple messages. Must not break. Close or Timeout closes socket
				}
			} while (len > 0);
		}
	exit:
	losMacs[pos].theHandle=NULL;
	shutdown(sock, 0);
	close(sock);
	if(comando.mensaje)
		free(comando.mensaje);
	globalSocks--;
	if (globalSocks<0)
		globalSocks=0;
	vTaskDelete(NULL);
}

static void buildMgr(void *pvParameters)
{
    char 						addr_str[50];
    int 						addr_family;
    int 						ip_protocol;
    int 						sock=0;
    struct sockaddr_in 			source_addr;
    uint 						addr_len = sizeof(source_addr);
    struct sockaddr_in 			dest_addr;
    char						tt[20];
    task_param					theP;

    while (true)
    {
		dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		dest_addr.sin_family = AF_INET;
		dest_addr.sin_port = htons(BUILDMGRPORT);
		addr_family = AF_INET;
		ip_protocol = IPPROTO_IP;
		inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);


		int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
		if (listen_sock < 0) {
			ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
			break;
		}

		int opt = 1;
		if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt))<0)
		{
			ESP_LOGE(TAG, "ReuseError: errno %d", errno);
			perror("setsockopt");
		}

		int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
		if (err != 0) {
			ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
			break;
		}

		err = listen(listen_sock, 10);
		if (err != 0) {
			ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
			break;
		}

		while (true)
		{
			sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
			if (sock < 0)
			{
				ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
				break;
			}

			struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&source_addr;
			struct in_addr ipAddr = pV4Addr->sin_addr;

			char str[INET_ADDRSTRLEN];
			char str2[INET_ADDRSTRLEN];
			inet_ntop( AF_INET, &ipAddr, str, INET_ADDRSTRLEN );
			inet_ntop( AF_INET,(in_addr*)&losMacs[0].theIp, str2, INET_ADDRSTRLEN );

			sprintf(tt,"getm%d",sock);		//launch a task with fd number as trailer
			int  a,b;
	//		memcpy((void*)&a,(void*)&losMacs[0].theIp,4);
			memcpy((void*)&b,(void*)&ipAddr,4);

			a=find_ip(b);
			if(a<0)
			{
	#ifdef DEBUGX
				if(theConf.traceflag & (1<<CMDD))
					printf("%sInternal error no Ip %x\n",CMDDT,b);
	#endif
			}
			else
				losMacs[a].theSock=sock;

			theP.pos_p=a;
			theP.sock_p=sock;
			theP.macf=a;

			xTaskCreate(&getMessage,tt,GETMT,(void*)&theP, 4, &losMacs[a].theHandle);
			globalSocks++;
        }
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		printf("%sBuildmgr listend error %d\n",CMDDT,errno);
#endif
    }

    char them[6];
    printf("BuildMgr will die OMG. GlobalCount %d VanMacs %d\n",globalSocks,vanMacs);
	for (int a=0;a<vanMacs;a++)
	{
		memcpy(&them,&losMacs[a].macAdd,4);
		printf("Mac[%d][%d]:%.2x:%.2x:%.2x:%.2x Ip:%s (%s)\n",a,losMacs[a].trans[0],
		them[0],them[1],them[2],them[3],(char*)ip4addr_ntoa((ip4_addr_t *)&losMacs[a].theIp),ctime(&losMacs[a].lastUpdate));
	}
	esp_restart();// no other option
    vTaskDelete(NULL);
}



static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
	u32 newmac;
	int estem;

	wifi_event_ap_staconnected_t *ev=(wifi_event_ap_staconnected_t*)event_data;
    switch (event_id) {

    case  WIFI_EVENT_AP_STACONNECTED:
#ifdef DEBUGX
		if(theConf.traceflag & (1<<WIFID))
			printf("%sWIFIMac connected %02x:%02x:%02x:%02x:%02x:%02x\n",WIFIDT,ev->mac[0],ev->mac[1],ev->mac[2],ev->mac[3],ev->mac[4],ev->mac[5]);
#endif
    	memcpy((void*)&newmac,&ev->mac[2],4);
    	estem=find_mac(newmac);
    	if(estem>=0)
    	{
    		whitelist[estem].dState=CONNSTATE;
    		whitelist[estem].seen=millis();
    		time(&whitelist[estem].ddate);
    	}
    //	update_mac(newmac);
    	break;
	case WIFI_EVENT_AP_STADISCONNECTED:
    	printf("Mac disco %02x:%02x:%02x:%02x:%02x:%02x\n",ev->mac[0],ev->mac[1],ev->mac[2],ev->mac[3],ev->mac[4],ev->mac[5]);
		memcpy((void*)&newmac,&ev->mac[2],4);
		estem=find_mac(newmac);
		if(estem>=0)
		{
			whitelist[estem].dState=BOOTSTATE;
			whitelist[estem].seen=millis();
			time(&whitelist[estem].ddate);
			//a controller is dying should report event to Host or keep an eye on it before sending msg
		}
		break;
    case WIFI_EVENT_AP_START:
        	break;
	case WIFI_EVENT_STA_STOP:
    	gpio_set_level((gpio_num_t)WIFILED, 0);		//indicate WIFI is inactive
    	break;
	case WIFI_EVENT_STA_START:
		if(!miscanf)
			esp_wifi_connect();
		wifif=false;
		break;
	case WIFI_EVENT_STA_DISCONNECTED:
    	gpio_set_level((gpio_num_t)WIFILED, 0);		//indicate WIFI is inactive
		if(!miscanf)
			esp_wifi_connect();
		wifif=false;
		xEventGroupClearBits(wifi_event_group, WIFI_BIT);
		break;
	default:
		break;
    }
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

static void updateDateTime(loginT loginData)
{
    struct tm timeinfo;
    return;
	localtime_r(&loginData.thedate, &timeinfo);
	diaHoraTarifa=loginData.theTariff;// Host will give us Hourly Tariff. No need to store

	oldMesg=mesg=timeinfo.tm_mon;
	oldDiag=diag=timeinfo.tm_mday-1;
	yearg=timeinfo.tm_year+1900;
	oldHorag=horag=timeinfo.tm_hour;
	oldYearDay=yearDay=timeinfo.tm_yday;
	struct timeval now = { .tv_sec = loginData.thedate, .tv_usec=0};
	settimeofday(&now, NULL);
}

static void sntpget(void *pArgs)
{
    char strftime_buf[64];
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
  //      printf("Time is not set yet. Connecting to WiFi and getting time over NTP.");

	initialize_sntp();
	struct tm timeinfo;
	memset(&timeinfo,0,sizeof(timeinfo));
	int retry = 0;
	const int retry_count = 30;
	while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
	if(retry>retry_count)
	{
		printf("SNTP failed\n");
	    vTaskDelete(NULL);
	}
	time(&now);
	setenv("TZ", LOCALTIME, 1);
	tzset();
	localtime_r(&now, &timeinfo);

	oldMesg=mesg=timeinfo.tm_mon;   // Global Month
	oldDiag=diag=timeinfo.tm_mday-1;    // Global Day
	yearg=timeinfo.tm_year;     // Global Year
	oldHorag=horag=timeinfo.tm_hour;     // Global Hour
	oldYearDay=yearDay=timeinfo.tm_yday;

	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		printf("%s[CMDD]The current date/time in %s is: %s day of Year %d\n",CMDDT, LOCALTIME,strftime_buf,timeinfo.tm_yday);
#endif

    }
    xEventGroupSetBits(wifi_event_group, SNTP_BIT);
	gpio_set_level((gpio_num_t)WIFILED, 1);		//indicate WIFI is active with IP and SNTP
    vTaskDelete(NULL);
}

static void update_ip()
{
	wifi_sta_list_t wifi_sta_list;
	tcpip_adapter_sta_list_t adapter_sta_list;
	tcpip_adapter_sta_info_t station;

	memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
	memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));

	esp_wifi_ap_get_sta_list(&wifi_sta_list);
	tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);

	for (int i = 0; i < adapter_sta_list.num; i++)
	{
		station = adapter_sta_list.sta[i];
#ifdef DEBUGX
		if(theConf.traceflag & (1<<CMDD))
		{
			printf("[%sstation[%d] MAC ",CMDDT,i);
			for(int i = 0; i< 6; i++)
			{
				printf("%02X", station.mac[i]);
				if(i<5)printf(":");
			}
		}
#endif
		u32 bigmac;
		memcpy(&bigmac,&station.mac[2],4);
		int este=find_mac(bigmac);
		if(este<0)
		{
#ifdef DEBUGX
			if(theConf.traceflag & (1<<CMDD))
				printf("%sNot registered mac %x not found \n",CMDDT,bigmac);
#endif

		}
		else
			memcpy((void*)&losMacs[este].theIp,(void*)&station.ip,4);
	}
}

static void close_mac(int cual)
{
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		printf("%sClosing[%d] FD %d\n",CMDDT,cual,losMacs[cual].theSock);
#endif
	if(losMacs[cual].theHandle)
		vTaskDelete(losMacs[cual].theHandle);// kill the task for rx
	if(losMacs[cual].theSock>=3)
		close(losMacs[cual].theSock); //close the socket
	if(losMacs[cual].timerH)			// delete TimerControl
	{
		xTimerStop(losMacs[cual].timerH,0);
		xTimerDelete(losMacs[cual].timerH,0);
		losMacs[cual].timerH=NULL;
	}

	if(cual==vanMacs-1)
	{
		memset((void*)&losMacs[cual],0,sizeof(losMacs[cual]));
		vanMacs--;
		if (vanMacs<0)//just in case
			vanMacs=0;
		return;
	}
	memmove(&losMacs[cual],&losMacs[cual+1],(vanMacs-cual-1)*sizeof(losMacs[0]));
	vanMacs--;
	if (vanMacs<0)//just in case
		vanMacs=0;
}

static void update_mac(u32 newmac)
{
	int este;

#ifdef DEBUGX

	if(theConf.traceflag & (1<<CMDD))
		printf("%supdate mac %x\n",CMDDT,newmac);
#endif

	do { //in case multiple instances which SHOULDNT happend but....
	 este=find_mac(newmac);
 	 if(este>=0)
 		close_mac(este);
	} while(este>=0);

	// check if this mac has reserved a slot else do no put it in the Active Macs

	este=find_mac(newmac);
	if(este<0)
	{
		if(theConf.traceflag & (1<<CMDD))
				printf("%sRejecting MAC %04x\n",CMDDT,newmac);
		return; //not in our reserved list
	}

	losMacs[vanMacs].macAdd=newmac;
	time(&losMacs[vanMacs].lastUpdate);
	vanMacs++;


	memset((void*)&losMacs[este],0,sizeof(losMacs[0]));
	losMacs[este].theIp=0;
	losMacs[este].theHandle=NULL;
	losMacs[este].theSock=-1;
	losMacs[este].macAdd=newmac;
}

static void ip_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
	 wifi_config_t wifi_config;
	 ip_event_got_ip_t *ev=(ip_event_got_ip_t*)event_data;
	 ip_event_ap_staipassigned_t *evip=(ip_event_ap_staipassigned_t*)event_data;

	    memset(&wifi_config,0,sizeof(wifi_config));//very important

    switch (event_id) {
    case IP_EVENT_AP_STAIPASSIGNED:
  //  	printf("\nAP IP Assigned:" IPSTR "\n", IP2STR(&evip->ip));
    	update_ip();
    	break;
        case IP_EVENT_STA_GOT_IP:
        	printf("\nIP Assigned:" IPSTR "\n", IP2STR(&ev->ip_info.ip));
        	wifif=true;
            xEventGroupSetBits(wifi_event_group, WIFI_BIT);
        	xTaskCreate(&sntpget,"sntp",2048,NULL, 4, NULL);
            break;
        case IP_EVENT_STA_LOST_IP:
    		gpio_set_level((gpio_num_t)WIFILED, 0);		//indicate WIFI is active with IP
    		break;
        default:
            break;
    }
    return;
}

static void wifi_init(void)
{
	wifi_init_config_t 				cfg=WIFI_INIT_CONFIG_DEFAULT();

	//	wifi_config_t 					sta_config,configap;
	//	tcpip_adapter_ip_info_t 		ipInfo;
	//	tcpip_adapter_init();

    esp_netif_init();

    //if imperative to change default 192.168.4.1 to anything else use below. Careful with esp_net_if deprecation warnings
//	memset(&ipInfo,0,sizeof(ipInfo));
//	//set IP Address of AP for Statiosn DHCP
//	ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
//	IP4_ADDR(&ipInfo.ip, 192,168,19,1);
//	IP4_ADDR(&ipInfo.gw, 192,168,19,1);
//	IP4_ADDR(&ipInfo.netmask, 255,255,255,0);
//	ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &ipInfo));
//	ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));

    wifi_event_group = xEventGroupCreate();
    xEventGroupClearBits(wifi_event_group, WIFI_BIT);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &ip_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    wifi_config_t wifi_config;

    //AP section
    memset(&wifi_config,0,sizeof(wifi_config));//very important
    sprintf(tempb,"CmgrIoT%04x",theMacNum);
    strcpy((char*)wifi_config.ap.ssid,tempb);
    strcpy((char*)wifi_config.ap.password,tempb);
	wifi_config.ap.authmode=WIFI_AUTH_WPA_PSK;
	if(usedMacs<MAXSTA)
		wifi_config.ap.ssid_hidden=false;
	else
		wifi_config.ap.ssid_hidden=true;

	wifi_config.ap.beacon_interval=400;
	wifi_config.ap.max_connection=50;
	wifi_config.ap.ssid_len=0;
	wifi_config.ap.channel=1;
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));

	//STA section
	memset(&wifi_config,0,sizeof(wifi_config));//very important, do it again for sta else corrupted for ESP_IF_WIFI_STA
	strcpy((char*)wifi_config.sta.ssid,INTERNET);
	strcpy((char*)wifi_config.sta.password,INTERNETPASS);

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    xEventGroupWaitBits(wifi_event_group, WIFI_BIT, false, true, portMAX_DELAY);
}

esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            esp_mqtt_client_subscribe(client, cmdQueue.c_str(), 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            xEventGroupClearBits(wifi_event_group, MQTT_BIT);
            break;
        case MQTT_EVENT_SUBSCRIBED:
            xEventGroupSetBits(wifi_event_group, MQTT_BIT);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            xEventGroupSetBits(wifi_event_group, PUB_BIT|DONE_BIT);//message sent bit
            break;
        case MQTT_EVENT_PUBLISHED:
#ifdef DEBUGX
        	if(theConf.traceflag & (1<<MQTTD))
            	printf("%sPublished\n",MQTTDT);
#endif
            esp_mqtt_client_unsubscribe(client, cmdQueue.c_str());//bit is set by unsubscribe
            break;
        case MQTT_EVENT_DATA:
#ifdef DEBUGX
       	if(theConf.traceflag & (1<<MQTTD))
           {
        	   printf("%sTOPIC=%.*s\r\n",MQTTDT,event->topic_len, event->topic);
        	   printf("%sDATA=%.*s\r\n", MQTTDT,event->data_len, event->data);
           }
#endif
           theCmd.mensaje=(char*)malloc(MAXBUFFER);
            if(event->data_len)// 0 will be the retained msg being erased
            {
            	memcpy(theCmd.mensaje,event->data,event->data_len);
            	theCmd.mensaje[event->data_len]=0;
            	xQueueSend( mqttR,&theCmd,0 );							//memory will be freed down the line by cmdManager
            	esp_mqtt_client_publish(clientCloud, cmdQueue.c_str(), "", 0, 0,1);//delete retained
            }
            break;
        case MQTT_EVENT_ERROR:
           xEventGroupClearBits(wifi_event_group, MQTT_BIT);
            break;
        case MQTT_EVENT_BEFORE_CONNECT:
            xEventGroupClearBits(wifi_event_group, MQTT_BIT|DONE_BIT);
        	break;
        default:
#ifdef DEBUGX
        	if(theConf.traceflag & (1<<MQTTD))
        		printf("%sEvent %d\n",MQTTDT,event->event_id);
#endif
            break;
    }
    return ESP_OK;
}

static void submode(void * pArg)
{
	meterType meter;
	u32 timetodelivery=0;
	while(true)
	{
		if(wifif) // is the network up?
		{
			if( xQueueReceive( mqttQ, &meter, portMAX_DELAY ))
			{
#ifdef DEBUGX
				if(theConf.traceflag & (1<<CMDD))
				{
					printf("%sHeap after submode rx %d\n",CMDDT,esp_get_free_heap_size());
					printf("%sReady to send for Pin %d Pos %d Qwaiting=%d\n",CMDDT,meter.pin,meter.pos,uxQueueMessagesWaiting( mqttQ ));
				}
#endif
				if(meter.saveit) //used as a Send cmd
				{
					timetodelivery=millis();
					if(!clientCloud) //just in case
							printf("Error no client mqtt\n");
					else
					{
#ifdef DEBUGX
						if(theConf.traceflag & (1<<CMDD))
							printf("%sStarting mqtt\n",CMDDT);
#endif
						xEventGroupClearBits(wifi_event_group, MQTT_BIT);//clear flag to wait on
						if(esp_mqtt_client_start(clientCloud)==ESP_OK) //we got an OK
						{
								// when connected send the message
							//wait for the CONNECT mqtt msg
							if(xEventGroupWaitBits(wifi_event_group, MQTT_BIT, false, true, 4000/  portTICK_RATE_MS))
							{
#ifdef DEBUGX
								if(theConf.traceflag & (1<<CMDD))
									printf("%sSending Mqtt state\n",CMDDT);
#endif
								xEventGroupClearBits(wifi_event_group, PUB_BIT); // clear our waiting bit

								// Call the sending routine if any
								if(meter.code)
									(*meter.code)();

								//Wait PUB_BIT or 4 secs for Publish then close connection

								if(!xEventGroupWaitBits(wifi_event_group, PUB_BIT, false, true,  4000/  portTICK_RATE_MS))
								{
#ifdef DEBUGX
									if(theConf.traceflag & (1<<CMDD))
										printf("Publish TimedOut\n");
#endif
								}

								esp_mqtt_client_stop(clientCloud);
#ifdef DEBUGX
								if(theConf.traceflag & (1<<CMDD))
									printf("%sStopping\n",CMDDT);
#endif
							}
							else
							{ //it failed to start shouldnt close it
							//	esp_mqtt_client_stop(clientCloud);
#ifdef DEBUGX
								if(theConf.traceflag & (1<<CMDD))
									printf("%sStart Timed out\n",CMDDT);
#endif
							}
						}
						else
						 printf("Failed to start when available\n");
					}
				}
#ifdef DEBUGX
				if(theConf.traceflag & (1<<CMDD))
				{
					printf("%sDelivery time %dms\n",CMDDT,millis()-timetodelivery);
					printf("%sSent %d pin %d. Heap after submode %d\n",CMDDT,++sentTotal,meter.pin,esp_get_free_heap_size());
				}
#endif
			}
		else
			vTaskDelay(100 /  portTICK_RATE_MS);
		}
	}
	//should crash if it gets here.Impossible in theory
}

static void mqtt_app_start(void)
{

	     memset((void*)&mqtt_cfg,0,sizeof(mqtt_cfg));
#ifdef MQTTLOCAL
	     mqtt_cfg.client_id=				"anybody";
	     mqtt_cfg.username=					"";
	     mqtt_cfg.password=					"";
	     mqtt_cfg.uri = 					"mqtt://192.168.100.7:1883";
	     mqtt_cfg.event_handle = 			mqtt_event_handler;
	     mqtt_cfg.disable_auto_reconnect=	true;
#else
	     mqtt_cfg.client_id=				"anybody";
	     mqtt_cfg.username=					"yyfmmvmh";
	     mqtt_cfg.password=					"zE6oUgjoBQq4";
	     mqtt_cfg.uri = 					"mqtt://m15.cloudmqtt.com:18247";
	     mqtt_cfg.event_handle = 			mqtt_event_handler;
	     mqtt_cfg.disable_auto_reconnect=	true;
#endif
		mqttQ = xQueueCreate( 20, sizeof( meterType ) );
		if(!mqttQ)
			printf("Failed queue\n");

	    clientCloud = esp_mqtt_client_init(&mqtt_cfg);

	    if(clientCloud)
	    	xTaskCreate(&submode,"U571",10240,NULL, 5, NULL);
	    else
	    {
	    	printf("Not configured Mqtt\n");
	    	return;
	    }
	//    if(esp_mqtt_client_start(clientCloud)!=;
	  //  if (err !=ESP_OK)
	    //	printf("Error connect mqtt %d\n",err);

	//    esp_mqtt_client_start(clientCloud); //should be started by the SubMode task
}

//load firmware
void firmwareCmd(parg *pArg) //called by cmdManager
{
	// use task so as to keep registering pulses
	xTaskCreate(&firmUpdate,"U571",10240,NULL, 5, NULL);
}

static void loginCmd(parg* argument)
{
	loginT loginData;

	u32 macc=(u32)argument->macn;
	int cual=find_mac(macc);
	if(cual<0)
		return;

	whitelist[cual].dState=MSGSTATE;
	whitelist[cual].seen=millis();

//	losMacs[cual].loginf=true;

	time(&loginData.thedate);
	loginData.theTariff=100;
	send(argument->pComm, &loginData, sizeof(loginData), 0);
}

static void reserveSlot(parg* argument)
{
	uint8_t yes=1;
	u32 macc=(u32)argument->macn;

	if(theConf.traceflag & (1<<CMDD))
		printf("%sReserve slot for %x\n",CMDDT,macc);

	for(int a=0;a<theConf.reservedCnt;a++)
	{
		if(theConf.reservedMacs[a]==macc)
		{
			yes=-2;		//duplicate HOW COME????? in normal conditions
			send(argument->pComm, &yes, sizeof(yes), 0);//already registered
			return;
		}
	}
//add it
	theConf.reservedMacs[theConf.reservedCnt]=macc;
	printf("Mac reserved %x slot %d\n",theConf.reservedMacs[theConf.reservedCnt],theConf.reservedCnt);
	yes=theConf.reservedCnt++;	//send slot assigned
	send(argument->pComm, &yes, sizeof(yes), 0);
	write_to_flash();		//independent from FRAM
}


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
			int pos=lpos->valueint;
			u32 macc=(u32)argument->macn;
			int cual=find_mac(macc);
			if(cual>=0)
			{
				tallies[cual][pos]++;
				if(mid)
					strcpy(losMacs[cual].meterSerial[pos],mid->valuestring);
				time(&losMacs[cual].lastUpdate);	//lastupdate time for this station. Used by the StationGuard
				if(kwh)
					losMacs[cual].controlLastKwH[pos]=kwh->valueint;
				losMacs[cual].trans[pos]++;
				if(beats)
					losMacs[cual].controlLastBeats[pos]=beats->valueint;


				if(!losMacs[cual].timerH)
				{
#ifdef DEBUGX
					if(theConf.traceflag & (1<<CMDD))
						printf("%sStarting timer for meterController %d\n",CMDDT,cual);
#endif
					losMacs[cual].timerH=xTimerCreate("Monitor",61000 /portTICK_PERIOD_MS,pdTRUE,( void * )cual,&monitorCallback);
					if(losMacs[cual].timerH==NULL)
						printf("Failed to create HourChange timer\n");
					xTimerStart(losMacs[cual].timerH,0); //Start it
					//kill timer and start a new one
				}
				else
				{
#ifdef DEBUGX
					if(theConf.traceflag & (1<<CMDD))
						printf("%sreseting timer for meterController %d\n",CMDDT,cual);
#endif
					xTimerReset(losMacs[cual].timerH,0); //Start it weith new

					//start a new one
				}
#ifdef DEBUGX
				if(theConf.traceflag & (1<<CMDD))
					printf("%sMeter[%d][%d]=%d\n",CMDDT,cual,pos,tallies[cual][pos]);
#endif
			}
			else
			{
#ifdef DEBUGX
				if(theConf.traceflag & (1<<CMDD))
					printf("%sMac not found %x\n",CMDDT,macc);
#endif
			}
		}//lmac && lpos

		if(answer)
		{
			time(&loginData.thedate);
			loginData.theTariff=100;
			send(argument->pComm, &loginData, sizeof(loginData), 0);
		}
}

 // Messages form Host Controller managed here
 // format is JSON in array "Batch" of Cmds

static void cmdManager(void* arg)
{
	cmdType cmd;
	cJSON *elcmd;

	mqttR = xQueueCreate( 20, sizeof( cmdType ) );
			if(!mqttR)
				printf("Failed queue Rx\n");

	while(1)
	{

		if( xQueueReceive( mqttR, &cmd, portMAX_DELAY ))
		{
#ifdef DEBUGX
			if(theConf.traceflag & (1<<MSGD))
				printf("%sReceived: %s Fd %d Queue %d Heap %d\n",MSGDT,cmd.mensaje,cmd.fd,uxQueueMessagesWaiting(mqttR),esp_get_free_heap_size());
#endif
			elcmd= cJSON_Parse(cmd.mensaje);
			if(elcmd)
			{
				cJSON *monton= cJSON_GetObjectItem(elcmd,"Batch");//its an array opf cmds
				cJSON *ddmac= cJSON_GetObjectItem(elcmd,"macn");

				if(monton)
				{
					int son=cJSON_GetArraySize(monton);
					time(&losMacs[cmd.pos].lastUpdate);
					for (int a=0;a<son;a++)
					{
						cJSON *cmdIteml = cJSON_GetArrayItem(monton, a);//next item
						cJSON *cmdd= cJSON_GetObjectItem(cmdIteml,"cmd"); //get cmd. Nopt detecting invalid cmd
#ifdef DEBUGX
						if(theConf.traceflag & (1<<CMDD))
							printf("%sArray[%d] cmd is %s\n",CMDDT,a,cmdd->valuestring);
#endif
						int cualf=findCommand(string(cmdd->valuestring));
						if(cualf>=0)
						{
							parg *argument=(parg*)malloc(sizeof(parg));
							argument->pMessage=(void*)cmdIteml;
							argument->typeMsg=1;
							argument->pComm=cmd.fd;
							if(ddmac)
								argument->macn=ddmac->valuedouble;
							if(cmds[cualf].source==HOSTT) //if from Central Host VALIDATE it is for US, our theConf.meterConnName id
							{
								cJSON *cmgr= cJSON_GetObjectItem(cmdIteml,"connmgr");
								if(!cmgr)
								{
									printf("Not our structure\n");
									goto exit;
								}
								if(strcmp(cmgr->valuestring,theConf.meterConnName)!=0)
								{
									printf("Not our connMgr %s\n",cmgr->valuestring);
									goto exit;
								}

							}
							(*cmds[cualf].code)(argument);	// call the cmd and wait for it to end
							free(argument);
						}
						else
						{
#ifdef DEBUGX
						if(theConf.traceflag & (1<<CMDD))
							printf("%sCmd %s not implemented\n",CMDDT,cmdd->valuestring);
#endif
						}
					}// array
				}//batch
			exit:
				if(elcmd)
					cJSON_Delete(elcmd);
			}
			else
			{
#ifdef DEBUGX
				if(theConf.traceflag & (1<<CMDD))
					printf("%sCould not parse cmd\n",CMDDT);
#endif
			}
			if(cmd.mensaje)
				free(cmd.mensaje); // Data is transfered via pointer from a malloc. Free it here.
#ifdef DEBUGX
			if(theConf.traceflag & (1<<MSGD))
				printf("%sMqttManger heap %d\n",MSGDT,esp_get_free_heap_size());
#endif
		}
		else
		{
#ifdef DEBUGX
			if(theConf.traceflag & (1<<CMDD))
				printf("%sCmdQueue Error\n",CMDDT);
#endif
		}
	}//while
}

#ifdef DISPLAY
static void initI2C()
{
	i2cp.sdaport=(gpio_num_t)SDAW;
	i2cp.sclport=(gpio_num_t)SCLW;
	i2cp.i2cport=I2C_NUM_0;
	miI2C.init(i2cp.i2cport,i2cp.sdaport,i2cp.sclport,400000,&I2CSem);//Will reserve a Semaphore for Control
}

void initScreen()
{
	if(xSemaphoreTake(I2CSem, portMAX_DELAY))
	{
		display.init();
		display.flipScreenVertically();
		display.clear();
		drawString(64,8,"ConnMgr",24,TEXT_ALIGN_CENTER,DISPLAYIT,NOREP);
		xSemaphoreGive(I2CSem);
	}
	else
		printf("Failed to InitScreen\n");
}
#endif

static void read_flash()
{
	esp_err_t q ;
	size_t largo;
	q = nvs_open("config", NVS_READONLY, &nvshandle);
	if(q!=ESP_OK)
	{
		printf("Error opening NVS Read File %x\n",q);
		return;
	}

	largo=sizeof(theConf);
	q=nvs_get_blob(nvshandle,"sysconf",(void*)&theConf,&largo);

	if (q !=ESP_OK)
		printf("Error read %x largo %d aqui %d\n",q,largo,sizeof(theConf));
	nvs_close(nvshandle);

}

void write_to_flash() //save our configuration
{
	esp_err_t q ;
	q = nvs_open("config", NVS_READWRITE, &nvshandle);
	if(q!=ESP_OK)
	{
		printf("Error opening NVS File RW %x\n",q);
		return;
	}
	size_t req=sizeof(theConf);
	q=nvs_set_blob(nvshandle,"sysconf",&theConf,req);
	if (q ==ESP_OK)
	{
		q = nvs_commit(nvshandle);
		if(q!=ESP_OK)
			printf("Flash commit write failed %d\n",q);
	}
	else
		printf("Fail to write flash %x\n",q);
	nvs_close(nvshandle);
}

void write_to_fram(u8 meter,bool addit)
{
	time_t timeH;
    struct tm timeinfo;
    uint16_t theG;


	// FRAM Semaphore is taken by the Interrupt Manager. Safe to work.
    time(&timeH);
	localtime_r(&timeH, &timeinfo);
	mesg=timeinfo.tm_mon;
	diag=timeinfo.tm_mday-1;
	yearg=timeinfo.tm_year+1900;
	horag=timeinfo.tm_hour;

	if(addit)
	{
		theMeters[meter].curLife++;
		theMeters[meter].curMonth++;
		theMeters[meter].curDay++;
		theMeters[meter].curHour++;
		theMeters[meter].curCycle++;
		time((time_t*)&theMeters[meter].lastKwHDate); //last time we saved data

		fram.write_beat(meter,theMeters[meter].currentBeat);
		fram.write_lifekwh(meter,theMeters[meter].curLife);
		fram.write_month(meter,mesg,theMeters[meter].curMonth);
		fram.write_monthraw(meter,mesg,theMeters[meter].curMonthRaw);
		fram.write_day(meter,yearDay,theMeters[meter].curDay);
		fram.write_dayraw(meter,yearDay,theMeters[meter].curDayRaw);
		fram.write_hour(meter,yearDay,horag,theMeters[meter].curHour);
		fram.write_hourraw(meter,yearDay,horag,theMeters[meter].curHourRaw);
		fram.write_lifedate(meter,theMeters[meter].lastKwHDate);  //should be down after scratch record???
	}
		else
		{
			fram.read_guard((uint8_t*)&theG);
			if(theG!=theGuard)
				printf("Fram is lost\n");
			fram.write_beat(meter,theMeters[meter].currentBeat);
			fram.writeMany(FRAMDATE,(uint8_t*)&timeH,sizeof(timeH));//last known date
		}
}

void load_from_fram(u8 meter)
{
	if(framSem)
		if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
		{
			fram.read_lifekwh(meter,(u8*)&theMeters[meter].curLife);
			fram.read_lifedate(meter,(u8*)&theMeters[meter].lastKwHDate);
			fram.read_month(meter, mesg, (u8*)&theMeters[meter].curMonth);
			fram.read_monthraw(meter, mesg, (u8*)&theMeters[meter].curMonthRaw);
			fram.read_day(meter, yearDay, (u8*)&theMeters[meter].curDay);
			fram.read_dayraw(meter, yearDay, (u8*)&theMeters[meter].curDayRaw);
			fram.read_hour(meter, yearDay, horag, (u8*)&theMeters[meter].curHour);
			fram.read_hourraw(meter, yearDay, horag, (u8*)&theMeters[meter].curHourRaw);
			fram.read_beat(meter,(u8*)&theMeters[meter].currentBeat);
			xSemaphoreGive(framSem);

			totalPulses+=theMeters[meter].currentBeat;

	#ifdef DEBUGX
			if(theConf.traceflag & (1<<FRAMD))
				printf("[FRAMD]Loaded Meter %d curLife %d beat %d\n",meter,theMeters[meter].curLife,theMeters[meter].currentBeat);
	#endif
		}
}

static void init_fram()
{
	// FRAM Setup
	theGuard = rand();
	framSem=NULL;
	fram.begin(FMOSI,FMISO,FCLK,FCS,&framSem); //will create SPI channel and Semaphore
	spi_flash_init();
	//load all devices counters from FRAM
	if(framSem)
	{
		fram.write_guard(theGuard);				// theguard is dynamic and will change every boot.

		for (int a=0;a<MAXDEVS;a++)
			load_from_fram(a);
	}
}

// save the data to fram
//lock semaphore and release after done

void framManager(void * pArg)
{
	meterType theMeter;

	framQ = xQueueCreate( 20, sizeof( meterType ) );
	if(!framQ)
		printf("Failed queue Fram\n");

	while(true)
	{
		if( xQueueReceive( framQ, &theMeter, portMAX_DELAY/  portTICK_RATE_MS ))
		{
			if(framSem)
			{
				if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
				{
					write_to_fram(theMeter.pos,theMeter.saveit);
	#ifdef DEBUGX
					if(theConf.traceflag & (1<<FRAMD))
						printf("%sSaving Meter %d add %d Beats %d\n",FRAMDT,theMeter.pos,theMeter.saveit,theMeters[theMeter.pos].currentBeat);
	#endif
					xSemaphoreGive(framSem);
				}
			}
		}
		else
		{
#ifdef DEBUGX
			if(theConf.traceflag & (1<<FRAMD))
				printf("%sFailed framQ Manager\n",FRAMDT);
#endif
			delay(1000);
		}
	}
}

static void pcntManager(void * pArg)
{
	pcnt_evt_t evt;
	portBASE_TYPE res;
	u16 residuo,count;
	u32 timeDiff=0;

	pcnt_evt_queue = xQueueCreate( 20, sizeof( pcnt_evt_t ) );
	if(!pcnt_evt_queue)
		printf("Failed queue PCNT\n");

	while(true)
	{
        res = xQueueReceive(pcnt_evt_queue, (void*)&evt,portMAX_DELAY / portTICK_PERIOD_MS);
        if (res == pdTRUE)
        {
			pcnt_get_counter_value((pcnt_unit_t)evt.unit,(short int *) &count);

#ifdef DEBUGX
			if(theConf.traceflag & (1<<INTD))
				printf("%sEvent PCNT unit[%d]; cnt: %d status %x\n",INTDT, evt.unit, count,evt.status);
#endif
			if (evt.status & PCNT_EVT_THRES_1)
			{
				pcnt_counter_clear((pcnt_unit_t)evt.unit);// MUST DO for it to be every 1/10 of BPK
				totalPulses+=count;

				// THE counters. the whole point is this
				theMeters[evt.unit].saveit=false;
				theMeters[evt.unit].currentBeat+=count;
				theMeters[evt.unit].beatSave+=count;
				if((theMeters[evt.unit].currentBeat % 10)!=0)
				{
					//force it to 10s. No idea how this happens but this is required else it will never have a Residuo of 0 and hence inc kwh
					//shouldnt happen often I guess
					//very important safeguard DO NOT REMOVE
					float theFloat=theMeters[evt.unit].currentBeat/10;
					int rounded=(int)round(theFloat)*10;
					theMeters[evt.unit].currentBeat=rounded;
					theFloat=theMeters[evt.unit].beatSave/10;
					rounded=(int)round(theFloat)*10;
					theMeters[evt.unit].beatSave=rounded;
				}

				if(tarifasDia[horag]==0)
					tarifasDia[horag]=100; //when Tariffs NOT initialized it will crash

				if(theMeters[evt.unit].beatsPerkW>0)
					residuo=theMeters[evt.unit].beatSave % (theMeters[evt.unit].beatsPerkW*tarifasDia[horag]/100);
				else
					residuo=1;

#ifdef DEBUGX
				if(theConf.traceflag & (1<<INTD))
					printf("%sAddKwH %s Beat %d MeterPos %d Time %d Hora %d Tarifa %02x BPK %d\n",INTDT,
							residuo?"N":"Y",theMeters[evt.unit].currentBeat,theMeters[evt.unit].pos,timeDiff,
									horag,tarifasDia[horag],theMeters[evt.unit].beatsPerkW);
#endif
				// if we have reached BPK, save it to fram but also add 1 kwh counter
				// else just save the 1/10 betas
				if(residuo==0 && theMeters[evt.unit].currentBeat>0)
				{
					if(theMeters[evt.unit].pos==(MAXDEVS-1))
						theMeters[evt.unit].code=testMqtt; 		//using this routine for mqtt test.Just the last one
					else
						theMeters[evt.unit].code=NULL; 		//do nothing
					theMeters[evt.unit].saveit=true;
					theMeters[evt.unit].beatSave-=theMeters[evt.unit].beatsPerkW*tarifasDia[horag]/100;
				}

				if(framQ)
					xQueueSend( framQ,&theMeters[evt.unit],0);//copy to fram Manager
		//		if(mqttQ)
		//			xQueueSend( mqttQ,&theMeters[evt.unit],0 );
			}
        } else
            printf("PCNT Failed Queue\n");
	}
}

static void init_vars()
{
	char them[40];
	esp_efuse_mac_get_default((uint8_t*)them);
	memcpy(&theMacNum,&them[2],4);

	//queue name
	sprintf(them,"MeterIoT/%s/CMD",theConf.meterConnName);
	cmdQueue=string(them);
	controlQueue="MeterIoT/EEQ/CONTROL";
	//zero some stuff
	vanadd=0;
	llevoMsg=0;
	miscanf=false;

	memset(&losMacs,0,sizeof(losMacs));
	memset(&setupHost,0,sizeof(setupHost));
	memset(&theMeters,0,sizeof(theMeters));

	for (int a=0;a<theConf.reservedCnt;a++)
	{
		whitelist[a].dState=BOOTSTATE;
		whitelist[a].seen=0;
	}

	vanMacs=0;
    qwait=QDELAY;
    qdelay=qwait*1000;

   	//activity led
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_down_en =GPIO_PULLDOWN_ENABLE;
	io_conf.pin_bit_mask = (1ULL<<WIFILED);
	gpio_config(&io_conf);
	gpio_set_level((gpio_num_t)WIFILED, 0);

	daysInMonth [0] =31;
	daysInMonth [1] =28;
	daysInMonth [2] =31;
	daysInMonth [3] =30;
	daysInMonth [4] =31;
	daysInMonth [5] =30;
	daysInMonth [6] =31;
	daysInMonth [7] =31;
	daysInMonth [8] =30;
	daysInMonth [9] =31;
	daysInMonth [10] =30;
	daysInMonth [11] =31;

	for(int a=0;a<MAXDEVS;a++)
	{
		theMeters[a].beatsPerkW=theConf.beatsPerKw[a];
		strcpy(theMeters[a].serialNumber,theConf.medidor_id[a]);
	}

	//message from Meters
	strcpy((char*)&cmds[0].comando,"/ga_firmware");			cmds[0].code=firmwareCmd;			cmds[0].source=HOSTT;	//from Host
	strcpy((char*)&cmds[1].comando,"/ga_tariff");			cmds[1].code=loadit;				cmds[1].source=HOSTT;	//from Host
	strcpy((char*)&cmds[2].comando,"/ga_status");			cmds[2].code=statusCmd;				cmds[2].source=LOCALT;	//from local
	strcpy((char*)&cmds[3].comando,"/ga_login");			cmds[3].code=loginCmd;				cmds[3].source=LOCALT;	//from local
	strcpy((char*)&cmds[4].comando,"/ga_telemetry");		cmds[4].code=sendTelemetryCmd;		cmds[4].source=HOSTT;	//from Host
	strcpy((char*)&cmds[5].comando,"/ga_reserve");			cmds[5].code=reserveSlot;			cmds[5].source=LOCALT;	//from local

	//debug trace flags
#ifdef KBD
	strcpy(lookuptable[0],"BOOTD");
	strcpy(lookuptable[1],"WIFID");
	strcpy(lookuptable[2],"MQTTD");
	strcpy(lookuptable[3],"PUBSUBD");
	strcpy(lookuptable[4],"OTAD");
	strcpy(lookuptable[5],"CMDD");
	strcpy(lookuptable[6],"WEBD");
	strcpy(lookuptable[7],"GEND");
	strcpy(lookuptable[8],"MQTTT");
	strcpy(lookuptable[9],"FRMCMD");
	strcpy(lookuptable[10],"INTD");
	strcpy(lookuptable[11],"FRAMD");
	strcpy(lookuptable[12],"MSGD");
	strcpy(lookuptable[13],"TIMED");
	strcpy(lookuptable[14],"SIMD");
	strcpy(lookuptable[15],"HOSTD");

	string debugs;

	// add - sign to Keys
	for (int a=0;a<NKEYS/2;a++)
	{
		debugs="-"+string(lookuptable[a]);
		strcpy(lookuptable[a+NKEYS/2],debugs.c_str());
	}
#endif
}

static void erase_config() //do the dirty work
{
	printf("Erase config\n");
	memset(&theConf,0,sizeof(theConf));
	theConf.centinel=CENTINEL;
	write_to_flash();
	//	if(  xSemaphoreTake(logSem, portMAX_DELAY/  portTICK_RATE_MS))
	//	{
	//		fclose(bitacora);
	//	    bitacora = fopen("/spiflash/log.txt", "w"); //truncate
	//	    if(bitacora)
	//	    {
	//	    	fclose(bitacora); //Close and reopen r+
	//		    bitacora = fopen("/spiflash/log.txt", "r+");
	//		    if(!bitacora)
	//		    	printf("Could not reopen logfile\n");
	//		    else
	//			    postLog(0,"Log cleared");
	//	    }
	//	    xSemaphoreGive(logSem);
	//	}
#ifdef DEBUGX
	if(theConf.traceflag & (1<<BOOTD))
		printf("%sCentinel %x\n",BOOTDT,theConf.centinel);
#endif
}

void makeMetercJSON(macControl *meterController,cJSON *arr)
{
	for(int a=0;a<MAXDEVS;a++)
	{
		cJSON *cmdJ=cJSON_CreateObject();
		cJSON_AddStringToObject(cmdJ,"mid",				meterController->meterSerial[a]);
		cJSON_AddNumberToObject(cmdJ,"KwH",				meterController->controlLastKwH[a]);
		cJSON_AddItemToArray(arr, cmdJ);
	}
}

cJSON * makeGroupMeters()
{
	time_t now;

	time(&now);

	/////// Create Message for Grouped Cmds ///////////////////
	cJSON *root=cJSON_CreateObject();
	if(root==NULL)
	{
		printf("cannot create root\n");
		return NULL;
	}

	cJSON_AddStringToObject(root,"connId",				theConf.meterConnName);
	cJSON_AddNumberToObject(root,"Ts",					now);
	cJSON_AddNumberToObject(root,"macn",				theMacNum);

	cJSON *ar = cJSON_CreateArray();
	for (int a=0;a<theConf.reservedCnt;a++)
	{
//		if(losMacs[a].lastUpdate>0)
			makeMetercJSON(&losMacs[a],ar);
	}

	//local meters
	for(int a=0;a<workingDevs;a++)
	{
			cJSON *cmdJ=cJSON_CreateObject();
			cJSON_AddStringToObject(cmdJ,"mid",				theMeters[a].serialNumber);
			cJSON_AddNumberToObject(cmdJ,"KwH",				theMeters[a].curLife);
			cJSON_AddItemToArray(ar, cmdJ);

	}

	cJSON_AddItemToObject(root, "Telemetry",ar);
	return root;
}

void testMqtt()
{
	char *lmessage=NULL;

	cJSON * telemetry=makeGroupMeters();

	if(telemetry)
	{
		lmessage=cJSON_Print(telemetry);
#ifdef DEBUGX
		if(theConf.traceflag & (1<<MQTTD))
			printf("To Host %s\n",lmessage);
#endif
	}
	else
		printf("Failed telemetry\n");

	//esp_mqtt_client_publish(clientCloud, "MeterIoT/Chillo/Chillo/CONTROL", lmessage, 0, 1,0);
	esp_mqtt_client_publish(clientCloud, controlQueue.c_str(), lmessage, 0, 1,0);

	if(lmessage)
		free(lmessage);
	if(telemetry)
		cJSON_Delete(telemetry);
}

static void sendTelemetry()
{
	time_t now=0;
	cJSON * telemetry=makeGroupMeters();
	char *lmessage=NULL;

	if(telemetry)
	{
		lmessage=cJSON_Print(telemetry);
		printf("To Host %s\n",lmessage);
	}
	else
	{
		printf("Failed telemetry\n");
	}

	time(&now);
	esp_mqtt_client_publish(clientCloud, controlQueue.c_str(), lmessage, 0, 1,0);
	fram.write_cycle(mesg,now);
	if(lmessage)
		free(lmessage);
	if(telemetry)
		cJSON_Delete(telemetry);
	}

void connMgr(void* pArg)
{
	meterType dummy;

// your baby to care for, Deliver the Message. Manage connection errors, out of slot delivery etc
	printf("Conn Manager Time %d\n",(int)pArg);
	delay((int)pArg);

	printf("Sending Telemetry\n");
	dummy.code=sendTelemetry;
	dummy.saveit=true;
	if(mqttQ)
		xQueueSend( mqttQ,&dummy,0 );

	delay(1000);
	connHandle=NULL;
	vTaskDelete(NULL);
}

void init_boot()
{
#ifdef DEBUGX

	if(theConf.traceflag & (1<<BOOTD))
			printf("%s Fram Id %04x Fram Size %d%s\n",MAGENTA,fram.prodId,fram.intframWords,RESETC);

	if((theConf.traceflag & (1<<BOOTD)) && fram.intframWords>0)
	{
		printf("%s=============== FRAM ===============%s\n",RED,YELLOW);
		printf("FRAMDATE(%s%d%s)=%s%d%s\n",GREEN,FRAMDATE,YELLOW,CYAN,GUARDM-FRAMDATE,RESETC);
		printf("METERVER(%s%d%s)=%s%d%s\n",GREEN,GUARDM,YELLOW,CYAN,USEDMACS-GUARDM,RESETC);
		printf("FREEFRAM(%s%d%s)=%s%d%s\n",GREEN,USEDMACS,YELLOW,CYAN,MCYCLE-USEDMACS,RESETC);
		printf("MCYCLE(%s%d%s)=%s%d%s\n",GREEN,MCYCLE,YELLOW,CYAN,STATIONS-MCYCLE,RESETC);
		printf("STATIONS(%s%d%s)=%s%d%s\n",GREEN,STATIONS,YELLOW,CYAN,STATIONSEND-STATIONS,RESETC);
		printf("STATIONSEND(%s%d%s)=%s%d%s\n",GREEN,STATIONSEND,YELLOW,CYAN,TARIFADIA-STATIONSEND,RESETC);
		printf("TARIFADIA(%s%d%s)=%s%d%s\n",GREEN,TARIFADIA,YELLOW,CYAN,FINTARIFA-TARIFADIA,RESETC);
		printf("FINTARIFA(%s%d%s)=%s%d%s\n",GREEN,FINTARIFA,YELLOW,CYAN,BEATSTART-FINTARIFA,RESETC);
		printf("BEATSTART(%s%d%s)=%s%d%s\n",GREEN,BEATSTART,YELLOW,CYAN,LIFEKWH-BEATSTART,RESETC);
		printf("LIFEKWH(%s%d%s)=%s%d%s\n",GREEN,LIFEKWH,YELLOW,CYAN,LIFEDATE-LIFEKWH,RESETC);
		printf("LIFEDATE(%s%d%s)=%s%d%s\n",GREEN,LIFEDATE,YELLOW,CYAN,MONTHSTART-LIFEDATE,RESETC);
		printf("MONTHSTART(%s%d%s)=%s%d%s\n",GREEN,MONTHSTART,YELLOW,CYAN,MONTHRAW-MONTHSTART,RESETC);
		printf("MONTHRAW(%s%d%s)=%s%d%s\n",GREEN,MONTHRAW,YELLOW,CYAN,DAYSTART-MONTHRAW,RESETC);
		printf("DAYSTART(%s%d%s)=%s%d%s\n",GREEN,DAYSTART,YELLOW,CYAN,DAYRAW-DAYSTART,RESETC);
		printf("DAYRAW(%s%d%s)=%s%d%s\n",GREEN,DAYRAW,YELLOW,CYAN,HOURSTART-DAYRAW,RESETC);
		printf("HOURSTART(%s%d%s)=%s%d%s\n",GREEN,HOURSTART,YELLOW,CYAN,HOURRAW-HOURSTART,RESETC);
		printf("HOURRAW(%s%d%s)=%s%d%s\n",GREEN,HOURRAW,YELLOW,CYAN,DATAEND-HOURRAW,RESETC);
		printf("DATAEND(%s%d%s)=%s%d%s\n",GREEN,DATAEND,YELLOW,CYAN,DATAEND-BEATSTART,RESETC);
		printf("TOTALFRAM(%s%d%s) Devices %s%d%s\n",GREEN,TOTALFRAM,YELLOW,CYAN,(DATAEND-BEATSTART)*MAXDEVS,RESETC);
		printf("%s=============== FRAM ===============%s\n",RED,RESETC);
	}
#endif
}

static void loadDefaultTariffs()
{
    cmdType cmd;

	cJSON *root=cJSON_CreateObject();
	cJSON *ar = cJSON_CreateArray();

	if(root==NULL || ar==NULL)
	{
		printf("cannot create root tariff\n");
		return NULL;
	}

	cJSON *cmdJ=cJSON_CreateObject();
	cJSON_AddStringToObject(cmdJ,"cmd","/ga_tariff");
	cJSON_AddNumberToObject(cmdJ,"tariff",1); //default is 1
	cJSON_AddStringToObject(cmdJ,"connmgr",theConf.meterConnName);
	cJSON_AddItemToArray(ar, cmdJ);
	cJSON_AddItemToObject(root,"Batch", ar);
	char *lmessage=cJSON_Print(root);
	if(lmessage==NULL)
	{
		sprintf(tempb,"Error creating JSON Tariff");
		cJSON_Delete(root);
		return;
	}

	cmd.mensaje=lmessage;
	cmd.fd=3;//send from internal
	cmd.pos=0;
	xQueueSend( mqttR,&cmd,0 );
}

void app_main()
{

	printf("MeterIoT Manager5555\n");

	esp_log_level_set("*", ESP_LOG_WARN);

	esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES|| err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    	printf("No free pages erased!!!!\n");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

	read_flash();// read configuration
#ifdef DEBUGX
	if(theConf.traceflag & (1<<BOOTD))
	{
		printf("%sBuildMgr starting up\n",BOOTDT);
		printf("%sFree memory: %d bytes\n", BOOTDT,esp_get_free_heap_size());
		printf("%sIDF version: %s\n", BOOTDT,esp_get_idf_version());
	}
	printf("\n%sFlash Button to erase config 3 secs%s\n",RED,RESETC);// chance to erase configuration and start fresh
	delay(3000);
#endif

	if (theConf.centinel!=CENTINEL || !gpio_get_level((gpio_num_t)0))
		erase_config();//start fresh

	init_vars();			// setup initial values
	init_fram();			// connect to fram to save readings
    init_boot();			// print boot stuff if defined
    wifi_init();			// this a Master Controller.
    mqtt_app_start();		// Start MQTT configuration and DO NOT connect. Connection will be on demand

#ifdef KBD
	xTaskCreate(&kbd,"kbd",10240,NULL, 4, NULL);
#endif

#ifdef DISPLAY // most likely heap damage. Routine seems unreliable
    initI2C();
    initScreen();
#endif

#ifdef TEMP // for the sake of Sensors sample
    init_temp();
#endif

   	// Managers Tasks
   	xTaskCreate(&cmdManager,"cmdMgr",4096,NULL, 5, NULL); 							//cmds received from the Host Controller via MQTT or internal TCP/IP. jSON based
	xTaskCreate(&framManager,"framMgr",4096,NULL, configMAX_PRIORITIES-5, NULL);	// Fram Manager
	xTaskCreate(&pcntManager,"pcntMgr",4096,NULL, configMAX_PRIORITIES-8, NULL);	// We also control 5 meters. Pseudo ISR. This is d manager
	xTaskCreate(&buildMgr,"TCP",4096,(void*)1, configMAX_PRIORITIES-10, NULL);		// Messages from the Meter Controllers via socket manager

	// wait time from SNTP for 10 secs. If failure use FRAM last saved time
	EventBits_t uxBits = xEventGroupWaitBits(wifi_event_group, SNTP_BIT, false, true, 10000/  portTICK_RATE_MS);//
	if( ( uxBits & SNTP_BIT ) != 0 )
	{
		//sntp got time, check tariffs are Ok. We need DATE to obtain correct tariffs 24 hours for THIS day of the year

		memset(&tarifasDia,0,sizeof(tarifasDia));
		int err=fram.read_tarif_day(yearDay,(u8*)&tarifasDia);
		if(err!=0)
			printf("Error %d reading Tariffs day %d...recovering from Host\n",err,yearDay);

		u16 ttar=0;
		for (int aa=0;aa<24;aa++)
			ttar+=tarifasDia[aa];
		if(ttar==0) // load tariffs...something not right. Loadit from Host
			loadDefaultTariffs();

#ifdef DEBUGX
		if(theConf.traceflag & (1<<BOOTD))
		{
			printf("%sDay[%d]",BOOTDT,yearDay);
			for (int aa=0;aa<24;aa++)
				printf("[%02d]=%02x ",aa,tarifasDia[aa]);
			printf("\nHora %d Tarifa %02x\n",horag,tarifasDia[horag]);
		}

		if(theConf.traceflag & (1<<BOOTD))
			printf("%sStarting Webserver\n",BOOTDT);
#endif

	}
	else
	{
		loginT now;
		printf("Emergency recover time\n");
		fram.readMany(FRAMDATE,(uint8_t*)&now.thedate,sizeof(now.thedate));
		updateDateTime(now);
	}

	pcnt_init();// initialize it. Several tricks apply. Read there. Depends on Tariffs so must be after we know tariffs and date are OK

	xTaskCreate(&watchDog,"dog",4096,(void*)1, 4, NULL);				// Care taker of MeterM to report to Host
	xTaskCreate(&start_webserver,"web",4096,(void*)1, 4, &webHandle);	// Messages from the Meters. Controller Section socket manager

#ifdef DISPLAY
	xTaskCreate(&displayManager,"dispMgr",4096,NULL, 4, NULL);
#endif
}
