#ifndef projTypes_h
#define projTypes_h

typedef struct macC{
	uint32_t 	macAdd;
	uint32_t 	trans;
	time_t		lastUpdate;
} macControl;

typedef struct meterType{
	 char serialNumber[20];
	 uint8_t meterid,state,pos,pin,pinB;
	 uint32_t timestamp,startConn,pulse;
	 uint32_t currentBeat,oldbeat,vanMqtt;
	 u16 elpin;
	 bool saveit;
	 u32 msNow, minamps,maxamps;
	 u16 curMonth,curMonthRaw,curDay,curDayRaw,beatSave,beatSaveRaw;
	 u8 curHour,cycleMonth,curHourRaw;
	 u32 curLife,curCycle,lastKwHDate;
	 u16 beatsPerkW,maxLoss;
	 u32 livingPulse,livingCount,startTimePulse;
} meterType;

typedef struct mqttMsg{
	u16		cmd;
	int		fd;
	char	*mensaje;
}cmdType;

typedef struct i2cType{
	gpio_num_t sdaport,sclport;
	i2c_port_t i2cport;
} i2ctype;

typedef struct loginTarif{
	time_t thedate;
	uint16_t theTariff;
} loginT;

typedef enum resetType {ONCE,TIMER,REPEAT,TIMEREPEAT} resetType;
typedef enum sendType {NOTSENT,SENT} sendType;
typedef enum overType {NOREP,REPLACE} overType;
typedef enum displayType {NODISPLAY,DISPLAYIT} displayType;
typedef enum displayModeType {DISPLAYPULSES,DISPLAYKWH,DISPLAYUSER,DISPLAYALL,DISPLAYALLK,DISPLAYAMPS,DISPLAYRSSI,DISPLAYNADA} displayModeType;

typedef struct pcomm{
    void *pMessage;
    uint8_t typeMsg;
    int pComm;
}parg;

typedef void (*functrsn)(parg *);

typedef struct cmdRecord{
    char comando[20];
    functrsn code;
}cmdRecord;




#endif
