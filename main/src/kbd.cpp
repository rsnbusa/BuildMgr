#define GLOBAL
#include "defines.h"
#include "includes.h"
#include "globals.h"
#include "projTypes.h"
#include <bits/stdc++.h>

#define KBDT		"\e[36m[KBD]\e[0m"
#define MAXCMDSK	34

extern void pprintf(const char * format, ...);
extern void delay(uint32_t a);
extern void write_to_flash();
extern void write_to_fram(u8 meter,bool addit);
extern void connMgr(void *pArg);
extern void firmUpdate(void *pArg);
extern float DS_get_temp(DS18B20_Info * cual);

using namespace std;

typedef void (*functkbd)(char * ss);

typedef struct cmd_kbd_t{
    char comando[20];
    functkbd code;
    char help[150];
}cmdRecord_t;

cmdRecord_t cmdds[MAXCMDSK];

uint16_t date2days(uint16_t y, uint8_t m, uint8_t d) {
	uint8_t daysInMonth [12] ={ 31,28,31,30,31,30,31,31,30,31,30,31 };
	uint16_t days = d;
	for (uint8_t i = 0; i < m; i++)
		days += daysInMonth[ i];
	if (m == 1 && y % 4 == 0)
		++days;
	return days ;

}

cJSON *makeJson(char *ss)
{
	char theVal[15], theK[20];

	if(strlen(ss)==0)
		return NULL;

	cJSON *root=cJSON_CreateObject();

	if(!root)
		return NULL;

	//key := value setup

	char *pch = strtok (ss,":=");
	while (pch != NULL)
	{
		strcpy(theK,pch);
		pch = strtok (NULL, " ,=:");
		if(pch==NULL)
			break;
		strcpy(theVal,pch);

		bool isdig=true;
		for (int i=0; i<strlen(theVal); ++i)
		{
			if(!isdigit(theVal[i]))
			{
				 isdig=false;
				 break;
			}
		}

		if(!(strlen(theVal)==0 || strlen(theK)==0))
		{
			if(!isdig)
				cJSON_AddStringToObject(root,theK,theVal);
			else
				cJSON_AddNumberToObject(root,theK,atoi(theVal));
		}
	}
	  return root;
}

int get_string(uart_port_t uart_num,uint8_t cual,char *donde)
{
	uint8_t ch;
	int son=0,len;
	son=0;
	while(1)
	{
		len = uart_read_bytes(UART_NUM_0, (uint8_t*)&ch, 1,4/ portTICK_RATE_MS);
		if(len>0)
		{
			if(ch==cual)
			{
				*donde=0;
				return son;
			}

			else
			{
				*donde=(char)ch;
				donde++;
				son++;
			}
		}
		vTaskDelay(30/portTICK_PERIOD_MS);
	}
}


int cmdfromstring(char* key)
{
	for (int i=0; i <NKEYS; i++)
	{
		if(strstr(lookuptable[i],key)!=NULL)
			return i;
	}
	return -1;
}

int kbdCmd(char *key)
{

	for (int i=0; i <MAXCMDSK; i++)
	{
		if(strstr(cmdds[i].comando,key)!=NULL)
			return i;
	}
	return ESP_FAIL;
}

int askMeter(cJSON *params) // json should be METER
{
	int len=-1;
	char s1[20];

	if(params)
	{
		cJSON *jentry= cJSON_GetObjectItem(params,"METER");
		if(jentry)
			len=jentry->valueint;
		else
			goto alla;
	}
	else
	{
		alla:
		pprintf("Meter:");
		fflush(stdout);
		len=get_string(UART_NUM_0,10,s1);
		if(len<=0)
		{
			pprintf("\n");
			return ESP_FAIL;
		}
		len= atoi(s1);
	}
		if(len<MAXDEVS)
			return len;
		else
		{
			pprintf("%sMeter out of range 0-%d%s\n",MAGENTA,MAXDEVS-1,RESETC);
			return ESP_FAIL;
		}
}

int askMonth(cJSON *params)	//JSON should be MONTH
{
	int len=-1;
	char s1[20];

	if(params)
	{
		cJSON *jentry= cJSON_GetObjectItem(params,"MONTH");
		if(jentry)
			len=jentry->valueint;
		else
			goto alla;
	}
	else
	{
		alla:
		pprintf("Month(0-11):");
		fflush(stdout);
		len=get_string(UART_NUM_0,10,s1);
		if(len<=0)
		{
			pprintf("\n");
			return ESP_FAIL;
		}
		len= atoi(s1);
	}
	if(len<12)
		return len;
	else
	{
		pprintf("%sMonth out of range 0-11%s\n",MAGENTA,RESETC);
		return ESP_FAIL;
	}
}

int askHour(cJSON *params)	//json should be HOUR
{
	int len=-1;
	char s1[20];

	if(params)
		{
			cJSON *jentry= cJSON_GetObjectItem(params,"HOUR");
			if(jentry)
				len=jentry->valueint;
			else
				goto alla;

		}
	else
	{
		alla:
		pprintf("Hour(0-23):");
		fflush(stdout);
		len=get_string(UART_NUM_0,10,s1);
		if(len<=0)
		{
			pprintf("\n");
			return ESP_FAIL;
		}
		len= atoi(s1);
	}
	if(len<24)
		return len;
	else
	{
		pprintf("%sHour out of range 0-23%s\n",MAGENTA,RESETC);
		return ESP_FAIL;
	}
}

int askDay(cJSON *params,int month)	// JSON should be DAY
{
	int len=-1;
	char s1[20];

	if(params)
		{
			cJSON *jentry= cJSON_GetObjectItem(params,"DAY");
			if(jentry)
				len=jentry->valueint;
			else
				goto alla;
		}
	else
	{
		alla:
		pprintf("Day(0-%d):",daysInMonth[month]-1);
		fflush(stdout);
		len=get_string(UART_NUM_0,10,s1);
		if(len<=0)
		{
			pprintf("\n");
			return ESP_FAIL;
		}
		len= atoi(s1);
	}
		if(len<daysInMonth[month])
			return len;
		else
		{
			pprintf("%sDay out of range 0-%d%s\n",MAGENTA,daysInMonth[month]-1,RESETC);
			return ESP_FAIL;
		}
}

int askValue(const char *title,cJSON *params)	//JSON should be VAL
{
	int len;
	char s1[20];

	if(params)
		{
			cJSON *jentry= cJSON_GetObjectItem(params,"VAL");
			if(jentry)
				return(jentry->valueint);
			else
				goto alla;
		}
	else
	{
		alla:
		pprintf("%s:",title);
		fflush(stdout);
		len=get_string(UART_NUM_0,10,s1);
		if(len<=0)
		{
			pprintf("\n");
			return ESP_FAIL;
		}

		return atoi(s1);
	}
	return ESP_FAIL;
}

