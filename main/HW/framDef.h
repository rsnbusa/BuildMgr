#include "defines.h"
#ifndef framDef_h
#define framDef_h
#define HOST
#define TXL 				SOC_SPI_MAXIMUM_BUFFER_SIZE-4
#define MAXDEVSUP           (5)
#define MAXDEVSS            (5)
#define MWORD				(2)
#define LLONG               (4)

#define FRAMDATE			0							//0 start of fram 4 bytes
#define METERVER			(FRAMDATE+LLONG)			//4 meter internal version
#define FREEFRAM			(METERVER+LLONG)			//8  2 bytes free
#define SCRATCH          	(FREEFRAM+MWORD)   			//10  10 bytres of free space for recovering date and others
#define SCRATCHEND          (SCRATCH+100)  				//110 100 bytes of scratch space at the beginning

#ifdef HOST
#define TARIFADIA           (SCRATCHEND) 				// 110 366 dias * 24 horas *2 this is the whole area required
#define FINTARIFA           (TARIFADIA+366*24*MWORD)	// 366 dias * 24 horas *2 this is the whole area required
#else
#define TARIFADIA			SCRATCHEND					// 110
#define FINTARIFA			SCRATCHEND					// 110
#endif

#define BEATSTART           (FINTARIFA)					// if HOST 17678 else 110
#define LIFEKWH             (BEATSTART+LLONG)
#define LIFEDATE            (LIFEKWH+LLONG)
#define MONTHSTART          (LIFEDATE+LLONG)
#define MONTHRAW			(MONTHSTART+MWORD*12)
#define DAYSTART            (MONTHRAW+MWORD*12)
#define DAYRAW				(DAYSTART+MWORD*12)
#define HOURSTART           (DAYRAW+MWORD*366)
#define HOURRAW				(HOURSTART+366*24)
#define DATAEND             (HOURRAW+366*24)
#define TOTALFRAM			(DATAEND*MAXDEVSS)


#endif /* framDef_h */
