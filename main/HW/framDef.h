//
//  framDef.h
//  MeterIoT4096
//
//  Created by Robert on 2/17/17.
//  Copyright © 2017 Robert. All rights reserved.
//

#ifndef framDef_h
#define framDef_h

#define MAXDEVSUP           (5)
#define MAXDEVSS            (4)
#define MWORD				(2)
#define LLONG               (4)

// BeatCount (L)
// LifeKWHCount (L)
// LifeDate (l) last update
// 12 Months KWH (W)
// 366 Days KWH (W)
// 8764 (366*24) Hours KWH (byte)
// Min Amperes (W)
// Max Amperes (W)
// Date Min Amps (L)
// Date Max Amps(L)

#define SCRATCH             100  // 100 bytes of scratch space at the beginning
#define SCRATCHOFF          0       // absolute 0 start of FRAM

#define BEATSTART           (SCRATCH)
#define LIFEKWH             (BEATSTART+LLONG)
#define LIFEDATE            (LIFEKWH+LLONG)
#define MONTHSTART          (LIFEDATE+LLONG)
#define MONTHRAW			(MONTHSTART+MWORD*12)
#define DAYSTART            (MONTHRAW+MWORD*12)
#define DAYRAW				(DAYSTART+MWORD*12)
#define HOURSTART           (DAYRAW+MWORD*366)
#define HOURRAW				(HOURSTART+366*24)
#define MINASTART           (HOURRAW+366*24)
#define MAXASTART           (MINASTART+MWORD)
#define CYCLECOUNT          (MAXASTART+MWORD)
#define CYCLEDATE           (CYCLECOUNT+MWORD*12)
#define VALORPAGO           (CYCLEDATE+LLONG*12)
#define FECHACORTADO        (VALORPAGO+LLONG)
#define MAXPOWER            (FECHACORTADO+LLONG)
#define MSPOWER             (MAXPOWER+LLONG)
#define TIMEPOWER           (MSPOWER+LLONG)
#define DATAEND             (TIMEPOWER+LLONG)

#define TARIFADIA           (DATAEND*MAXDEVSS) // 1
#define FINTARIFA           (TARIFADIA+366*24*MWORD) // 366 dias * 24 horas *2 this is the whole area required

#endif /* framDef_h */