void cmd_confStatus(char * ss)
{
	struct tm timeinfo;
	char strftime_buf[64];
	time_t now;
	bool shortans=false;
	cJSON *params=NULL;

	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"SHORT");
		if(jmeter)
			shortans=true;

		cJSON_Delete(params);
	}

	time(&now);
	pprintf("%s====================\nConfiguration Status Heap %d %s",KBDT,esp_get_free_heap_size(),ctime(&now));

	pprintf("ConnMgr %s%s%s Whitelisted %s%d%s",CYAN,theConf.meterConnName,RESETC,GREEN,theConf.reservedCnt,RESETC);
	pprintf("%sConfiguration RunStatus[%s] Trace %x %sBootCount %d LReset %x @%s",YELLOW,theConf.active?"Run":"Setup",theConf.traceflag,GREEN,theConf.bootcount,theConf.lastResetCode,ctime(&theConf.lastReboot));
	delay(100);
	if(!shortans)
	{
		for (int a=0;a<MAXDEVS;a++)
		{
			localtime_r(&theConf.bornDate[a], &timeinfo);
			strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
			pprintf("%sMeter[%d] Serial %s BPW %d Born %s BornkWh %d %s\n",(a % 2)?CYAN:GREEN,a,theConf.medidor_id[a],theConf.beatsPerKw[a],
					strftime_buf,theConf.bornKwh[a],theConf.configured[a]?"Active":"Inactive");
			delay(100);
		}
	}

	pprintf("===============\n");
	pprintf("  MQTT Section\n");
	pprintf("===============%s\n",RESETC);
	pprintf("Command Queue %s%s%s\n",GREEN,cmdQueue,RESETC);
	pprintf("Host Queue %s%s%s\n",CYAN,controlQueue,RESETC);

	if(!shortans)
	{
		pprintf("%s===============\n",YELLOW);
		pprintf("Control Tasks\n");
		pprintf("===============%s\n",RESETC);

		pprintf("CmdMgr %s @%s",cmdHandle?GREEN "Alive" RESETC:RED "Dead" RESETC,ctime(&mgrTime[CMDMGRT]));
		pprintf("FramMgr %s @%s",framHandle?GREEN "Alive" RESETC:RED "Dead" RESETC,ctime(&mgrTime[FRAMMGRT]));
		delay(100);
		pprintf("PinMgr %s @%s",pinHandle?GREEN "Alive" RESETC:RED "Dead" RESETC,ctime(&mgrTime[PINMGRT]));
		pprintf("WatchMgr %s @%s",watchHandle?GREEN "Alive" RESETC:RED "Dead" RESETC,ctime(&mgrTime[WATCHMGRT]));
		delay(100);
		pprintf("DisplayMgr %s @%s",displayHandle?GREEN "Alive" RESETC:RED "Dead" RESETC,ctime(&mgrTime[DISPLAYMGRT]));
		pprintf("SubMgr %s @%s",submHandle?GREEN "Alive" RESETC:RED "Dead" RESETC,ctime(&mgrTime[SUBMGRT]));
	}
	pprintf("====================%s\n",KBDT);

}

void cmd_meterStatus(char * ss)
{
	uint32_t valr;
	cJSON *params=NULL;
	int meter=-1;

	params=makeJson(ss);

	pprintf("%s============\nMeter status\n",KBDT);
	meter=askMeter(params);
	if(params)
		cJSON_Delete(params);
	if(meter<0||meter>MAXDEVS-1)
	{
		pprintf("Meter %d out of range\n",meter);
		return;
	}

	pprintf("============%s\n\n",YELLOW);

	if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
	{
	//	pprintf("Meter Compare test Year %d Month %d Day %d Hour %d YDAY %d\n",yearg,mesg,diag,horag,yearDay);
		valr=0;
		fram.read_beat(meter,(uint8_t*)&valr);
		pprintf("Beat[%d]=%d %d\n",meter,valr,theMeters[meter].currentBeat);
		valr=0;
		fram.read_lifekwh(meter,(uint8_t*)&valr);
		pprintf("LastKwh[%d]=%d %d\n",meter,valr,theMeters[meter].curLife);
		valr=0;
		fram.read_lifedate(meter,(uint8_t*)&valr);
		pprintf("LifeDate[%d]=%d %d %s",meter,valr,theMeters[meter].lastKwHDate,ctime((time_t*)&theMeters[meter].lastKwHDate));
		valr=0;
		fram.read_month(meter,mesg,(uint8_t*)&valr);
		pprintf("Month[%d]Mes[%d]=%d %d\n",meter,mesg,valr,theMeters[meter].curMonth);
		valr=0;
		fram.read_monthraw(meter,mesg,(uint8_t*)&valr);
		pprintf("MonthRaw[%d]Mes[%d]=%d %d\n",meter,mesg,valr,theMeters[meter].curMonthRaw);
		valr=0;
		fram.read_day(meter,yearDay,(uint8_t*)&valr);
		pprintf("Day[%d]Mes[%d]Dia[%d]=%d %d\n",meter,mesg,diag,valr,theMeters[meter].curDay);
		valr=0;
		fram.read_dayraw(meter,yearDay,(uint8_t*)&valr);
		pprintf("DayRaw[%d]Mes[%d]Dia[%d]=%d %d\n",meter,mesg,diag,valr,theMeters[meter].curDayRaw);
		valr=0;
		fram.read_hour(meter,yearDay,horag,(uint8_t*)&valr);
		pprintf("Hour[%d]Mes[%d]Dia[%d]Hora[%d]=%d %d\n",meter,mesg,diag,horag,valr,theMeters[meter].curHour);
		valr=0;
		fram.read_hourraw(meter,yearDay,horag,(uint8_t*)&valr);
		pprintf("HourRaw[%d]Mes[%d]Dia[%d]Hora[%d]=%d %d%s\n",meter,mesg,diag,horag,valr,theMeters[meter].curHourRaw,RESETC);

		xSemaphoreGive(framSem);
		pprintf("%s============\n",KBDT);

	}
}

void cmd_meterTest(char * ss)
{
	cJSON *params=NULL;
	int val=0,meter=0;
	uint32_t valr;

	params=makeJson(ss);

	meter=askMeter(params);
	if(meter<0||meter>MAXDEVS-1)
	{
		pprintf("Meter %d out of range\n",meter);
		if(params)
			cJSON_Delete(params);
		return;
	}

	val=askValue((char*)"Value",params);

	if(params)
		cJSON_Delete(params);

	pprintf("%s================\nTest Write Meter\n",KBDT);
	pprintf("================%s\n\n",RESETC);

	if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
	{
		pprintf("Fram Write Meter %d Year %d Month %d Day %d Hour %d Value %d YDAY %d\n",meter,yearg,mesg,diag,horag,val,yearDay);

		fram.write_beat(meter,val);
		fram.write_lifekwh(meter,val);
		fram.write_lifedate(meter,val);
		fram.write_month(meter,mesg,val);
		fram.write_monthraw(meter,mesg,val);
		fram.write_day(meter,yearDay,val);
		fram.write_dayraw(meter,yearDay,val);
		fram.write_hour(meter,yearDay,horag,val);
		fram.write_hourraw(meter,yearDay,horag,val);

		valr=0;
		pprintf("Reading now\n");
		fram.read_beat(meter,(uint8_t*)&valr);
		pprintf("Beat[%d]=%d\n",meter,valr);
		fram.read_lifekwh(meter,(uint8_t*)&valr);
		pprintf("LifeKwh[%d]=%d\n",meter,valr);
		fram.read_lifedate(meter,(uint8_t*)&valr);
		pprintf("LifeDate[%d]=%d\n",meter,valr);
		fram.read_month(meter,mesg,(uint8_t*)&valr);
		pprintf("Month[%d]=%d\n",meter,valr);
		fram.read_monthraw(meter,mesg,(uint8_t*)&valr);
		pprintf("MonthRaw[%d]=%d\n",meter,valr);
		fram.read_day(meter,yearDay,(uint8_t*)&valr);
		pprintf("Day[%d]=%d\n",meter,valr);
		fram.read_dayraw(meter,yearDay,(uint8_t*)&valr);
		pprintf("DayRaw[%d]=%d\n",meter,valr);
		fram.read_hour(meter,yearDay,horag,(uint8_t*)&valr);
		pprintf("Hour[%d]=%d\n",meter,valr);
		fram.read_hourraw(meter,yearDay,horag,(uint8_t*)&valr);
		pprintf("HourRaw[%d]=%d\n",meter,valr);

		xSemaphoreGive(framSem);
	}
}

void cmd_webReset(char * ss)
{
	theConf.active=!theConf.active;
	pprintf("%sWeb %s\n",KBDT,theConf.active?"RunConf":"SetupConf");
	pprintf("===========%s\n",KBDT);
//	if(theConf.active)
//		memset(theConf.configured,3,sizeof(theConf.configured));
//	else
//		memset(theConf.configured,0,sizeof(theConf.configured));
	write_to_flash();
}

