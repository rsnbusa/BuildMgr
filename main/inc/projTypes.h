#ifndef projTypes_h
#define projTypes_h

typedef void (*mqttp)();

typedef enum displayType {NODISPLAY,DISPLAYIT} displayType;
typedef enum overType {NOREP,REPLACE} overType;
enum sourceFlags{HOSTT,LOCALT};
enum debugflags{BOOTD,WIFID,MQTTD,PUBSUBD,OTAD,CMDD,WEBD,GEND,MQTTT,FRMCMD,INTD,FRAMD,MSGD,TIMED,SIMD,HOSTD};
typedef enum macState{BOOTSTATE,CONNSTATE,LOGINSTATE,MSGSTATE,TOSTATE} meterState;
typedef enum reportState{NOREPORT,REPORTED} reportState_t;

typedef struct whitel{
	meterState		dState;
	uint32_t		seen;
	reportState_t	report;
	time_t			ddate;
} whitelist_t;

typedef struct macC{
	int				theSock;
	TaskHandle_t 	theHandle;
	time_t			lastUpdate;
	u32				theIp;
	uint32_t 		macAdd;
	uint32_t 		trans[MAXDEVS],controlLastKwH[MAXDEVS],controlLastBeats[MAXDEVS];
	TimerHandle_t	timerH;
	char			meterSerial[MAXDEVS][20];
	meterState		dState,pState;
	reportState		report;
	uint32_t		stateChangeTS[sizeof(reportState)];
} macControl;

typedef struct taskp{
	int	sock_p;
	int pos_p;
	int macf;
} task_param;

typedef struct meterType{
	 bool saveit,lowThresf;
	 char serialNumber[20];
	 u16 beatsPerkW,curMonth,curMonthRaw,curDay,curDayRaw;
	 u32 curLife,curCycle,lastKwHDate,msNow, minamps,maxamps,currentBeat,vanMqtt,ampTime,beatSave;
	 u8 curHour,cycleMonth,curHourRaw,pos,pin,pinB;
	 mqttp code;
} meterType;

typedef struct mqttMsg{
	char	*mensaje;
	int		fd;
	u16		pos;
}cmdType;

typedef struct i2cType{
	gpio_num_t sdaport,sclport;
	i2c_port_t i2cport;
} i2ctype;

typedef struct loginTarif{
	time_t thedate;
	uint16_t theTariff;
} loginT;


typedef struct pcomm{
    int pComm;
    uint8_t typeMsg;
    void *pMessage;
    double macn;
}parg;



typedef void (*functrsn)(parg *);

typedef struct cmdRecord{
    char comando[20];
    uint8_t source;
    functrsn code;
}cmdRecord;

typedef struct conId{
	int8_t		altDay;
	u8			dDay;
	u16			connSlot;
}connStruct;

typedef struct slot{
	u8			slot_time;
	u8			server_num;
	u8			tariff_id;
}slt_t;

typedef struct config {
    char 		medidor_id[MAXDEVS][MAXCHARS],meterConnName[MAXCHARS];
    time_t 		bornDate[MAXDEVS];
    u16 		beatsPerKw[MAXDEVS],bootcount,lastResetCode;
    slt_t 		slot_Server;
    u32 		bornKwh[MAXDEVS],centinel,traceflag;
    u8 			configured[MAXDEVS],active;
    connStruct	connId;
    uint32_t	reservedMacs[10];
    uint16_t	reservedCnt;
} config_flash;

typedef struct framq{
	int whichMeter;
	bool addit;
}framMeterType;

typedef struct pcntt{
    int unit;  				// the PCNT unit that originated an interrupt
    uint32_t status; 		// information on the event type that caused the interrupt
} pcnt_evt_t;

typedef struct internalHost{
	char		meterid[20];
	uint32_t	startKwh;
	uint16_t	bpkwh;
	uint16_t	diaCorte;
	uint16_t	tariff;
	bool		valid;
}host_t;
#endif
