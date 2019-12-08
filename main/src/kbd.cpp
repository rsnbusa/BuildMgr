#define GLOBAL
#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern void drawString(int x, int y, string que, int fsize, int align,displayType showit,overType erase);


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
		printf("Mac[%d][%d]: %.2x:%.2x:%.2x:%.2x %s\n",a, losMacs[a].trans,
		them[0],them[1],them[2],them[3],strftime_buf);
	}
	printf("Loops %d\n",vanvueltas);

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

void kbd(void *arg) {
	int len;
	uart_port_t uart_num = UART_NUM_0 ;
	char s1[20];
	char data[20];


	uart_config_t uart_config = {
			.baud_rate = 115200,
			.data_bits = UART_DATA_8_BITS,
			.parity = UART_PARITY_DISABLE,
			.stop_bits = UART_STOP_BITS_1,
			.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
			.rx_flow_ctrl_thresh = 122,
	};
	uart_param_config(uart_num, &uart_config);
	uart_set_pin(uart_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	esp_err_t err= uart_driver_install(uart_num, 1024 , 1024, 10, NULL, 0);
	if(err!=ESP_OK)
		printf("Error UART Install %d\n",err);


	while(1)
	{
		len = uart_read_bytes((uart_port_t)uart_num, (uint8_t*)data, sizeof(data),20);
		if(len>0)
		{
			switch(data[0])
			{
			case '9':
				printf("Total Received Msgs %d\n",llevoMsg);
				break;
			case 'm':
			case 'M':
					printStationList();
					break;
			case 'S':
			case 's':
					displayf=!displayf;
					if(!displayf)
					{
						display.clear();
						drawString(64,12,"MeterIoT",24,TEXT_ALIGN_CENTER,DISPLAYIT,NOREP);
					}
					break;
			case 'd':
			case 'D':
					deb=!deb;
					printf("Debug %s\n",deb?"On":"Off");
					break;
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
			default:
				break;
			}

		}
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}