void cmd_meterCount(char * ss)
{
	time_t timeH;

	int tots=0;
	pprintf("%s==============\nMeter Counters%s\n",KBDT,RESETC);

	for (int a=0;a<MAXDEVS;a++)
	{
		tots+=theMeters[a].currentBeat;
		pprintf("%sMeter[%d]=%s Beats %d kWh %d BPK %d\n",(a % 2)?GREEN:CYAN,a,RESETC,theMeters[a].currentBeat,theMeters[a].curLife,
				theMeters[a].beatsPerkW);
	}
	pprintf("%sTotal Pulses rx=%s %d (%s)\n",RED,RESETC,totalPulses,tots==totalPulses?"Ok":"No");
	fram.readMany(FRAMDATE,(uint8_t*)&timeH,sizeof(timeH));//last known date
	pprintf("Last Date %s",ctime(&timeH));
	pprintf("==============%s\n\n",KBDT);

}

void cmd_dumpCore(char * ss)
{

	u8 *p;
	pprintf("%sDumping core ins 3 secs%s\n",RED,RESETC);
	delay(3000);
	p=0;
	*p=0;
}

void cmd_formatFram(char * ss)
{
	cJSON *params=NULL;
	int val=0,val1=0;

	params=makeJson(ss);

	if(params)
	{
		cJSON *jval= cJSON_GetObjectItem(params,"VAL");
		if(jval)
			val=jval->valueint;
		else
			goto alla;
		cJSON_Delete(params);
		params=NULL;

	}
	else
	{
		alla:
		if(params)
			cJSON_Delete(params);

		pprintf("%s===========\nFormat FRAM\n",KBDT);

		val=askValue((char*)"Init value",params);
		pprintf("===========%s\n\n",KBDT);
		val1=askValue((char*)"Erase Conf?",params);

	}

	if(val<0)
		return;

	fram.format(val,NULL,100,true);
	pprintf("Format done...rebooting\n");
	uint32_t cent=0x12345678;

	fram.write_centinel(cent);

	if(val1)
	{
		bzero(&theConf.medidor_id,sizeof(theConf.medidor_id));
		bzero(&theConf.beatsPerKw,sizeof(theConf.beatsPerKw));
		bzero(&theConf.configured,sizeof(theConf.configured));
		bzero(&theConf.bornKwh, sizeof(theConf.bornKwh));
		bzero(&theConf.bornDate ,sizeof(theConf.bornDate));
	}
	write_to_flash();

	esp_restart();
}


void cmd_readFram(char * ss)
{
	cJSON *params=NULL;
	int framAddress=0,fueron=0;

	params=makeJson(ss);
	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"ADDR");
		if(jmeter)
			framAddress=jmeter->valueint;
		else
			goto alla;
		cJSON *jval= cJSON_GetObjectItem(params,"COUNT");
		if(jval)
			fueron=jval->valueint;
		else
			goto alla;
		cJSON_Delete(params);
		params=NULL;
	}
	else
	{
		alla:
		if(params)
			cJSON_Delete(params);

		pprintf("%s=========\nRead FRAM\n",KBDT);
		framAddress=askValue((char*)"Address",params);
		fueron=askValue((char*)"Count",params);
		pprintf("=========%s\n\n",KBDT);
	}
	if(fueron<0 || framAddress<0)
		return;

	fram.readMany(framAddress,(uint8_t*)tempb,fueron);
	for (int a=0;a<fueron;a++)
		pprintf("%02x-",tempb[a]);
	pprintf("\n");

}

void cmd_writeFram(char * ss)
{
	cJSON *params=NULL;
	int fueron=0,framAddress=0;
	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"ADDR");
		if(jmeter)
			framAddress=jmeter->valueint;
		else
			goto alla;
		cJSON *jval= cJSON_GetObjectItem(params,"VAL");
		if(jval)
			fueron=jval->valueint;
		else
			goto alla;
		cJSON_Delete(params);
		params=NULL;
	}
	else
	{
		alla:
		if(params)
			cJSON_Delete(params);
		pprintf("%s==========\nWrite FRAM\n",KBDT);
		framAddress=askValue((char*)"Address",params);
		fueron=askValue((char*)"Value",params);
		pprintf("==========%s\n\n",KBDT);
	}
	if(fueron<0 || framAddress<0)
		return;

	fram.write8(framAddress,fueron);

}

void cmd_logLevel(char * ss)
{
	cJSON *params=NULL;
	char s1[10];

	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"LEVEL");
		if(jmeter)
			s1[0]=jmeter->valuestring[0];
		else
			goto alla;
		cJSON_Delete(params);
		params=NULL;
	}
	else
	{
		alla:
		if(params)
			cJSON_Delete(params);

		pprintf("%sLOG level:(N)one (I)nfo (E)rror (V)erbose (W)arning:",KBDT);
		fflush(stdout);
		int len=get_string(UART_NUM_0,10,s1);
		if(len<=0)
		{
			pprintf("\n");
			return;
		}
	}
		switch (s1[0])
		{
			case 'n':
			case 'N':
					esp_log_level_set("*", ESP_LOG_NONE);
					break;
			case 'i':
			case 'I':
					esp_log_level_set("*", ESP_LOG_INFO);
					break;
			case 'e':
			case 'E':
					esp_log_level_set("*", ESP_LOG_ERROR);
					break;
			case 'v':
			case 'V':
					esp_log_level_set("*", ESP_LOG_VERBOSE);
					break;
			case 'w':
			case 'W':
					esp_log_level_set("*", ESP_LOG_WARN);
					break;
			default:
				pprintf("No Level defined\n");
				break;
		}

}

void cmd_framDays(char * ss)
{
	uint16_t valor;
	uint32_t tots=0;
	cJSON *params=NULL;
	int meter=0,month=0;

	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"METER");
		if(jmeter)
			meter=jmeter->valueint;
		else
			goto alla;
		cJSON *jval= cJSON_GetObjectItem(params,"MONTH");
		if(jval)
			month=jval->valueint;
		else
			goto alla;
		cJSON_Delete(params);
		params=NULL;
	}
	else
	{
		alla:
		if(params)
			cJSON_Delete(params);

		pprintf("%s==================\nFram Days in Month\n",KBDT);
		meter=askMeter(params);
		if(meter<0)
			return;
		month=askMonth(params);
		if(month<0)
			return;
	}
	if(xSemaphoreTake(framSem, portMAX_DELAY))
	{
		int startday=date2days(yearg,month,0);//this year, this month, first day

		for (int a=0;a<daysInMonth[month];a++)
		{
			fram.read_day(meter,startday+a, (u8*)&valor);
			if(valor>0 || (startday==yearDay && theMeters[meter].curDay>0 ))
			{
				if(startday==yearDay && theMeters[meter].curDay>0 )
					pprintf("M[%d]D[%d]=%d RAM=%d ",month,a,valor,theMeters[meter].curDay);
				else
					pprintf("M[%d]D[%d]=%d ",month,a,valor);
			}
			tots+=valor;
		}
		xSemaphoreGive(framSem);
		pprintf(" Total=%d\n",tots);
		pprintf("==================%s\n",KBDT);
	}

}

void cmd_framHours(char * ss)
{
	uint8_t valor;
	uint32_t tots=0;
	cJSON *params=NULL;
	int meter=0,month=0,dia=0;

	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"METER");
		if(jmeter)
			meter=jmeter->valueint;
		else
			goto alla;
		cJSON *jval= cJSON_GetObjectItem(params,"MONTH");
		if(jval)
			month=jval->valueint;
		else
			goto alla;
		cJSON *jday= cJSON_GetObjectItem(params,"DAY");
		if(jday)
			dia=jval->valueint;
		else
			goto alla;
		cJSON_Delete(params);
		params=NULL;
	}
	else
	{
		alla:
		if(params)
			cJSON_Delete(params);

		pprintf("%s=================\nFram Hours in Day\n",KBDT);

		meter=askMeter(params);
		if(meter<0)
			return;
		month=askMonth(params);
		if(month<0)
			return;
		dia=askDay(params,month);
		if(dia<0)
			return;
	}

	if(xSemaphoreTake(framSem, portMAX_DELAY))
	{
		int startday=date2days(yearg,month,dia);//this year, this month, this day

		for(int a=0;a<24;a++)
		{
			fram.read_hour(meter, startday, a, (u8*)&valor);
				if(valor>0 || (startday==yearDay && horag==a && theMeters[meter].curHour>0))
				{
					if(startday==yearDay && horag==a && theMeters[meter].curHour>0)
						pprintf("%sM[%d]D[%d]H[%d]=%d RAM=%d ",(a % 2)?CYAN:GREEN,month,dia,a,valor,theMeters[meter].curHour);
					else
						pprintf("%sM[%d]D[%d]H[%d]=%d ",(a % 2)?CYAN:GREEN,month,dia,a,valor);
				}
				tots+=valor;
		}
		xSemaphoreGive(framSem);
		pprintf(" Total=%d\n",tots);
		pprintf("=================%s\n",KBDT);
	}
}

