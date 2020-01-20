#include "defines.h"
#ifndef framDef_h
#define framDef_h

#define HOST
#define TXL 				SOC_SPI_MAXIMUM_BUFFER_SIZE
#define MAXDEVSUP           (5)
#define MAXDEVSS            (5)
#define MWORD				(2)
#define LLONG               (4)

#define FRAMDATE			0
#define METERVER			(FRAMDATE+LLONG)
#define USEDMACS			(METERVER+LLONG)
#define MCYCLE				(USEDMACS+MWORD)
#define STATIONS          	(MCYCLE+12*LLONG)		//STORE for Reserved MACs (u32 MAC and U32 time +2 free)*10=100
#define STATIONSEND         (STATIONS+100)

#ifdef HOST
#define TARIFADIA           (STATIONSEND)
#define FINTARIFA           (TARIFADIA+366*24*MWORD)
#else
#define TARIFADIA			STATIONSEND
#define FINTARIFA			STATIONSEND
#endif

#define BEATSTART           (FINTARIFA)
#define LIFEKWH             (BEATSTART+LLONG)
#define LIFEDATE            (LIFEKWH+LLONG)
#define MONTHSTART          (LIFEDATE+LLONG)
#define MONTHRAW			(MONTHSTART+MWORD*12)
#define DAYSTART            (MONTHRAW+MWORD*12)
#define DAYRAW				(DAYSTART+MWORD*366)
#define HOURSTART           (DAYRAW+MWORD*366)
#define HOURRAW				(HOURSTART+366*24)
#define DATAEND             (HOURRAW+366*24)
#define TOTALFRAM			(DATAEND*MAXDEVSS)


#endif /* framDef_h */
