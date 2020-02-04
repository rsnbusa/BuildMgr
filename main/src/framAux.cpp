/*
 * framMgr.cpp
 *
 *  Created on: Jan 26, 2020
 *      Author: rsn
 */


#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern uint32_t millis();

void write_to_fram(u8 meter,bool addit)
{
	time_t timeH;
    struct tm timeinfo;
    uint16_t theG;


	// FRAM Semaphore is taken by the Interrupt Manager. Safe to work.
    time(&timeH);
	localtime_r(&timeH, &timeinfo);
	mesg=timeinfo.tm_mon;
	diag=timeinfo.tm_mday-1;
	yearg=timeinfo.tm_year+1900;
	horag=timeinfo.tm_hour;

	if(addit)
	{
		theMeters[meter].curLife++;
		theMeters[meter].curMonth++;
		theMeters[meter].curDay++;
		theMeters[meter].curHour++;
		theMeters[meter].curCycle++;
		time((time_t*)&theMeters[meter].lastKwHDate); //last time we saved data

		fram.write_beat(meter,theMeters[meter].currentBeat);
		fram.write_lifekwh(meter,theMeters[meter].curLife);
		fram.write_month(meter,mesg,theMeters[meter].curMonth);
		fram.write_monthraw(meter,mesg,theMeters[meter].curMonthRaw);
		fram.write_day(meter,yearDay,theMeters[meter].curDay);
		fram.write_dayraw(meter,yearDay,theMeters[meter].curDayRaw);
		fram.write_hour(meter,yearDay,horag,theMeters[meter].curHour);
		fram.write_hourraw(meter,yearDay,horag,theMeters[meter].curHourRaw);
		fram.write_lifedate(meter,theMeters[meter].lastKwHDate);  //should be down after scratch record???
	}
		else
		{
			fram.read_guard((uint8_t*)&theG);
			if(theG!=theGuard)
				printf("Fram is lost\n");

			if(millis()-startGuard>600000)
			{
				theGuard=esp_random();
				fram.write_guard(theGuard);				// theguard is dynamic and will change every 10 minutes
				startGuard=millis();

			}

			fram.write_beat(meter,theMeters[meter].currentBeat);
			fram.writeMany(FRAMDATE,(uint8_t*)&timeH,sizeof(timeH));//last known date
		}
}

void load_from_fram(u8 meter)
{
	if(framSem)
		if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
		{
			fram.read_lifekwh(meter,(u8*)&theMeters[meter].curLife);
			fram.read_lifedate(meter,(u8*)&theMeters[meter].lastKwHDate);
			fram.read_month(meter, mesg, (u8*)&theMeters[meter].curMonth);
			fram.read_monthraw(meter, mesg, (u8*)&theMeters[meter].curMonthRaw);
			fram.read_day(meter, yearDay, (u8*)&theMeters[meter].curDay);
			fram.read_dayraw(meter, yearDay, (u8*)&theMeters[meter].curDayRaw);
			fram.read_hour(meter, yearDay, horag, (u8*)&theMeters[meter].curHour);
			fram.read_hourraw(meter, yearDay, horag, (u8*)&theMeters[meter].curHourRaw);
			fram.read_beat(meter,(u8*)&theMeters[meter].currentBeat);
			xSemaphoreGive(framSem);

			totalPulses+=theMeters[meter].currentBeat;

	#ifdef DEBUGX
			if(theConf.traceflag & (1<<FRAMD))
				printf("[FRAMD]Loaded Meter %d curLife %d beat %d\n",meter,theMeters[meter].curLife,theMeters[meter].currentBeat);
	#endif
		}
}