void cmd_framMonthsHours(char * ss)
{
	uint8_t valor;
	uint32_t tots=0;
	bool was;
	cJSON *params=NULL;
	int meter=0,month=0;

	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"METER");
		if(jmeter)
			meter=jmeter->valueint;
		else
			goto alla;
		cJSON *jval= cJSON_GetObjectItem(params,"MONTH");
		if(jval)
			month=jval->valueint;
		else
			goto alla;

		cJSON_Delete(params);
		params=NULL;
	}
	else
	{
		alla:
		if(params)
			cJSON_Delete(params);

		pprintf("%s=================\nFram Hours in Month\n",KBDT);
		meter=askMeter(params);
		if(meter<0)
			return;
		month=askMonth(params);
		if(month<0)
			return;
	}

	if(xSemaphoreTake(framSem, portMAX_DELAY))
	{
		for(int b=0;b<daysInMonth[month];b++)
		{
			int startday=date2days(yearg,month,b);//this year, this month, this day
			was=false;
			for(int a=0;a<24;a++)
			{
				fram.read_hour(meter, startday, a, (u8*)&valor);
					if(valor>0 || (startday==yearDay && a==horag && theMeters[meter].curHour>0))
					{
						was=true;
						if(startday==yearDay && a==horag && theMeters[meter].curHour>0)
							pprintf("%sM[%d]D[%d]H[%d]=%d RAM=%d ",(a % 2)?CYAN:GREEN,month,b,a,valor,theMeters[meter].curHour);
						else
							pprintf("%sM[%d]D[%d]H[%d]=%d ",(a % 2)?CYAN:GREEN,month,b,a,valor);
					}
					tots+=valor;
			}
			if(was)
				pprintf(" Total=%d\n",tots);
		}
			xSemaphoreGive(framSem);
			pprintf("=================%s\n",KBDT);

	}
}

void cmd_framHourSearch(char * ss)
{
	uint8_t valor;
	cJSON *params=NULL;
	int meter=0,month=0,dia=0,hora=0;

	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"METER");
		if(jmeter)
			meter=jmeter->valueint;
		else
			goto alla;
		cJSON *jval= cJSON_GetObjectItem(params,"MONTH");
		if(jval)
			month=jval->valueint;
		else
			goto alla;
		jval= cJSON_GetObjectItem(params,"DAY");
		if(jval)
			dia=jval->valueint;
		else
			goto alla;
		jval= cJSON_GetObjectItem(params,"HOUR");
		if(jval)
			hora=jval->valueint;
		else
			goto alla;

		cJSON_Delete(params);
		params=NULL;
	}
	else
	{
		alla:
		if(params)
			cJSON_Delete(params);

		pprintf("%s================\nFram Hour Search\n",KBDT);

		meter=askMeter(params);
		if(meter<0)
			return;
		month=askMonth(params);
		if(month<0)
			return;
		dia=askDay(params,month);
		if(dia<0)
			return;
		hora=askHour(params);
		if(hora<0)
			return;
	}

	if(xSemaphoreTake(framSem, portMAX_DELAY))
	{
		int startday=date2days(yearg,month,dia);//this year, this month, first day

		fram.read_hour(meter, startday, hora,(u8*)&valor);
		xSemaphoreGive(framSem);
		if(valor>0 || (horag==horag && startday==yearDay && theMeters[meter].curHour>0))
			{
			if(horag==horag && startday==yearDay && theMeters[meter].curHour>0)
				pprintf("Date %d/%d/%d %d:00:00=%d RAM=%d\n",yearg,month,dia,hora,valor,theMeters[meter].curHour);
			else
				pprintf("Date %d/%d/%d %d:00:00=%d\n",yearg,month,dia,hora,valor);
			}
	}
	pprintf("================%s\n",KBDT);

}


void cmd_framDaySearch(char * ss)
{
	uint16_t valor;
	cJSON *params=NULL;
	int meter=0,month=0,dia=0;

	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"METER");
		if(jmeter)
			meter=jmeter->valueint;
		else
			goto alla;
		cJSON *jval= cJSON_GetObjectItem(params,"MONTH");
		if(jval)
			month=jval->valueint;
		else
			goto alla;
		jval= cJSON_GetObjectItem(params,"DAY");
		if(jval)
			dia=jval->valueint;
		else
			goto alla;
		cJSON_Delete(params);
		params=NULL;
	}
	else
	{
		alla:
		if(params)
			cJSON_Delete(params);

		pprintf("%s===============\nFram Day Search\n",KBDT);

		meter=askMeter(params);
		if(meter<0)
			return;
		month=askMonth(params);
		if(month<0)
			return;
		dia=askDay(params,month);
		if(dia<0)
			return;
	}

	if(xSemaphoreTake(framSem, portMAX_DELAY))
	{
		int startday=date2days(yearg,month,dia);//this year, this month, first day

		fram.read_day(meter, startday,(u8*)&valor);
		xSemaphoreGive(framSem);
		if(valor>0 || (theMeters[meter].curDay>0 && startday==yearDay))
		{
		if(theMeters[meter].curDay>0 && startday==yearDay)
			pprintf("Date %d/%d/%d =%d RAM=%d\n",yearg,month,dia,valor,theMeters[meter].curDay);
		else
			pprintf("Date %d/%d/%d =%d\n",yearg,month,dia,valor);

		}
	}
	pprintf("===============%s\n",KBDT);

}

void cmd_framMonthSearch(char * ss)
{
	uint16_t valor;
	cJSON *params=NULL;
	int meter=0,month=0;

	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"METER");
		if(jmeter)
			meter=jmeter->valueint;
		else
			goto alla;
		cJSON *jval= cJSON_GetObjectItem(params,"MONTH");
		if(jval)
			month=jval->valueint;
		else
			goto alla;
		cJSON_Delete(params);
		params=NULL;
	}
	else
	{
		alla:
		if(params)
			cJSON_Delete(params);

		pprintf("%s=================\nFram Month Search\n",KBDT);

		meter=askMeter(params);
		if(meter<0)
			return;
		month=askMonth(params);
		if(month<0)
			return;
	}
	if(xSemaphoreTake(framSem, portMAX_DELAY))
	{
		fram.read_month(meter,month,(u8*)&valor);
		xSemaphoreGive(framSem);
		if(valor>0 || (theMeters[meter].curMonth>0 && mesg==month))
		{
			if(theMeters[meter].curMonth>0 && mesg==month)
				pprintf("Month[%d] =%d RAM=%d\n",month,valor,theMeters[meter].curMonth);
			else
				pprintf("Month[%d] =%d\n",month,valor);
		}
	}
	pprintf("\n=================%s\n",KBDT);
}

