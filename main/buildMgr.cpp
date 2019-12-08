#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"
#include <inttypes.h>
#include "esp_sntp.h"

void drawString(int x, int y, string que, int fsize, int align,displayType showit,overType erase);
void submode(void * pArg);
void displayManager(void * pArg);
void kbd(void *pArg);
void clearScreen();

static const char *TAG = "BDMGR";

static EventGroupHandle_t wifi_event_group;
const static int MQTT_BIT = BIT0;
const static int WIFI_BIT = BIT1;
const static int PUB_BIT  = BIT2;
const static int DONE_BIT  = BIT3;

int van=0;

uint32_t IRAM_ATTR millisISR()
{
	return xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;

}

uint32_t IRAM_ATTR millis()
{
	return xTaskGetTickCount() * portTICK_PERIOD_MS;

}

void delay(uint16_t a)
{
	vTaskDelay(a /  portTICK_RATE_MS);
}

void ConfigSystem(void *pArg)
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

void task_fatal_error()
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
				if(deb)
					printf( "Error: esp_ota_write failed! err=0x%x\n", err);
				return false;
			} else {
				if(deb)
					printf( "esp_ota_write header OK\n");
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
	if(deb)
		printf("RSNServer IP: %s Server Port:%s\n", EXAMPLE_SERVER_IP, EXAMPLE_SERVER_PORT);
	sprintf(http_request, "GET %s HTTP/1.1\r\nHost: %s:%s \r\n\r\n", EXAMPLE_FILENAME, EXAMPLE_SERVER_IP, EXAMPLE_SERVER_PORT);
	int  http_connect_flag = -1;
	struct sockaddr_in sock_info;

	socket_id = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_id == -1) {
		if(deb)
			printf( "Create socket failed!\n");
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
		if(deb)
			printf( "Connect to server failed! errno=%d\n", errno);
		close(socket_id);
		return false;
	} else {
		if(deb)
			printf( "Connected to server\n");
		return true;
	}
	return false;
}

static void firmUpdate(void *pArg)
{
	esp_err_t err;
	uint8_t como=1;
	TaskHandle_t blinker=NULL;
	deb=true;
	esp_ota_handle_t update_handle = 0 ;
	const esp_partition_t *update_partition = NULL;

#ifdef WITHMETERS
	for (int a=0;a<4;a++)
		gpio_isr_handler_remove((gpio_num_t)theMeters[a].pin);
#endif

	if(deb)
		printf("Starting OTA...\n");

	const esp_partition_t *configured = esp_ota_get_boot_partition();
	const esp_partition_t *running = esp_ota_get_running_partition();

	assert(configured == running); /* fresh from reset, should be running from configured boot partition */
	if(deb)
		printf("Running partition type %d subtype %d (offset 0x%08x)\n",configured->type, configured->subtype, configured->address);

	/*connect to http server*/
	if (connect_to_http_server()) {
		if(deb)
			printf( "Connected to http server\n");
	} else {
		if(deb)
			printf( "Connect to http server failed!\n");
		task_fatal_error();
		goto exit;
	}

	int res;
	/*send GET request to http server*/
	res = send(socket_id, http_request, strlen(http_request), 0);
	if (res == -1) {
		if(deb)
			printf("Send GET request to server failed\n");
		task_fatal_error();
		goto exit;
	} else {
		if(deb)
			printf("Send GET request to server succeeded\n");
	}

	update_partition = esp_ota_get_next_update_partition(NULL);
	if(deb)
		printf("Writing to partition %s subtype %d at offset 0x%x\n",
			update_partition->label,update_partition->subtype, update_partition->address);
	assert(update_partition != NULL);
	err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
	if (err != ESP_OK) {
		if(deb)
			printf( "esp_ota_begin failed, error=%d\n", err);
		task_fatal_error();
		goto exit;
	}
	if(deb)
		printf("esp_ota_begin succeeded\n");

	xTaskCreate(&ConfigSystem, "cfg", 512, (void*)50, 3, &blinker);

	bool resp_body_start = false, flag = true;
	/*deal with all receive packet*/
	bool  bb=false;
	while (flag) {
		memset(tempb, 0, BUFFSIZE);
		int buff_len = recv(socket_id, tempb, TEXT_BUFFSIZE, 0);
		if (buff_len < 0) { /*receive error*/
			printf("Error: receive data error! errno=%d\n", errno);
			task_fatal_error();
			if(blinker)
				vTaskDelete(blinker);
			goto exit;

		} else
			if (buff_len > 0 && !resp_body_start && !bb) { /*deal with response header*/
				bb=true;
				resp_body_start = read_past_http_header(tempb, buff_len, update_handle);
				if(deb)
					printf("Header found %s\n",resp_body_start?"Yes":"No");
			} else
				if (buff_len > 0 && resp_body_start) { /*deal with response body*/
					err = esp_ota_write( update_handle, (const void *)tempb, buff_len);
					if (err != ESP_OK) {
						if(deb)
							printf( "Error: esp_ota_write failed! err=0x%x\n", err);
						task_fatal_error();
						if(blinker)
							vTaskDelete(blinker);
						goto exit;
					}
					binary_file_length += buff_len;
					//if(deb)
					//	printf("Have written image length %d\n", binary_file_length);

					como = !como;
					if(deb)
					{
						if(como)
							printf(".");
						else
							printf("!");
						fflush(stdout);
					}

				} else
					if (buff_len == 0) {  /*packet over*/
						flag = false;
						if(deb)
							printf( "Connection closed, all packets received\n");
						close(socket_id);
					} else {
						if(deb)
							printf("Unexpected recv result\n");
					}
	}

	if(deb)
		printf("\nTotal Write binary data length : %d\n", binary_file_length);

	if (esp_ota_end(update_handle) != ESP_OK) {
		if(deb)
			printf( "esp_ota_end failed!\n");
		task_fatal_error();
		if(blinker)
			vTaskDelete(blinker);
		goto exit;
	}
	err = esp_ota_set_boot_partition(update_partition);
	if (err != ESP_OK) {
		if(deb)
			printf( "esp_ota_set_boot_partition failed! err=0x%x\n", err);
		task_fatal_error();
		if(blinker)
			vTaskDelete(blinker);
		goto exit;
	}
exit:	printf("Prepare to restart system!\n");
	vTaskDelay(3000 /  portTICK_RATE_MS);
	esp_restart();
	return ;
}

