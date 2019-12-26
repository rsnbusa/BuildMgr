#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"
#define WITHMETERS

static void submode(void * pArg);
void kbd(void *pArg);
static void update_mac(u32 newmac);
static void write_to_fram(u8 meter,bool addit);

#ifdef DISPLAY
void clearScreen();
void displayManager(void * pArg);
void drawString(int x, int y, string que, int fsize, int align,displayType showit,overType erase);
#endif

static const char *TAG = "BDMGR";

static EventGroupHandle_t wifi_event_group;
const static int MQTT_BIT = BIT0;
const static int WIFI_BIT = BIT1;
const static int PUB_BIT  = BIT2;
const static int DONE_BIT = BIT3;
const static int SNTP_BIT = BIT4;

static int find_mac(uint32_t esteMac)
{
	for (int a=0;a<vanMacs;a++)
	{
		if(losMacs[a].macAdd==esteMac)
			return a;
	}
	return -1;
}

static int find_ip(uint32_t esteIp)
{
	for (int a=0;a<vanMacs;a++)
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
	return;


	for (int a=0;a<MAXDEVS;a++)
	{
#ifdef DEBUGX
		if(theConf.traceflag & (1<<CMDD))
			printf("%sHour change meter %d val %d\n",CMDDT,a,theMeters[a].curHour);
#endif
		if(xSemaphoreTake(framSem, portMAX_DELAY))
		{
			fram.write_hour(a, yearg,oldMesg,oldDiag,oldHorag, theMeters[a].curHour);//write old one before init new
			fram.write_hourraw(a, yearg,oldMesg,oldDiag,oldHorag, theMeters[a].curHourRaw);//write old one before init new
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

void dayChange()
{
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		printf("%sDay change Old %d New %d\n",CMDDT,oldDiag,diag);
#endif

	return;

	for (int a=0;a<MAXDEVS;a++)
			{
				if(theConf.traceflag & (1<<CMDD))
					printf("%sDay change mes %d day %d oldday %d corte %d sent %d\n",CMDDT,oldMesg,diag,oldDiag,theConf.diaDeCorte[a],theConf.corteSent[a]);
				if(xSemaphoreTake(framSem, portMAX_DELAY))
				{
					fram. write_day(a,yearg, oldMesg,oldDiag, theMeters[a].curDay);
					theMeters[a].curDay=0;
					xSemaphoreGive(framSem);
				}
			}
			oldDiag=diag;
}

void monthChange()
{
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		printf("%sMonth change Old %d New %d\n",CMDDT,oldMesg,mesg);
#endif

	return;

	for (int a=0;a<MAXDEVS;a++)
			{
				if(xSemaphoreTake(framSem, portMAX_DELAY))
				{
					fram.write_month(a, oldMesg, theMeters[a].curMonth);
					xSemaphoreGive(framSem);
					theMeters[a].curMonth=0;
				}
			}
			oldMesg=mesg;
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

	if(horag!=oldHorag) // hour change up or down
		hourChange();

	if(diag!=oldDiag) // day change up or down. Also hour MUST HAVE CHANGED before
	{
		//	setCycleChange(diag,oldDiag);
		dayChange();
	}

	if(mesg!=oldMesg) // month change up or down. What to do with prev Year???? MONTH MUST HAVE CHANGED
		monthChange();
}

void timerCallback( TimerHandle_t xTimer )
{
#define CUANTO 3600000
	check_date_change();

	u32 firstt = ( uint32_t ) pvTimerGetTimerID( xTimer );
	if(firstt)
	{
#ifdef DEBUGX
		if(theConf.traceflag & (1<<CMDD))
			printf("%sTimer Hour\n",CMDDT);
#endif

		xTimerGenericCommand(xTimer,tmrCOMMAND_CHANGE_PERIOD,CUANTO /portTICK_PERIOD_MS,NULL,0); //every hour from now on
		vTimerSetTimerID( xTimer,( void * ) 0 );
	}

}

static void timeKeeper()
{
#define QUE 1000 //used to test timer
	time_t now;


	time(&now);
	int faltan=3600- (now % 3600)+2; //second to next hour +2 secs
#ifdef DEBUGX
	if(theConf.traceflag & (1<<BOOTD))
		printf("%sSecs to Hour %d now %d\n",BOOTDT,faltan,(u32)now);
#endif

	hourChangeT=xTimerCreate("OpenTimer",faltan*QUE /portTICK_PERIOD_MS,pdTRUE,( void * ) 1,&timerCallback);
	if(hourChange==NULL)
		printf("Failed to create HourChange timer\n");
	xTimerStart(hourChangeT,0); //Start it
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

static void task_fatal_error()
								{
	printf("Exiting task due to fatal error...");
	close(socket_id);
								}

/*read buffer by byte still delim ,return read bytes counts*/
static int read_until(char *buffer, char delim, int len)
{
	//  /*TODO: delim check,buffer check,further: do an buffer length limited*/
	int i = 0;
	while (buffer[i] != delim && i < len) {
		++i;
	}
	return i + 1;
}

/* resolve a packet from http socket
 * return true if packet including \r\n\r\n that means http packet header finished,start to receive packet body
 * otherwise return false
 * */
static bool read_past_http_header(char text[], int total_len, esp_ota_handle_t update_handle)
{
	/* i means current position */
	int i = 0, i_read_len = 0;

	while (text[i] != 0 && i < total_len) {
		i_read_len = read_until(&text[i], '\n', total_len);
		// if we resolve \r\n line,we think packet header is finished
		if (i_read_len == 2) {// final \n found should just have 2 bytes read end of Header
			int i_write_len = total_len - (i + 2);
			esp_err_t err = esp_ota_write( update_handle, (const void *)&(text[i + 2]), i_write_len);
			if (err != ESP_OK) {
#ifdef DEBUGX
				if(theConf.traceflag & (1<<OTAD))
					printf( "%sError: esp_ota_write failed! err=0x%x\n",OTADT, err);
#endif
				return false;
			} else {
#ifdef DEBUGX
				if(theConf.traceflag & (1<<OTAD))
					printf( "%sesp_ota_write header OK\n",OTADT);
#endif
				binary_file_length += i_write_len;
			}
			return true;
		}
		i += i_read_len;
	}
	return false;
}

static bool connect_to_http_server()
{
#ifdef DEBUGX
	if(theConf.traceflag & (1<<WEBD))
		printf("%sIP: %s Server Port:%s\n",WEBDT, EXAMPLE_SERVER_IP, EXAMPLE_SERVER_PORT);
#endif
	sprintf(http_request, "GET %s HTTP/1.1\r\nHost: %s:%s \r\n\r\n", EXAMPLE_FILENAME, EXAMPLE_SERVER_IP, EXAMPLE_SERVER_PORT);
	int  http_connect_flag = -1;
	struct sockaddr_in sock_info;

	socket_id = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_id == -1) {
#ifdef DEBUGX
		if(theConf.traceflag & (1<<WEBD))
			printf( "%sCreate socket failed!\n",WEBDT);
#endif
		return false;
	}

	// set connect info
	memset(&sock_info, 0, sizeof(struct sockaddr_in));
	sock_info.sin_family = AF_INET;
	sock_info.sin_addr.s_addr = inet_addr(EXAMPLE_SERVER_IP);
	sock_info.sin_port = htons(atoi(EXAMPLE_SERVER_PORT));

	// connect to http server
	http_connect_flag = connect(socket_id, (struct sockaddr *)&sock_info, sizeof(sock_info));
	if (http_connect_flag == -1) {
#ifdef DEBUGX
		if(theConf.traceflag & (1<<WEBD))
			printf( "%sConnect to server failed! errno=%d\n",WEBDT, errno);
#endif
		close(socket_id);
		return false;
	} else {
#ifdef DEBUGX
		if(theConf.traceflag & (1<<WEBD))
			printf( "%sConnected to server\n",WEBDT);
#endif
		return true;
	}
	return false;
}

static void firmUpdate(void *pArg)
{
	esp_err_t err;
	uint8_t como=1;
	TaskHandle_t blinker=NULL;
	esp_ota_handle_t update_handle = 0 ;
	const esp_partition_t *update_partition = NULL;
#ifdef DEBUGX
	if(theConf.traceflag & (1<<OTAD))
		printf("%sStarting OTA...\n",OTADT);
#endif

	const esp_partition_t *configured = esp_ota_get_boot_partition();
	const esp_partition_t *running = esp_ota_get_running_partition();

	assert(configured == running); /* fresh from reset, should be running from configured boot partition */

#ifdef DEBUGX
	if(theConf.traceflag & (1<<OTAD))
		printf("%sRunning partition type %d subtype %d (offset 0x%08x)\n",OTADT,configured->type, configured->subtype, configured->address);
#endif

	/*connect to http server*/
	if (connect_to_http_server()) {
#ifdef DEBUGX
		if(theConf.traceflag & (1<<OTAD))
			printf( "%sConnected to http server\n",OTADT);
#endif
	} else {
#ifdef DEBUGX
		if(theConf.traceflag & (1<<OTAD))
			printf( "%sConnect to http server failed!\n",OTADT);
#endif
		task_fatal_error();
		goto exit;
	}

	int res;
	/*send GET request to http server*/
	res = send(socket_id, http_request, strlen(http_request), 0);
	if (res == -1) {
#ifdef DEBUGX
		if(theConf.traceflag & (1<<OTAD))
			printf("%sSend GET request to server failed\n",OTADT);
#endif
		task_fatal_error();
		goto exit;
	} else {
#ifdef DEBUGX
		if(theConf.traceflag & (1<<OTAD))
			printf("%sSend GET request to server succeeded\n",OTADT);
#endif
	}

	update_partition = esp_ota_get_next_update_partition(NULL);
#ifdef DEBUGX
	if(theConf.traceflag & (1<<OTAD))
		printf("%sWriting to partition %s subtype %d at offset 0x%x\n",OTADT,
			update_partition->label,update_partition->subtype, update_partition->address);
#endif
	assert(update_partition != NULL);
	err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
	if (err != ESP_OK) {
#ifdef DEBUGX
		if(theConf.traceflag & (1<<OTAD))
			printf( "%sesp_ota_begin failed, error=%d\n",OTADT, err);
#endif
		task_fatal_error();
		goto exit;
	}
#ifdef DEBUGX
	if(theConf.traceflag & (1<<OTAD))
		printf("%sesp_ota_begin succeeded\n",OTADT);
#endif

	xTaskCreate(&ConfigSystem, "cfg", 512, (void*)50, 3, &blinker);

	bool resp_body_start,fflag;
	resp_body_start=false;
	fflag = true;
	/*deal with all receive packet*/
	bool  bb;
	bb=false;
	while (fflag) {
		memset(tempb, 0, BUFFSIZE);
		int buff_len = recv(socket_id, tempb, TEXT_BUFFSIZE, 0);
		if (buff_len < 0) { /*receive error*/
#ifdef DEBUGX
			if(theConf.traceflag & (1<<OTAD))
				printf("%sError: receive data error! errno=%d\n", OTADT,errno);
#endif
			task_fatal_error();
			if(blinker)
				vTaskDelete(blinker);
			goto exit;

		} else
			if (buff_len > 0 && !resp_body_start && !bb) { /*deal with response header*/
				bb=true;
				resp_body_start = read_past_http_header(tempb, buff_len, update_handle);
#ifdef DEBUGX
				if(theConf.traceflag & (1<<OTAD))
					printf("%sHeader found %s\n",OTADT,resp_body_start?"Yes":"No");
#endif
			} else
				if (buff_len > 0 && resp_body_start) { /*deal with response body*/
					err = esp_ota_write( update_handle, (const void *)tempb, buff_len);
					if (err != ESP_OK) {
#ifdef DEBUGX
						if(theConf.traceflag & (1<<OTAD))
							printf( "%sError: esp_ota_write failed! err=0x%x\n",OTADT, err);
#endif
						task_fatal_error();
						if(blinker)
							vTaskDelete(blinker);
						goto exit;
					}
					binary_file_length += buff_len;
					//if(deb)
					//	printf("Have written image length %d\n", binary_file_length);

					como = !como;
#ifdef DEBUGX
					if(theConf.traceflag & (1<<OTAD))
					{
						if(como)
							printf(".");
						else
							printf("!");
						fflush(stdout);
					}
#endif
				} else
					if (buff_len == 0) {  /*packet over*/
						fflag = false;
#ifdef DEBUGX
						if(theConf.traceflag & (1<<OTAD))
							printf( "%sConnection closed, all packets received\n",OTADT);
#endif
						close(socket_id);
					} else {
#ifdef DEBUGX
						if(theConf.traceflag & (1<<OTAD))
							printf("%sUnexpected recv result\n",OTADT);
#endif
					}
	}
#ifdef DEBUGX
	if(theConf.traceflag & (1<<OTAD))
		printf("\n%sTotal Write binary data length : %d\n",OTADT, binary_file_length);
#endif

	if (esp_ota_end(update_handle) != ESP_OK) {
#ifdef DEBUGX
		if(theConf.traceflag & (1<<OTAD))
			printf( "%sesp_ota_end failed!\n",OTADT);
#endif
		task_fatal_error();
		if(blinker)
			vTaskDelete(blinker);
		goto exit;
	}
	err = esp_ota_set_boot_partition(update_partition);
	if (err != ESP_OK) {
#ifdef DEBUGX
		if(theConf.traceflag & (1<<OTAD))
			printf( "%sesp_ota_set_boot_partition failed! err=0x%x\n",OTADT, err);
#endif
		task_fatal_error();
		if(blinker)
			vTaskDelete(blinker);
		goto exit;
	}
exit:	printf("Prepare to restart system!\n");
	for(int a=0;a<MAXDEVS;a++)
		pcnt_counter_pause((pcnt_unit_t)a);
	for(int a=0;a<MAXDEVS;a++)
		write_to_fram(a,false); //save shit

	vTaskDelay(3000 /  portTICK_RATE_MS);
	esp_restart();
	return ;
}

static esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
    //        printf("%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
            	memcpy(evt->user_data+van,evt->data,evt->data_len);
            	van+=evt->data_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}


void loadit()
{
	van=0;
	esp_http_client_config_t lconfig;
	memset(&lconfig,0,sizeof(lconfig));
	lconfig.user_data=malloc(20000);
	u8* aqui=(u8*)lconfig.user_data;


	if(!lconfig.user_data)
	{
		printf("Failed RAM \n");
		return;
	}
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		printf("%sLoading tariffs\n",CMDDT);
#endif
	// = {
	lconfig.url = "http://feediot.c1.biz/tarifasPer.txt";
	lconfig.event_handler = _http_event_handle;
	//};
	esp_http_client_handle_t client = esp_http_client_init(&lconfig);
	esp_err_t err = esp_http_client_perform(client);
	if (err == ESP_OK) {
	   printf("%sStatus = %d, content_length = %d\n",WEBDT,
	           esp_http_client_get_status_code(client),
	           esp_http_client_get_content_length(client));
	}
	esp_http_client_cleanup(client);

#ifdef DEBUGX
	for (int a=0;a<30;a++)
	{
		aqui=(u8*)lconfig.user_data+a;
		printf("%02x ",*aqui);
		fflush(stdout);
	}
	printf("\n");
#endif

	err=fram.writeMany(TARIFADIA,(u8*)lconfig.user_data,van);
	if(err!=0)
		printf("Failed write tariff %d\n",err);

	if(lconfig.user_data)
		free(lconfig.user_data);
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
    int i;
    pcnt_evt_t evt;
    portBASE_TYPE HPTaskAwoken = pdFALSE;

    for (i = 0; i < PCNT_UNIT_MAX+1; i++) {
        if (intr_status & (BIT(i))) {
            evt.unit = i;
            evt.status = PCNT.status_unit[i].val;
            PCNT.int_clr.val = BIT(i);

            xQueueSendFromISR(pcnt_evt_queue, &evt, &HPTaskAwoken);
            if (HPTaskAwoken == pdTRUE) {
                portYIELD_FROM_ISR();
            }
        }
    }
}

static void pcnt_basic_init(uint16_t a, uint16_t th1)
{
	pcnt_config_t pcnt_config;
	memset((void*)&pcnt_config,0,sizeof(pcnt_config));

	pcnt_config.ctrl_gpio_num 	= -1; // -1 DOES NOT work
	pcnt_config .channel 		= PCNT_CHANNEL_0;
	pcnt_config .pos_mode 		= PCNT_COUNT_INC;   // Count up on the positive edge
	pcnt_config.neg_mode 		= PCNT_COUNT_DIS;   // Keep the counter value on the negative edge
	pcnt_config.lctrl_mode 		= PCNT_MODE_KEEP; // Reverse counting direction if low
	pcnt_config .hctrl_mode 	= PCNT_MODE_KEEP;    // Keep the primary counter mode if high

	pcnt_config.pulse_gpio_num = theMeters[a].pin;
	pcnt_config.unit = (pcnt_unit_t)a;
	pcnt_unit_config(&pcnt_config);

	pcnt_event_disable((pcnt_unit_t)a, PCNT_EVT_H_LIM);
	pcnt_event_disable((pcnt_unit_t)a, PCNT_EVT_L_LIM);
	pcnt_event_disable((pcnt_unit_t)a, PCNT_EVT_ZERO);

	pcnt_set_filter_value((pcnt_unit_t)a, 1000);
	pcnt_filter_enable((pcnt_unit_t)a);

	pcnt_set_event_value((pcnt_unit_t)a, PCNT_EVT_THRES_1, th1);
	pcnt_event_enable((pcnt_unit_t)a, PCNT_EVT_THRES_1);
#ifdef WITHWATCH
	pcnt_set_event_value((pcnt_unit_t)a, PCNT_EVT_THRES_0, 10);
	pcnt_event_enable((pcnt_unit_t)a, PCNT_EVT_THRES_0);
#endif

	pcnt_counter_pause((pcnt_unit_t)a);
	pcnt_counter_clear((pcnt_unit_t)a);
	pcnt_intr_enable((pcnt_unit_t)a);
	pcnt_counter_resume((pcnt_unit_t)a);
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

	uint16_t sobran;

    pcnt_isr_register(pcnt_intr_handler, NULL, 0,&user_isr_handle);

	for(int a=0;a<MAXDEVS;a++)
	{
		theMeters[a].pos=a;

//		sobran=theMeters[a].beatsPerkW/10 -(theMeters[a].currentBeat % (theMeters[a].beatsPerkW/10));
		sobran=theMeters[a].beatsPerkW/10 -(theMeters[a].currentBeat % (theMeters[a].beatsPerkW/10));
#ifdef DEBUGX
		if(theConf.traceflag & (1<<INTD))
			printf("%sInit TH1 %d\n",INTDT,sobran);
#endif

		theMeters[a].minamps=0xffffffff;
		theMeters[a].maxamps=0;
		theMeters[a].ampTime=0;
		pcnt_basic_init(a,10);
	}
}

static void getMessage(void *pArg)
//static void getMessage(int sock)
{
    int len;
  //  struct timeval to;
    cmdType comando;
    task_param *theP=(task_param*)pArg;


    int sock =theP->sock_p;
    int pos=theP->pos_p;
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		printf("%sStarting GetM Fd %d Pos %d\n",CMDDT,sock,pos);
#endif
  //  to.tv_sec = 2;
    //to.tv_usec = 0;

//	if (setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&to,sizeof(to)) < 0)
//	{
//		printf("Unable to set read timeout on socket!");
//		return;
//	}
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		printf("%sGetm%d heap %d\n",CMDDT,sock,esp_get_free_heap_size());
#endif

		while(1)
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
					gpio_set_level((gpio_num_t)WIFILED, 1);
					llevoMsg++;
					comando.mensaje[len] = 0;
					comando.pos=pos;
					comando.fd=sock;
					if(mqttR)
						xQueueSend(mqttR,&comando,0);
					gpio_set_level((gpio_num_t)WIFILED, 0);

					//break; //if break, will not allow for stream of multiple messages. Must not break. Close or Timeout closes socket
				}
			} while (len > 0);
		}
	exit:
	losMacs[pos].theHandle=NULL;
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		printf("%sGetm task failed leaving\n",CMDDT);
#endif
	shutdown(sock, 0);
	close(sock);
	if(comando.mensaje)
		free(comando.mensaje);
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

    while (true) {

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
         if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt))<0) {
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


        while (true) {
            sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
            if (sock < 0) {
                ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
                break;
            }

            struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&source_addr;
            struct in_addr ipAddr = pV4Addr->sin_addr;

            char str[INET_ADDRSTRLEN];
            char str2[INET_ADDRSTRLEN];
            inet_ntop( AF_INET, &ipAddr, str, INET_ADDRSTRLEN );
            inet_ntop( AF_INET,(in_addr*)&losMacs[0].theIp, str2, INET_ADDRSTRLEN );

            sprintf(tt,"getm%d",sock);
    //        printf("%s Heap %d\n",tt,esp_get_free_heap_size());
	//		gpio_set_level((gpio_num_t)WIFILED, 1);
            int  a,b;
            memcpy((void*)&a,(void*)&losMacs[0].theIp,4);
            memcpy((void*)&b,(void*)&ipAddr,4);

        //    printf("Source Ip (%s)%x == (%s)%x \n",str2,a,str,b);
            a=find_ip(b);
			if(a<0)
			{
#ifdef DEBUGX
				if(theConf.traceflag & (1<<CMDD))
					printf("%sInternal error no Ip %x\n",CMDDT,b);
#endif
			}
            //	else
            //		printf("Found Ip %x at Pos %d ip %x\n",b,a,losMacs[a].theIp);
			losMacs[a].theSock=sock;
			theP.pos_p=a;
			theP.sock_p=sock;
        	xTaskCreate(&getMessage,tt,GETMT,(void*)&theP, 4, &losMacs[a].theHandle);
       // 	getMessage(sock);
	//		gpio_set_level((gpio_num_t)WIFILED, 0);

        }
#ifdef DEBUGX
		if(theConf.traceflag & (1<<CMDD))
			printf("%sBuildmgr listend error %d\n",CMDDT,errno);
#endif
    }
    printf("BuildMgr will die OMG....\n");
    vTaskDelete(NULL);
}