void cmd_framMonths(char * ss)
{
	uint16_t valor;
	uint32_t tots=0;
	cJSON *params=NULL;
	int meter=0;

	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"METER");
		if(jmeter)
			meter=jmeter->valueint;
		else
			goto alla;
		cJSON_Delete(params);
		params=NULL;
	}
	else
	{
		alla:
		if(params)
			cJSON_Delete(params);

		pprintf("%s=================\nFram Month All\n",KBDT);
		meter=askMeter(params);
	}

	if(meter<0)
		return;

	if(xSemaphoreTake(framSem, portMAX_DELAY))
	{
		pprintf("Meter[%d]",meter);
		for(int a=0;a<12;a++)
		{
			fram.read_month(meter,a,(u8*)&valor);
			if(valor>0)
			{
			if(theMeters[meter].curMonth>0 && a==mesg)
				pprintf("%s[%d]=%d RAM=%d ",(a % 2)?CYAN:GREEN,a,valor,theMeters[meter].curMonth);
			else
				pprintf("%s[%d]=%d ",(a % 2)?CYAN:GREEN,a,valor);
			}
			tots+=valor;
		}
		pprintf(" Total=%d\n",tots);
		pprintf("=================%s\n",KBDT);

		xSemaphoreGive(framSem);
	}
	pprintf("\n");
}

void cmd_flushFram(char * ss)
{
	pprintf("%s=============\nFlushing Fram\n",KBDT);
	for(int a=0;a<MAXDEVS;a++)
		write_to_fram(a,false);
	pprintf("%d meters flushed\n",MAXDEVS);
	pprintf("=============%s\n",KBDT);
}

void cmd_msgCount(char * ss)
{
	pprintf("%s=============\nMessage Count\n",KBDT);
	for (int a=0;a<MAXDEVS;a++)
		//pprintf("TotalMsg[%d] sent msgs %d\n",a,totalMsg[a]);
	pprintf("TotalMsg[%d] sent msgs %d\n",a,1);
	pprintf("Controller Total %d\n",sentTotal);
	pprintf("=============%s\n",KBDT);

}

void cmd_traceFlags(char * sss)
{
	char s1[60],s2[20];

	pprintf("%sTrace Flags:%s",KBDT,CYAN);

	for (int a=0;a<NKEYS/2;a++)
	if (theConf.traceflag & (1<<a))
		pprintf("%s ",lookuptable[a]);
	pprintf("\n");

	if(strlen(sss)==0)
	{
		pprintf("%s\nEnter TRACE FLAG:%s",RED,RESETC);
		fflush(stdout);
		memset(s1,0,sizeof(s1));
		get_string(UART_NUM_0,10,s1);
		memset(s2,0,sizeof(s2));
		for(int a=0;a<strlen(s1);a++)
			s2[a]=toupper(s1[a]);

		if(strlen(s2)<=1)
			return;
	}
	else
		strcpy(s2,sss);

	  char *ch;
	  ch = strtok(s2, " ");
	  while (ch != NULL)
	  {

		if(strcmp(ch,"NONE")==0)
		{
			theConf.traceflag=0;
			break;
		}

		if(strcmp(ch,"ALL")==0)
		{
			theConf.traceflag=0xFFFF;
			break;
		}

		int cualf=cmdfromstring(ch);
		if(cualf<0)
		{
			pprintf("Invalid Debug Option\n");
			return;
		}

		if(cualf<NKEYS/2 )
		{
			pprintf("%s%s+ ",GREEN,lookuptable[cualf]);
			theConf.traceflag |= 1<<cualf;
			break;
		}
		else
		{
			cualf=cualf-NKEYS/2;
			pprintf("%s%s- ",RED,lookuptable[cualf]);
			theConf.traceflag ^= (1<<cualf);
			break;
		}
		  ch = strtok(NULL, " ");

	  }
	  write_to_flash();
	  pprintf("%s\n",KBDT);
}


bool compareCmd(cmdRecord_t r1, cmdRecord_t r2)
{
	int a=strcmp(r1.comando,r2.comando);
	if (a<0)
		return true;
	else
		return false;
}

void cmd_showHelp(char * ss)
{
	cJSON *params=NULL;
	bool longf=false;
	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"LONG");
		if(jmeter)
			longf=true;
		cJSON_Delete(params);
		params=NULL;
	}
    int n = sizeof(cmdds)/sizeof(cmdds[0]);

    std::sort(cmdds, cmdds+n, compareCmd);

	pprintf("%s======CMDS======\n",RED);
	for (int a=0;a<MAXCMDSK;a++)
	{
		if(a % 2)
		{
			pprintf("%s%s ",CYAN,cmdds[a].comando);
			if (longf)
				pprintf("(%s)\n",cmdds[a].help);
		}
		else
		{
			pprintf("%s%s ",GREEN,cmdds[a].comando);
			if (longf)
				pprintf("(%s)\n",cmdds[a].help);
		}
	}
	pprintf("\n======CMDS======%s\n",RESETC);
}

static void cmd_printControllers(char * ss)
{
	char str2[INET_ADDRSTRLEN];
	int antes,son;
	cJSON *params=NULL;
	bool shortans=true,trk;

	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"LONG");
		if(jmeter)
			shortans=false;
		cJSON_Delete(params);
		params=NULL;
	}

	pprintf("%sListing %d Controllers\n",KBDT,theConf.reservedCnt);

	if(!shortans)
	{
		pprintf("%s============%s\n",MAGENTA,RESETC);
		pprintf("%sLocal Meters%s\n",MAGENTA,RESETC);
		pprintf("%s============%s\n",MAGENTA,RESETC);
		for (int a=0;a<MAXDEVS;a++)
			pprintf("%sMeter[%d]=%s KwH %6d Beats %9d\n",a%2?CYAN:GREEN,a,theMeters[a].serialNumber,theMeters[a].curLife,theMeters[a].currentBeat);
		pprintf("\n");
	}
	pprintf("===============\n");
	pprintf("Configured MtMs\n");
	pprintf("===============\n");

	for(int a=0;a<theConf.reservedCnt;a++)
	{
		trk=(theConf.macTrace & 1<<a);
		time_t t = time(NULL);
		struct tm * p = localtime(&t);
		strftime(tempb, 100, "%D %T ", p);

		inet_ntop( AF_INET,(in_addr*)&losMacs[a].theIp, str2, INET_ADDRSTRLEN );
		pprintf("%sSlot[%d]%s=%06x%s %sIP=%s %sState(#%d)%s%s%s(%s) GTask=%s%s %sHW=%s %sBorn=%s%s %sSeen%s@%s",GREEN,a,CYAN,theConf.reservedMacs[a],trk?"(TRK)":"",YELLOW, str2,WHITEC,
				losMacs[a].msgCount,GREEN,stateName[losMacs[a].dState],CYAN,losMacs[a].report==REPORTED? RED "Reported" RESETC:GREEN "OK" RESETC,
				losMacs[a].theHandle?LYELLOW "Alive":MAGENTA "Dead",RESETC,GRAY,losMacs[a].hwState?LRED "FAIL":LGREEN "OK",LYELLOW,RESETC,tempb,YELLOW,RESETC,ctime(&losMacs[a].lastUpdate));
		if(!shortans)
		{
			antes=0;
			pprintf("StateChanges:");
			for(int b=0;b<4;b++)
			{
				son=losMacs[a].stateChangeTS[b]-antes;
				pprintf("[%s]=%dms(%d) ",stateName[b],losMacs[a].stateChangeTS[b],son);
				antes=losMacs[a].stateChangeTS[b];
			}
				pprintf("\n");

					for (int b=0;b<MAXDEVS;b++)
						pprintf("%sMeter[%d]=%s KwH %6d Beats %9d\n",b%2?CYAN:GREEN,b,losMacs[a].meterSerial[b],losMacs[a].controlLastKwH[b],losMacs[a].controlLastBeats[b]);
		}
		delay(150);
		if(strcmp(losMacs[a].mtmName,"")!=0)
		{
			pprintf("\tMeter[%d](%s)\n",a,losMacs[a].mtmName);
			for (int z=0;z<MAXDEVS;z++)
			{
				if(strcmp(losMacs[a].meterSerial[z],"")!=0)
					pprintf("\t\tMID[%d] %s kWh %d Beats %d\n",z,losMacs[a].meterSerial[z],losMacs[a].controlLastKwH[z],losMacs[a].controlLastBeats[z]);
			}
		}
	}

	pprintf("%s\n",KBDT);
}

