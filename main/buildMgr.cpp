#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

//External declarations. Routines are on its own source file
//-------------------------------------------------------------------------------
void kbd(void *pArg);
void write_to_fram(u8 meter,bool addit);
//void testMqtt();
char *sendTelemetry();
void start_webserver(void* pArg);
void watchDog(void* pArg);
int loadit(parg *pArg);
void sntpget(void *pArgs);
void loadDefaultTariffs();
//void check_date_change();
void write_to_fram(u8 meter,bool addit);
void load_from_fram(u8 meter);
int sendHostCmd(parg *argument);

#ifdef DISPLAY
void displayManager(void * pArg);
void drawString(int x, int y, string que, int fsize, int align,displayType showit,overType erase);
#endif

// ------------------------------------------------------------------------------
// CMDS
int statusCmd(parg *argument);
int loginCmd(parg* argument);
int firmwareCmd(parg *pArg);
int aes_decrypt(const char* src, size_t son, char *dst,const unsigned char *cualKey);
//------------------------------------------------------------------------------
#define OTA_URL_SIZE 1024
static const char *TAG = "BDMGR";

#ifdef HEAPSAMPLE
void heapSample(char *subr)
{
	time(&theheap[vanHeap].ts);
	theheap[vanHeap].theHeap=esp_get_free_heap_size();
	strcpy(theheap[vanHeap].routine,subr);
	vanHeap++;
	if(vanHeap>MAXSAMPLESHEAP)
		vanHeap=0;
}
#endif

void pprintf(const char * format, ...)
{
	  char *buffer;
	  va_list args;
	  if(xSemaphoreTake(printSem, portMAX_DELAY))
	  	{
		  buffer=(char*)malloc(4096);
		  if(buffer)
		  {
			  va_start (args, format);
			  vsprintf (buffer,format, args);
			  printf(buffer);
			  va_end (args);
			  free(buffer);
		  }
		  xSemaphoreGive(printSem);
	}

}

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