static void close_mac(int cual)
{
	int err;
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		printf("%sClosing[%d] FD %d\n",CMDDT,cual,losMacs[cual].theSock);
#endif
	if(losMacs[cual].theHandle)
		vTaskDelete(losMacs[cual].theHandle);// kill the task
	if(losMacs[cual].theBuffer)
		free(losMacs[cual].theBuffer); //free the buffer
	if(losMacs[cual].theSock>=3)
		close(losMacs[cual].theSock); //close the socket

	if(cual==vanMacs-1)
	{
		memset((void*)&losMacs[cual],0,sizeof(losMacs[cual]));
		vanMacs--;
		return;
	}
	memmove(&losMacs[cual],&losMacs[cual+1],(vanMacs-cual-1)*sizeof(losMacs[0]));
	vanMacs--;
	if (vanMacs<0)//just in case
		vanMacs=0;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
	u32 newmac;
	wifi_event_ap_staconnected_t *ev=(wifi_event_ap_staconnected_t*)event_data;
    switch (event_id) {

    case  WIFI_EVENT_AP_STACONNECTED:
#ifdef DEBUGX
		if(theConf.traceflag & (1<<WIFID))
    	printf("%sWIFIMac connected %02x:%02x:%02x:%02x:%02x:%02x\n",WIFIDT,ev->mac[0],ev->mac[1],ev->mac[2],ev->mac[3],ev->mac[4],ev->mac[5]);
#endif
    	memcpy((void*)&newmac,&ev->mac[2],4);
    	update_mac(newmac);
    	break;
	case WIFI_EVENT_AP_STADISCONNECTED:
    //	printf("Mac disco %02x:%02x:%02x:%02x:%02x:%02x\n",ev->mac[0],ev->mac[1],ev->mac[2],ev->mac[3],ev->mac[4],ev->mac[5]);
		break;
    case WIFI_EVENT_AP_START:
        	break;
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            wifif=false;
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            wifif=false;
            xEventGroupClearBits(wifi_event_group, WIFI_BIT);
            break;
        default:
            break;
    }
    return;
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

	localtime_r(&loginData.thedate, &timeinfo);
	diaHoraTarifa=loginData.theTariff;// Host will give us Hourly Tariff. No need to store

	oldMesg=mesg=timeinfo.tm_mon;
	oldDiag=diag=timeinfo.tm_mday-1;
	yearg=timeinfo.tm_year+1900;
	oldHorag=horag=timeinfo.tm_hour;
	yearDay=timeinfo.tm_yday;
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
	yearDay=timeinfo.tm_yday;

	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		printf("%s[CMDD]The current date/time in %s is: %s day of Year %d\n",CMDDT, LOCALTIME,strftime_buf,timeinfo.tm_yday);
#endif

    }
    xEventGroupSetBits(wifi_event_group, SNTP_BIT);

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
				printf("%sNot registered mac %x found \n",CMDDT,bigmac);