esp_err_t _http_event_handle(esp_http_client_event_t *evt)
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
            printf("%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
//            memcpy(temp,(char*)evt->data,evt->data_len);
//            temp[evt->data_len]=0;
//            printf("Data %s\n",temp);
            if (!esp_http_client_is_chunked_response(evt->client)) {
            	memcpy(&tempb[van],evt->data,evt->data_len);
            	van+=evt->data_len;
             //   printf("%.*s", evt->data_len, (char*)evt->data);
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
	memset(tempb,0,sizeof(tempb));
	esp_http_client_config_t config;// = {
			  config.url = "http://feediot.c1.biz/tarifasPer.txt";
			//   .url = "http://feediot.c1.biz/httptest.txt",
	  config.event_handler = _http_event_handle;
	//};
	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_err_t err = esp_http_client_perform(client);

	if (err == ESP_OK) {
	   ESP_LOGI(TAG, "Status = %d, content_length = %d",
	           esp_http_client_get_status_code(client),
	           esp_http_client_get_content_length(client));
	}
	esp_http_client_cleanup(client);
	printf("Received %d bytes\n",van);
	u32 monton=0;
	for(int a=0;a<van;a++)
		monton+=tempb[a];
	if(monton!=van*100)
		printf("%s\n",monton!=van*100?"True":"False");
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


void init_temp()
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

#ifdef WITHMETERS
static void IRAM_ATTR gpio_isr_handler(void * arg)
{
	BaseType_t tasker;
	u32 fueron;
	meterType *meter=(meterType*)arg;

	uint8_t como=gpio_get_level(meter->pin);
//	if(deb)
	//	ets_printf("%d pin %d pos %d\n",como,meter->pin,meter->pos);

		if(!como)
		{
			meter->startTimePulse=millisISR();
			fueron=meter->startTimePulse-meter->timestamp;
		//	ets_printf("Pulse %d\n",fueron);

			 if(fueron>=80)
			 {
				 meter->timestamp=millisISR(); //last valid isr
				 meter->beatSave++;
				 meter->beatSaveRaw++;
				 meter->currentBeat++;
				if((meter->currentBeat % 80)==0) //every GMAXLOSSPER interval
				{
			//		ets_printf("Save lot %d pin %d\n",meter->currentBeat,meter->pin);
					if(meter->beatSaveRaw>=800)
					{
						meter->beatSaveRaw=0;
						meter->currentKwH++;
			//			ets_printf("Kwh\n");
						meter->beatSave=0;

					}
					meter->startConn=millisISR();
				//	ets_printf("Start Time %d Pin %d\n",meter->startConn,meter->pin);
				//	ets_printf("SendISR pin %d pos %d\n",meter->pin,meter->pos);
					if(mqttQ)
						xQueueSendFromISR( mqttQ,arg,&tasker );
							if (tasker)
									portYIELD_FROM_ISR();
				}
			 }
		}
		else
		{
			// try to get pulse width
			if (meter->startTimePulse>0) //first time no frame of reference, skip
			{
				meter->timestamp=millisISR();
				meter->livingPulse+=millisISR()-meter->startTimePulse; //array pulseTime has time elapsed in ms between low and high
				meter->livingCount++;
				meter->pulse=meter->livingPulse/meter->livingCount;

					if (meter->livingCount>100)
					{
				//		ets_printf("Pulse %d\n",meter->livingPulse/meter->livingCount);
						meter->livingPulse=0;
						meter->livingCount=0;
					}
			}
		}
	}
#endif

static void getMessage(const int sock)
{
    int len;
    struct timeval to;
    cmdType comando;


    	to.tv_sec = 3;
        to.tv_usec = 0;

        if (setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&to,sizeof(to)) < 0)

        {
            printf("Unable to set read timeout on socket!");
            return;
        }

    do {
        len = recv(sock, comando.mensaje, sizeof(comando.mensaje) - 1, 0);

        if (len < 0) {
    	//	shutdown(sock, 0);
        //	close(sock);
            printf("Error occurred during receiving: errno %d\n", errno);
            break;
        } else if (len == 0) {
    	//	shutdown(sock, 0);
       // 	close(sock);
           printf( "Connection closed: errno %d\n", errno);
           break;
        } else {
        	comando.mensaje[len] = 0;
        	comando.cmd=0;
        	comando.fd=sock;
        	printf("Add queue\n");
        	if(mqttR)
        		xQueueSend(mqttR,&comando,0);
        	break;
        }
    } while (len > 0);
    printf("Leaving\n");

//    delay(10000);
    shutdown(sock, 0);
    close(sock);
}

static void buildMgr(void *pvParameters)
{
    char addr_str[50];
    int addr_family;
    int ip_protocol;
    int sock=0;
    struct sockaddr_in6 source_addr;
    uint addr_len = sizeof(source_addr);

    while (true) {

        struct sockaddr_in dest_addr;
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
        ESP_LOGI(TAG, "Socket created");

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
        ESP_LOGI(TAG, "Socket bound, port %d", BUILDMGRPORT);

        err = listen(listen_sock, 1);
        if (err != 0) {
            ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
         //   break;
        }
        ESP_LOGI(TAG, "Socket listening");


        while (true) {
            sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
            if (sock < 0) {
                ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
                break;
            }
//#ifdef KBD
            if (source_addr.sin6_family == PF_INET) {
                inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
            } else if (source_addr.sin6_family == PF_INET6) {
                inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
            }
            ESP_LOGW(TAG, "Socket accepted ip address: %s heap:%d", addr_str,esp_get_free_heap_size());

//#endif
            getMessage(sock);
            printf("done getmessage\n");
        }
    }
    vTaskDelete(NULL);
}

#ifdef WITHMETERS
void install_meter_interrupts()
{
	char 	temp[20];
	memset(&theMeters,0,sizeof(theMeters));

	theMeters[0].pin=16;
	theMeters[1].pin=17;
	theMeters[2].pin=4;
	theMeters[3].pin=5;

	gpio_install_isr_service(ESP_INTR_FLAG_IRAM);

	io_conf.intr_type = GPIO_INTR_ANYEDGE;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_down_en =GPIO_PULLDOWN_DISABLE;
	io_conf.pull_up_en =GPIO_PULLUP_ENABLE;
	for (int a=0;a<4;a++)
	{
		sprintf(temp,"Meter%d",a);
		strcpy(theMeters[a].name,temp);
		theMeters[a].pos=a;
		io_conf.pin_bit_mask = (1ULL<<theMeters[a].pin);
		gpio_config(&io_conf);
		gpio_isr_handler_add((gpio_num_t)theMeters[a].pin, gpio_isr_handler, (void*)&theMeters[a]);
		if(deb)
			printf("Install %d Pin %d Pos %d name %s\n",a,theMeters[a].pin,theMeters[a].pos,theMeters[a].name);
	}
#ifdef MULTIX
	io_conf.intr_type = GPIO_INTR_ANYEDGE;
	io_conf.pin_bit_mask = (1ULL<<MUXPIN);
	gpio_config(&io_conf);
	gpio_isr_handler_add((gpio_num_t)MUXPIN, gpio_isr_handler_mx, (void*)MUXPIN);
	io_conf.intr_type = GPIO_INTR_ANYEDGE;
	io_conf.pin_bit_mask = (1ULL<<MUXPIN1);
	gpio_config(&io_conf);
	gpio_isr_handler_add((gpio_num_t)MUXPIN1, gpio_isr_handler_mx, (void*)MUXPIN1);
#endif

	isrf=true;

	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_down_en =GPIO_PULLDOWN_ENABLE;
	io_conf.pin_bit_mask = (1ULL);
	gpio_config(&io_conf);
	gpio_set_level((gpio_num_t)WIFILED, 0);
	}
#endif

void add_new_mac(uint32_t esteMac)
{
	vanadd++;
	if(vanadd>100)
	{
		vanadd=0;
		vanvueltas++;
		memset(&losMacs,0,sizeof(losMacs));
		vanMacs=0;
	}
	for (int a=0;a<vanMacs;a++)
	{
		if(losMacs[a].macAdd==esteMac){
			losMacs[a].trans++;
			time(&losMacs[a].lastUpdate);
			return;
		}
	}
	losMacs[vanMacs++].macAdd=esteMac;
}
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
	wifi_sta_list_t wifi_sta_list;
	tcpip_adapter_sta_list_t adapter_sta_list;
	uint32_t bigmac;
	tcpip_adapter_sta_info_t station;


    switch (event_id) {
    case  WIFI_EVENT_AP_STACONNECTED:
   // 	printf("AP Station in\n");
    	memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
    	memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));
    	ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&wifi_sta_list));
    	ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list));

    	for(int i = 0; i < adapter_sta_list.num; i++)
    	{
			station = adapter_sta_list.sta[i];
			bigmac=0;
			memcpy(&bigmac,&station.mac[2],4);
			add_new_mac(bigmac);
    	}
    	break;
	case WIFI_EVENT_AP_STADISCONNECTED:
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

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
}