static void cmd_sendTelemetry(char * ss)
{
	pprintf("%sSend Telemetry\n",KBDT);
	xTaskCreate(&connMgr,"cnmgr",4096,(void*)123, 4, NULL);
}

static void cmd_tariffs(char * ss)
{
	const char *title="YearDay:";
	int err=0;
	cJSON *params=NULL;


	params=makeJson(ss);
	int cuando=askValue(title,params);
	if(cuando<0)
		return;

	if(params)
		cJSON_Delete(params);

	err=fram.read_tarif_day(cuando,(u8*)&tarifasDia);
	if(err!=0)
		pprintf("LoadTar Error %d reading Tariffs day %d...recovering from Host\n",err,cuando);
	pprintf("Tariffs ");
	for (int a=0;a<24;a++)
		pprintf("0x%04x ",tarifasDia[a]);
	pprintf("\n");
	err=fram.read_tarif_day(yearDay,(u8*)&tarifasDia);
		if(err!=0)
			pprintf("LoadTar Error %d reading Tariffs day %d...recovering from Host\n",err,yearDay);
}

static void cmd_firmware(char * ss)
{
	xTaskCreate(&firmUpdate,"U571",10240,NULL, 5, NULL);

}

void cmd_eraseTariff(char * ss)
{
	fram.erase_tarif();
	pprintf("%sTariffs erased\n",KBDT);
}

static void cmd_sendDelay(char * ss)
{
	char s1[20];

	cJSON *params=makeJson(ss);

	if(params)
	{
		cJSON *del= cJSON_GetObjectItem(params,"DELAY");
		if(del)
			theConf.msgTimeOut=del->valueint;
		else
			goto alla;
		cJSON_Delete(params);
		params=NULL;
	}
	else
	{
		alla:
		if(params)
			cJSON_Delete(params);

		pprintf("Delay(%d):",theConf.msgTimeOut);
		fflush(stdout);
		int fueron=get_string(UART_NUM_0,10,s1);
		if(fueron<=0)
			return;
		fueron=atoi(s1);
		theConf.msgTimeOut=fueron;
	}
	write_to_flash();
}

static void cmd_clearWL(char * ss)
{
	char s1[20];
	pprintf("%sAre you sure clear Whitelist?:%s",MAGENTA,RESETC);
	fflush(stdout);
	int fueron=get_string(UART_NUM_0,10,s1);
	if(fueron<=0)
		return;
	memset(theConf.reservedMacs,0,sizeof(theConf.reservedMacs));
	theConf.reservedCnt=0;
	write_to_flash();
	pprintf("White List Cleared\n");
}

void cmd_WhiteList(char * ss)
{
	char s1[20];
	int pos=-1;

	cJSON *params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"POS");
		if(jmeter)
			pos=jmeter->valueint;
		else
			goto alla;
		cJSON_Delete(params);
		params=NULL;
	}
	else
	{
		alla:
		if(params)
			cJSON_Delete(params);

		pprintf("%sPosition to Delete:%s",MAGENTA,RESETC);
		fflush(stdout);
		pos=get_string(UART_NUM_0,10,s1);
		if(pos<=0)
			return;
	}
	if(pos<0 || pos>theConf.reservedCnt-1)
	{
		pprintf("Pos %d Out of Range(%d)\n",pos,theConf.reservedCnt);
		return;
	}
	int son=(MAXSTA-(pos+1))*sizeof(uint32_t);
	if(pos<MAXSTA-1)
		memmove(&theConf.reservedMacs[pos],&theConf.reservedMacs[pos+1],son);
	memset(&theConf.reservedMacs[MAXSTA-1],0,sizeof(theConf.reservedMacs[0]));

	theConf.reservedCnt--;
	write_to_flash();
	pprintf("WhiteList Entry %d Cleared Count %d\n",pos,theConf.reservedCnt);
}

static void cmd_zeroKeys(char * ss)
{
	char s1[10];
	int len;

	pprintf("Zero Keys sure?");
	fflush(stdout);
	len=get_string(UART_NUM_0,10,s1);
	if(len<=0)
		return;
	for (int a=0;a<MAXSTA;a++)
	{
		memset(&theConf.lkey[a],0,AESL);
		memset(&losMacs[a].theKey[a],0,AESL);

	}
	write_to_flash();
	pprintf("Keys erased\n");
}

static void cmd_mqttDelay(char * ss)
{
	char s1[10];
	int pos;

	cJSON *params=makeJson(ss);

	if(params)
	{
		cJSON *dly= cJSON_GetObjectItem(params,"DELAY");
		if(dly)
			MQTTDelay=dly->valueint;
		else
			goto alla;
		cJSON_Delete(params);
		params=NULL;
	}
	else
	{
		alla:
		if(params)
			cJSON_Delete(params);

		pprintf("%sMqtt Delay(%d):%s",MAGENTA,MQTTDelay,RESETC);
		fflush(stdout);
		pos=get_string(UART_NUM_0,10,s1);
		if(pos<=0)
			return;
		MQTTDelay=atoi(s1);
	}
}

static void cmd_cryptoOption(char * ss)
{
	char s1[10];
	int pos;

	cJSON *params=makeJson(ss);

	if(params)
	{
		cJSON *dly= cJSON_GetObjectItem(params,"MODE");
		if(dly)
			theConf.crypt=dly->valueint;
		else
			goto alla;
		cJSON_Delete(params);
		params=NULL;
	}
	else
	{
		alla:
		if(params)
			cJSON_Delete(params);
		pprintf("%sCrypto(%d):%s",MAGENTA,theConf.crypt,RESETC);
		fflush(stdout);
		pos=get_string(UART_NUM_0,10,s1);
		if(pos<=0)
			return;
		theConf.crypt=atoi(s1);
	}
	write_to_flash();
}

static void cmd_macTrack(char * ss)
{
	char s1[10];
	int pos,len;

	cJSON *params=makeJson(ss);

	if(params)
	{
		cJSON *dly= cJSON_GetObjectItem(params,"MAC");
		if(dly)
			pos=dly->valueint;
		else
			goto alla;
		cJSON_Delete(params);
		params=NULL;
	}
	else
	{
		alla:
		if(params)
			cJSON_Delete(params);

		pprintf("%sMacTrace(%1x):%s",MAGENTA,theConf.macTrace,RESETC);
		fflush(stdout);
		len=get_string(UART_NUM_0,10,s1);
		if(len<=0)
			return;
		pos=atoi(s1);
		if(abs(pos)<MAXSTA)
			pprintf("Tracking MAC %06x\n",losMacs[abs(pos)].macAdd);
	}

	if(pos>=0)
	{
		if(pos<MAXSTA)
			theConf.macTrace |= 1<<pos;
		else
			theConf.macTrace=0xFFFF;
	}
	else
	{
		if(abs(pos)>MAXSTA)
			theConf.macTrace=0;
		else
			theConf.macTrace  &= ~(1 << (abs(pos)));
	}
	write_to_flash();

}

static void cmd_readTemp(char * ss)
{
	theTemperature=DS_get_temp(ds18b20_info);
	pprintf("Temp %f\n",theTemperature);
}

static void cmd_setSlice(char * ss)
{
	char s1[10];
	int pos,val;

	cJSON *params=makeJson(ss);

	if(params)
	{
		cJSON *dly= cJSON_GetObjectItem(params,"NUM");
		if(dly)
			val=dly->valueint;
		else
			goto alla;
		cJSON_Delete(params);
		params=NULL;
	}
	else
	{
		alla:
		if(params)
			cJSON_Delete(params);

		pprintf("%sNum os Slices(%d):%s",MAGENTA,theConf.numSlices,RESETC);
		fflush(stdout);
		pos=get_string(UART_NUM_0,10,s1);
		if(pos<=0)
			return;
		val=atoi(s1);
	}
	theConf.numSlices=val;

	write_to_flash();
	pprintf("Restarting\n");
	delay(1000);
	esp_restart();
}