#endif

		}
		else
			memcpy((void*)&losMacs[este].theIp,(void*)&station.ip,4);
	}
}

static void update_mac(u32 newmac)
{


	 int este=find_mac(newmac);
 	 if(este>=0)
 		close_mac(este);

 	 losMacs[vanMacs].trans=0;
 	 losMacs[vanMacs].theIp=0;
 	 losMacs[vanMacs].theHandle=NULL;
 	 losMacs[vanMacs].theBuffer=NULL;
 	 losMacs[vanMacs].theSock=-1;
 	 losMacs[vanMacs++].macAdd=newmac;
	//now add it
}

static void ip_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
	 wifi_config_t wifi_config;
	// ip_event_got_ip_t *ev=(ip_event_got_ip_t*)event_data;

	    memset(&wifi_config,0,sizeof(wifi_config));//very important

    switch (event_id) {
    case IP_EVENT_AP_STAIPASSIGNED:
//    	printf("\nIP Assigned:" IPSTR, IP2STR(&ev->ip_info.ip));
    	update_ip();
    	break;
        case IP_EVENT_STA_GOT_IP:
//            strcpy((char*)wifi_config.ap.ssid,"Meteriot");
//			strcpy((char*)wifi_config.ap.password,"Meteriot");
//			wifi_config.ap.authmode=WIFI_AUTH_WPA_PSK;
//			wifi_config.ap.ssid_hidden=true;
//			wifi_config.ap.beacon_interval=400;
//			wifi_config.ap.max_connection=50;
//			wifi_config.ap.ssid_len=0;
//			wifi_config.ap.channel=1;
//			ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));

        	wifif=true;
            xEventGroupSetBits(wifi_event_group, WIFI_BIT);
        	xTaskCreate(&sntpget,"sntp",2048,NULL, 4, NULL);
            break;
        default:
            break;
    }
    return;
}

