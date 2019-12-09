#ifndef MAIN_GLOBALS_H_
#define MAIN_GLOBALS_H_

#ifdef GLOBAL
#define EXTERN extern
#else
#define EXTERN
#endif
#include "defines.h"
#include "projTypes.h"
#include "includes.h"
using namespace std;

EXTERN esp_mqtt_client_handle_t 		clientCloud;
EXTERN QueueHandle_t 					mqttQ,mqttR;
EXTERN SemaphoreHandle_t 				wifiSem;
EXTERN bool								isrf,connf,wifif,apstaf,firstTimeTimer,displayf,deb,msgf;
EXTERN int								numsensors;
EXTERN u32								sentTotal,llevo;
EXTERN u8								qwait,lastalign,lastFont;
EXTERN u16 								qdelay;
EXTERN char								texto[101];
EXTERN meterType						theMeters[4],algo;
EXTERN gpio_config_t 					io_conf;
EXTERN cmdType							theCmd;
EXTERN char								tempb[5000];
EXTERN int 								socket_id,binary_file_length ;
EXTERN char								http_request[100];
#ifdef MULTIX
EXTERN SemaphoreHandle_t				I2CSem;
EXTERN u16								icadd[2],llevoMsg;
EXTERN QueueHandle_t					muxQueue;
#endif

EXTERN OneWireBus 						* owb;
EXTERN owb_rmt_driver_info 				rmt_driver_info;
EXTERN DS18B20_Info 					* ds18b20_info;
EXTERN float							ttemp;
EXTERN	I2C								miI2C;
EXTERN	i2ctype 						i2cp;
#ifdef GLOBAL
EXTERN	SSD1306             			display;
#else
EXTERN SSD1306 							display(0x3c, &miI2C);
#endif
EXTERN int								vanMacs,vanadd,vanvueltas;
EXTERN macControl						losMacs[50];
EXTERN u32								tallies[50][5];

#endif /* MAIN_GLOBALS_H_ */