static int findCommand(char * cual)
{
	for (int a=0;a<MAXCMDS;a++)
	{
		if(strcmp(cmds[a].comando,cual)==0)
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

static void read_flash()
{
	esp_err_t q ;
	size_t largo;
	q = nvs_open("config", NVS_READONLY, &nvshandle);
	if(q!=ESP_OK)
	{
		pprintf("Error opening NVS Read File %x\n",q);
		return;
	}

	largo=sizeof(theConf);
	q=nvs_get_blob(nvshandle,"sysconf",(void*)&theConf,&largo);

	if (q !=ESP_OK)
		pprintf("Error read %x largo %d aqui %d\n",q,largo,sizeof(theConf));
	nvs_close(nvshandle);

}

void write_to_flash() //save our configuration
{
	esp_err_t q ;
	q = nvs_open("config", NVS_READWRITE, &nvshandle);
	if(q!=ESP_OK)
	{
		pprintf("Error opening NVS File RW %x\n",q);
		return;
	}
	size_t req=sizeof(theConf);
	q=nvs_set_blob(nvshandle,"sysconf",&theConf,req);
	if (q ==ESP_OK)
	{
		q = nvs_commit(nvshandle);
		if(q!=ESP_OK)
			pprintf("Flash commit write failed %d\n",q);
	}
	else
		pprintf("Fail to write flash %x\n",q);
	nvs_close(nvshandle);
}

void mqttCallback(int res)
{
#ifdef DEBUGX
	if(theConf.traceflag & (1<<HOSTD))
		pprintf("%sTelemetry result %d\n",HOSTDT,res);
#endif
	telemResult=res;
    xEventGroupSetBits(wifi_event_group, TELEM_BIT);//message sent bit
}

int sendTelemetryCmd(parg *argument)
{
	mqttMsg_t mqttMsgHandle;

	memset(&mqttMsgHandle,0,sizeof(mqttMsgHandle));

#ifdef DEBUGX
	if(theConf.traceflag & (1<<HOSTD))
		pprintf("%sHost requested telemetry readings\n",HOSTDT);
#endif
	if(mqttQ)
	{
		char *mensaje=sendTelemetry();

		mqttMsgHandle.msgLen=strlen(mensaje);
		mqttMsgHandle.message=(uint8_t*)mensaje;
		mqttMsgHandle.maxTime=4000;
		mqttMsgHandle.queueName=(char*)controlQueue.c_str();
		mqttMsgHandle.cb=mqttCallback;

		xQueueSend( mqttQ,&mqttMsgHandle,0 );			//Submode will free the mensaje variable
		if(!xEventGroupWaitBits(wifi_event_group, TELEM_BIT, false, true,  mqttMsgHandle.maxTime+10/  portTICK_RATE_MS))
		{
			pprintf("Telemetric result timeout");
			telemResult=BITTM;
			free(mensaje);
		}
		return telemResult;
	}
	else
		return -1;
}

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

static int reserveSlot(parg* argument)
{
	int8_t yes=1;
	u32 macc=(u32)argument->macn;

#ifdef DEBUGX

	if(theConf.traceflag & (1<<CMDD))
		pprintf("%sReserve slot for %x\n",CMDDT,macc);
#endif

	for(int a=0;a<theConf.reservedCnt;a++)
	{
		if(theConf.reservedMacs[a]==macc)
		{
			yes=-2;		//duplicate HOW COME????? in normal conditions
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
			pprintf("Slot already exists\n");
#endif
			send(argument->pComm, &yes, sizeof(yes), 0);//already registered
			return ESP_OK;
		}
	}
//add it
	theConf.reservedMacs[theConf.reservedCnt]=macc;
	time(&theConf.slotReserveDate[theConf.reservedCnt]);
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
	pprintf("%sMac reserved %x slot %d %s",CMDDT,theConf.reservedMacs[theConf.reservedCnt],theConf.reservedCnt,ctime(&theConf.slotReserveDate[theConf.reservedCnt]));
#endif
	//todo make a better answer in JSON
	yes=theConf.reservedCnt++;	//send slot assigned
	send(argument->pComm, &yes, sizeof(yes), 0);
	write_to_flash();		//independent from FRAM
	return ESP_OK;
}

static void getMessage(void *pArg)
{
    int len;
    cmdType comando;
    task_param *theP=(task_param*)pArg;
    char s1[10];
    parg argument;
    char *elmensaje;

    elmensaje=NULL;
    int sock =theP->sock_p;		// MtM socket
    int pos=theP->pos_p;		//which MtM

#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
	{
		pprintf("%sStarting GetM Fd %d Pos %d GlobaCount %d Macf %d\n",CMDDT,sock,pos,globalSocks,theP->macf);
		pprintf("%sGetm%d heap %d\n",CMDDT,sock,esp_get_free_heap_size());
	}
#endif
		elmensaje=(char*)malloc(MAXBUFFER);		//once
		tParam[pos].getM=elmensaje;				//copy if we crash or something

	while(true)
	{
		do {
			bzero(elmensaje,MAXBUFFER);
			len = recv(sock,elmensaje, MAXBUFFER-1, 0);
			if (len < 0)
			{
#ifdef DEBUGX
				if(theConf.traceflag & (1<<CMDD))
					pprintf("%sError occurred during receiving Taskdead: errno %d fd: %d Pos %d\n",CMDDT, errno,sock,pos);
#endif

				goto exit;
			}
			else if (len == 0)
			{
#ifdef DEBUGX
				if(theConf.traceflag & (1<<CMDD))
					pprintf("%sConnection closedn TaskDead: errno %d \n", CMDDT,errno);
#endif
			   goto exit;
			}
			else
			{		// valid Msg send it to CmdMgr

#ifdef HEAPSAMPLE
				 heapSample((char*)"GetmsgSt");
#endif
				char * output=(char*)malloc(len);
				if(!output)
				{
					pprintf("Exiting GetMessage no Heap\n");
					goto exit;
				}

				// check special reserve msg
				if(strstr(elmensaje,"rsvp")!=NULL)
				{
					// reserve cmd rx. Process it without queue
					// msg struct simple = rsvp123456789 rsvp=key numbers are macn in chars
					memcpy(s1,elmensaje+4,len-4);
					argument.macn=atof(s1);
					argument.pComm=sock;
					reserveSlot(&argument);
					goto exit; //done. Kill task and start again. MtM Station will reboot
				}

				aes_decrypt((const char*)elmensaje,(size_t)len,output,(const unsigned char*)losMacs[pos].theKey);
				comando.mensaje=output;
#ifdef DEBUGX
				if(theConf.traceflag & (1<<CMDD))
					pprintf("%sMsg Rx %s Len %d\n",CMDDT, comando.mensaje,len);
#endif


				if(theP->macf<0) //no mac found in buildmgr.
				{
					pprintf("No mac. Exiting Getmsg\n");
					free(output);
					output=NULL;
					goto exit;
				}

				// could check only for {

				if(*comando.mensaje!='{')
				{
					losMacs[pos].lostSync++;
					if(losMacs[pos].lostSync>MAXLOSTSYNC-1)
					{
						printf("Resync\n");
						memset(theConf.lkey[pos],65,AESL);
						memset(losMacs[pos].theKey,65,AESL);
						losMacs[pos].lostSync=0;
						write_to_flash();
					}
					free(output);
					output=NULL;
					//if no answer it will timeout
					//cannot answer him since all is encrypted including an answer
					//try to resync
				}
				else
				{
#ifdef DEBUGX
					if(theConf.traceflag & (1<<CMDD))
						pprintf("%sDecrypted Msg Rx %s Len %d\n",CMDDT, comando.mensaje,len);
#endif

					time(&losMacs[pos].lastUpdate);
					losMacs[pos].msgCount++;

					llevoMsg++;
					comando.pos=pos;
					comando.fd=sock;
					theP->mensaje=comando.mensaje;		//copy in case of death
					theP->argument=NULL;
					theP->elcmd=NULL;
					comando.tParam=theP;
					if(!(theConf.pause & (1<<PCMD)))
					{
						if(mqttR)
							xQueueSend(mqttR,&comando,(TickType_t)10);
						else
						{	//nobody will free this message so do it now
							free(comando.mensaje);
							comando.mensaje=NULL;
							goto exit;
						}
					}
					else		//debugging only
					{
						free(comando.mensaje);
						comando.mensaje=NULL;
						goto exit;
					}

#ifdef HEAPSAMPLE
					 heapSample((char*)"Getmsgend");
#endif
				}
			}
		} while (len > 0);
	}
	exit:
//something went wrong like mtm closed socket. Need to delete task and wait for a restart form MtM

	//printf("GetMsg exiting...erase buffers\n");

	losMacs[pos].theHandle=NULL;			//we will exit now

	if(tParam[pos].argument)
	{
	//	printf("Free argument\n");
		free(tParam[pos].argument);
		tParam[pos].argument=NULL;
	}

	if(tParam[pos].elcmd)
	{
	//	printf("Free elcmd\n");
		cJSON_Delete(tParam[pos].elcmd);
		tParam[pos].elcmd=NULL;
	}

	if(tParam[pos].mensaje)
	{
//		printf("Free mensaje]\n");
		free(tParam[pos].mensaje);
		tParam[pos].mensaje=NULL;
	}

	if(tParam[pos].getM)
	{
	//	printf("Free getM\n");
		free(tParam[pos].getM);
		tParam[pos].getM=NULL;
		elmensaje=NULL;
	}

	if(sock)
	{
		shutdown(sock, 0);
		close(sock);
	}

	globalSocks--;
	if (globalSocks<0)
		globalSocks=0;
	vTaskDelete(NULL);
}

bool decryptLogin(char* b64, uint16_t blen, char *decryp, size_t * fueron)
{

	int ret;
	size_t ilen=0;
	bzero(iv,sizeof(iv));
	unsigned char *encryp=(unsigned char*)malloc(1024);
	if(encryp)
	{
		ret=mbedtls_base64_decode((unsigned char*)encryp,1024,&ilen,(const unsigned char*)b64,blen);
		if(ret!=0)
		{
			printf("Failed b64 %d\n",ret);
			free(encryp);
			return false;
		}
		else
		{
		//	printf("Private Ok\n");
			size_t aca;
			if( ( ret = mbedtls_pk_decrypt( &pk, encryp, ilen,(unsigned char*)decryp, &aca,1024, mbedtls_ctr_drbg_random, &ctr_drbg ) ) != 0 )
			{
				printf( " failed  mbedtls_pk_decrypt returned -0x%04x\n", -ret );
				free(encryp);
				return false;

			}
			else
			{
		//		printf("Decrypted[%d] >%s<\n",aca,decryp);
				*fueron=aca;
			}
			if(encryp)
				free(encryp);
		}
	}
	else
		return false;
	return true;
}

static void tcpConnMgr(void *pvParameters)
{
    char 						addr_str[50];
    int 						addr_family;
    int 						ip_protocol;
    int 						sock=0,errcount=0;
    struct sockaddr_in 			source_addr;
    uint 						addr_len = sizeof(source_addr);
    struct sockaddr_in 			dest_addr;
    char						tt[20];
  //  task_param					theP;

    while (true)
    {
		reListen:
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
			again:sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
			if (sock < 0)
			{
				ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
				errcount++;
				if(errcount>3)			//not helping really but...
				{
					for (int a=0;a<theConf.reservedCnt;a++)
						{
							if(losMacs[a].theSock)
								close(losMacs[a].theSock);
						}
					goto reListen;
				}
				else
				{
					delay(100);
					goto again;
				}
			}
			errcount=0;
			time(&mgrTime[BUILDMGR]);

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
					pprintf("%sInternal error no Ip %x\n",CMDDT,b);
	#endif
				// do not test MAC here since we need to get the message for the RSVP command, when there are 0 MACs reserved
				a=0;
			}
			else
			{
				losMacs[a].theSock=sock;

			}
			tParam[a].pos_p=a;
			tParam[a].sock_p=sock;
			tParam[a].macf=a;
			tParam[a].argument=NULL;
			tParam[a].elcmd=NULL;

//			theP.pos_p=a;
//			theP.sock_p=sock;
//			theP.macf=a;

			globalSocks++;
			if(theConf.traceflag & (1<<INTD))
				pprintf("Connection Sock %d van %d\n",sock,globalSocks);

			xTaskCreate(&getMessage,tt,GETMT,(void*)&tParam[a], 4, &losMacs[a].theHandle);
        }
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		pprintf("%sBuildmgr listend error %d\n",CMDDT,errno);
#endif
    }

    char them[6];
    pprintf("BuildMgr will die OMG. GlobalCount %d VanMacs %d\n",globalSocks,vanMacs);
	for (int a=0;a<vanMacs;a++)
	{
		memcpy(&them,&losMacs[a].macAdd,4);
		pprintf("Mac[%d][%d]:%.2x:%.2x:%.2x:%.2x Ip:%s (%s)\n",a,losMacs[a].trans[0],
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
	//	if(theConf.traceflag & (1<<WIFID))
			pprintf("%sWIFIMac connected %02x:%02x:%02x:%02x:%02x:%02x\n",WIFIDT,ev->mac[0],ev->mac[1],ev->mac[2],ev->mac[3],ev->mac[4],ev->mac[5]);
#endif
    	memcpy((void*)&newmac,&ev->mac[2],4);
    	estem=find_mac(newmac);
    	if(estem>=0)
    	{
    		if(losMacs[estem].dState>BOOTSTATE)
    		{
    			pprintf("MtM %d Rebooted %x last State %s seen %s",estem,losMacs[estem].macAdd,
    					stateName[losMacs[estem].dState],ctime(&losMacs[estem].lastUpdate));
    		}
    		if(losMacs[estem].theSock)
    			close(losMacs[estem].theSock);

    			//in theory a Disconnect should have happened but not in reality so kill tasks
    		if(losMacs[estem].theHandle)
			{
					vTaskDelete(losMacs[estem].theHandle);
					losMacs[estem].theHandle=NULL;

    				//free memory if available
    				if(tParam[estem].argument)
    				{
    					printf("WIFI argument\n");
    					free(tParam[estem].argument);
    					tParam[estem].argument=NULL;
    				}
    				if(tParam[estem].elcmd)
    				{
    					printf("WIFI elcmd\n");
    					cJSON_Delete(tParam[estem].elcmd);
    					tParam[estem].elcmd=NULL;
    				}
    				if(tParam[estem].mensaje)
    				{
    					printf("WIFI mensaje]\n");
    					free(tParam[estem].mensaje);
    					tParam[estem].mensaje=NULL;
    				}

    				if(tParam[estem].getM)
    				{
    					printf("WIFI Free getM\n");
    					free(tParam[estem].getM);
    					tParam[estem].getM=NULL;
    				}
			}
			if(losMacs[estem].timerH)
			{
				xTimerStop(losMacs[estem].timerH,0);
						xTimerDelete(losMacs[estem].timerH,0);
						losMacs[estem].timerH=NULL;
			}

//			losMacs[estem].hostCmd=(char*)malloc(1500); should be created by the rx part. NULL means no message for that MtM
 //   		memset(&losMacs[estem],0,sizeof(losMacs[0]));	//initialize it
//			memcpy(&losMacs[estem].theKey,&theConf.lkey[estem],AESL);
//			printf("LosMacs[%d] %s Flash %s\n",estem,losMacs[estem].theKey,theConf.lkey[estem]);
    		losMacs[estem].macAdd=newmac;
    		losMacs[estem].dState=CONNSTATE;
    		losMacs[estem].stateChangeTS[CONNSTATE]=millis();
    		time(&losMacs[estem].lastUpdate);
    	}
    	break;
	case WIFI_EVENT_AP_STADISCONNECTED:
//		if(theConf.traceflag & (1<<WIFID))
			pprintf("Mac disco %02x:%02x:%02x:%02x:%02x:%02x\n",ev->mac[0],ev->mac[1],ev->mac[2],ev->mac[3],ev->mac[4],ev->mac[5]);
		memcpy((void*)&newmac,&ev->mac[2],4);
		estem=find_mac(newmac);
		if(estem>=0)
		{
			losMacs[estem].dState=BOOTSTATE;
			losMacs[estem].stateChangeTS[BOOTSTATE]=millis();

    		//in theory this should happend here but it gets called to late. Still...

			if(losMacs[estem].theHandle)
			{
				vTaskDelete(losMacs[estem].theHandle);
				losMacs[estem].theHandle=NULL;

				//free memory if available
				if(tParam[estem].argument)
				{
					printf("WIFI argument\n");
					free(tParam[estem].argument);
					tParam[estem].argument=NULL;
				}
				if(tParam[estem].elcmd)
				{
					printf("WIFI elcmd\n");
					cJSON_Delete(tParam[estem].elcmd);
					tParam[estem].elcmd=NULL;
				}
				if(tParam[estem].mensaje)
				{
					printf("WIFI mensaje]\n");
					free(tParam[estem].mensaje);
					tParam[estem].mensaje=NULL;
				}

				if(tParam[estem].getM)
				{
					printf("WIFI Free getM\n");
					free(tParam[estem].getM);
					tParam[estem].getM=NULL;
				}
			}
			if(losMacs[estem].timerH)
			{
				xTimerStop(losMacs[estem].timerH,0);
						xTimerDelete(losMacs[estem].timerH,0);
						losMacs[estem].timerH=NULL;
			}
    		if(losMacs[estem].theSock)
    			close(losMacs[estem].theSock);
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
			pprintf("[%sstation[%d] MAC ",CMDDT,i);
			for(int i = 0; i< 6; i++)
			{
				pprintf("%02X", station.mac[i]);
				if(i<5)pprintf(":");
			}
			pprintf("\n");
		}
#endif
		u32 bigmac;
		memcpy(&bigmac,&station.mac[2],4);
		int este=find_mac(bigmac);
		if(este<0)
		{
#ifdef DEBUGX
			if(theConf.traceflag & (1<<CMDD))
				pprintf("%sNot registered mac %x not found \n",CMDDT,bigmac);
#endif

		}
		else
			memcpy((void*)&losMacs[este].theIp,(void*)&station.ip,4);
	}
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
		pprintf("Failed to InitScreen\n");
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
    	pprintf("\nAP MTM IP Assigned:" IPSTR "\n", IP2STR(&evip->ip));
    	update_ip();
    	break;
        case IP_EVENT_STA_GOT_IP:
        	pprintf("\nHOST IP Assigned:" IPSTR "\n", IP2STR(&ev->ip_info.ip));
        	wifif=true;
            xEventGroupSetBits(wifi_event_group, WIFI_BIT);
        	xTaskCreate(&sntpget,"sntp",2048,NULL, 4, NULL);
            break;
        case IP_EVENT_STA_LOST_IP:
        	pprintf("\nLOST HOST IP:" IPSTR "\n", IP2STR(&ev->ip_info.ip));
    		gpio_set_level((gpio_num_t)WIFILED, 0);		//indicate WIFI is active with IP
    		break;
        default:
            break;
    }
    return;
}