static void wifi_init(void)
{
	wifi_init_config_t 				cfg=WIFI_INIT_CONFIG_DEFAULT();
		wifi_config_t 					sta_config,configap;
		tcpip_adapter_ip_info_t 		ipInfo;

		tcpip_adapter_init();

   // esp_netif_init();

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
   // esp_netif_create_default_wifi_sta();
    //esp_netif_create_default_wifi_ap();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &ip_event_handler, NULL));

 //   wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    wifi_config_t wifi_config;

    //AP section
    memset(&wifi_config,0,sizeof(wifi_config));//very important
    strcpy((char*)wifi_config.ap.ssid,"Meteriot");
	strcpy((char*)wifi_config.ap.password,"Meteriot");
	wifi_config.ap.authmode=WIFI_AUTH_WPA_PSK;
	wifi_config.ap.ssid_hidden=false;
	wifi_config.ap.beacon_interval=400;
	wifi_config.ap.max_connection=50;
	wifi_config.ap.ssid_len=0;
	wifi_config.ap.channel=1;
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));

	//STA section
	memset(&wifi_config,0,sizeof(wifi_config));//very important
	strcpy((char*)wifi_config.sta.ssid,INTERNET);
	strcpy((char*)wifi_config.sta.password,INTERNETPASS);

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_LOGI(TAG, "start the WIFI SSID:[%s] password:[%s]", wifi_config.sta.ssid,wifi_config.sta.password );
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Waiting for wifi");
    xEventGroupWaitBits(wifi_event_group, WIFI_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Wifi Acquired");
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            esp_mqtt_client_subscribe(client, "MeterIoT/Chillo/Chillo/CMD", 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits(wifi_event_group, MQTT_BIT);
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            xEventGroupSetBits(wifi_event_group, MQTT_BIT);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            xEventGroupSetBits(wifi_event_group, PUB_BIT|DONE_BIT);//message sent bit
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
#ifdef DEBUGX
        	if(theConf.traceflag & (1<<MQTTD))
            	printf("%sPublished\n",MQTTDT);
#endif
            esp_mqtt_client_unsubscribe(client, "MeterIoT/Chillo/Chillo/CMD");//bit is set by unsubscribe
            break;

        case MQTT_EVENT_DATA:
           ESP_LOGI(TAG, "MQTT_EVENT_DATA");
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
            	xQueueSend( mqttR,&theCmd,0 );
            }
            break;
        case MQTT_EVENT_ERROR:
           ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
           xEventGroupClearBits(wifi_event_group, MQTT_BIT);
            break;
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG, "MQTT_EVENT_BEFORE");
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