static void cmd_cmdSession(char * ss)
{
	char s1[10];
	int pos;

	cJSON *params=makeJson(ss);

	if(params)
	{
		cJSON *dly= cJSON_GetObjectItem(params,"MODE");
		if(dly)
			sessionf=dly->valueint;
		else
			goto alla;
		cJSON_Delete(params);
		params=NULL;
	}
	else
	{
		alla:
		if(params)
			cJSON_Delete(params);

		pprintf("%sSession On/OFF(%d):%s",MAGENTA,sessionf,RESETC);
		fflush(stdout);
		pos=get_string(UART_NUM_0,10,s1);
		if(pos<=0)
			return;
		sessionf=atoi(s1);
	}

}


#ifdef HEAPSAMPLE
static void cmd_showHeap(char * ss)
{
	int van=vanHeap;

	pprintf("Heap Record %d current %d\n",van,esp_get_free_heap_size());
	uint32_t prev=theheap[0].theHeap;

	for (int a=0;a<van;a++)
	{
		pprintf("Routine %s\t Heap %d diff=%6d\t @%s",theheap[a].routine,theheap[a].theHeap,theheap[a].theHeap-prev,ctime(&theheap[a].ts));
		prev=theheap[a].theHeap;
	}
}
#endif

void init_kbd_commands()
{
	int x=0;
//just add a new cmd by using same expression and changing  comando, code and help mind the last [xx++] in the line and increasing NKEYS in defines.h

	strcpy((char*)&cmdds[x].comando,"ClearWL");			cmdds[x].code=cmd_clearWL;				strcpy(cmdds[x++].help,"Clear Macs White list");
	strcpy((char*)&cmdds[x].comando,"Config");			cmdds[x].code=cmd_confStatus;			strcpy(cmdds[x++].help,"SHORT");
	strcpy((char*)&cmdds[x].comando,"Controllers");		cmdds[x].code=cmd_printControllers;		strcpy(cmdds[x++].help,"LONG");
	strcpy((char*)&cmdds[x].comando,"Crypto");			cmdds[x].code=cmd_cryptoOption;			strcpy(cmdds[x++].help,"SetCrypt");
	strcpy((char*)&cmdds[x].comando,"DumpCore");		cmdds[x].code=cmd_dumpCore;				strcpy(cmdds[x++].help,"DumpCore");
	strcpy((char*)&cmdds[x].comando,"EraseTariff");		cmdds[x].code=cmd_eraseTariff;			strcpy(cmdds[x++].help,"EraseTariff");
	strcpy((char*)&cmdds[x].comando,"Firmware");		cmdds[x].code=cmd_firmware;				strcpy(cmdds[x++].help,"Load Firmaware");
	strcpy((char*)&cmdds[x].comando,"Flush");			cmdds[x].code=cmd_flushFram;			strcpy(cmdds[x++].help,"Flush to Fram");
	strcpy((char*)&cmdds[x].comando,"FormatFram");		cmdds[x].code=cmd_formatFram;			strcpy(cmdds[x++].help,"VAL");
	strcpy((char*)&cmdds[x].comando,"FramDay");			cmdds[x].code=cmd_framDaySearch;		strcpy(cmdds[x++].help,"METER MONTH");
	strcpy((char*)&cmdds[x].comando,"FramDaysAll");		cmdds[x].code=cmd_framDays;				strcpy(cmdds[x++].help,"METER MONTH");
	strcpy((char*)&cmdds[x].comando,"FramHour");		cmdds[x].code=cmd_framHourSearch;		strcpy(cmdds[x++].help,"METER MONTH DAY HOUR");
	strcpy((char*)&cmdds[x].comando,"FramMonth");		cmdds[x].code=cmd_framMonthSearch;		strcpy(cmdds[x++].help,"METER MONTH");
	strcpy((char*)&cmdds[x].comando,"FramMonthsAll");	cmdds[x].code=cmd_framMonths;			strcpy(cmdds[x++].help,"METER");
	strcpy((char*)&cmdds[x].comando,"Help");			cmdds[x].code=cmd_showHelp;				strcpy(cmdds[x++].help,"LONG");
	strcpy((char*)&cmdds[x].comando,"LogLevel");		cmdds[x].code=cmd_logLevel;				strcpy(cmdds[x++].help,"None Info Err Verb Warn");
	strcpy((char*)&cmdds[x].comando,"MacTrack");		cmdds[x].code=cmd_macTrack;				strcpy(cmdds[x++].help,"Trace MAC Msgs");
	strcpy((char*)&cmdds[x].comando,"MessageCount");	cmdds[x].code=cmd_msgCount;				strcpy(cmdds[x++].help,"Messasge Count");
	strcpy((char*)&cmdds[x].comando,"MeterCount");		cmdds[x].code=cmd_meterCount;			strcpy(cmdds[x++].help,"MeterCount");
	strcpy((char*)&cmdds[x].comando,"MeterStat");		cmdds[x].code=cmd_meterStatus;			strcpy(cmdds[x++].help,"METER");
	strcpy((char*)&cmdds[x].comando,"MQTT");			cmdds[x].code=cmd_mqttDelay;			strcpy(cmdds[x++].help,"Mqtt Delay");
	strcpy((char*)&cmdds[x].comando,"ReadFram");		cmdds[x].code=cmd_readFram;				strcpy(cmdds[x++].help,"COUNT");
	strcpy((char*)&cmdds[x].comando,"SendDelay");		cmdds[x].code=cmd_sendDelay;			strcpy(cmdds[x++].help,"Status check TimeOut");
	strcpy((char*)&cmdds[x].comando,"Session");			cmdds[x].code=cmd_cmdSession;			strcpy(cmdds[x++].help,"SetSession");
	strcpy((char*)&cmdds[x].comando,"Slice");			cmdds[x].code=cmd_setSlice;				strcpy(cmdds[x++].help,"Set Slice Mqtt");
	strcpy((char*)&cmdds[x].comando,"Tariff");			cmdds[x].code=cmd_tariffs;				strcpy(cmdds[x++].help,"Get Tariffs");
	strcpy((char*)&cmdds[x].comando,"Telemetry");		cmdds[x].code=cmd_sendTelemetry;		strcpy(cmdds[x++].help,"Send Telemetry");
	strcpy((char*)&cmdds[x].comando,"Temp");			cmdds[x].code=cmd_readTemp;				strcpy(cmdds[x++].help,"Temp");
	strcpy((char*)&cmdds[x].comando,"Trace");			cmdds[x].code=cmd_traceFlags;			strcpy(cmdds[x++].help,"NONE ALL BOOTD WIFID MQTTD PUBSUBD OTAD CMDD WEBD GEND MQTTT FRMCMD INTD FRAMD MSGD TIMED SIMD HOSTD");
	strcpy((char*)&cmdds[x].comando,"WebReset");		cmdds[x].code=cmd_webReset;				strcpy(cmdds[x++].help,"WebReset");
	strcpy((char*)&cmdds[x].comando,"WhiteList");		cmdds[x].code=cmd_WhiteList;			strcpy(cmdds[x++].help,"POS");
	strcpy((char*)&cmdds[x].comando,"WriteFram");		cmdds[x].code=cmd_writeFram;			strcpy(cmdds[x++].help,"ADDR VAL");
	strcpy((char*)&cmdds[x].comando,"ZeroK");			cmdds[x].code=cmd_zeroKeys;				strcpy(cmdds[x++].help,"ZKeys");
#ifdef HEAPSAMPLE
	strcpy((char*)&cmdds[x].comando,"Heap");			cmdds[x].code=cmd_showHeap;				strcpy(cmdds[x++].help,"HeapRec");
#endif
	for (int a=0;a<NKEYS;a++)
		for (int b=0;b<strlen(cmdds[a].comando);b++)
			cmdds[a].comando[b]=toupper(cmdds[a].comando[b]);
}
//void init_kbd_commands()
//{
//	strcpy((char*)&cmdds[0].comando,"Config");			cmdds[ 0].code=confStatus;			strcpy(cmdds[0].help,"SHORT");
//	strcpy((char*)&cmdds[1].comando,"WebReset");		cmdds[ 1].code=webReset;			strcpy(cmdds[1].help,"WebReset");
//	strcpy((char*)&cmdds[2].comando,"Controllers");		cmdds[ 2].code=printControllers;	strcpy(cmdds[2].help,"LONG");
//	strcpy((char*)&cmdds[3].comando,"MeterStat");		cmdds[ 3].code=meterStatus;			strcpy(cmdds[3].help,"METER");
//	strcpy((char*)&cmdds[4].comando,"EraseTariff");		cmdds[ 4].code=eraseTariff;			strcpy(cmdds[4].help,"EraseTariff");
//	strcpy((char*)&cmdds[5].comando,"MeterCount");		cmdds[ 5].code=meterCount;			strcpy(cmdds[5].help,"MeterCount");
//	strcpy((char*)&cmdds[6].comando,"DumpCore");		cmdds[ 6].code=dumpCore;			strcpy(cmdds[6].help,"DumpCore");
//	strcpy((char*)&cmdds[7].comando,"FormatFram");		cmdds[ 7].code=formatFram;			strcpy(cmdds[7].help,"VAL");
//	strcpy((char*)&cmdds[8].comando,"ReadFram");		cmdds[ 8].code=readFram;			strcpy(cmdds[8].help,"COUNT");
//	strcpy((char*)&cmdds[9].comando,"WriteFram");		cmdds[ 9].code=writeFram;			strcpy(cmdds[9].help,"ADDR VAL");
//	strcpy((char*)&cmdds[10].comando,"LogLevel");		cmdds[10].code=logLevel;			strcpy(cmdds[10].help,"None Info Err Verb Warn");
//	strcpy((char*)&cmdds[11].comando,"FramDaysAll");	cmdds[11].code=framDays;			strcpy(cmdds[11].help,"METER MONTH");
//	strcpy((char*)&cmdds[12].comando,"FramHour");		cmdds[12].code=framHourSearch;		strcpy(cmdds[12].help,"METER MONTH DAY HOUR");
//	strcpy((char*)&cmdds[13].comando,"FramDay");		cmdds[13].code=framDaySearch;		strcpy(cmdds[13].help,"METER MONTH");
//	strcpy((char*)&cmdds[14].comando,"FramMonth");		cmdds[14].code=framMonthSearch;		strcpy(cmdds[14].help,"METER MONTH");
//	strcpy((char*)&cmdds[15].comando,"Flush");			cmdds[15].code=flushFram;			strcpy(cmdds[15].help,"Flush to Fram");
//	strcpy((char*)&cmdds[16].comando,"MessageCount");	cmdds[16].code=msgCount;			strcpy(cmdds[16].help,"Messasge Count");
//	strcpy((char*)&cmdds[17].comando,"Help");			cmdds[17].code=showHelp;			strcpy(cmdds[17].help,"LONG");
//	strcpy((char*)&cmdds[18].comando,"Trace");			cmdds[18].code=traceFlags;
//	strcpy(cmdds[18].help,"NONE ALL BOOTD WIFID MQTTD PUBSUBD OTAD CMDD WEBD GEND MQTTT FRMCMD INTD FRAMD MSGD TIMED SIMD HOSTD");
//	strcpy((char*)&cmdds[19].comando,"FramMonthsAll");	cmdds[19].code=framMonths;			strcpy(cmdds[19].help,"METER");
//	strcpy((char*)&cmdds[20].comando,"Telemetry");		cmdds[20].code=sendTelemetry;		strcpy(cmdds[20].help,"Send Telemetry");
//	strcpy((char*)&cmdds[21].comando,"Tariff");			cmdds[21].code=tariffs;				strcpy(cmdds[21].help,"Get Tariffs");
//	strcpy((char*)&cmdds[22].comando,"Firmware");		cmdds[22].code=firmware;			strcpy(cmdds[22].help,"Load Firmaware");
//	strcpy((char*)&cmdds[23].comando,"SendDelay");		cmdds[23].code=sendDelay;			strcpy(cmdds[23].help,"Status check TimeOut");
//	strcpy((char*)&cmdds[24].comando,"ClearWL");		cmdds[24].code=clearWL;				strcpy(cmdds[24].help,"Clear Macs White list");
//	strcpy((char*)&cmdds[25].comando,"WhiteList");		cmdds[25].code=WhiteList;			strcpy(cmdds[25].help,"POS");
//	strcpy((char*)&cmdds[26].comando,"ZeroK");			cmdds[26].code=zeroKeys;			strcpy(cmdds[26].help,"ZKeys");
//	strcpy((char*)&cmdds[27].comando,"MQTT");			cmdds[27].code=mqttDelay;			strcpy(cmdds[27].help,"Mqtt Delay");
//	strcpy((char*)&cmdds[28].comando,"Crypto");			cmdds[28].code=cryptoOption;		strcpy(cmdds[28].help,"SetCrypt");
//	strcpy((char*)&cmdds[29].comando,"MacTrace");		cmdds[29].code=macTrace;			strcpy(cmdds[29].help,"Trace MAC Msgs");
//	strcpy((char*)&cmdds[30].comando,"Temp");			cmdds[30].code=readTemp;			strcpy(cmdds[30].help,"Temp");
//	strcpy((char*)&cmdds[31].comando,"Slice");			cmdds[31].code=setSlice;			strcpy(cmdds[31].help,"Set Slice Mqtt");
//	strcpy((char*)&cmdds[32].comando,"Session");		cmdds[32].code=cmdSession;			strcpy(cmdds[32].help,"SetSession");
//#ifdef HEAPSAMPLE
//	strcpy((char*)&cmdds[33].comando,"Heap");			cmdds[33].code=showHeap;			strcpy(cmdds[33].help,"HeapRec");
//#endif
//	for (int a=0;a<NKEYS;a++)
//		for (int b=0;b<strlen(cmdds[a].comando);b++)
//			cmdds[a].comando[b]=toupper(cmdds[a].comando[b]);
//}


