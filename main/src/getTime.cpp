/*
 * getTime.cpp
 *
 *  Created on: Jan 26, 2020
 *      Author: rsn
 */

#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

static void initialize_sntp(void)
{
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
    printf("Initializing SNTP\n");
#endif
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

void sntpget(void *pArgs)
{
    char strftime_buf[64];
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
  //      printf("Time is not set yet. Connecting to WiFi and getting time over NTP.");

	initialize_sntp();
	struct tm timeinfo;
	memset(&timeinfo,0,sizeof(timeinfo));
	int retry = 0;
	const int retry_count = 30;
	while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
	if(retry>retry_count)
	{
		printf("SNTP failed\n");
	    vTaskDelete(NULL);
	}
	time(&now);
	setenv("TZ", LOCALTIME, 1);
	tzset();
	localtime_r(&now, &timeinfo);

	oldMesg=mesg=timeinfo.tm_mon;   // Global Month
	oldDiag=diag=timeinfo.tm_mday-1;    // Global Day
	yearg=timeinfo.tm_year;     // Global Year
	oldHorag=horag=timeinfo.tm_hour;     // Global Hour
	oldYearDay=yearDay=timeinfo.tm_yday;

	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
#ifdef DEBUGX
	if(theConf.traceflag & (1<<CMDD))
		printf("%s[CMDD]The current date/time in %s is: %s day of Year %d\n",CMDDT, LOCALTIME,strftime_buf,timeinfo.tm_yday);
#endif

    }
    xEventGroupSetBits(wifi_event_group, SNTP_BIT);
	gpio_set_level((gpio_num_t)WIFILED, 1);		//indicate WIFI is active with IP and SNTP
    vTaskDelete(NULL);
}