static void sendStatusNow(meterType* meter)
{
	cJSON *root=cJSON_CreateObject();
	if(root==NULL)
	{
		printf("cannot create root\n");
		return;
	}

	cJSON_AddStringToObject(root,"MeterPin",		meter->serialNumber);
	cJSON_AddNumberToObject(root,"Transactions",	++meter->vanMqtt);
	cJSON_AddNumberToObject(root,"KwH",				meter->curLife);
	cJSON_AddNumberToObject(root,"Beats",			meter->currentBeat);
	cJSON_AddNumberToObject(root,"Pos",				meter->pos);
#ifdef TEMP
		cJSON_AddNumberToObject(root,"Temp",			ttemp);
#endif

	char *rendered=cJSON_Print(root);
#ifdef DEBUGX
	if(theConf.traceflag & (1<<MQTTT))
	{
		printf("%sPublishing message Sent %d\n",MQTTTT,++sentTotal);
	}
#endif
	esp_mqtt_client_publish(clientCloud, "MeterIoT/Chillo/Chillo/CONTROL", rendered, 0, 1,0);

	free(rendered);
	cJSON_Delete(root);
	}


static void submode(void * pArg)
{
	 meterType meter;

	while(1)
	{
		if(wifif) // is the network up?
		{
			if( xQueueReceive( mqttQ, &meter, portMAX_DELAY ))
			{
				xQueueSend( framQ,&meter,0);//copy to fram Manager
#ifdef DEBUGX
				if(theConf.traceflag & (1<<CMDD))
					printf("%sHeap after submode rx %d\n",CMDDT,esp_get_free_heap_size());

				if(theConf.traceflag & (1<<CMDD))
					printf("%sReady to send for Pin %d Pos %d Qwaiting=%d\n",CMDDT,meter.pin,meter.pos,uxQueueMessagesWaiting( mqttQ ));
#endif
				if(!clientCloud) //just in case
						printf("Error no client mqtt\n");
				else
				{
#ifdef DEBUGX
					if(theConf.traceflag & (1<<CMDD))
						printf("%sStarting mqtt\n",CMDDT);
#endif
					xEventGroupClearBits(wifi_event_group, MQTT_BIT);//clear flag to wait on

				//	if(esp_mqtt_get_state(clientCloud) < MQTT_STATE_INIT)// must be 0 or less
						if(1)// must be 0 or less
					{
						if(esp_mqtt_client_start(clientCloud)==ESP_OK) //we got an OK
						{
								// when connected send the message
						//	if (deb)
						//		printf("Wait Connect State %d\n",esp_mqtt_get_state(clientCloud));
							//wait for the CONNECT mqtt msg
							if(xEventGroupWaitBits(wifi_event_group, MQTT_BIT, false, true, 4000/  portTICK_RATE_MS))
							{
								//wait max 4secs maxs for connection
#ifdef DEBUGX
								if(theConf.traceflag & (1<<CMDD))
									printf("%sSending Mqtt state\n",CMDDT);
#endif
								xEventGroupClearBits(wifi_event_group, PUB_BIT); // clear our waiting bit
								sendStatusNow(&theMeters[meter.pos]);

							//	sendStatusNow(&algo);//send whatever message
								//wait 4 secs tops for Publish
								if(!xEventGroupWaitBits(wifi_event_group, PUB_BIT, false, true,  4000/  portTICK_RATE_MS))
											printf("Wait fail send\n");
							}
							//have to stop in either case
						//	if(esp_mqtt_get_state(clientCloud) >= MQTT_STATE_INIT)
								if(1)
								{
									//xEventGroupClearBits(wifi_event_group, STOP_BIT); // clear our waiting bit
									esp_mqtt_client_stop(clientCloud);
								//	if(!xEventGroupWaitBits(wifi_event_group, STOP_BIT, false, true,  4000/  portTICK_RATE_MS))
									//	printf("Could not stop\n");
								//	else
#ifdef DEBUGX
									if(theConf.traceflag & (1<<CMDD))
										printf("%sStopping\n",CMDDT);
#endif
								}
					}
						else
						 printf("Failed to start when available\n");
				}
					else
					{
					//	if(esp_mqtt_get_state(clientCloud) >= MQTT_STATE_INIT)
							if(1)
						{
							//xEventGroupClearBits(wifi_event_group, STOP_BIT); // clear our waiting bit
								esp_mqtt_client_stop(clientCloud);
							//if(!xEventGroupWaitBits(wifi_event_group, STOP_BIT, false, true,  4000/  portTICK_RATE_MS))
							//	printf("Could not stop2\n");
							//else
							//	printf("Stopped 2\n");

						}
					}
				}
#ifdef DEBUGX
				if(theConf.traceflag & (1<<CMDD))
					printf("%sSent %d pin %d. Heap after submode %d\n",CMDDT,++sentTotal,meter.pin,esp_get_free_heap_size());
#endif
			}
		else
			vTaskDelay(100 /  portTICK_RATE_MS);
		}
		else
			vTaskDelay(100 /  portTICK_RATE_MS);
	}
}

static void mqtt_app_start(void)
{
	     esp_mqtt_client_config_t mqtt_cfg;
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

//load firmware. Rather expensive task. Should shutdown everyting
void firmwareCmd()
{

		xTaskCreate(&firmUpdate,"U571",10240,NULL, 5, NULL);
}

 static void loginCmd(parg* argument)
{
	loginT loginData;
	time(&loginData.thedate);
	loginData.theTariff=100;
	send(argument->pComm, &loginData, sizeof(loginData), 0);
}

 void statusCmd(parg *argument)
{
	loginT loginData;

		cJSON *lmac= cJSON_GetObjectItem((cJSON*)argument->pMessage,"macn");
		cJSON *lpos= cJSON_GetObjectItem((cJSON*)argument->pMessage,"Pos");
		if(lmac && lpos)
		{
			int pos=lpos->valueint;
			double dmacc=lmac->valuedouble;
			u32 macc=(u32)dmacc;
			int cual=find_mac(macc);
			if(cual>=0)
			{
				tallies[cual][pos]++;
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
#ifdef DISPLAY
		if(displayf)
		{
			clearScreen();
			cJSON *meter= cJSON_GetObjectItem((cJSON*)argument->pMessage,"MeterId");
			cJSON *tran= cJSON_GetObjectItem((cJSON*)argument->pMessage,"Transactions");
			cJSON *pulse= cJSON_GetObjectItem((cJSON*)argument->pMessage,"Pulse");
			cJSON *beats= cJSON_GetObjectItem((cJSON*)argument->pMessage,"Beats");

			if(xSemaphoreTake(I2CSem, portMAX_DELAY))
			{
				display.clear();
				if(meter)
					drawString(64,1,meter->valuestring,16,TEXT_ALIGN_CENTER,NODISPLAY,NOREP);
				if(tran)
				{
					sprintf(text,"Trans:%d",tran->valueint);
					drawString(64,18,text,12,TEXT_ALIGN_CENTER,NODISPLAY,NOREP);
				}
				if(beats)
				{
					sprintf(text,"Beats:%d",beats->valueint);
					drawString(64,32,text,12,TEXT_ALIGN_CENTER,NODISPLAY,NOREP);
				}
				if(pulse)
				{
					sprintf(text,"Pulse:%d",pulse->valueint);
					drawString(64,48,text,12,TEXT_ALIGN_CENTER,DISPLAYIT,NOREP);
				}
				xSemaphoreGive(I2CSem);
			}
		}
#endif
		time(&loginData.thedate);
		loginData.theTariff=100;
		send(argument->pComm, &loginData, sizeof(loginData), 0);
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
				cJSON *monton= cJSON_GetObjectItem(elcmd,"Batch");
				if(monton)
				{
					int son=cJSON_GetArraySize(monton);
					losMacs[cmd.pos].trans+=son;
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
							(*cmds[cualf].code)(argument);
							free(argument);
						}


					}// array
				}//batch

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
		drawString(64,10,"WiFi",24,TEXT_ALIGN_CENTER,DISPLAYIT,NOREP);
		xSemaphoreGive(I2CSem);
	}
	else
		printf("Failed to InitScreen\n");
}
#endif

