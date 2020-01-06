#define GLOBAL
#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

#ifdef DISPLAY
extern void drawString(int x, int y, string que, int fsize, int align,displayType showit,overType erase);
#endif

extern void write_to_flash();
extern void load_from_fram(u8 meter);
extern void firmwareCmd(parg *pArg);
extern void loadit(parg *pArg);

void printStationList()
{
	uint8_t them[4];
	struct tm timeinfo;
	char strftime_buf[64];

	for (int a=0;a<vanMacs;a++)
	{
		localtime_r(&losMacs[a].lastUpdate, &timeinfo);
			strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
		memcpy(&them,&losMacs[a].macAdd,4);
		printf("Mac[%d][%d]:%.2x:%.2x:%.2x:%.2x Ip:%s (%s)\n",a,losMacs[a].trans[0],
		them[0],them[1],them[2],them[3],(char*)ip4addr_ntoa((ip4_addr_t *)&losMacs[a].theIp),strftime_buf);
	}
}


int get_string(uart_port_t uart_num,uint8_t cual,char *donde)
{
	uint8_t ch;
	int son=0,len;
	son=0;
	while(1)
	{
		len = uart_read_bytes(UART_NUM_0, (uint8_t*)&ch, 1,4/ portTICK_RATE_MS);
		if(len>0)
		{
			if(ch==cual)
			{
				*donde=0;
				return son;
			}
			else
			{
				*donde=(char)ch;
				donde++;
				son++;
			}
		}
		vTaskDelay(30/portTICK_PERIOD_MS);
	}
}

int cmdfromstring(string key)
{
    for (int i=0; i <NKEYS; i++)
    {
    	string s1=string(lookuptable[i]);
    	if(strstr(s1.c_str(),key.c_str())!=NULL)
            return i;
    }
    return -1;
}


int keyfromstring(char *key)
{

    int i;
    for (i=0; i < NKEYS; i++) {

        if (strcmp(lookuptable[i], key) == 0)
            return i;
    }
    return 100;
}

static void printControllers()
{
	printf("Listing %d Active Controllers\n",vanMacs);
	for (int a=0;a<vanMacs;a++)
	{
		printf("Controller[%d] Mac %06x seen %s\n",a,losMacs[a].macAdd,ctime(&losMacs[a].lastUpdate));
		for (int b=0;b<MAXDEVS;b++)
			printf("Meter[%d]=%s KwH %6d Beats %9d\n",b,losMacs[a].meterSerial[b],losMacs[a].controlLastKwH[b],losMacs[a].controlLastBeats[b]);
	}
}


