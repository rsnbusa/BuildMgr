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

EXTERN bool								isrf,connf,wifif,apstaf,displayf,miscanf;
EXTERN char								http_request[100],tempb[5000],texto[101],stateName[5][20];
EXTERN cmdRecord 						cmds[MAXCMDS];
EXTERN cmdType							theCmd;
EXTERN config_flash						theConf;
EXTERN esp_mqtt_client_config_t 		mqtt_cfg;
EXTERN esp_mqtt_client_handle_t 		clientCloud;
EXTERN EventGroupHandle_t 				wifi_event_group;
EXTERN FramSPI							fram;
EXTERN gpio_config_t 					io_conf;
EXTERN host_t							setupHost[MAXDEVS];
EXTERN int 								socket_id,binary_file_length,numsensors,diaHoraTarifa,vanMacs,usedMacs,vanadd,vanvueltas,globalSocks;
EXTERN macControl						losMacs[MAXSTA];
EXTERN mbedtls_md_context_t 			mbedtls_ctx;
EXTERN meterType						theMeters[MAXDEVS];
EXTERN nvs_handle 						nvshandle;
EXTERN QueueHandle_t 					mqttQ,mqttR,framQ,pcnt_evt_queue;
EXTERN SemaphoreHandle_t 				wifiSem,framSem;
EXTERN string							controlQueue,cmdQueue;
EXTERN TaskHandle_t						webHandle,timeHandle,simHandle,blinkHandle,cmdHandle,framHandle,pinHandle,buildHandle,watchHandle,displayHandle,submHandle;
EXTERN time_t							mgrTime[7];
EXTERN TimerHandle_t					hourChangeT,connHandle;
EXTERN u16 								theGuard,qdelay,llevoMsg,mesg,diag,horag,yearg,wDelay,tarifasDia[24],oldMesg,oldDiag,oldHorag,yearDay,oldYearDay;
EXTERN uint32_t							totalPulses,oldCurBeat[MAXDEVS],oldCurLife[MAXDEVS],sentTotal,llevo,tallies[MAXSTA][MAXDEVS],theMacNum;
EXTERN uint8_t 							daysInMonth [12],qwait,lastalign,lastFont,workingDevs;
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