void trim(char * s) {	//left and right
    char * p = s;
    int l = strlen(p);

    while(isspace(p[l - 1])) p[--l] = 0;
    while(* p && isspace(* p)) ++p, --l;

    memmove(s, p, l + 1);
}

void kbd(void *arg)
{
	int len,cualf;
	uart_port_t uart_num = UART_NUM_0 ;
	char kbdstr[100],nstr[100],oldcmd[100];
	bzero(kbdstr,sizeof(kbdstr));
	bzero(oldcmd,sizeof(oldcmd));
	bzero(nstr,sizeof(nstr));


	uart_config_t uart_config = {
			.baud_rate = 460800,
			.data_bits = UART_DATA_8_BITS,
			.parity = UART_PARITY_DISABLE,
			.stop_bits = UART_STOP_BITS_1,
			.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
			.rx_flow_ctrl_thresh = 122,
			.use_ref_tick=false
	};
	uart_param_config(uart_num, &uart_config);
	uart_set_pin(uart_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	esp_err_t err= uart_driver_install(uart_num, 1024 , 1024, 10, NULL, 0);
	if(err!=ESP_OK)
		pprintf("Error UART Install %d\n",err);

	init_kbd_commands();

	while(1)
	{
		bzero(kbdstr,sizeof(kbdstr));
		bzero(nstr,sizeof(nstr));

		len=get_string(UART_NUM_0,10,kbdstr);
		if(len<1)
			strcpy(kbdstr,oldcmd);
		else
		{
			kbdstr[len]=0;
			for(int a=0;a<len;a++)
				kbdstr[a]=toupper(kbdstr[a]);
		}

		if(strlen(kbdstr)>0)
			trim(kbdstr);

		strcpy(nstr,kbdstr);
		char *pch = strtok (nstr," ");		//find first space

		int llen=strlen(pch);
		//arguments start at kbdstr+llen+1

		cualf=kbdCmd(pch);
		if(cualf>=0)
		{
			if(cmdds[cualf].code)
				(*cmdds[cualf].code)((kbdstr+llen+1));
			strcpy(oldcmd,pch);
		}

		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}