static void wifi_init(void)
{
    esp_netif_dns_info_t dnsInfo;
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
    //STA and AP defaults MUST be created
    esp_netif_t* wifiSTA=esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

//    esp_netif_ip_info_t ipInfo;
//    IP4_ADDR(&ipInfo.ip, 192,168,19,1);
//	IP4_ADDR(&ipInfo.gw, 192,168,19,1);
//	IP4_ADDR(&ipInfo.netmask, 255,255,255,0);
//	esp_netif_dhcps_stop(wifiAP);
//	esp_netif_set_ip_info(wifiAP, &ipInfo);
//	esp_netif_dhcps_start(wifiAP);
//    inet_pton(AF_INET, "192.168.19.1", &dnsInfo.ip);
//    esp_netif_set_dns_info(wifiAP,	ESP_NETIF_DNS_MAIN,&dnsInfo);

#define ENABLE_STATIC_IP
#define STATIC_IP		"192.168.100.8"
#define SUBNET_MASK		"255.255.255.0"
#define GATE_WAY		"192.168.100.1"
#define DNS_SERVER		"8.8.8.8"

#ifdef ENABLE_STATIC_IP
    pprintf("%sSet static ip sta%s\n",LRED,RESETC);

	esp_netif_ip_info_t ipInfo1;
	inet_pton(AF_INET, STATIC_IP, &ipInfo1.ip);
	inet_pton(AF_INET, GATE_WAY, &ipInfo1.gw);
	inet_pton(AF_INET, SUBNET_MASK, &ipInfo1.netmask);

	esp_netif_dhcpc_stop(wifiSTA);
	esp_netif_set_ip_info(wifiSTA, &ipInfo1);
	// do not activate DHCP
	//Now set DNS server address

	//esp_netif_dns_info_t dnsInfo;
	inet_pton(AF_INET, DNS_SERVER, &dnsInfo.ip);
	esp_netif_set_dns_info(wifiSTA,	ESP_NETIF_DNS_MAIN,&dnsInfo);

	//old style deprecated
//	//For using of static IP
//	tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA); // Don't run a DHCP client
//
//	//Set static IP
//	tcpip_adapter_ip_info_t ipInfo;
//	inet_pton(AF_INET, STATIC_IP, &ipInfo.ip);
//	inet_pton(AF_INET, GATE_WAY, &ipInfo.gw);
//	inet_pton(AF_INET, SUBNET_MASK, &ipInfo.netmask);
//	tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);
//
//	//Set Main DNS server
//	tcpip_adapter_dns_info_t dnsInfo;
//	inet_pton(AF_INET, DNS_SERVER, &dnsInfo.ip);
//	tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA, ESP_NETIF_DNS_MAIN, &dnsInfo);
#endif

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &ip_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

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

//	wifi_config.ap.beacon_interval=100;
	wifi_config.ap.max_connection=10;
	wifi_config.ap.ssid_len=0;
//	wifi_config.ap.channel=1;
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
#ifdef DEBUGX
			if(theConf.traceflag & (1<<MQTTD))
				pprintf("%sMqtt connected\n",MQTTDT);
#endif
        	mqttf=true;
            esp_mqtt_client_subscribe(client, cmdQueue.c_str(), 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
#ifdef DEBUGX
			if(theConf.traceflag & (1<<MQTTD))
				pprintf("%sMqtt Disco\n",MQTTDT);
#endif
        	firstmqtt=false;
        	mqttf=false;
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
        		pprintf("%sPublished MsgId %d\n",MQTTDT,event->msg_id);
#endif
            esp_mqtt_client_unsubscribe(client, cmdQueue.c_str());//bit is set by unsubscribe
            break;
        case MQTT_EVENT_DATA:
#ifdef DEBUGX
     //  	if(theConf.traceflag & (1<<MQTTD))
           {
        	   pprintf("%sTOPIC=%.*s\r\n",MQTTDT,event->topic_len, event->topic);
        	   pprintf("%sDATA=%.*s\r\n", MQTTDT,event->data_len, event->data);
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
#ifdef DEBUGX
			if(theConf.traceflag & (1<<MQTTD))
				pprintf("%sMqtt Error\n",MQTTDT);
#endif
           xEventGroupClearBits(wifi_event_group, MQTT_BIT);
            break;
        case MQTT_EVENT_BEFORE_CONNECT:
            xEventGroupClearBits(wifi_event_group, MQTT_BIT|DONE_BIT);
        	break;
        default:
#ifdef DEBUGX
        	if(theConf.traceflag & (1<<MQTTD))
        		pprintf("%sEvent %d\n",MQTTDT,event->event_id);
#endif
            break;
    }
    return ESP_OK;
}

static void submode(void * pArg)
{
	mqttMsg_t mqttHandle;
	u32 timetodelivery=0;
	int mqtterr;

	while(true)
	{
		if(wifif) // is the network up?
		{
			if( xQueueReceive( mqttQ, &mqttHandle, portMAX_DELAY ))
			{
#ifdef HEAPSAMPLE
				 heapSample((char*)"SubmodeStart");
#endif

#ifdef DEBUGX
				time(&mgrTime[SUBMGR]);
				if(theConf.traceflag & (1<<CMDD))
					pprintf("%sHeap after submode rx %d\n",CMDDT,esp_get_free_heap_size());
#endif
				timetodelivery=millis();
				if(!clientCloud || !mqttf) //just in case
				{
					if(!clientCloud)
						pprintf("Error no client mqtt\n");
					mqtterr=NOCLIENT;
					if(!mqttf && clientCloud)
					{
						mqtterr=esp_mqtt_client_reconnect(clientCloud);
						if(!firstmqtt)
							mqtterr= esp_mqtt_client_start(clientCloud); //try restarting
						goto alla;

					}
					else
						goto mal;
				}
				else
				{
					alla:
#ifdef DEBUGX
					if(theConf.traceflag & (1<<CMDD))
						pprintf("%sStarting mqtt\n",CMDDT);
#endif
					xEventGroupClearBits(wifi_event_group, MQTT_BIT);//clear flag to wait on
					mqtterr=0;

					if(firstmqtt)
					{
						mqtterr=esp_mqtt_client_reconnect(clientCloud);
						mqtterr=esp_mqtt_client_start(clientCloud);
					}
					if(mqtterr==ESP_OK) //we got an OK
					{
						firstmqtt=true;
							// when connected send the message
						//wait for the CONNECT mqtt msg.Set bit MQTT_BIT
						if(xEventGroupWaitBits(wifi_event_group, MQTT_BIT, false, true, 4000/  portTICK_RATE_MS))
						{
#ifdef DEBUGX
							if(theConf.traceflag & (1<<CMDD))
								pprintf("%sSending Mqtt state\n",CMDDT);
#endif
							xEventGroupClearBits(wifi_event_group, PUB_BIT); // clear our waiting bit
							if(clientCloud)
							{
								mqtterr=esp_mqtt_client_publish(clientCloud, mqttHandle.queueName, (char*)mqttHandle.message, mqttHandle.msgLen, 2,0);
							//Wait PUB_BIT or 4 secs for Publish then close connection
								if (mqtterr<0)
								{
#ifdef DEBUGX
									if(theConf.traceflag & (1<<CMDD))
										pprintf("%sPublish error %d\n",CMDDT,mqtterr);
#endif
									goto mal;
								}

								if(!xEventGroupWaitBits(wifi_event_group, PUB_BIT, false, true,  mqttHandle.maxTime/  portTICK_RATE_MS))
								{
#ifdef DEBUGX
									if(theConf.traceflag & (1<<CMDD))
										pprintf("Publish TimedOut\n");
									mqtterr=PUBTM;
									goto mal;
#endif
								}

								esp_mqtt_client_stop(clientCloud);
//								pprintf("Callback in %p\n",mqttHandle.cb);
//								if(mqttHandle.cb!=NULL)
//									(*mqttHandle.cb)(0);
								mqtterr=ESP_OK;
#ifdef DEBUGX
								if(theConf.traceflag & (1<<CMDD))
									pprintf("%sStopping\n",CMDDT);
#endif
							}
//							else
//							{ //it failed to start shouldnt close it
//								//	esp_mqtt_client_stop(clientCloud);
//								mqtterr=STARTERR;
//#ifdef DEBUGX
//								if(theConf.traceflag & (1<<CMDD))
//									pprintf("%sStart Timed out\n",CMDDT);
//#endif
//							}
						}
						else
						{
#ifdef DEBUGX
							if(theConf.traceflag & (1<<CMDD))
								pprintf("%sError start timeout mqtt\n",CMDDT);
#endif
							mqtterr=STARTTM;
						}
					}
					else
					{
#ifdef DEBUGX
						if(theConf.traceflag & (1<<CMDD))
							pprintf("%sFailed to start when available %d\n",CMDDT,mqtterr);
#endif
						mqtterr=STARTERR;
					}

#ifdef DEBUGX
				if(theConf.traceflag & (1<<CMDD))
				{
					pprintf("%sDelivery time %dms\n",CMDDT,millis()-timetodelivery);
					pprintf("%sSent %d. Heap after submode %d\n",CMDDT,++sentTotal,esp_get_free_heap_size());
				}
#endif
			}// no clientcloud

		}// queuerx
		else
		{
#ifdef DEBUGX
			if(theConf.traceflag & (1<<CMDD))
				pprintf("%sMqtt Failed queue\n",CMDDT);
#endif
			mqtterr=QUEUEFAIL;
			vTaskDelay(100 /  portTICK_RATE_MS);
		}
mal:
		if(mqttHandle.cb!=NULL)
			(*mqttHandle.cb)(mqtterr);
		if(mqttHandle.message)
		{
			free(mqttHandle.message);			//we free the message HERE
			mqttHandle.message=NULL;
		}
#ifdef HEAPSAMPLE
		 heapSample((char*)"SubmodeEnd");
#endif
	}
	//should crash if it gets here.Impossible in theory
	}
}

static void mqtt_app_start(void)
{
 //	extern const unsigned char ssl_pem_start[] asm("_binary_server_crt_start");
// 	extern const unsigned char ssl_pem_end[] asm("_binary_server_crt_end");
 //	int len=ssl_pem_end-ssl_pem_start;


	     memset((void*)&mqtt_cfg,0,sizeof(mqtt_cfg));
#ifdef MQTTLOCAL
	     mqtt_cfg.client_id=				"anybody";
	     mqtt_cfg.username=					"";
	     mqtt_cfg.password=					"";
	   //  mqtt_cfg.uri = 					"mqtt://192.168.100.7:1883";
	     mqtt_cfg.event_handle = 			mqtt_event_handler;
	     mqtt_cfg.disable_auto_reconnect	=true;
	     mqtt_cfg.disable_clean_session		=false;
//	     mqtt_cfg.cert_pem					=(const char*)ssl_pem_start;
	     mqtt_cfg.uri = 					"mqtts://192.168.100.7:8883";
	//     mqtt_cfg.cert_len					=len;

#else
	     mqtt_cfg.client_id=				"anybody";
	     mqtt_cfg.username=					"yyfmmvmh";
	     mqtt_cfg.password=					"zE6oUgjoBQq4";
	     mqtt_cfg.uri = 					"mqtt://m15.cloudmqtt.com:18247";
	     mqtt_cfg.event_handle = 			mqtt_event_handler;
	     mqtt_cfg.disable_auto_reconnect=	true;
#endif
		mqttQ = xQueueCreate( 20, sizeof( mqttMsg_t ) );
		if(!mqttQ)
			pprintf("Failed queue\n");

		clientCloud=NULL;

	    clientCloud = esp_mqtt_client_init(&mqtt_cfg);

	    if(clientCloud)
	    	xTaskCreate(&submode,"U571",10240,NULL, 5, &submHandle);
	    else
	    {
	    	pprintf("Not configured Mqtt\n");
	    	return;
	    }
}

 // Messages form Host Controller managed here
 // format is JSON in array "Batch" of Cmds

int aes_encrypt(char* src, size_t son, char *dst,char *cualKey)
{
	if(!theConf.crypt)
	{
		memcpy(dst,src,son);
		return son;
	}
	bzero(iv,sizeof(iv));
	int theSize=son;
	int rem= theSize % 16;
	theSize+=16-rem;			//round to next 16 for AES

#ifdef HEAPSAMPLE
			 heapSample((char*)"EncryptSt");
#endif

	char *donde=(char*)malloc(theSize);
	if (!donde)
	{
		printf("No memory copy message\n");
		return -1;
	}
	bzero(donde,theSize);
	memcpy(donde,src,son);

	if(esp_aes_setkey( &ctx, (const unsigned char*)cualKey, 256 )!=0)
		printf("Could not set key encrypt\n");
	else
	{
		if(esp_aes_crypt_cbc( &ctx, ESP_AES_ENCRYPT, theSize, (unsigned char*)iv, (const unsigned char*)donde, ( unsigned char*)dst )!=0)
			printf("Could not encrypt\n");
	}
	free(donde);
#ifdef HEAPSAMPLE
			 heapSample((char*)"EncryptEnd");
#endif
	return theSize;
}

int aes_decrypt(const char* src, size_t son, char *dst,const unsigned char *cualKey)
{
	if(!theConf.crypt)
	{
		memcpy(dst,src,son);
		return son;
	}
#ifdef HEAPSAMPLE
			 heapSample((char*)"DecryptSt");
#endif
	bzero(dst,son);
	bzero(iv,sizeof(iv));
	if(esp_aes_setkey( &ctx, (const unsigned char*)cualKey, 256 )!=0)
		printf("Could not set key decrypt\n");
	else
	{
		if(esp_aes_crypt_cbc( &ctx, ESP_AES_DECRYPT, son,(unsigned char*) iv, ( const unsigned char*)src, (unsigned char*)dst )!=0)
			printf("Could not decrypt\n");
	}
#ifdef HEAPSAMPLE
			 heapSample((char*)"DecryptEnd");
#endif
	return son;
}

cJSON *answerMsg(char* toWhom, int err,int tariff,char *cmds)
{
	time_t now;
	time(&now);
#ifdef HEAPSAMPLE
			 heapSample((char*)"AnswerSt");
#endif
	cJSON *root=cJSON_CreateObject();
	if(root==NULL)
	{
		pprintf("cannot create root\n");
		return NULL;
	}
	cJSON_AddStringToObject(root,"To",toWhom);
	cJSON_AddStringToObject(root,"From", theConf.meterConnName);
	cJSON_AddNumberToObject(root,"err",	err);
	cJSON_AddNumberToObject(root,"Ts",	now);
	cJSON_AddNumberToObject(root,"Tar",	tariff);
	cJSON_AddStringToObject(root,"connmgr",	theConf.meterConnName);

	if(cmds)
		cJSON_AddStringToObject(root,"cmdHost",cmds);
	pprintf("AnswerMsgEnd %d\n",esp_get_free_heap_size());
#ifdef HEAPSAMPLE
			 heapSample((char*)"AnswerEnd");
#endif
	return root;	//the calling routine shall free this buffer
}

int findMeter(char * cualM,uint8_t* mtm,uint8_t*meter)
{
	for (int a=0;a<MAXSTA;a++)
	{
		for(int b=0;b<MAXDEVS;b++)
			if(strcmp(cualM,losMacs[a].meterSerial[b])==0)
			{
				*mtm=a;
				*meter=b;
				return ESP_OK;
			}
	}
	return -1;
}

static void deliverToMTM(cJSON* dest, char *mensaje)
{
	uint8_t theMtM,theMeter;
	hostCmd	theCmd;

//	if(strcmp(dest->valuestring,theConf.meterConnName)!=0)			//check its address to thsi ConnMgr
//	{
		//find if name is one of our stored meters
		int ret=findMeter(dest->valuestring,&theMtM,&theMeter);
		if(ret<0)
		{
			pprintf("Invalid meter sent %s from Host\n",dest->valuestring);
			return;
		}
		//cmd for the MtM-theMeter combo
		theCmd.meter=theMeter;
		theCmd.msg=mensaje;					//this memory will be freed by the sending routine in SendStatus (should be in any answer to MtM)
//		printf("add to queue %s\n",theCmd.msg);
		xQueueSend(mtm[theMtM],&theCmd,0);
//	}
//	else
//		pprintf("Destination %s not us %s\n",dest->valuestring,theConf.meterConnName);
}

static void checkFramStatus(uint16_t pos, uint8_t framSt,double dmac, char *mtmName)
{
	uint8_t oldhw=losMacs[pos].hwState;
	losMacs[pos].hwState=framSt;
	if(framSt>0 && oldhw==0)
	{
#ifdef DEBUGX
		if(theConf.traceflag & (1<<CMDD))
			pprintf("%sFram HW Failure in meter MtM %s\n",CMDDT,(uint32_t)dmac,mtmName);
#endif
		//send Host warning
		mqttMsg_t 	mqttMsgHandle;
		memset(&mqttMsgHandle,0,sizeof(mqttMsgHandle));
		char		*textt=(char*)malloc(100);			//MUST be a malloc, will be Freed by Submode
		sprintf(textt,"Fram HW Failure in meter MAC %06x MtM %s\n",(uint32_t)dmac,mtmName);
		mqttMsgHandle.msgLen=strlen(textt);
		mqttMsgHandle.message=(uint8_t*)textt;
		mqttMsgHandle.maxTime=4000;
		mqttMsgHandle.queueName=(char*)"MeterIoT/EEQ/SOS";
		if(mqttQ)
			xQueueSend( mqttQ,&mqttMsgHandle,0 );			//Submode will free the mensaje variable
	}
}

static void cmdManager(void* arg)
{
	cmdType cmd;
	cJSON 	*elcmd;
	char 	mtmName[20];
	char	kk[40];
	bool 	rsynck;
	int		ret,fueron=0;

	mqttR = xQueueCreate( 20, sizeof( cmdType ) );
			if(!mqttR)
				pprintf("Failed queue Rx\n");

	// Process is
	// 1) Get message from Queue
	// 2) MUST be BATCH set of commands, be it 1 or more
	// 3) Per Cmd in Batch process THAT cmd whic is cmdIteml

	while(1)
	{
		again:
		if( xQueueReceive( mqttR, &cmd, portMAX_DELAY ))
		{
#ifdef HEAPSAMPLE
			 heapSample((char*)"CmdMgrSt");
#endif
			time(&mgrTime[CMDMGR]);
			rsynck=false;
#ifdef DEBUGX
			if(theConf.traceflag & (1<<MSGD))
				pprintf("%sReceivedCmdMgr: %s len %d Fd %d Queue %d Heap %d\n",MSGDT,cmd.mensaje,strlen(cmd.mensaje),cmd.fd,uxQueueMessagesWaiting(mqttR),esp_get_free_heap_size());
#endif

				elcmd= cJSON_Parse(cmd.mensaje);
				if(elcmd)
				{
					if(cmd.tParam)		//for host cmd which has no tparam var yet
						cmd.tParam->elcmd=elcmd;
				// each message rx MUST have a BATCH (monton) and MAC (macn) others are optional
				// dest is use to relay messages to host via SendHost Cmd via a Queue
			//	printf("Parsed %s\n",cmd.mensaje);
				cJSON *monton= 		cJSON_GetObjectItem(elcmd,"Batch");
				cJSON *ddmac= 		cJSON_GetObjectItem(elcmd,"macn");
				cJSON *framGuard= 	cJSON_GetObjectItem(elcmd,"fram");
				cJSON *jmtmName= 	cJSON_GetObjectItem(elcmd,"Controller");
				cJSON *kkey= 		cJSON_GetObjectItem(elcmd,"rsync");
				cJSON *dest= 		cJSON_GetObjectItem(elcmd,"to");
			//	cJSON *from= 		cJSON_GetObjectItem(elcmd,"from");
			//	cJSON *cmdHost= 	cJSON_GetObjectItem(elcmd,"cmd");

				if(dest)
				{
					deliverToMTM(dest,cmd.mensaje);
					cJSON_Delete(elcmd);
					goto again;
				}
				if(kkey)
				{
					ret=mbedtls_base64_decode((unsigned char*)kk,sizeof(kk),(size_t*)&fueron,(const unsigned char*)kkey->valuestring,strlen(kkey->valuestring));
					if(ret!=0)
					{
						printf("Failed b64 %d\n",ret);
					//	return false;
					}
					else
						rsynck=true;
				}

				if (jmtmName)
					strcpy(mtmName,jmtmName->valuestring);
				else
					memset(mtmName,0,sizeof(mtmName));

				if(strcmp(losMacs[cmd.pos].mtmName,mtmName)!=0)
					strcpy(losMacs[cmd.pos].mtmName,mtmName);


				if(framGuard)
					checkFramStatus(cmd.pos,framGuard->valueint,ddmac->valuedouble,mtmName);

				if(monton && ddmac)	// Process cmds MUST have a Batch and macn entry
				{
					int son=cJSON_GetArraySize(monton);
					for (int a=0;a<son;a++)
					{
						cJSON *cmdIteml 	=cJSON_GetArrayItem(monton, a);//next item
						cJSON *cmdd			=cJSON_GetObjectItem(cmdIteml,"cmd"); //get cmd.

						//saving meter name and position for Host commands
						cJSON *mid			=cJSON_GetObjectItem(cmdIteml,"mid"); //get meter id
						cJSON *lpos			=cJSON_GetObjectItem(cmdIteml,"Pos"); //get meter which meter in the MtM

						int thepos=0;
						if(lpos)
							thepos=lpos->valueint;
						if(mid)
							strcpy(losMacs[cmd.pos].meterSerial[thepos],mid->valuestring);	//copy the name always

#ifdef DEBUGX
						if(theConf.traceflag & (1<<CMDD))
							pprintf("%sArray[%d] cmd is %s\n",CMDDT,a,cmdd->valuestring);
#endif
						int cualf=findCommand(cmdd->valuestring);
						if(cualf>=0)
						{
							parg *argument;
							cmd.tParam->argument	=(parg*)malloc(sizeof(parg));
							argument				=cmd.tParam->argument;
							argument->pMessage		=(void*)cmdIteml;				//the command line parameters
							argument->pos			=cmd.pos;						//MtM array pos
							argument->pComm			=cmd.fd;						//Socket fro direct response
							argument->macn			=ddmac->valuedouble;			//MAC #

							if(cmds[cualf].source==HOSTT) 					//if from Central Host VALIDATE it is for us, our theConf.meterConnName id
							{
								cJSON *cmgr= cJSON_GetObjectItem(cmdIteml,"connmgr");
								if(!cmgr)
								{
#ifdef DEBUGX
									if(theConf.traceflag & (1<<CMDD))
										pprintf("%sNot our structure\n",CMDDT);
#endif
									goto nexter;
								}
								if(strcmp(cmgr->valuestring,theConf.meterConnName)!=0)
								{
#ifdef DEBUGX
									if(theConf.traceflag & (1<<CMDD))
										pprintf("%sNot our connMgr %s\n",CMDDT,cmgr->valuestring);
#endif
									goto nexter;
								}
//								argument->macn=(u32)theMacNum;				//this allows HOST to NOT know the local MAC. If implemented its a great security feature
							}

							cmds[cualf].count++;			//keep a count of cmds rx
							(*cmds[cualf].code)(argument);	// call the cmd and wait for it to end
							nexter:
							free(argument);
							cmd.tParam->argument=NULL;

						}
						else
						{
#ifdef DEBUGX
							if(theConf.traceflag & (1<<CMDD))
								pprintf("%sCmd %s not implemented\n",CMDDT,cmdd->valuestring);
#endif
						}
					}// array done. Free some memory

					if(cmd.mensaje)
					{
						free(cmd.mensaje);
						cmd.mensaje=NULL;
						cmd.tParam->mensaje=NULL;
					}
					//we are here because elcmd exists so just delete cJson
					cJSON_Delete(elcmd);
					elcmd=NULL;
					cmd.tParam->elcmd=NULL;

					if(rsynck)	//save in flash and RAM the new key BUT Now not when received else cannt decrypt
					{
						memcpy(theConf.lkey[cmd.pos],kk,AESL);
						memcpy(losMacs[cmd.pos].theKey,kk,AESL);
						write_to_flash();
					}
				}//batch
				if(elcmd)
				{
					cJSON_Delete(elcmd);
					elcmd=NULL;
					cmd.tParam->elcmd=NULL;
				}
			}
			else
			{
#ifdef DEBUGX
				if(theConf.traceflag & (1<<MSGD))
					pprintf("%sCould not parse cmd\n",CMDDT);
#endif
			}
			if(cmd.mensaje)
			{
				free(cmd.mensaje); 							// Data is transfered via pointer from a malloc. Free it here.
				cmd.mensaje=NULL;
				if(cmd.tParam)
					cmd.tParam->mensaje=NULL;
			}
#ifdef HEAPSAMPLE
			 heapSample((char*)"CmdMgrEnd");
#endif
				#ifdef DEBUGX
			if(theConf.traceflag & (1<<MSGD))
				pprintf("%sCmdMgr %d heap %d\n",MSGDT,stcount++,esp_get_free_heap_size());
#endif
		}
		else
		{
#ifdef DEBUGX
			if(theConf.traceflag & (1<<CMDD))
				pprintf("%sCmdQueue Error\n",CMDDT);
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

#endif

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
	vanadd							=0;
	llevoMsg						=0;
	miscanf							=false;
	mqttf							=false;
	firstmqtt						=false;
	MQTTDelay						=-1;
	vanMacs							=0;
    qwait							=QDELAY;
    qdelay							=qwait*1000;
    vanHeap							=0;
    stcount							=0;

    //debug stuff

	tempb=(char*)malloc(5000);// big sucker with our new psram jaja

	if(theConf.msgTimeOut<10000)
		theConf.msgTimeOut=61000;	//just in case

	memset(&losMacs,0,sizeof(losMacs));
	memset(&theMeters,0,sizeof(theMeters));

   	//activity led
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_down_en =GPIO_PULLDOWN_ENABLE;
	io_conf.pin_bit_mask = (1ULL<<WIFILED);
	gpio_config(&io_conf);
	gpio_set_level((gpio_num_t)WIFILED, 0);

	daysInMonth [0]		=31;
	daysInMonth [1]		=28;
	daysInMonth [2]		=31;
	daysInMonth [3] 	=30;
	daysInMonth [4] 	=31;
	daysInMonth [5] 	=30;
	daysInMonth [6] 	=31;
	daysInMonth [7] 	=31;
	daysInMonth [8] 	=30;
	daysInMonth [9] 	=31;
	daysInMonth [10]	=30;
	daysInMonth [11]	=31;

	strcpy(stateName[BOOTSTATE],"BOOT");
	strcpy(stateName[CONNSTATE],"CONN");
	strcpy(stateName[LOGINSTATE],"LOGIN");
	strcpy(stateName[MSGSTATE],"MSG");
	strcpy(stateName[TOSTATE],"TO");


	for(int a=0;a<MAXDEVS;a++)
	{
		theMeters[a].beatsPerkW=theConf.beatsPerKw[a];
		strcpy(theMeters[a].serialNumber,theConf.medidor_id[a]);
	}

	//message from Meters
	strcpy((char*)&cmds[0].comando,"cmd_firmware");			cmds[0].code=firmwareCmd;			cmds[0].source=HOSTT;	//from Host
	strcpy((char*)&cmds[1].comando,"cmd_tariff");			cmds[1].code=loadit;				cmds[1].source=HOSTT;	//from Host
	strcpy((char*)&cmds[2].comando,"cmd_status");			cmds[2].code=statusCmd;				cmds[2].source=LOCALT;	//from local
	strcpy((char*)&cmds[3].comando,"cmd_login");			cmds[3].code=loginCmd;				cmds[3].source=LOCALT;	//from local
	strcpy((char*)&cmds[4].comando,"cmd_telemetry");		cmds[4].code=sendTelemetryCmd;		cmds[4].source=HOSTT;	//from Host
	strcpy((char*)&cmds[5].comando,"cmd_reserve");			cmds[5].code=reserveSlot;			cmds[5].source=LOCALT;	//from local
	strcpy((char*)&cmds[6].comando,"cmd_sendHost");			cmds[6].code=sendHostCmd;				cmds[6].source=LOCALT;	//from local

	//debug trace flags
#ifdef KBD
	strcpy(lookuptable[ 0],"BOOTD");
	strcpy(lookuptable[ 1],"WIFID");
	strcpy(lookuptable[ 2],"MQTTD");
	strcpy(lookuptable[ 3],"PUBSUBD");
	strcpy(lookuptable[ 4],"OTAD");
	strcpy(lookuptable[ 5],"CMDD");
	strcpy(lookuptable[ 6],"WEBD");
	strcpy(lookuptable[ 7],"GEND");
	strcpy(lookuptable[ 8],"MQTTT");
	strcpy(lookuptable[ 9],"FRMCMD");
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

	for (int a=0;a<theConf.reservedCnt;a++)
	{
		mtm[a] = xQueueCreate( MAXDEVS, sizeof( hostCmd ) );
		if(!mtm[a])
			pprintf("Failed queue mtm\n");

	}
	// AES Key initialization

	memset( iv, 0, sizeof( iv ) );
	memset( key, 65, sizeof( key ) );
	void *zb=malloc(AESL);
	bzero(zb,AESL);

	esp_aes_init( &ctx );
	for (int a=0;a<MAXSTA;a++)
	{
		if(memcmp(&theConf.lkey[a],zb,AESL)==0)
		{
		//	printf("New Key[%d] ",a);
			memset(&losMacs[a].theKey,65,AESL);
		}
		else
		{
			memcpy(&losMacs[a].theKey,&theConf.lkey[a],AESL);
		//	printf("Saved Key[%d] ",a);
		}
#ifdef DEBUGX
		if(theConf.traceflag & (1<<BOOTD))
		{
			for (int aa=0;aa<AESL;aa++)
				printf("%02x ",losMacs[a].theKey[aa]);
			printf("\n");
		}
#endif
	}
	free(zb);

	// RSA Init process
	int ret;

	extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
	extern const unsigned char prvtkey_pem_end[] asm("_binary_prvtkey_pem_end");
	int len = prvtkey_pem_end - prvtkey_pem_start;

	mbedtls_ctr_drbg_init(&ctr_drbg);
	mbedtls_entropy_init(&entropy);
	if((ret=mbedtls_ctr_drbg_seed(&ctr_drbg,mbedtls_entropy_func,&entropy,NULL,0))!=0)
	{
		printf("failed seed %x\n",ret);
	//	return false;
	}

	mbedtls_pk_init( &pk );

	if( ( ret = mbedtls_pk_parse_key( &pk, prvtkey_pem_start,len,NULL,0)) != 0 )
	{
		    printf( " failed\n  ! mbedtls_pk_parse_private_keyfile returned -0x%04x\n", -ret );
	//	    return false;
	}
}


static void init_fram()
{
	// FRAM Setup
	theGuard = esp_random();
	framSem=NULL;
	spi_flash_init();

	if(!fram.begin(FMOSI,FMISO,FCLK,FCS,&framSem)) //will create SPI channel and Semaphore
	{
		pprintf("FRAM Begin Failed\n");
		xTaskCreate(&ConfigSystem,"error",1024,(void*)100, 4, NULL);
		return;
	}
	//load all devices counters from FRAM
	if(framSem)
	{
		fram.write_guard(theGuard);				// theguard is dynamic and will change every boot.
		startGuard=millis();
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
		pprintf("Failed queue Fram\n");

	while(true)
	{
		if( xQueueReceive( framQ, &theMeter, portMAX_DELAY/  portTICK_RATE_MS ))
		{
			if(framSem)
			{
				time(&mgrTime[FRAMMGR]);

				if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
				{
					write_to_fram(theMeter.pos,theMeter.saveit);
	#ifdef DEBUGX
					if(theConf.traceflag & (1<<FRAMD))
						pprintf("%sSaving Meter %d add %d Beats %d\n",FRAMDT,theMeter.pos,theMeter.saveit,theMeters[theMeter.pos].currentBeat);
	#endif
					xSemaphoreGive(framSem);
				}
			}
		}
		else
		{
#ifdef DEBUGX
			if(theConf.traceflag & (1<<FRAMD))
				pprintf("%sFailed framQ Manager\n",FRAMDT);
#endif
			delay(1000);
		}
	}
}

static void pcntManager(void * pArg)
{
	pcnt_evt_t evt;
	u16 residuo,count;
	u32 timeDiff=0;

	pcnt_evt_queue = xQueueCreate( 20, sizeof( pcnt_evt_t ) );
	if(!pcnt_evt_queue)
		pprintf("Failed queue PCNT\n");

	while(true)
	{
        if( xQueueReceive(pcnt_evt_queue, (void*)&evt,portMAX_DELAY / portTICK_PERIOD_MS))
        {
			time(&mgrTime[PINMGR]);
			pcnt_get_counter_value((pcnt_unit_t)evt.unit,(short int *) &count);

#ifdef DEBUGX
			if(theConf.traceflag & (1<<INTD))
				pprintf("%sEvent PCNT unit[%d]; cnt: %d status %x\n",INTDT, evt.unit, count,evt.status);
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
					pprintf("%sAddKwH %s Beat %d MeterPos %d Time %d Hora %d Tarifa %02x BPK %d\n",INTDT,
							residuo?"N":"Y",theMeters[evt.unit].currentBeat,theMeters[evt.unit].pos,timeDiff,
									horag,tarifasDia[horag],theMeters[evt.unit].beatsPerkW);
#endif
				// if we have reached BPK, save it to fram but also add 1 kwh counter
				// else just save the 1/10 betas
				if(residuo==0 && theMeters[evt.unit].currentBeat>0)
				{
				//	if(theMeters[evt.unit].pos==(MAXDEVS-1))
				//		theMeters[evt.unit].code=testMqtt; 		//using this routine for mqtt test.Just the last one
				//	else
					theMeters[evt.unit].code=NULL; 		//do nothing
					theMeters[evt.unit].saveit=true;
					theMeters[evt.unit].beatSave-=theMeters[evt.unit].beatsPerkW*tarifasDia[horag]/100;
				}

				if(framQ)
					xQueueSend( framQ,&theMeters[evt.unit],0);//copy to fram Manager
			}
        } else
            pprintf("PCNT Failed Queue\n");
	}
}



static void erase_config() //do the dirty work
{
	pprintf("Erase config\n");
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
	//		    	pprintf("Could not reopen logfile\n");
	//		    else
	//			    postLog(0,"Log cleared");
	//	    }
	//	    xSemaphoreGive(logSem);
	//	}
#ifdef DEBUGX
	if(theConf.traceflag & (1<<BOOTD))
		pprintf("%sCentinel %x\n",BOOTDT,theConf.centinel);
#endif
}

void makeMetercJSON(macControl *meterController,cJSON *arr)
{
	for(int a=0;a<MAXDEVS;a++)
	{
		if(strcmp(meterController->meterSerial[a],"")!=0)
		{
			cJSON *cmdJ=cJSON_CreateObject();
			cJSON_AddStringToObject(cmdJ,"MID",				meterController->meterSerial[a]);
			cJSON_AddNumberToObject(cmdJ,"KwH",				meterController->controlLastKwH[a]);
			cJSON_AddNumberToObject(cmdJ,"BPS",				meterController->controlLastBeats[a]);
			cJSON_AddItemToArray(arr, cmdJ);
		}
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
		pprintf("cannot create root\n");
		return NULL;
	}

	cJSON_AddStringToObject(root,"CONID",				theConf.meterConnName);
	cJSON_AddNumberToObject(root,"TS",					now);
	cJSON_AddNumberToObject(root,"MAC",					theMacNum);

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
		cJSON_AddStringToObject(cmdJ,"MID",				theMeters[a].serialNumber);
		cJSON_AddNumberToObject(cmdJ,"KwH",				theMeters[a].curLife);
		cJSON_AddNumberToObject(cmdJ,"BPS",				theMeters[a].currentBeat);
		cJSON_AddItemToArray(ar, cmdJ);

	}

	cJSON_AddItemToObject(root, "Telemetry",ar);
	return root;
}

//void testMqtt()
//{
//	char *lmessage=NULL;
//
//	cJSON * telemetry=makeGroupMeters();
//
//	if(telemetry)
//	{
//		lmessage=cJSON_Print(telemetry);
//#ifdef DEBUGX
//		if(theConf.traceflag & (1<<MQTTD))
//			pprintf("To Host %s\n",lmessage);
//#endif
//	}
//	else
//		pprintf("Failed telemetry\n");
//
//	//esp_mqtt_client_publish(clientCloud, "MeterIoT/Chillo/Chillo/CONTROL", lmessage, 0, 1,0);
//	esp_mqtt_client_publish(clientCloud, controlQueue.c_str(), lmessage, 0, 1,0);
//
//	if(lmessage)
//		free(lmessage);
//	if(telemetry)
//		cJSON_Delete(telemetry);
//}

char *sendTelemetry()
{
	time_t now=0;
	cJSON * telemetry=makeGroupMeters();
	char *lmessage=NULL;
#ifdef HEAPSAMPLE
	 heapSample((char*)"TelemetSt");
#endif
	if(telemetry)
	{
		lmessage=cJSON_Print(telemetry);
		cJSON_Delete(telemetry);
#ifdef DEBUGX
		if(theConf.traceflag & (1<<MSGD))
			pprintf("%sTo Host %s %d\n",MSGDT,lmessage,esp_get_free_heap_size());
#endif
	}
	else
		pprintf("Failed telemetry\n");

	time(&now);
	fram.write_cycle(mesg,now);
#ifdef HEAPSAMPLE
	 heapSample((char*)"TelemetEnd");
#endif
	return lmessage;
}

void connMgr(void* pArg)
{
	mqttMsg_t mqttMsgHandle;
	memset(&mqttMsgHandle,0,sizeof(mqttMsgHandle));

	delay((int)pArg);

	char *mensaje=sendTelemetry();
	mqttMsgHandle.msgLen=strlen(mensaje);
	mqttMsgHandle.message=(uint8_t*)mensaje;
	mqttMsgHandle.maxTime=4000;
	mqttMsgHandle.queueName=(char*)controlQueue.c_str();
	mqttMsgHandle.cb=mqttCallback;
	if(mqttQ)
		xQueueSend( mqttQ,&mqttMsgHandle,0 );			//submode will free Mensaje
//	pprintf("Sending Telemetry\n");
	delay(1000);
	connHandle=NULL;
	vTaskDelete(NULL);
}

void init_boot()
{
#ifdef DEBUGX

	if(theConf.traceflag & (1<<BOOTD))
			pprintf("%s Fram Id %04x Fram Size %d%s\n",MAGENTA,fram.prodId,fram.intframWords,RESETC);

	if((theConf.traceflag & (1<<BOOTD)) && fram.intframWords>0)
	{
		pprintf("%s=============== FRAM ===============%s\n",RED,YELLOW);
		pprintf("FRAMDATE(%s%d%s)=%s%d%s\n",GREEN,FRAMDATE,YELLOW,CYAN,GUARDM-FRAMDATE,RESETC);
		pprintf("METERVER(%s%d%s)=%s%d%s\n",GREEN,GUARDM,YELLOW,CYAN,USEDMACS-GUARDM,RESETC);
		pprintf("FREEFRAM(%s%d%s)=%s%d%s\n",GREEN,USEDMACS,YELLOW,CYAN,MCYCLE-USEDMACS,RESETC);
		pprintf("MCYCLE(%s%d%s)=%s%d%s\n",GREEN,MCYCLE,YELLOW,CYAN,STATIONS-MCYCLE,RESETC);
		pprintf("STATIONS(%s%d%s)=%s%d%s\n",GREEN,STATIONS,YELLOW,CYAN,STATIONSEND-STATIONS,RESETC);
		pprintf("STATIONSEND(%s%d%s)=%s%d%s\n",GREEN,STATIONSEND,YELLOW,CYAN,TARIFADIA-STATIONSEND,RESETC);
		pprintf("TARIFADIA(%s%d%s)=%s%d%s\n",GREEN,TARIFADIA,YELLOW,CYAN,FINTARIFA-TARIFADIA,RESETC);
		pprintf("FINTARIFA(%s%d%s)=%s%d%s\n",GREEN,FINTARIFA,YELLOW,CYAN,BEATSTART-FINTARIFA,RESETC);
		pprintf("BEATSTART(%s%d%s)=%s%d%s\n",GREEN,BEATSTART,YELLOW,CYAN,LIFEKWH-BEATSTART,RESETC);
		pprintf("LIFEKWH(%s%d%s)=%s%d%s\n",GREEN,LIFEKWH,YELLOW,CYAN,LIFEDATE-LIFEKWH,RESETC);
		pprintf("LIFEDATE(%s%d%s)=%s%d%s\n",GREEN,LIFEDATE,YELLOW,CYAN,MONTHSTART-LIFEDATE,RESETC);
		pprintf("MONTHSTART(%s%d%s)=%s%d%s\n",GREEN,MONTHSTART,YELLOW,CYAN,MONTHRAW-MONTHSTART,RESETC);
		pprintf("MONTHRAW(%s%d%s)=%s%d%s\n",GREEN,MONTHRAW,YELLOW,CYAN,DAYSTART-MONTHRAW,RESETC);
		pprintf("DAYSTART(%s%d%s)=%s%d%s\n",GREEN,DAYSTART,YELLOW,CYAN,DAYRAW-DAYSTART,RESETC);
		pprintf("DAYRAW(%s%d%s)=%s%d%s\n",GREEN,DAYRAW,YELLOW,CYAN,HOURSTART-DAYRAW,RESETC);
		pprintf("HOURSTART(%s%d%s)=%s%d%s\n",GREEN,HOURSTART,YELLOW,CYAN,HOURRAW-HOURSTART,RESETC);
		pprintf("HOURRAW(%s%d%s)=%s%d%s\n",GREEN,HOURRAW,YELLOW,CYAN,DATAEND-HOURRAW,RESETC);
		pprintf("DATAEND(%s%d%s)=%s%d%s\n",GREEN,DATAEND,YELLOW,CYAN,DATAEND-BEATSTART,RESETC);
		pprintf("TOTALFRAM(%s%d%s) Devices %s%d%s\n",GREEN,TOTALFRAM,YELLOW,CYAN,(DATAEND-BEATSTART)*MAXDEVS,RESETC);
		pprintf("%s=============== FRAM ===============%s\n",RED,RESETC);
	}
#endif
}


void shaMake(char * key,uint8_t klen,uint8_t* shaResult)
{
	mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

	char payload[]="me mima mucho";
	mbedtls_md_init(&mbedtls_ctx);
	ESP_ERROR_CHECK(mbedtls_md_setup(&mbedtls_ctx, mbedtls_md_info_from_type(md_type), 1));
	ESP_ERROR_CHECK(mbedtls_md_hmac_starts(&mbedtls_ctx, (const unsigned char *) key, klen));
	ESP_ERROR_CHECK(mbedtls_md_hmac_update(&mbedtls_ctx, (const unsigned char *) payload, strlen(payload)));
	ESP_ERROR_CHECK(mbedtls_md_hmac_finish(&mbedtls_ctx, shaResult));
	mbedtls_md_free(&mbedtls_ctx);
}

void app_main()
{


	esp_log_level_set("*", ESP_LOG_WARN);
	printSem= xSemaphoreCreateBinary();
	xSemaphoreGive(printSem);

	esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES|| err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    	pprintf("No free pages erased!!!!\n");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

	read_flash();// read configuration

	theConf.lastResetCode=rtc_get_reset_reason(0);

#ifdef DEBUGX
	if(theConf.traceflag & (1<<BOOTD))
	{
		pprintf("MeterIoT ConnManager\n");
		pprintf("%sBuildMgr starting up\n",BOOTDT);
		pprintf("%sFree memory: %d bytes\n", BOOTDT,esp_get_free_heap_size());
		pprintf("%sIDF version: %s\n", BOOTDT,esp_get_idf_version());
	}
	pprintf("\n%sFlash Button to erase config 1 secs%s\n",RED,RESETC);// chance to erase configuration and start fresh
	delay(1000);
#endif

	//if wrong CENTINEL or Flash Button, erase configuration. Loses a lot of info. Must Configure the Manager
	if (theConf.centinel!=CENTINEL || !gpio_get_level((gpio_num_t)0))
		erase_config();//start fresh

	init_vars();			// setup initial values
	init_fram();			// connect to fram to save readings
    init_boot();			// print boot stuff if defined
    wifi_init();			// this a Master Controller.
    mqtt_app_start();		// Start MQTT configuration and DO NOT connect. Connection will be on demand

#ifdef KBD
	xTaskCreate(&kbd,"kbd",20480,NULL, 4, NULL);
#endif

#ifdef DISPLAY // most likely heap damage. Routine seems unreliable
    initI2C();
    initScreen();
#endif

#ifdef TEMP // for the sake of Sensors sample
    init_temp();
#endif

   	// Managers Tasks
   	xTaskCreate(&cmdManager,"cmdMgr",20480,NULL, 5, &cmdHandle); 							//cmds received from the Host Controller via MQTT or internal TCP/IP. jSON based
	xTaskCreate(&framManager,"framMgr",4096,NULL, configMAX_PRIORITIES-5, &framHandle);	// Fram Manager
	xTaskCreate(&pcntManager,"pcntMgr",4096,NULL, configMAX_PRIORITIES-8, &pinHandle);	// We also control 5 meters. Pseudo ISR. This is d manager
	xTaskCreate(&tcpConnMgr,"TCP",4096,(void*)1, configMAX_PRIORITIES-10, &buildHandle);		// Messages from the Meter Controllers via socket manager

	// wait time from SNTP for 10 secs. If failure use FRAM last saved time
	EventBits_t uxBits = xEventGroupWaitBits(wifi_event_group, SNTP_BIT, false, true, 10000/  portTICK_RATE_MS);//
	if( ( uxBits & SNTP_BIT ) != 0 )
	{
		//sntp got time, check tariffs are Ok. We need DATE to obtain correct tariffs 24 hours for THIS day of the year

		memset(&tarifasDia,0,sizeof(tarifasDia));
		int err=fram.read_tarif_day(yearDay,(u8*)&tarifasDia);
		if(err!=0)
			pprintf("Error %d reading Tariffs day %d...recovering from Host\n",err,yearDay);

		u16 ttar=0;
		for (int aa=0;aa<24;aa++)
			ttar+=tarifasDia[aa];
		if(ttar==0) // load tariffs...something not right. Loadit from Host
			loadDefaultTariffs();

#ifdef DEBUGX
		if(theConf.traceflag & (1<<BOOTD))
		{
			pprintf("%sDay[%d]",BOOTDT,yearDay);
			for (int aa=0;aa<24;aa++)
				pprintf("[%02d]=%02x ",aa,tarifasDia[aa]);
			pprintf("\nHora %d Tarifa %02x\n",horag,tarifasDia[horag]);
		}
#endif

	}
	else
	{
		loginT now;
		pprintf("Emergency recover time\n");
		fram.readMany(FRAMDATE,(uint8_t*)&now.thedate,sizeof(now.thedate));
		updateDateTime(now);
	}

	theConf.bootcount++;
	time(&theConf.lastReboot);
	write_to_flash();

	pcnt_init();// initialize it. Several tricks apply. Read there. Depends on Tariffs so must be after we know tariffs and date are OK

	xTaskCreate(&watchDog,"dog",4096,(void*)1, 4, &watchHandle);				// Care taker of MeterM to report to Host

	if(theConf.traceflag & (1<<BOOTD))
		pprintf("%sStarting Webserver\n",BOOTDT);

	xTaskCreate(&start_webserver,"web",4096,(void*)1, 4, &webHandle);	// Messages from the Meters. Controller Section socket manager

#ifdef DISPLAY
	xTaskCreate(&displayManager,"dispMgr",4096,NULL, 4, &displayHandle);
#endif

}