#ifdef MQT
static void sender(void *pArg)
{
	meterType algo;

	while(true)
	{
		if(theConf.traceflag & (1<<CMDD))
			printf("%sHeap before send %d\n",CMDDT,esp_get_free_heap_size());
		if(!xQueueSend( mqttQ,&algo,0))
			printf("Error sending queue %d\n",uxQueueMessagesWaiting( mqttQ ));
		else
			if(theConf.traceflag & (1<<CMDD))
			{
				//printf("Sending %d\n",algo++);
				printf("%sHeap after send %d\n",CMDDT,esp_get_free_heap_size());
			}


		if(uxQueueMessagesWaiting(mqttQ)>uxQueueSpacesAvailable(mqttQ))
		{
			while(uxQueueMessagesWaiting(mqttQ)>2)
				delay(1000);
		}
		delay(qdelay);
	}
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

static void write_to_fram(u8 meter,bool addit)
{
	time_t timeH;
    struct tm timeinfo;

	// FRAM Semaphore is taken by the Interrupt Manager. Safe to work.
	scratchTypespi scratch;
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


		scratch.medidor.state=1;                    //scratch written state. Must be 0 to be ok. Every 800-1000 beats so its worth it
		scratch.medidor.meter=meter;
		scratch.medidor.month=theMeters[meter].curMonth;
		scratch.medidor.life=theMeters[meter].curLife;
		scratch.medidor.day=theMeters[meter].curDay;
		scratch.medidor.hora=theMeters[meter].curHour;
		scratch.medidor.cycle=theMeters[meter].curCycle;
		scratch.medidor.mesg=mesg;
		scratch.medidor.diag=diag;
		scratch.medidor.horag=horag;
		scratch.medidor.yearg=yearg;
		fram.write_recover(scratch);            //Power Failure recover register

		fram.write_beat(meter,theMeters[meter].currentBeat);
		fram.write_lifekwh(meter,theMeters[meter].curLife);
		fram.write_month(meter,mesg,theMeters[meter].curMonth);
		fram.write_monthraw(meter,mesg,theMeters[meter].curMonthRaw);
		fram.write_day(meter,yearg,mesg,diag,theMeters[meter].curDay);
		fram.write_dayraw(meter,yearg,mesg,diag,theMeters[meter].curDayRaw);
		fram.write_hour(meter,yearg,mesg,diag,horag,theMeters[meter].curHour);
		fram.write_hourraw(meter,yearg,mesg,diag,horag,theMeters[meter].curHourRaw);
		fram.write_lifedate(meter,theMeters[meter].lastKwHDate);  //should be down after scratch record???
		fram.write8(SCRATCH,0); //Fast write first byte of Scratch record to 0=done.
	}
		else
		{
			fram.write_beat(meter,theMeters[meter].currentBeat);
			fram.writeMany(FRAMDATE,(uint8_t*)&timeH,sizeof(timeH));//last known date
		}
}

void load_from_fram(u8 meter)
{
	if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
	{
		if (diaHoraTarifa==0)
			diaHoraTarifa=100;
		fram.read_lifekwh(meter,(u8*)&theMeters[meter].curLife);
		fram.read_lifedate(meter,(u8*)&theMeters[meter].lastKwHDate);
		fram.read_month(meter, mesg, (u8*)&theMeters[meter].curMonth);
		fram.read_monthraw(meter, mesg, (u8*)&theMeters[meter].curMonthRaw);
		fram.read_day(meter, yearg,mesg, diag, (u8*)&theMeters[meter].curDay);
		fram.read_dayraw(meter, yearg,mesg, diag, (u8*)&theMeters[meter].curDayRaw);
		fram.read_hour(meter, yearg,mesg, diag, horag, (u8*)&theMeters[meter].curHour);
		fram.read_hourraw(meter, yearg,mesg, diag, horag, (u8*)&theMeters[meter].curHourRaw);
		fram.read_beat(meter,(u8*)&theMeters[meter].currentBeat);
		totalPulses+=theMeters[meter].currentBeat;
		if(theConf.beatsPerKw[meter]==0)
			theConf.beatsPerKw[meter]=400;// just in case div by 0 crash
		if(theMeters[meter].beatsPerkW==0)
			theMeters[meter].beatsPerkW=400;// just in case div by 0 crash
		u16 nada=theMeters[meter].currentBeat/theConf.beatsPerKw[meter];
		theMeters[meter].beatSave=theMeters[meter].currentBeat-(nada*theConf.beatsPerKw[meter]);
		xSemaphoreGive(framSem);
#ifdef DEBUGX
		if(theConf.traceflag & (1<<FRAMD))
			printf("[FRAMD]Loaded Meter %d curLife %d beat %d\n",meter,theMeters[meter].curLife,theMeters[meter].currentBeat);
#endif
	}
}


static void recover_fram()
{
	char textl[100];

	scratchTypespi scratch;
	if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
	{
		fram.read_recover(&scratch);

		if (scratch.medidor.state==0)
		{
			xSemaphoreGive(framSem);
			return;
		}

		sprintf(textl,"PF Recover. Meter %d State %d Life %x\n",scratch.medidor.meter,scratch.medidor.state,scratch.medidor.life);
		fram.write_lifekwh(scratch.medidor.meter,scratch.medidor.life);
		fram.write_month(scratch.medidor.meter,scratch.medidor.mesg,scratch.medidor.month);
		fram.write_day(scratch.medidor.meter,scratch.medidor.yearg,scratch.medidor.mesg,scratch.medidor.diag,scratch.medidor.day);
		fram.write_hour(scratch.medidor.meter,scratch.medidor.yearg,scratch.medidor.mesg,scratch.medidor.diag,scratch.medidor.horag,scratch.medidor.hora);
		fram.write8(SCRATCH,0); //Fast write first byte of Scratch record to 0=done.

		xSemaphoreGive(framSem);
		printf("Recover %s",textl);
	}
}


