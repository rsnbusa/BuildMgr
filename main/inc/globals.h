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

EXTERN bool								isrf,connf,wifif,apstaf,displayf;
EXTERN char								http_request[100],tempb[5000],texto[101];
EXTERN cmdRecord 						cmds[MAXCMDS];
EXTERN cmdType							theCmd;
EXTERN config_flash						theConf;
EXTERN DS18B20_Info 					*ds18b20_info;
EXTERN esp_mqtt_client_handle_t 		clientCloud;
EXTERN float							ttemp;
EXTERN FramSPI							fram;
EXTERN gpio_config_t 					io_conf;
EXTERN int 								socket_id,binary_file_length,numsensors,diaHoraTarifa,vanMacs,vanadd,vanvueltas,van,globalSocks;
EXTERN macControl						losMacs[MAXSTAS];
EXTERN meterType						theMeters[MAXDEVS],algo;
EXTERN nvs_handle 						nvshandle;
EXTERN OneWireBus 						*owb;
EXTERN owb_rmt_driver_info 				rmt_driver_info;
EXTERN QueueHandle_t 					mqttQ,mqttR,framQ,pcnt_evt_queue;
EXTERN SemaphoreHandle_t 				wifiSem,framSem;
EXTERN TimerHandle_t					hourChangeT;
EXTERN u16 								qdelay,llevoMsg,mesg,diag,horag,yearg,wDelay,tarifasDia[24],oldMesg,oldDiag,oldHorag,yearDay;
EXTERN u32								sentTotal,llevo,tallies[MAXSTAS][MAXDEVS];
EXTERN u8								qwait,lastalign,lastFont;
EXTERN uint32_t							totalPulses,oldCurBeat[MAXDEVS],oldCurLife[MAXDEVS];
EXTERN host_t							setupHost[MAXDEVS];
EXTERN TaskHandle_t						webHandle;

#ifdef KBD
EXTERN char								lookuptable[NKEYS][10];
#endif

#ifdef MULTIX
EXTERN QueueHandle_t					muxQueue;
#endif

#ifdef DISPLAY
EXTERN SemaphoreHandle_t				I2CSem;
EXTERN u16								icadd[2];
EXTERN I2C								miI2C;
EXTERN i2ctype 							i2cp;
#ifdef GLOBAL
EXTERN	SSD1306             			display;
#else
EXTERN SSD1306 							display(0x3c, &miI2C);
#endif
#endif

#endif /* MAIN_GLOBALS_H_ */
