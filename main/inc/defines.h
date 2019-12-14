#ifndef defines_h
#define defines_h

//#define MULTIX		// if we are going to have 9575 GPIO Expanders
//#define TEMP
//#define WITHMETERS
//#define MQT
//#define DISPLAY
#define KBD			// only for testing build

#define BUILDMGRPORT					30000


#ifdef MULTIX

#define I2CPORT 						I2C_NUM_0
#define I2CSPEED						400000

// PCA9575 commands
#define IN0	 							0x00
#define IN1								0x01
#define	INVRT0							0x02
#define	INVRT1							0x03
#define	BKEN0							0x04
#define	BKEN1							0x05
#define	PUPD0							0x06
#define	PUPD1							0x07
#define	CFG0							0x08
#define	CFG1							0x09
#define	OUT0							0x0a
#define	OUT1							0x0b
#define	MSK0							0x0c
#define	MSK1							0x0d
#define	INTS0							0x0e
#define	INTS1							0x0f
//#define MUXPIN							27
//#define MUXPIN1							26

#define SDAW                			21      // SDA
#define SCLW                			22      // SCL for Wire service
#define DSPIN							23		// ds18b20

#define WRITE_BIT 						I2C_MASTER_WRITE              	/*!< I2C master write */
#define READ_BIT 						I2C_MASTER_READ              	/*!< I2C master read */
#define ACK_CHECK_EN 					0x1                        		/*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 					0x0                       		/*!< I2C master will not check ack from slave */
#define ACK_VAL 						0x0                             /*!< I2C ack value */
#define NACK_VAL 						0x1                            	/*!< I2C nack value */

#endif

#define u32								uint32_t
#define u16								uint16_t
#define u8								uint8_t

#define QDELAY							10
#define MAXWIFI							100
#define WIFILED							2

#define BUFFSIZE 						4096
#define TEXT_BUFFSIZE 					4096
#define EXAMPLE_SERVER_IP   			"185.176.43.60"
#define EXAMPLE_SERVER_PORT 			"80"
#define EXAMPLE_FILENAME 				"http://feediot.c1.biz/emitter_client.bin"

#define INTERNET						"Porton"
#define INTERNETPASS					"csttpstt"
#define LOCALTIME						"GMT+5" // Quito time

#define MAXBUFFER						1500
#define MAXCMDS							20
#define MAXDEVS							5

#define METER0							4
#define METER1							22
#define METER2							16
#define METER3							17
#define METER4							21

#define BREAK0							13
#define BREAK1							14
#define BREAK2							25
#define BREAK3							26
#define BREAK4							27

#endif
