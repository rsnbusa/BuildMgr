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

const static int MQTT_BIT 				= BIT0;
const static int WIFI_BIT 				= BIT1;
const static int PUB_BIT 				= BIT2;
const static int DONE_BIT 				= BIT3;
const static int SNTP_BIT 				= BIT4;
const static int TELEM_BIT 				= BIT5;
const static int SENDH_BIT 				= BIT6;

EXTERN bool								isrf,connf,wifif,apstaf,displayf,miscanf,mqttf,firstmqtt,sessionf;
EXTERN char								*tempb,texto[101],stateName[5][20],iv[16],key[32],controlQueue[30],cmdQueue[30],respondQueue[30],sosQueue[30],sharesult[32];
EXTERN cmdRecord 						cmds[MAXCMDS];
EXTERN cmdType							theCmd;
EXTERN config_flash						theConf;
EXTERN esp_aes_context					ctx ;
EXTERN esp_mqtt_client_config_t 		mqtt_cfg;
EXTERN esp_mqtt_client_handle_t 		clientCloud;
EXTERN EventGroupHandle_t 				wifi_event_group;
EXTERN FramSPI							fram;
EXTERN gpio_config_t 					io_conf;
EXTERN heaper							theheap[MAXSAMPLESHEAP];
EXTERN host_t							*setupHost[MAXDEVS];
EXTERN int 								socket_id,binary_file_length,diaHoraTarifa,vanMacs,usedMacs,vanadd,vanvueltas,globalSocks,telemResult,MQTTDelay,sendH,framOk;
EXTERN macControl						losMacs[MAXSTA];
EXTERN mactemp							tempMac[MAXSTA];
EXTERN mbedtls_ctr_drbg_context			ctr_drbg;
EXTERN mbedtls_entropy_context 			entropy;
EXTERN mbedtls_md_context_t 			mbedtls_ctx;
EXTERN mbedtls_pk_context 				pk;
EXTERN meterType						theMeters[MAXDEVS];
EXTERN nvs_handle 						nvshandle;
EXTERN QueueHandle_t 					mqttQ,mqttR,framQ,pcnt_evt_queue,mtm[MAXSTA];
EXTERN SemaphoreHandle_t 				wifiSem,framSem,printSem,flashSem;
EXTERN task_param						tParam[MAXSTA];
EXTERN TaskHandle_t						webHandle,timeHandle,simHandle,blinkHandle,cmdHandle,framHandle,pinHandle,buildHandle,watchHandle,displayHandle,submHandle;
EXTERN time_t							mgrTime[7],oldtimehh;
EXTERN TimerHandle_t					hourChangeT,connHandle;
EXTERN u16 								theGuard,qdelay,mesg,diag,horag,yearg,wDelay,tarifasDia[24],oldMesg,oldDiag,oldHorag,yearDay,oldYearDay,vanHeap;
EXTERN uint32_t							totalPulses,oldCurBeat[MAXDEVS],oldCurLife[MAXDEVS],sentTotal,llevo,tallies[MAXSTA][MAXDEVS],theMacNum,startGuard,stcount,hh,oldhh,runningHeap,firstHeap;
EXTERN uint8_t 							daysInMonth [12],qwait,lastalign,lastFont,workingDevs,llevoTempMac;
#ifdef KBD
EXTERN char								lookuptable[NKEYS][10];
#endif
#ifdef TEMP
EXTERN DS18B20_Info 					*ds18b20_info;
EXTERN float							theTemperature;
EXTERN OneWireBus 						*owb;
EXTERN owb_rmt_driver_info 				rmt_driver_info;
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
