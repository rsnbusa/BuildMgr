#ifndef projTypes_h
#define projTypes_h

typedef struct intPin{
	char		name[20];
	gpio_num_t	pin;
	u8			pos;
	u32 		startTimePulse,beatSave,beatSaveRaw,vanMqtt,currentKwH;
	u32			currentBeat,msNow,livingPulse,livingCount,timestamp,pulse,startConn;
}meterType;

typedef struct mqttMsg{
	u16		cmd;
	int		fd;
	char	mensaje[500];
}cmdType;

typedef struct i2cType{
	gpio_num_t sdaport,sclport;
	i2c_port_t i2cport;
} i2ctype;

typedef enum resetType {ONCE,TIMER,REPEAT,TIMEREPEAT} resetType;
typedef enum sendType {NOTSENT,SENT} sendType;
typedef enum overType {NOREP,REPLACE} overType;
typedef enum displayType {NODISPLAY,DISPLAYIT} displayType;
typedef enum displayModeType {DISPLAYPULSES,DISPLAYKWH,DISPLAYUSER,DISPLAYALL,DISPLAYALLK,DISPLAYAMPS,DISPLAYRSSI,DISPLAYNADA} displayModeType;

#endif