void sntpget(void *pArgs)
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
	struct tm timeinfo = { 0 };
	int retry = 0;
	const int retry_count = 10;
	while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
	time(&now);
	setenv("TZ", LOCALTIME, 1);
	tzset();
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	//printf( "The current date/time in %s is: %s\n", LOCALTIME,strftime_buf);

    }
    vTaskDelete(NULL);
}

static void ip_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
	 wifi_config_t wifi_config;

	    memset(&wifi_config,0,sizeof(wifi_config));//very important

    switch (event_id) {
    case IP_EVENT_AP_STAIPASSIGNED:
    	break;
        case IP_EVENT_STA_GOT_IP:
            strcpy((char*)wifi_config.ap.ssid,"Meteriot");
			strcpy((char*)wifi_config.ap.password,"Meteriot");
			wifi_config.ap.authmode=WIFI_AUTH_WPA_PSK;
			wifi_config.ap.ssid_hidden=false;
			wifi_config.ap.beacon_interval=400;
			wifi_config.ap.max_connection=50;
			wifi_config.ap.ssid_len=0;
			wifi_config.ap.channel=1;
			ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));

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

  //  #define DIRECT
    #ifdef DIRECT
	tcpip_adapter_ip_info_t 		ipInfo;

    	//For using of static IP to the Internet AP. Fast vs DHCP request
	ESP_ERROR_CHECK(tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA)); // Don't run a DHCP client
    	memset(&ipInfo,0,sizeof(ipInfo));

    	//Set static IP
    	inet_pton(AF_INET, "192.168.100.10", &ipInfo.ip);
    	inet_pton(AF_INET, "192.168.100.1", &ipInfo.gw);
    	inet_pton(AF_INET, "255.255.255.0", &ipInfo.netmask);
    	tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);

    	//Set Main DNS server
    	tcpip_adapter_dns_info_t dnsInfo;
    	inet_pton(AF_INET, "8.8.8.8",&dnsInfo.ip);
    	tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA, TCPIP_ADAPTER_DNS_MAIN,&dnsInfo);
    #endif
    wifi_event_group = xEventGroupCreate();
    xEventGroupClearBits(wifi_event_group, WIFI_BIT);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t wifi_config;
    // To get both AP y STA to work, first set the local STA section
    // When STA got IP set the AP part
    memset(&wifi_config,0,sizeof(wifi_config));//very important
	strcpy((char*)wifi_config.sta.ssid,INTERNET);
	strcpy((char*)wifi_config.sta.password,INTERNETPASS);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
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
            if(deb)
            	printf("Published\n");
            esp_mqtt_client_unsubscribe(client, "MeterIoT/Chillo/Chillo/CMD");//bit is set by unsubscribe
            break;

        case MQTT_EVENT_DATA:
           ESP_LOGI(TAG, "MQTT_EVENT_DATA");
           if(deb)
           {
        	   printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        	   printf("DATA=%.*s\r\n", event->data_len, event->data);
           }
            memset(theCmd.mensaje,0,sizeof(theCmd.mensaje));
            if(event->data_len)// 0 will be the retained msg being erased
            {
            	memcpy(theCmd.mensaje,event->data,event->data_len);
            	theCmd.cmd=event->data_len;
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
        	printf("Event %d\n",event->event_id);
            break;
    }
    return ESP_OK;
}

