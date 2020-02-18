/*
 * dataManager.cpp
 *
 *  Created on: Jan 26, 2020
 *      Author: rsn
 */

#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"
extern void pprintf(const char * format, ...);

void hourChange()
{
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		pprintf("%sHour change Old %d New %d\n",CMDDT,oldHorag,horag);
#endif

	for (int a=0;a<MAXDEVS;a++)
	{
#ifdef DEBUGX
		if(theConf.traceflag & (1<<CMDD))
			pprintf("%sHour change meter %d val %d\n",CMDDT,a,theMeters[a].curHour);
#endif
		if(framSem)
		{
			if(xSemaphoreTake(framSem, portMAX_DELAY))
			{
				fram.write_hour(a, oldYearDay,oldHorag, theMeters[a].curHour);//write old one before init new
				fram.write_hourraw(a, oldYearDay,oldHorag, theMeters[a].curHourRaw);//write old one before init new
				xSemaphoreGive(framSem);
			}

			theMeters[a].curHour=0;
			theMeters[a].curHourRaw=0;
			u16 oldt=tarifasDia[oldHorag];

			if (diag !=oldDiag)
			{
				int err=fram.read_tarif_day(yearDay,(u8*)&tarifasDia);
				if(err!=0)
					pprintf("Error reading tarifadia %d...using same\n",yearDay);
			}

			// calculate remaining Beats to convert to next Tariff IF different
			if(oldt!=tarifasDia[horag])
			{
				u16 oldBeats=theMeters[a].beatsPerkW*tarifasDia[oldHorag]/100;
				float oldTarRemain=(float)(theMeters[a].beatSave-oldBeats);
				float perc=(float)oldTarRemain/(float)oldBeats;
				theMeters[a].beatSave=(int)(perc*(float)(theMeters[a].beatsPerkW*tarifasDia[horag]/100));
			} //else keep counting in the same fashion
			oldHorag=horag;
		}
	}
}

void dayChange()
{
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		pprintf("%sDay change Old %d New %d\n",CMDDT,oldDiag,diag);
#endif

	if(framSem)
	{
		for (int a=0;a<MAXDEVS;a++)
		{
	#ifdef DEBUGX
			if(theConf.traceflag & (1<<CMDD))
				pprintf("%sDay change mes %d day %d oldday %d\n",CMDDT,oldMesg,diag,oldDiag);
	#endif
			if(xSemaphoreTake(framSem, portMAX_DELAY))
			{
				fram. write_day(a,oldYearDay, theMeters[a].curDay);
				theMeters[a].curDay=0;
				xSemaphoreGive(framSem);
			}
		}
	}

	oldDiag=diag;
	oldYearDay=yearDay;
}

void monthChange()
{
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		pprintf("%sMonth change Old %d New %d\n",CMDDT,oldMesg,mesg);
#endif

	if(framSem)
	{
		for (int a=0;a<MAXDEVS;a++)
		{
			if(xSemaphoreTake(framSem, portMAX_DELAY))
			{
				fram.write_month(a, oldMesg, theMeters[a].curMonth);
				xSemaphoreGive(framSem);
				theMeters[a].curMonth=0;
			}
		}
	}
	oldMesg=mesg;
	oldYearDay=yearDay;
}

void check_date_change()
{
	time_t now;
	struct tm timep;

	time(&now);
	localtime_r(&now,&timep);
	mesg=timep.tm_mon;   // Global Month
	diag=timep.tm_mday-1;    // Global Day
	yearg=timep.tm_year;     // Global Year
	horag=timep.tm_hour;     // Global Hour
	yearDay=timep.tm_yday;

#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		pprintf("%sHour change mes %d- %d day %d- %d hora %d- %d Min %d Sec %d dYear %d\n",CMDDT,mesg,oldMesg,diag,oldDiag,horag,oldHorag,
				timep.tm_min,timep.tm_sec,yearDay);
#endif

	if(horag==oldHorag && diag==oldDiag && mesg==oldMesg)
		return;

	if(horag!=oldHorag) // hour change up or down(day change)
		hourChange();

	if(diag!=oldDiag) // day change up or down(month change). Also hour MUST HAVE CHANGED before
		dayChange();

	if(mesg!=oldMesg) // month change up or down(year change). What to do with prev Year???? MONTH MUST HAVE CHANGED
		monthChange();
}
