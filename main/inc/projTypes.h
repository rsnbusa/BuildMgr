#ifndef projTypes_h
#define projTypes_h

typedef struct macC{
	int				theSock;
	TaskHandle_t 	theHandle;
	time_t			lastUpdate;
	u32				theIp;
	uint32_t 		macAdd;
	uint32_t 		trans;
	uint8_t 		*theBuffer;
} macControl;

typedef struct taskp{
	int	sock_p;
	int pos_p;
} task_param;

typedef struct meterType{
	 bool saveit,lowThresf;
	 char serialNumber[20];
	 u16 beatsPerkW,curMonth,curMonthRaw,curDay,curDayRaw;
	 u32 curLife,curCycle,lastKwHDate,msNow, minamps,maxamps,currentBeat,vanMqtt,ampTime,beatSave;
	 u8 curHour,cycleMonth,curHourRaw,pos,pin,pinB;
	 TaskHandle_t lowThresHandle;
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

typedef enum displayModeType {DISPLAYPULSES,DISPLAYKWH,DISPLAYUSER,DISPLAYALL,DISPLAYALLK,DISPLAYAMPS,DISPLAYRSSI,DISPLAYNADA} displayModeType;
typedef enum displayType {NODISPLAY,DISPLAYIT} displayType;
typedef enum overType {NOREP,REPLACE} overType;
typedef enum resetType {ONCE,TIMER,REPEAT,TIMEREPEAT} resetType;
typedef enum sendType {NOTSENT,SENT} sendType;
enum debugflags{BOOTD,WIFID,MQTTD,PUBSUBD,OTAD,CMDD,WEBD,GEND,MQTTT,HEAPD,INTD,FRAMD,MSGD};

typedef struct pcomm{
    int pComm;
    uint8_t typeMsg;
    void *pMessage;
}parg;

typedef void (*functrsn)(parg *);

typedef struct cmdRecord{
    char comando[20];
    functrsn code;
}cmdRecord;

typedef struct config {
    bool 	corteSent[MAXDEVS];
    char 	medidor_id[MAXDEVS][MAXCHARS],meterName[MAXCHARS];
    time_t 	lastUpload,lastTime,preLastTime,bornDate[MAXDEVS],lastBootDate;
    u16 	beatsPerKw[MAXDEVS],bootcount,bounce[MAXDEVS],diaDeCorte[MAXDEVS],lastResetCode;
    u16 	ssl,traceflag; // to make it mod 16 for AES encryption
    u32 	bornKwh[MAXDEVS],centinel;
    u8 		breakers[MAXDEVS],statusSend;
} config_flash;

typedef struct framq{
	int whichMeter;
	bool addit;
}framMeterType;

typedef struct pcntt{
    int unit;  // the PCNT unit that originated an interrupt
    uint32_t status; // information on the event type that caused the interrupt
} pcnt_evt_t;
#endif
