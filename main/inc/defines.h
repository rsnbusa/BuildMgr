#ifndef defines_h
#define defines_h

//#define MULTIX		// if we are going to have 9575 GPIO Expanders
//#define TEMP
//#define WITHMETERS
//#define MQT
#define DISPLAY
#define MQTTLOCAL
#define KBD			// only for testing build
#define DEBUGX

#ifdef MULTIX

#define ACK_CHECK_DIS 					0x0                       		/*!< I2C master will not check ack from slave */
#define ACK_CHECK_EN 					0x1                        		/*!< I2C master will check ack from slave*/
#define ACK_VAL 						0x0                             /*!< I2C ack value */
#define	BKEN0							0x04
#define	BKEN1							0x05
#define	CFG0							0x08
#define	CFG1							0x09
#define DSPIN							23		// ds18b20
#define I2CPORT 						I2C_NUM_0
#define I2CSPEED						400000

// PCA9575 commands
#define IN0	 							0x00
#define IN1								0x01
#define	INTS0							0x0e
#define	INTS1							0x0f
#define	INVRT0							0x02
#define	INVRT1							0x03
#define	MSK0							0x0c
#define	MSK1							0x0d
#define NACK_VAL 						0x1                            	/*!< I2C nack value */
#define	OUT0							0x0a
#define	OUT1							0x0b
#define	PUPD0							0x06
#define	PUPD1							0x07
#define READ_BIT 						I2C_MASTER_READ              	/*!< I2C master read */
#endif
#ifdef DISPLAY
#define SCLW                			22      // SCL for Wire service
//#define MUXPIN							27
//#define MUXPIN1							26

#define SDAW                			21      // SDA

#define WRITE_BIT 						I2C_MASTER_WRITE              	/*!< I2C master write */

#endif

#define BUFFSIZE 						4096
#define BUILDMGRPORT					30000

#define CENTINEL						0x12345678
#define EXAMPLE_FILENAME 				"http://feediot.c1.biz/buildMgrOled.bin"
#define EXAMPLE_SERVER_IP   			"185.176.43.60"
#define EXAMPLE_SERVER_PORT 			"80"

#define GETMT							2048 //getmessage task stack size

#define INTERNET						"Porton"
#define INTERNETPASS					"csttpstt"
#define LOCALTIME						"GMT+5" // Quito time

#define MAXBUFFER						2500
#define MAXCHARS						40
#define MAXCMDS							20
#define MAXDEVS							5
#define MAXSTAS							20
#define MAXWIFI							100

#define METER0							4
#define METER1							14 //22
#define METER2							16
#define METER3							17
#define METER4							13 //21

#define BREAK0							13
#define BREAK1							14
#define BREAK2							25
#define BREAK3							26
#define BREAK4							27

//FRAM pins SPI
#define FMOSI							23
#define FMISO							19
#define FCLK							18
#define FCS								5

#define NKEYS							30
#define QDELAY							10
#define TEXT_BUFFSIZE 					4096
#define u16								uint16_t
#define u32								uint32_t
#define u8								uint8_t
#define WIFILED							2
#define TIMEWAITPCNT 					60000 // 1 minute
#define MIN30							TIMEWAITPCNT*30 //30 minutes

//30	Black
//31	Red
//32	Green
//33	Yellow
//34	Blue
//35	Magenta
//36	Cyan
//37	White

#define BOOTDT							"\e[31m[BOOTD]\e[0m"
#define BOOTDT							"\e[31m[BOOTD]\e[0m"
#define WIFIDT							"\e[31m[WIFID]\e[0m"
#define MQTTDT							"\e[32m[MQQTD]\e[0m"
#define MQTTTT							"\e[32m[MQTTT]\e[0m"
#define PUBSUBDT						"\e[33m[PUNSUBD]\e[0m"
#define CMDDT							"\e[35m[CMDD]\e[0m"
#define OTADT							"\e[34m[OTAD]\e[0m"
#define MSGDT							"\e[32m[MSGD]\e[0m"
#define INTDT							"\e[36m[INTD]\e[0m"
#define WEBDT							"\e[37m[WEBD]\e[0m"
#define GENDT							"\e[37m[GEND]\e[0m"
#define HEAPDT							"\e[37m[HEAPD]\e[0m"
#define FRAMDT							"\e[37m[FRAMD]\e[0m"
#define FRMCMDT							"\e[35m[FRMCMDT]\e[0m"
#define TIEMPOT							"\e[31m[TIMED]\e[0m"
#define SIMDT							"\e[36m[SIMD]\e[0m"
#define ERASET							"\e[2J"

#define BLACKC							"\e[30m"
#define RED								"\e[31m"
#define GREEN							"\e[32m"
#define YELLOW							"\e[33m"
#define BLUE							"\e[34m"
#define MAGENTA							"\e[35m"
#define CYAN							"\e[36m"
#define WHITEC							"\e[37m"
#define RESETC							"\e[0m"


#endif