void kbd(void *arg) {
	int len,cualf,a;
	uart_port_t uart_num = UART_NUM_0 ;
	char s1[20],s2[20];
	char data[20];
	u8 *p;
	string ss;
	char lastcmd=10;
	uint32_t tots=0,totsp;
	uint16_t count,th1;
	u32 framAddress;
	u8 fueron;
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

	uart_config_t uart_config = {
			.baud_rate = 460800,
			.data_bits = UART_DATA_8_BITS,
			.parity = UART_PARITY_DISABLE,
			.stop_bits = UART_STOP_BITS_1,
			.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
			.rx_flow_ctrl_thresh = 122,
	};
	uart_param_config(uart_num, &uart_config);
	esp_err_t err= uart_driver_install(uart_num, 1024 , 1024, 10, NULL, 0);
	if(err!=ESP_OK)
	{
		printf("Error UART Install %d\n",err);
		vTaskDelete(NULL);
	}

	printf("Kbd Ready\n");
	while(1)
	{
		len = uart_read_bytes((uart_port_t)uart_num, (uint8_t*)data, sizeof(data),20);
		if(len>0)
		{
			if(data[0]==10)
				data[0]=lastcmd;
			lastcmd=data[0];

			switch(data[0])
			{
			case '+':
				printControllers();
				break;
			case'-':
				for (int a=0;a<24;a++)
					printf("Tarifa[%d]=%d\n",a,tarifasDia[a]);
				break;
			case 'u':
			case 'U':
				fram.readMany(FRAMDATE,(uint8_t*)&now,sizeof(now));
				setenv("TZ", LOCALTIME, 1);
				tzset();
				localtime_r(&now, &timeinfo);
				strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
				printf("The recovered date/time in %s is: %s day of Year %d\n", LOCALTIME,strftime_buf,timeinfo.tm_yday);
				break;
			case 'W':
				printf("Firmware \n");
				firmwareCmd(NULL);
				break;
			case 'w':
				printf("Tariffs\n");
				loadit(NULL);
				break;
			case 'd':
			case 'D':
				printf("WatchDog delay(%d):",wDelay);
				fflush(stdout);
				len=get_string((uart_port_t)uart_num,10,s1);
				if(len<=0)
				{
					printf("\n");
					break;
				}
				wDelay=atoi(s1);
				break;
			case 'f':
			case 'F':
				printf("Format FRAM initValue:");
				fflush(stdout);
				len=get_string((uart_port_t)uart_num,10,s1);
				if(len<=0)
				{
					printf("\n");
					break;
				}
				fram.format(atoi(s1),tempb,sizeof(tempb),true);
				printf("Format done\n");
				totalPulses=0;
				for(int a=0;a<MAXDEVS;a++)
					load_from_fram(a);
				break;
			case '0':
				printf("Dumping core...\n");
				vTaskDelay(3000/portTICK_PERIOD_MS);
				p=0;
				*p=0;
				break;
			case 't':
			case 'T':
				tots=totsp=0;
				for (int a=0;a<MAXDEVS;a++)
				{
					if(theMeters[a].currentBeat>0)
					{
						pcnt_get_counter_value((pcnt_unit_t)a,(short int *) &count);
						tots+=theMeters[a].currentBeat;
						totsp+=count;
						pcnt_get_event_value((pcnt_unit_t)a, PCNT_EVT_THRES_1, &th1);
						printf("%sMeter[%d]=%s Beats %d kWh %d \n",YELLOW,a,RESETC,theMeters[a].currentBeat,theMeters[a].curLife);
					}
				}
				printf("%sTotal Pulses rx=%s %d (%s) Pending %d\n",RED,RESETC,totalPulses,tots==totalPulses?"Ok":"No",totsp);
				break;
			case '9':
				for (int a=0;a<50;a++)
				{
					if(tallies[a][0]>0)
					{
						printf("\nController[%d]",a);
						for(int b=0;b<5;b++)
						{
							if(tallies[a][b]>0)
								printf(" [%d]=%d",b,tallies[a][b]);
						}
					}
				}
				printf("\nTotal Received Msgs %d\n",llevoMsg);
				break;
			case 'm':
			case 'M':
					printStationList();
					break;
#ifdef DISPLAY
			case 'S':
			case 's':
					displayf=!displayf;
					if(!displayf)
					{
						display.clear();
						drawString(64,12,"MeterIoT",24,TEXT_ALIGN_CENTER,DISPLAYIT,NOREP);
					}
					break;
#endif
			case 'l':
			case 'L':
					printf("LOG level:(N)one (I)nfo (E)rror (V)erbose (W)arning:");
					fflush(stdout);
					len=get_string((uart_port_t)uart_num,10,s1);
					if(len<=0)
					{
						printf("\n");
						break;
					}
						switch (s1[0])
						{
							case 'n':
							case 'N':
									esp_log_level_set("*", ESP_LOG_NONE);
									break;
							case 'i':
							case 'I':
									esp_log_level_set("*", ESP_LOG_INFO);
									break;
							case 'e':
							case 'E':
									esp_log_level_set("*", ESP_LOG_ERROR);
									break;
							case 'v':
							case 'V':
									esp_log_level_set("*", ESP_LOG_VERBOSE);
									break;
							case 'w':
							case 'W':
									esp_log_level_set("*", ESP_LOG_WARN);
									break;
						}
					break;
					case 'r':
					case 'R':
						printf("Read FRAM address:");
						fflush(stdout);
						len=get_string((uart_port_t)uart_num,10,s1);
						if(len<=0)
						{
							printf("\n");
							break;
						}
						framAddress=atoi(s1);
						printf("Count:");
						fflush(stdout);
						len=get_string((uart_port_t)uart_num,10,s1);
						if(len<=0)
						{
							printf("\n");
							break;
						}
						fueron=atoi(s1);
						fram.readMany(framAddress,tempb,fueron);
						for (int a=0;a<fueron;a++)
							printf("%02x-",tempb[a]);
						printf("\n");
						break;
			case 'v':
			case 'V':{
				printf("Trace Flags ");
				for (int a=0;a<NKEYS/2;a++)
					if (theConf.traceflag & (1<<a))
					{
						if(a<(NKEYS/2)-1)
							printf("%s-",lookuptable[a]);
						else
							printf("%s",lookuptable[a]);
					}
				printf("\nEnter TRACE FLAG:");
				fflush(stdout);
				memset(s1,0,sizeof(s1));
				get_string((uart_port_t)uart_num,10,s1);
				memset(s2,0,sizeof(s2));
				for(a=0;a<strlen(s1);a++)
					s2[a]=toupper(s1[a]);
				ss=string(s2);
				if(strlen(s2)<=1)
					break;
				if(strcmp(ss.c_str(),"NONE")==0)
				{
					theConf.traceflag=0;
					write_to_flash();
					break;
				}
				if(strcmp(ss.c_str(),"ALL")==0)
				{
					theConf.traceflag=0xFFFF;
					write_to_flash();
					break;
				}
				cualf=cmdfromstring((char*)ss.c_str());
				if(cualf<0)
				{
					printf("Invalid Debug Option\n");
					break;
				}
				if(cualf<NKEYS/2 )
				{
					printf("Debug Key %s added\n",lookuptable[cualf]);
					theConf.traceflag |= 1<<cualf;
					write_to_flash();
					break;
				}
				else
				{
					cualf=cualf-NKEYS/2;
					printf("Debug Key %s removed\n",lookuptable[cualf]);
					theConf.traceflag ^= (1<<cualf);
					write_to_flash();
					break;
				}

				}

			default:
				break;
			}

		}
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}