static void init_fram()
{
	scratchTypespi scratch;
	// FRAM Setup
	fram.begin(FMOSI,FMISO,FCLK,FCS,&framSem); //will create SPI channel and Semaphore
	//framWords=fram.intframWords;
	spi_flash_init();

	if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
	{
		fram.read_recover(&scratch);
		xSemaphoreGive(framSem);
	}

	if (scratch.medidor.state!=0)
	{
		//  check_log_file(); //Our log file. Need to open it before normal sequence
#ifdef DEBUGX
		if(theConf.traceflag & (1<<FRAMD))
			printf("Recover Fram\n");
#endif
		recover_fram();
		//recf=true;
	}

	//load all devices counters from FRAM
	for (int a=0;a<MAXDEVS;a++)
		load_from_fram(a);

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
			if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
			{
				write_to_fram(theMeter.pos,theMeter.saveit);
				if(theConf.traceflag & (1<<FRAMD))
					printf("%sSaving Meter %d add %d Beats %d\n",FRAMDT,theMeter.pos,theMeter.saveit,theMeters[theMeter.pos].currentBeat);
				xSemaphoreGive(framSem);
			}
		}
		else
		{
			if(theConf.traceflag & (1<<FRAMD))
				printf("%sFailed framQ Manager\n",FRAMDT);
			delay(1000);
		}
	}
}
/*
static void framManager(void * pArg)
{
	 meterType theMeter;

	framQ = xQueueCreate( 20, sizeof( meterType ) );
	if(!framQ)
		printf("Failed queue Fram\n");

	while(1)
	{
		while(uxQueueMessagesWaiting(framQ)>0)
		{
			if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
			{
				while(uxQueueMessagesWaiting(framQ)>0)
				{
					if( xQueueReceive( framQ, &theMeter, 500/  portTICK_RATE_MS ))
					{
						write_to_fram(theMeter.pos,theMeter.saveit);
#ifdef DEBUGX
						if(theConf.traceflag & (1<<FRAMD))
							printf("%sSaving Meter %d add %d Beats %d\n",FRAMDT,theMeter.pos,theMeter.saveit,theMeter.currentBeat);
#endif
					}
				}
				xSemaphoreGive(framSem);
			}
		}
		delay(400);
	}
}
*/
#ifdef WITHWATCH
static void watchGuard(void * pArg)
{
	int wunit=(int)pArg;
	u16 countold,countnow,residuo,nth1;

	pcnt_get_counter_value(wunit, &countold);
#ifdef DEBUGX
	if(theConf.traceflag & (1<<INTD))
		printf("%sWatchTimer for Meter %d at %d\n",INTDT,wunit,countold);
#endif
	while(true)
	{
		delay(wDelay);

		pcnt_get_counter_value(wunit, &countnow);

#ifdef DEBUGX
		if(theConf.traceflag & (1<<INTD))
			printf("%sWatchTimer for Meter %d Countnow %d expired. Saving...\n",INTDT,wunit,countnow);
#endif

			nth1= theMeters[wunit].beatsPerkW/10-countnow;// new threshold for the main TH1.
			pcnt_basic_init(wunit,nth1);// reconfigure cause The pcnt_set_event does not have effect when changing value.

			theMeters[wunit].currentBeat+=countnow;
			theMeters[wunit].beatSave+=countnow;

			residuo=theMeters[wunit].beatSave / (theMeters[wunit].beatsPerkW*tarifasDia[horag]/100);// remmber to calculate if we reached BPK

#ifdef DEBUGX
			if(theConf.traceflag & (1<<INTD))
				printf("%sWatchResiduo %d Beat %d MeterPos %d\n",INTDT,residuo,theMeters[wunit].currentBeat,theMeters[wunit].pos);
#endif

			if(residuo && theMeters[wunit].currentBeat>0)
			{
				theMeters[wunit].saveit=true;
				theMeters[wunit].beatSave-=theMeters[wunit].beatsPerkW*tarifasDia[horag]/100;
				//printf("Beatsave Watch %d\n",theMeters[wunit].beatSave);
			}
			totalPulses+=countnow;//global sanity counter

			if(mqttQ)
				xQueueSend( mqttQ,&theMeters[wunit],0 );// send it to the Meter manager
			theMeters[wunit].lowThresHandle=NULL;//task is going to die. NULL the handle

			pcnt_event_enable((pcnt_unit_t)wunit, PCNT_EVT_THRES_0);//enable ourselves for next iteration
			vTaskDelete(NULL);//we now die
	}
}
#endif

// Pulse Counter Manager
// 2 Thresholds set: Main (TH0) for every 1/10 of Meter BPK(Beats per kwh) Secondary (TH1) for WatchDog after first 10 pulses

static void pcntManager(void * pArg)
{
	pcnt_evt_t evt;
	portBASE_TYPE res;
	u16 residuo,count,count1,tlimit;
	uint32_t timeDiff=0;

	pcnt_evt_queue = xQueueCreate( 20, sizeof( pcnt_evt_t ) );
	if(!pcnt_evt_queue)
		printf("Failed queue PCNT\n");

	while(1)
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

#ifdef WITHWATCH
				tlimit=theMeters[evt.unit].beatsPerkW*tarifasDia[horag]/1000;

				if(count!=tlimit)
					pcnt_basic_init(evt.unit,tlimit);//next to be round to 80

				pcnt_event_enable((pcnt_unit_t)evt.unit, PCNT_EVT_THRES_0);// enable Watchdog event
#endif

				if(theMeters[evt.unit].ampTime>0)
					timeDiff=millis()-theMeters[evt.unit].ampTime;
				theMeters[evt.unit].ampTime=millis();

///// Amps section calculations  /////////////
//				if(count>0 && theMeters[evt.unit].ampTime>0 )
//				{
//					float amps=(3600000.0/theMeters[evt.unit].beatsPerkW)/(theMeters[evt.unit].ampTime/count)*3.0303;
//					uint32_t iAmps=(uint32_t)amps;
//					if(iAmps<theMeters[evt.unit].minamps)
//						theMeters[evt.unit].minamps=iAmps;
//					if(iAmps>theMeters[evt.unit].maxamps)
//						theMeters[evt.unit].maxamps=iAmps;
//				}

				totalPulses+=count;

// At this point timer is irrelevant. Kill it if active
				if(theMeters[evt.unit].lowThresHandle)
				{
#ifdef DEBUGX
					if(theConf.traceflag & (1<<INTD))
						printf("%sKilling task unit %d\n",INTDT,evt.unit);
#endif
					vTaskDelete(theMeters[evt.unit].lowThresHandle);
					theMeters[evt.unit].lowThresHandle=NULL;
				}

				// THE counters. the whole point is this
				theMeters[evt.unit].saveit=false;
				theMeters[evt.unit].currentBeat+=count;
				theMeters[evt.unit].beatSave+=count;

				if(tarifasDia[horag]==0)
					tarifasDia[horag]=100;

				residuo=theMeters[evt.unit].beatSave % (theMeters[evt.unit].beatsPerkW*tarifasDia[horag]/100);

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
					theMeters[evt.unit].saveit=true;

					//theMeters[evt.unit].beatSave-=theMeters[evt.unit].beatsPerkW*diaHoraTarifa/100;
					theMeters[evt.unit].beatSave-=theMeters[evt.unit].beatsPerkW*tarifasDia[horag]/100;
				}

				if(mqttQ)
					xQueueSend( mqttQ,&theMeters[evt.unit],0 );// send the message to the Meter Manager
			}
			else
				if (evt.status & PCNT_EVT_THRES_0) // minimum to consider firing a timer
				{
				//	delay(300);// see any more coming before firing timer
#ifdef WITHWATCH
					pcnt_get_counter_value((pcnt_unit_t)evt.unit,(short int *) &count1);
					if(!(count1>count)) //used if delay(300) else dumb
					{	// nope, fire the timer. For now immediate
#ifdef DEBUGX
						if(theConf.traceflag & (1<<INTD))
							printf("%sFire Watch Th1 ThresMain %d count %d\n",INTDT,count1,count);
#endif
						pcnt_event_disable((pcnt_unit_t)evt.unit, PCNT_EVT_THRES_0);
						xTaskCreate(&watchGuard,"wguard",2048,(void*)evt.unit, 4, &theMeters[evt.unit].lowThresHandle);// launch task. 2048 if Printf
					}
#endif

				}
        } else
            printf("PCNT Failed Queue\n");
	}
}


