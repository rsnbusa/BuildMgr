#ifndef defines_h
#define defines_h

#define FREEANDNULL(x)		if(x) {free(x);x=NULL;}
#define DISPLAY
#define MQTTLOCAL
#define KBD					// only for testing build
#define DEBUGX

#define THEKEY							"me mima mucho"
#ifdef 	DISPLAY
#define SCLW                			22      // SCL for Wire service
#define SDAW                			21      // SDA
#endif

#define TEMP
#ifdef  TEMP
#define DSPIN							27
#endif

#define MAXBEATLOSS						10
#define AESL							32
#define BUFFSIZE 						4096
#define BUILDMGRPORT					30000
#define CENTINEL						0x12345678
#define MAXLOSTSYNC						3
#define MAXSAMPLESHEAP					500
#define MAXTIMEOUTS						5
#define NUMTELSLOTS						6
#define OTA_BIN_FILE	 				"http://feediot.c1.biz/buildMgrOled.bin"

#define GETMT							4096 //getmessage task stack size

#define INTERNET						"Porton"
#define INTERNETPASS					"csttpstt"
#define LOCALTIME						"GMT+5" // Quito time

#define MAXBUFFER						2500
#define MAXCHARS						40
#define MAXCMDS							24
#define MAXDEVS							5
#define MAXSTA							10
#define MAXWIFI							100

#define METER0							4
#define METER1							14 //22
#define METER2							25 //16
#define METER3							26 //17
#define METER4							13 //21

#define BREAK0							33
#define BREAK1							33
#define BREAK2							33
#define BREAK3							33
#define BREAK4							33

//FRAM pins SPI
#define FMOSI							23
#define FMISO							19
#define FCLK							18
#define FCS								5

#define NKEYS							33
#define OTA_URL_SIZE 					1024
#define QDELAY							10
#define SUBMODETO						4000
#define TEXT_BUFFSIZE 					4096
#define u16								uint16_t
#define u32								uint32_t
#define u8								uint8_t
#define WIFILED							2

#define TSTATE1							10000
#define TSTATE2							5000
#define TSTATE3							65000

//30	Black
//31	Red
//32	Green
//33	Yellow
//34	Blue
//35	Magenta
//36	Cyan
//37	White

#define BOOTDT							"\e[31m[BOOTD]\e[0m"
#define CMDDT							"\e[35m[CMDD]\e[0m"
#define ERASET							"\e[2J"
#define FRAMDT							"\e[37m[FRAMD]\e[0m"
#define FRMCMDT							"\e[35m[FRMCMDT]\e[0m"
#define GENDT							"\e[37m[GEND]\e[0m"
#define HEAPDT							"\e[37m[HEAPD]\e[0m"
#define HOSTDT							"\e[31m[HOSTD]\e[0m"
#define INTDT							"\e[36m[INTD]\e[0m"
#define MQTTDT							"\e[32m[MQQTD]\e[0m"
#define MQTTTT							"\e[32m[MQTTT]\e[0m"
#define MSGDT							"\e[32m[MSGD]\e[0m"
#define OTADT							"\e[34m[OTAD]\e[0m"
#define PUBSUBDT						"\e[33m[PUNSUBD]\e[0m"
#define SIMDT							"\e[36m[SIMD]\e[0m"
#define SUBMT							"\e[31m[SUBMGR]\e[0m"
#define TIEMPOT							"\e[31m[TIMED]\e[0m"
#define WEBDT							"\e[37m[WEBD]\e[0m"
#define WIFIDT							"\e[31m[WIFID]\e[0m"

#define BLACKC							"\e[30m"
#define BLUE							"\e[34m"
#define CYAN							"\e[36m"
#define GRAY							"\e[90m"
#define GREEN							"\e[32m"
#define LGREEN							"\e[92m"
#define LRED							"\e[91m"
#define LYELLOW							"\e[93m"
#define MAGENTA							"\e[35m"
#define RED								"\e[31m"
#define RESETC							"\e[0m"
#define WHITEC							"\e[37m"
#define YELLOW							"\e[33m"


#define INDEX(base,a,bb,c,d1,d2) (char*)((char*)base+(a*d1*d2)+(bb*d2)+c)
#endif