void sendStatusNow(meterType* meter)
{
	cJSON *root=cJSON_CreateObject();
	if(root==NULL)
	{
		printf("cannot create root\n");
		return;
	}

	cJSON_AddStringToObject(root,"MeterPin",		meter->name);
	cJSON_AddNumberToObject(root,"Transactions",	++meter->vanMqtt);
	cJSON_AddNumberToObject(root,"KwH",				meter->currentKwH);
	cJSON_AddNumberToObject(root,"Beats",			meter->currentBeat);
	cJSON_AddNumberToObject(root,"Pulse",			meter->pulse);
#ifdef TEMP
		cJSON_AddNumberToObject(root,"Temp",			ttemp);
#endif

	char *rendered=cJSON_Print(root);
	if (deb)
	{
		//printf("[MQTTD]Json %s\n",rendered);
		printf("Publishing message Sent %d\n",++sentTotal);
	}
	esp_mqtt_client_publish(clientCloud, "MeterIoT/Chillo/Chillo/CONTROL", rendered, 0, 1,0);

	free(rendered);
	cJSON_Delete(root);
	}


 void submode(void * pArg)
{
	 meterType meter;

	while(1)
	{
		if(wifif) // is the network up?
		{
			if( xQueueReceive( mqttQ, &meter, portMAX_DELAY ))
			{
				if (deb)
					printf("Heap after submode rx %d\n",esp_get_free_heap_size());

				if (deb)
					printf("Ready to send for Pin %d Pos %d Qwaiting=%d\n",meter.pin,meter.pos,uxQueueMessagesWaiting( mqttQ ));

				if(!clientCloud) //just in case
						printf("Error no client mqtt\n");
				else
				{
					if (deb)
						printf("Starting mqtt\n");

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
							//	if (deb)
								//	printf("Sending state %d\n",esp_mqtt_get_state(clientCloud));

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
									if (deb)
										printf("Stopping\n");
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
				if(deb)
					printf("Sent %d pin %d. Heap after submode %d\n",++sentTotal,meter.pin,esp_get_free_heap_size());

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

	     mqtt_cfg.client_id=					"anybody";
	     mqtt_cfg.username=					"yyfmmvmh";
	     mqtt_cfg.password=					"zE6oUgjoBQq4";
	     mqtt_cfg.uri = 						"mqtt://m15.cloudmqtt.com:18247";
	     mqtt_cfg.event_handle = 			mqtt_event_handler;
	     mqtt_cfg.disable_auto_reconnect=	true;

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
	//    esp_mqtt_client_start(clientCloud); //should be started by the SubMode task
}

static void mqttManager(void* arg)
{
	cmdType cmd;
	char text[15];
	cJSON *elcmd;
	time_t now;
	loginT loginData;

	mqttR = xQueueCreate( 100, sizeof( cmdType ) );
			if(!mqttR)
				printf("Failed queue Rx\n");

	while(1)
	{

		if( xQueueReceive( mqttR, &cmd, portMAX_DELAY ))
		{
			ESP_LOGW(TAG, "Received: %s Fd %d Queue %d ",cmd.mensaje,cmd.fd,uxQueueMessagesWaiting(mqttR));

			elcmd= cJSON_Parse(cmd.mensaje);
			if(elcmd)
			{
				cJSON *monton= cJSON_GetObjectItem(elcmd,"Batch");
				if(monton)
				{
					int son=cJSON_GetArraySize(monton);
					for (int a=0;a<son;a++)
					{
						cJSON *cmdIteml = cJSON_GetArrayItem(monton, a);
						cJSON *cmdd= cJSON_GetObjectItem(cmdIteml,"cmd");
						if(deb)
							printf("Array[%d] cmd is %s\n",a,cmdd->valuestring);
						if (strcmp(cmdd->valuestring,"tariff")==0)
							loadit();
						if (strcmp(cmdd->valuestring,"firmware")==0)
						{
							for (int a=0;a<4;a++)
								gpio_isr_handler_remove((gpio_num_t)theMeters[a].pin);
							//gpio_uninstall_isr_service();
							xTaskCreate(&firmUpdate,"U571",8192,NULL, 5, NULL);
						}
						if (strcmp(cmdd->valuestring,"/ga_login")==0)
						{
							time(&now);
							loginData.thedate=now;
							loginData.theTariff=100;
						//	printf("Time %d\n",(int)now);

							send(cmd.fd, &loginData, sizeof(loginData), 0);
						}
						if (strcmp(cmdd->valuestring,"/ga_status")==0)
						{
							if(displayf)
							{
								clearScreen();
								cJSON *meter= cJSON_GetObjectItem(cmdIteml,"MeterId");
								cJSON *tran= cJSON_GetObjectItem(cmdIteml,"Transactions");
								cJSON *pulse= cJSON_GetObjectItem(cmdIteml,"Pulse");
								cJSON *beats= cJSON_GetObjectItem(cmdIteml,"Beats");

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
							time(&now);
							loginData.thedate=now;
							loginData.theTariff=100;
							send(cmd.fd, &loginData, sizeof(loginData), 0);
						}
					}
				}
				cJSON_Delete(elcmd);
		//		shutdown(cmd.fd, 0);
		//		close(cmd.fd);
			}
			else
				printf("Could not parse cmd\n");
		}
		else
			printf("CmdQueue Error\n");
	}//while
}

static void initI2C()
{
	i2cp.sdaport=(gpio_num_t)SDAW;
	i2cp.sclport=(gpio_num_t)SCLW;
	i2cp.i2cport=I2C_NUM_0;
	miI2C.init(i2cp.i2cport,i2cp.sdaport,i2cp.sclport,400000,&I2CSem);//Will reserve a Semaphore for Control
}

	void sender(void *pArg)
	{
		meterType algo;

		while(true)
		{
			if(deb)
				printf("Heap before send %d\n",esp_get_free_heap_size());
			if(!xQueueSend( mqttQ,&algo,0))
				printf("Error sending queue %d\n",uxQueueMessagesWaiting( mqttQ ));
			else
				if(deb)
				{
					//printf("Sending %d\n",algo++);
					printf("Heap after send %d\n",esp_get_free_heap_size());
				}


			if(uxQueueMessagesWaiting(mqttQ)>uxQueueSpacesAvailable(mqttQ))
			{
				while(uxQueueMessagesWaiting(mqttQ)>2)
					delay(1000);
			}
			delay(qdelay);
		}
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


void app_main()
{
	ESP_LOGI(TAG, "[APP] BuildMgr starting up..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    deb=false;

//#ifdef KBD
//    esp_log_level_set("*", ESP_LOG_NONE);
//#else
    esp_log_level_set("*", ESP_LOG_WARN);
//#endif


    esp_err_t err = nvs_flash_init();
       if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
       {
           ESP_ERROR_CHECK(nvs_flash_erase());
           err = nvs_flash_init();
       }
       ESP_ERROR_CHECK( err );

		vanadd=0;
		memset(&losMacs,0,sizeof(losMacs));
		vanMacs=0;

    wifi_init();
    mqtt_app_start();
    initI2C();
    initScreen();

#ifdef TEMP
    init_temp();
#endif
    qwait=QDELAY;
    qdelay=qwait*1000;
   	xTaskCreate(&sender,"U571",10240,NULL, 5, NULL);
   	xTaskCreate(&mqttManager,"mqtt",10240,NULL, 5, NULL);
   	apstaf=false;
#ifdef WITHMETERS
      install_meter_interrupts();
#endif

	xTaskCreate(&buildMgr,"TCP",10240,(void*)1, 4, NULL);

#ifdef KBD
	xTaskCreate(&kbd,"kbd",4096,NULL, 4, NULL);
#endif
	xTaskCreate(&displayManager,"dispm",4096,NULL, 4, NULL);

}