static void init_vars()
{
	//zero some stuff
	vanadd=0;
	llevoMsg=0;
	memset(&losMacs,0,sizeof(losMacs));
	vanMacs=0;
    qwait=QDELAY;
    qdelay=qwait*1000;
   	wDelay=TIMEWAITPCNT;// pulse WatchDog timer default value

   	//activity led
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_down_en =GPIO_PULLDOWN_ENABLE;
	io_conf.pin_bit_mask = (1ULL<<WIFILED);
	gpio_config(&io_conf);
	gpio_set_level((gpio_num_t)WIFILED, 0);

	//message from Meters
	strcpy((char*)&cmds[0].comando,"/ga_firmware");			cmds[0].code=firmwareCmd;
	strcpy((char*)&cmds[1].comando,"/ga_tariff");			cmds[1].code=loadit;
	strcpy((char*)&cmds[2].comando,"/ga_status");			cmds[2].code=statusCmd;
	strcpy((char*)&cmds[3].comando,"/ga_login");			cmds[3].code=loginCmd;

	//debug stuff
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
	strcpy(lookuptable[9],"HEAPD");
	strcpy(lookuptable[10],"INTD");
	strcpy(lookuptable[11],"FRAMD");
	strcpy(lookuptable[12],"MSGD");

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
	theConf.ssl=0;
	theConf.beatsPerKw[0]=800;//old way
	theConf.bounce[0]=100;
	//    fram.write_tarif_bpw(0, 800); // since everything is going to be 0, BPW[0]=800 HUMMMMMM????? SHould load Tariffs after this

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

void app_main()
{

	//#ifdef KBD
	//    esp_log_level_set("*", ESP_LOG_NONE);
	//#else
	esp_log_level_set("*", ESP_LOG_WARN);
	//#endif

	printf("Version Direct2 Tarifadia %d\n",TARIFADIA);

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
#endif
	printf("\n%sFlash Button to erase config 3 secs%s\n",RED,RESETC);// chance to erase configuration and start fresh
	delay(3000);

	if (theConf.centinel!=CENTINEL || !gpio_get_level((gpio_num_t)0))
		erase_config();//start fresh

	init_vars();// setup initial values
	init_fram();//connect to fram to save readings
    wifi_init();// this a Master Controller. Connect to WiFi
    mqtt_app_start();// Start MQTT connection


    if(theConf.traceflag & (1<<BOOTD))
    		printf("%s Fram Id %04x Fram Size %d%s\n",MAGENTA,fram.prodId,fram.intframWords,RESETC);

	if((theConf.traceflag & (1<<BOOTD)) && fram.intframWords>0)
	{
		printf("%s=============== FRAM ===============%s\n",RED,YELLOW);
		printf("FRAMDATE(%s%d%s)=%s%d%s\n",GREEN,FRAMDATE,YELLOW,CYAN,METERVER-FRAMDATE,RESETC);
		printf("METERVER(%s%d%s)=%s%d%s\n",GREEN,METERVER,YELLOW,CYAN,FREEFRAM-METERVER,RESETC);
		printf("FREEFRAM(%s%d%s)=%s%d%s\n",GREEN,FREEFRAM,YELLOW,CYAN,SCRATCH-FREEFRAM,RESETC);
		printf("SCRATCH(%s%d%s)=%s%d%s\n",GREEN,SCRATCH,YELLOW,CYAN,SCRATCHEND-SCRATCH,RESETC);
		printf("SCRATCHEND(%s%d%s)=%s%d%s\n",GREEN,SCRATCHEND,YELLOW,CYAN,TARIFADIA-SCRATCHEND,RESETC);
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
		printf("DATAEND(%s%d%s)=%s%d%s\n",GREEN,DATAEND,YELLOW,CYAN,TOTALFRAM-DATAEND,RESETC);
		printf("TOTALFRAM(%s%d%s) Devices %s%d%s\n",GREEN,TOTALFRAM,YELLOW,CYAN,TOTALFRAM/DATAEND,RESETC);
		printf("%s=============== FRAM ===============%s\n",RED,RESETC);

	}

#ifdef KBD
	xTaskCreate(&kbd,"kbd",4096,NULL, 4, NULL);
#endif

#ifdef DISPLAY // most likely heap damage. Routine seems unreliable
    initI2C();
    initScreen();
#endif

#ifdef TEMP // for the sake of Sensors sample
    init_temp();
#endif

#ifdef MQT //for testing only
   	xTaskCreate(&sender,"U571",10240,NULL, 5, NULL);
#endif

   	xTaskCreate(&cmdManager,"cmdMgr",10240,NULL, 5, NULL); //cmds recevied from the Host Controller or Meters
	xTaskCreate(&framManager,"framMgr",4096,NULL, 4, NULL);// Fram Manager

	xTaskCreate(&pcntManager,"pcntMgr",4096,NULL, 4, NULL);// We also control 5 meters. Pseudo ISR. This is d manager

	xTaskCreate(&buildMgr,"TCP",10240,(void*)1, 4, NULL);// Messages from the Meters. Controller Section socket manager

#ifdef DISPLAY
	xTaskCreate(&displayManager,"dispMgr",4096,NULL, 4, NULL);
#endif
	EventBits_t uxBits = xEventGroupWaitBits(wifi_event_group, SNTP_BIT, false, true, 10000/  portTICK_RATE_MS);//
	if( ( uxBits & SNTP_BIT ) != 0 )
	{
		memset(&tarifasDia,0,sizeof(tarifasDia));
		int err=fram.read_tarif_day(yearDay,(u8*)&tarifasDia);
		if(err!=0)
			printf("Error %d reading Tariffs day %d...recovering from Host\n",err,yearDay);

		u16 ttar=0;
		for (int aa=0;aa<24;aa++)
			ttar+=tarifasDia[aa];
		if(ttar==0) // load tariffs
		{
			printf("%sLoading Tariffs from host%s %s%s\n",RED,MAGENTA,"http://feediot.c1.biz/tarifasPer.txt",RESETC);
			loadit();
		}
		pcnt_init();// initialize it. Several tricks apply. Read there

#ifdef DEBUGX
		if(theConf.traceflag & (1<<CMDD))
		{
			printf("%sDay[%d]",CMDDT,yearDay);
			for (int aa=0;aa<24;aa++)
				printf("[%02d]=%02x ",aa,tarifasDia[aa]);
			printf("\nHora %d Tarifa %02x\n",horag,tarifasDia[horag]);
		}
#endif
		timeKeeper();//care for the FRAM time changes in hour, day and month
	}
	else
	{
		loginT now;
		printf("Emergency recover time\n");
		fram.readMany(FRAMDATE,(uint8_t*)&now.thedate,sizeof(now.thedate));
		updateDateTime(now);
	}
}
