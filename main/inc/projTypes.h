#ifndef projTypes_h
#define projTypes_h

typedef void (*mqttp)();

typedef enum displayType {NODISPLAY,DISPLAYIT} displayType;
typedef enum overType {NOREP,REPLACE} overType;
enum sourceFlags{HOSTT,LOCALT,TOCONN};
enum debugflags{BOOTD,WIFID,MQTTD,PUBSUBD,OTAD,CMDD,WEBD,GEND,MQTTT,FRMCMD,INTD,FRAMD,MSGD,TIMED,SUBMGRD,HOSTD};
typedef enum macState{BOOTSTATE,CONNSTATE,LOGINSTATE,MSGSTATE,TOSTATE} meterState;
typedef enum reportState{NOREPORT,REPORTED} reportState_t;
enum mgrT{BUILDMGRT,CMDMGRT,WATCHMGRT,DISPLAYMGRT,FRAMMGRT,PINMGRT,SUBMGRT};
enum mqtters{MQTTOK=5500,NOCLIENT,PUBTM,STARTTM,STARTERR,QUEUEFAIL,BITTM};
enum pausers{PNONE,PSTAT,PTEL,PCMD};


typedef struct pcomm{
    int 	pComm;
    uint8_t pos;
    void 	*pMessage;
    double	macn;
}parg;

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
	uint32_t 		trans[MAXDEVS],controlLastKwH[MAXDEVS],controlLastBeats[MAXDEVS],msgCount;
	TimerHandle_t	timerH;
	char			meterSerial[MAXDEVS][20];
	meterState		dState,pState;
	reportState		report;
	uint32_t		stateChangeTS[sizeof(reportState)];
	uint8_t			hwState,toCount,lostSync;
	char			mtmName[20];
	char			theKey[33];
	uint16_t		aid;
} macControl;

typedef struct heaper{
	time_t		ts;
	uint32_t	theHeap;
	char		routine[20];
}heaper;

typedef struct hostC{
	uint8_t	meter;
	char	*msg;
} hostCmd;

typedef struct taskp{
	int		sock_p;
	int 	pos_p;
	int 	macf;
	cJSON	*elcmd;
	parg	*argument;
	char	*mensaje;
	char 	*getM;
} task_param;

typedef struct meterType{
	 bool 	saveit,lowThresf;
	 char 	serialNumber[20];
	 u16 	beatsPerkW,curMonth,curMonthRaw,curDay,curDayRaw;
	 u32 	curLife,curCycle,lastKwHDate,msNow, minamps,maxamps,currentBeat,vanMqtt,ampTime,beatSave;
	 u8 	curHour,cycleMonth,curHourRaw,pos,pin,pinB;
	 mqttp 	code;
} meterType;

typedef struct mqttMsgCmd{
	char		*mensaje;
	int			fd;
	u16			pos;
	task_param	*tParam;
}cmdType;

typedef struct i2cType{
	gpio_num_t sdaport,sclport;
	i2c_port_t i2cport;
} i2ctype;

typedef struct loginTarif{
	time_t		thedate;
	uint16_t 	theTariff;
} loginT;



typedef void (*functmqtt)(int);

typedef struct mqttMsgInt{
	uint8_t 	*message;	// memory of message. MUST be freed by the Submode Routine and allocated by caller
	uint16_t	msgLen;
	char		*queueName;	// queue name to send
	uint32_t	maxTime;	//max ms to wait
	functmqtt	cb;
}mqttMsg_t;

typedef struct{
	int		theSock;
	int		theTempMacPos;
} tempS;

typedef int (*functrsn)(parg *);

typedef struct cmdRecord{
    char 		comando[20];
    uint8_t 	source;
    functrsn 	code;
    uint32_t	count;
}cmdRecord;

typedef struct config {
    char 		medidor_id[MAXDEVS][MAXCHARS],meterConnName[MAXCHARS];
    time_t 		bornDate[MAXDEVS];
    u16 		beatsPerKw[MAXDEVS],bootcount,lastResetCode;
    u32 		bornKwh[MAXDEVS],centinel,traceflag;
    u8 			configured[MAXDEVS],active;
    uint32_t	reservedMacs[MAXSTA],msgTimeOut;
    uint16_t	reservedCnt;
    time_t		lastReboot,slotReserveDate[MAXSTA];
    char		lkey[MAXSTA][32];
    uint16_t	crypt,pause,displayMode,closedForRSVP,macTrace,numSlices;
} config_flash;

typedef struct mactemp{
	char			mac[6];
	uint32_t		ip;
	uint16_t		aid;
	int				myPos;
	TaskHandle_t	theRSVP;
	char			*theMsg;
} mactemp;

typedef struct framq{
	int 	whichMeter;
	bool 	addit;
}framMeterType;

typedef struct pcntt{
    int 		unit;  				// the PCNT unit that originated an interrupt
    uint32_t 	status; 		// information on the event type that caused the interrupt
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
