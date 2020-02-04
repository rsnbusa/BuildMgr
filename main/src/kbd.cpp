#define GLOBAL
#include "defines.h"
#include "includes.h"
#include "globals.h"
#include "projTypes.h"
#include <bits/stdc++.h>

#define KBDT		"\e[36m[KBD]\e[0m"
#define MAXCMDSK	26

extern void delay(uint32_t a);
extern void write_to_flash();
extern void write_to_fram(u8 meter,bool addit);
extern void connMgr(void *pArg);
extern void firmUpdate(void *pArg);
extern void shaMake(char * key,uint8_t klen,uint8_t* shaResult);

using namespace std;

typedef void (*functkbd)(string ss);

typedef struct cmd_kbd_t{
    char comando[20];
    functkbd code;
    string help;
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

cJSON *makeJson(string ss)
{
	cJSON *root=cJSON_CreateObject();

	size_t start=0,found,fkey;
	int van=0;
	string cmdstr,theK;
	std::locale loc;

	if(ss=="")
		return NULL;

	do{
		found=ss.find(" ",start);
		if (found!=std::string::npos)
		{
			cmdstr=ss.substr(start,found-start);
			fkey=cmdstr.find_first_of("=:");
			if(fkey!=std::string::npos)
			{
				theK=cmdstr.substr(0,fkey);
				for (size_t i=0; i<theK.length(); ++i)
				    theK[i]= std::toupper(theK[i],loc);

				string valor=cmdstr.substr(fkey+1,cmdstr.length());

				bool isdig=true;
				for (size_t i=0; i<valor.length(); ++i)
				{
					if(!isdigit(valor[i]))
					{
							 isdig=false;
							 break;
					}
				}
				if(!isdig)
					cJSON_AddStringToObject(root,theK.c_str(),valor.c_str());
				else
					cJSON_AddNumberToObject(root,theK.c_str(),atoi(valor.c_str()));

			}
			else
				return NULL; //error is drastic
			start=found+1;
			van++;
		}

	} while(found!=std::string::npos);

//	char *lmessage=cJSON_Print(root);
//	if(lmessage)
//	{
//		printf("Root %s\n",lmessage);
//		free(lmessage);
//	}
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


int cmdfromstring(string key)
{
	for (int i=0; i <NKEYS; i++)
	{
		string s1=string(lookuptable[i]);
		if(strstr(s1.c_str(),key.c_str())!=NULL)
			return i;
	}
	return -1;
}

int kbdCmd(string key)
{
	string s1,s2,skey;
	s1=s2=skey="";

//	printf("Key %s\n",key.c_str());
	skey=string(key);

	for(int a=0;a<key.length();a++)
		skey[a]=toupper(skey[a]);

//	printf("Kbdin =%s=\n",skey.c_str());

	for (int i=0; i <MAXCMDSK; i++)
	{
		s1=string(cmdds[i].comando);

		for(int a=0;a<s1.length();a++)
			s2[a]=toupper(s1[a]);
	//	printf("Cmd =%s=\n",s2.c_str());


		if(strstr(s2.c_str(),skey.c_str())!=NULL)
			return i;
		s1=s2="";
	}
	return -1;
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
	}
	else
	{
		printf("Meter:");
		fflush(stdout);
		len=get_string(UART_NUM_0,10,s1);
		if(len<=0)
		{
			printf("\n");
			return -1;
		}
		len= atoi(s1);
	}
		if(len<MAXDEVS)
			return len;
		else
		{
			printf("%sMeter out of range 0-%d%s\n",MAGENTA,MAXDEVS-1,RESETC);
			return -1;
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
	}
	else
	{
		printf("Month(0-11):");
		fflush(stdout);
		len=get_string(UART_NUM_0,10,s1);
		if(len<=0)
		{
			printf("\n");
			return -1;
		}
		len= atoi(s1);
	}
	if(len<12)
		return len;
	else
	{
		printf("%sMonth out of range 0-11%s\n",MAGENTA,RESETC);
		return -1;
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
		}
	else
	{
		printf("Hour(0-23):");
		fflush(stdout);
		len=get_string(UART_NUM_0,10,s1);
		if(len<=0)
		{
			printf("\n");
			return -1;
		}
		len= atoi(s1);
	}
	if(len<24)
		return len;
	else
	{
		printf("%sHour out of range 0-23%s\n",MAGENTA,RESETC);
		return -1;
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
		}
	else
	{
		printf("Day(0-%d):",daysInMonth[month]-1);
		fflush(stdout);
		len=get_string(UART_NUM_0,10,s1);
		if(len<=0)
		{
			printf("\n");
			return -1;
		}
		len= atoi(s1);
	}
		if(len<daysInMonth[month])
			return len;
		else
		{
			printf("%sDay out of range 0-%d%s\n",MAGENTA,daysInMonth[month]-1,RESETC);
			return -1;
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
				return -1;
		}
	else
	{
		printf("%s:",title);
		fflush(stdout);
		len=get_string(UART_NUM_0,10,s1);
		if(len<=0)
		{
			printf("\n");
			return -1;
		}

		return atoi(s1);
	}
	return -1;
}

void confStatus(string ss)
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
	printf("%s====================\nConfiguration Status %s",KBDT,ctime(&now));

	printf("ConnMgr %s%s%s Whitelisted %s%d%s",CYAN,theConf.meterConnName,RESETC,GREEN,theConf.reservedCnt,RESETC);
	printf("%sConfiguration RunStatus[%s] Trace %x %sBootCount %d LReset %x @%s",YELLOW,theConf.active?"Run":"Setup",theConf.traceflag,GREEN,theConf.bootcount,theConf.lastResetCode,ctime(&theConf.lastReboot));

	if(!shortans)
	{
		for (int a=0;a<MAXDEVS;a++)
		{
			localtime_r(&theConf.bornDate[a], &timeinfo);
			strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
			printf("%sMeter[%d] Serial %s BPW %d Born %s BornkWh %d %s\n",(a % 2)?CYAN:GREEN,a,theConf.medidor_id[a],theConf.beatsPerKw[a],
					strftime_buf,theConf.bornKwh[a],theConf.configured[a]==3?"Active":"Inactive");
		}
	}

	printf("===============\n");
	printf("  MQTT Section\n");
	printf("===============%s\n",RESETC);
	printf("Command Queue %s%s%s\n",GREEN,cmdQueue.c_str(),RESETC);
	printf("Host Queue %s%s%s\n",CYAN,controlQueue.c_str(),RESETC);

	if(!shortans)
	{
		printf("%s===============\n",YELLOW);
		printf("Control Tasks\n");
		printf("===============%s\n",RESETC);

		printf("CmdMgr %s @%s",cmdHandle?GREEN "Alive" RESETC:RED "Dead" RESETC,ctime(&mgrTime[CMDMGR]));
		printf("FramMgr %s @%s",framHandle?GREEN "Alive" RESETC:RED "Dead" RESETC,ctime(&mgrTime[FRAMMGR]));
		printf("PinMgr %s @%s",pinHandle?GREEN "Alive" RESETC:RED "Dead" RESETC,ctime(&mgrTime[PINMGR]));
		printf("WatchMgr %s @%s",watchHandle?GREEN "Alive" RESETC:RED "Dead" RESETC,ctime(&mgrTime[WATCHMGR]));
		printf("DisplayMgr %s @%s",displayHandle?GREEN "Alive" RESETC:RED "Dead" RESETC,ctime(&mgrTime[DISPLAYMGR]));
		printf("SubMgr %s @%s",submHandle?GREEN "Alive" RESETC:RED "Dead" RESETC,ctime(&mgrTime[SUBMGR]));
	}
	printf("====================%s\n",KBDT);

}

void meterStatus(string ss)
{
	uint32_t valr;
	cJSON *params=NULL;
	int meter=-1;

	params=makeJson(ss);

	printf("%s============\nMeter status\n",KBDT);
	meter=askMeter(params);
	if(meter<0||meter>MAXDEVS-1)
	{
		printf("Meter %d out of range\n",meter);
		return;
	}

	printf("============%s\n\n",YELLOW);

	if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
	{
	//	printf("Meter Compare test Year %d Month %d Day %d Hour %d YDAY %d\n",yearg,mesg,diag,horag,yearDay);
		valr=0;
		fram.read_beat(meter,(uint8_t*)&valr);
		printf("Beat[%d]=%d %d\n",meter,valr,theMeters[meter].currentBeat);
		valr=0;
		fram.read_lifekwh(meter,(uint8_t*)&valr);
		printf("LastKwh[%d]=%d %d\n",meter,valr,theMeters[meter].curLife);
		valr=0;
		fram.read_lifedate(meter,(uint8_t*)&valr);
		printf("LifeDate[%d]=%d %d %s",meter,valr,theMeters[meter].lastKwHDate,ctime((time_t*)&theMeters[meter].lastKwHDate));
		valr=0;
		fram.read_month(meter,mesg,(uint8_t*)&valr);
		printf("Month[%d]Mes[%d]=%d %d\n",meter,mesg,valr,theMeters[meter].curMonth);
		valr=0;
		fram.read_monthraw(meter,mesg,(uint8_t*)&valr);
		printf("MonthRaw[%d]Mes[%d]=%d %d\n",meter,mesg,valr,theMeters[meter].curMonthRaw);
		valr=0;
		fram.read_day(meter,yearDay,(uint8_t*)&valr);
		printf("Day[%d]Mes[%d]Dia[%d]=%d %d\n",meter,mesg,diag,valr,theMeters[meter].curDay);
		valr=0;
		fram.read_dayraw(meter,yearDay,(uint8_t*)&valr);
		printf("DayRaw[%d]Mes[%d]Dia[%d]=%d %d\n",meter,mesg,diag,valr,theMeters[meter].curDayRaw);
		valr=0;
		fram.read_hour(meter,yearDay,horag,(uint8_t*)&valr);
		printf("Hour[%d]Mes[%d]Dia[%d]Hora[%d]=%d %d\n",meter,mesg,diag,horag,valr,theMeters[meter].curHour);
		valr=0;
		fram.read_hourraw(meter,yearDay,horag,(uint8_t*)&valr);
		printf("HourRaw[%d]Mes[%d]Dia[%d]Hora[%d]=%d %d%s\n",meter,mesg,diag,horag,valr,theMeters[meter].curHourRaw,RESETC);

		xSemaphoreGive(framSem);
		printf("%s============\n",KBDT);

	}
}

void meterTest(string ss)
{
	cJSON *params=NULL;
	int val=0,meter=0;

	params=makeJson(ss);

	meter=askMeter(params);
	if(meter<0||meter>MAXDEVS-1)
	{
		printf("Meter %d out of range\n",meter);
		return;
	}
		val=askValue((char*)"Value",params);


	printf("%s================\nTest Write Meter\n",KBDT);


	printf("================%s\n\n",RESETC);

	uint32_t valr;

	if(xSemaphoreTake(framSem, portMAX_DELAY/  portTICK_RATE_MS))
	{
		printf("Fram Write Meter %d Year %d Month %d Day %d Hour %d Value %d YDAY %d\n",meter,yearg,mesg,diag,horag,val,yearDay);

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
		printf("Reading now\n");
		fram.read_beat(meter,(uint8_t*)&valr);
		printf("Beat[%d]=%d\n",meter,valr);
		fram.read_lifekwh(meter,(uint8_t*)&valr);
		printf("LifeKwh[%d]=%d\n",meter,valr);
		fram.read_lifedate(meter,(uint8_t*)&valr);
		printf("LifeDate[%d]=%d\n",meter,valr);
		fram.read_month(meter,mesg,(uint8_t*)&valr);
		printf("Month[%d]=%d\n",meter,valr);
		fram.read_monthraw(meter,mesg,(uint8_t*)&valr);
		printf("MonthRaw[%d]=%d\n",meter,valr);
		fram.read_day(meter,yearDay,(uint8_t*)&valr);
		printf("Day[%d]=%d\n",meter,valr);
		fram.read_dayraw(meter,yearDay,(uint8_t*)&valr);
		printf("DayRaw[%d]=%d\n",meter,valr);
		fram.read_hour(meter,yearDay,horag,(uint8_t*)&valr);
		printf("Hour[%d]=%d\n",meter,valr);
		fram.read_hourraw(meter,yearDay,horag,(uint8_t*)&valr);
		printf("HourRaw[%d]=%d\n",meter,valr);

		xSemaphoreGive(framSem);
	}
}

void webReset(string ss)
{
	theConf.active=!theConf.active;
	printf("%sWeb %s\n",KBDT,theConf.active?"RunConf":"SetupConf");
	printf("===========%s\n",KBDT);
	if(theConf.active)
		memset(theConf.configured,3,sizeof(theConf.configured));
	else
		memset(theConf.configured,0,sizeof(theConf.configured));
	write_to_flash();
}

void meterCount(string ss)
{
	time_t timeH;

	int tots=0;
	printf("%s==============\nMeter Counters%s\n",KBDT,RESETC);

	for (int a=0;a<MAXDEVS;a++)
	{
		tots+=theMeters[a].currentBeat;
		printf("%sMeter[%d]=%s Beats %d kWh %d BPK %d\n",(a % 2)?GREEN:CYAN,a,RESETC,theMeters[a].currentBeat,theMeters[a].curLife,
				theMeters[a].beatsPerkW);
	}
	printf("%sTotal Pulses rx=%s %d (%s)\n",RED,RESETC,totalPulses,tots==totalPulses?"Ok":"No");
	fram.readMany(FRAMDATE,(uint8_t*)&timeH,sizeof(timeH));//last known date
	printf("Last Date %s",ctime(&timeH));
	printf("==============%s\n\n",KBDT);

}

void dumpCore(string ss)
{

	u8 *p;
	printf("%sDumping core ins 3 secs%s\n",RED,RESETC);
	delay(3000);
	p=0;
	*p=0;
}

void formatFram(string ss)
{
	cJSON *params=NULL;
	int val=0;

	params=makeJson(ss);

	if(params)
	{
		cJSON *jval= cJSON_GetObjectItem(params,"VAL");
		if(jval)
			val=jval->valueint;
		cJSON_Delete(params);
	}
	else
	{

		printf("%s===========\nFormat FRAM\n",KBDT);

		val=askValue((char*)"Init value",params);
		printf("===========%s\n\n",KBDT);
	}
	if(val<0)
		return;

	fram.format(val,NULL,100,true);
	printf("Format done...rebooting\n");

	memset(&theConf.medidor_id,0,sizeof(theConf.medidor_id));
	memset(&theConf.beatsPerKw,0,sizeof(theConf.beatsPerKw));
	memset(&theConf.configured,0,sizeof(theConf.configured));
	memset(&theConf.bornKwh ,0,sizeof(theConf.bornKwh));
	memset(&theConf.bornDate ,0,sizeof(theConf.bornDate));

	write_to_flash();

	esp_restart();
//	for(int a=0;a<MAXDEVS;a++)
//		load_from_fram(a);
}


void readFram(string ss)
{
	cJSON *params=NULL;
	int framAddress=0,fueron=0;
	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"ADDR");
		if(jmeter)
			framAddress=jmeter->valueint;
		cJSON *jval= cJSON_GetObjectItem(params,"COUNT");
		if(jval)
			fueron=jval->valueint;
		cJSON_Delete(params);
	}
	else
	{
		printf("%s=========\nRead FRAM\n",KBDT);
		framAddress=askValue((char*)"Address",params);
		fueron=askValue((char*)"Count",params);
		printf("=========%s\n\n",KBDT);
	}
	if(fueron<0 || framAddress<0)
		return;

	fram.readMany(framAddress,(uint8_t*)tempb,fueron);
	for (int a=0;a<fueron;a++)
		printf("%02x-",tempb[a]);
	printf("\n");

}

void writeFram(string ss)
{
	cJSON *params=NULL;
	int fueron=0,framAddress=0;
	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"ADDR");
		if(jmeter)
			framAddress=jmeter->valueint;
		cJSON *jval= cJSON_GetObjectItem(params,"VAL");
		if(jval)
			fueron=jval->valueint;
		cJSON_Delete(params);
	}
	else
	{
		printf("%s==========\nWrite FRAM\n",KBDT);
		framAddress=askValue((char*)"Address",params);
		fueron=askValue((char*)"Value",params);
		printf("==========%s\n\n",KBDT);
	}
	if(fueron<0 || framAddress<0)
		return;

	fram.write8(framAddress,fueron);

}

void logLevel(string ss)
{
	cJSON *params=NULL;
	char s1[10];

	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"LEVEL");
		if(jmeter)
			s1[0]=jmeter->valuestring[0];
		cJSON_Delete(params);
	}
	else
	{

		printf("%sLOG level:(N)one (I)nfo (E)rror (V)erbose (W)arning:",KBDT);
		fflush(stdout);
		int len=get_string(UART_NUM_0,10,s1);
		if(len<=0)
		{
			printf("\n");
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
		}

}

void framDays(string ss)
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
		cJSON *jval= cJSON_GetObjectItem(params,"MONTH");
		if(jval)
			month=jval->valueint;
		cJSON_Delete(params);
	}
	else
	{

		printf("%s==================\nFram Days in Month\n",KBDT);
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
					printf("M[%d]D[%d]=%d RAM=%d ",month,a,valor,theMeters[meter].curDay);
				else
					printf("M[%d]D[%d]=%d ",month,a,valor);
			}
			tots+=valor;
		}
		xSemaphoreGive(framSem);
		printf(" Total=%d\n",tots);
		printf("==================%s\n",KBDT);
	}

}

void framHours(string ss)
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
		cJSON *jval= cJSON_GetObjectItem(params,"MONTH");
		if(jval)
			month=jval->valueint;
		cJSON *jday= cJSON_GetObjectItem(params,"DAY");
		if(jday)
			dia=jval->valueint;
		cJSON_Delete(params);
	}
	else
	{
		printf("%s=================\nFram Hours in Day\n",KBDT);

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
						printf("%sM[%d]D[%d]H[%d]=%d RAM=%d ",(a % 2)?CYAN:GREEN,month,dia,a,valor,theMeters[meter].curHour);
					else
						printf("%sM[%d]D[%d]H[%d]=%d ",(a % 2)?CYAN:GREEN,month,dia,a,valor);
				}
				tots+=valor;
		}
		xSemaphoreGive(framSem);
		printf(" Total=%d\n",tots);
		printf("=================%s\n",KBDT);
	}
}

void framMonthsHours(string ss)
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
		cJSON *jval= cJSON_GetObjectItem(params,"MONTH");
		if(jval)
			month=jval->valueint;

		cJSON_Delete(params);
	}
	else
	{
		printf("%s=================\nFram Hours in Month\n",KBDT);
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
							printf("%sM[%d]D[%d]H[%d]=%d RAM=%d ",(a % 2)?CYAN:GREEN,month,b,a,valor,theMeters[meter].curHour);
						else
							printf("%sM[%d]D[%d]H[%d]=%d ",(a % 2)?CYAN:GREEN,month,b,a,valor);
					}
					tots+=valor;
			}
			if(was)
				printf(" Total=%d\n",tots);
		}
			xSemaphoreGive(framSem);
			printf("=================%s\n",KBDT);

	}
}

void framHourSearch(string ss)
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
		cJSON *jval= cJSON_GetObjectItem(params,"MONTH");
		if(jval)
			month=jval->valueint;
		jval= cJSON_GetObjectItem(params,"DAY");
		if(jval)
			dia=jval->valueint;
		jval= cJSON_GetObjectItem(params,"HOUR");
		if(jval)
			hora=jval->valueint;

		cJSON_Delete(params);
	}
	else
	{
		printf("%s================\nFram Hour Search\n",KBDT);

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
				printf("Date %d/%d/%d %d:00:00=%d RAM=%d\n",yearg,month,dia,hora,valor,theMeters[meter].curHour);
			else
				printf("Date %d/%d/%d %d:00:00=%d\n",yearg,month,dia,hora,valor);
			}
	}
	printf("================%s\n",KBDT);

}


void framDaySearch(string ss)
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
		cJSON *jval= cJSON_GetObjectItem(params,"MONTH");
		if(jval)
			month=jval->valueint;
		jval= cJSON_GetObjectItem(params,"DAY");
		if(jval)
			dia=jval->valueint;
		cJSON_Delete(params);
	}
	else
	{
		printf("%s===============\nFram Day Search\n",KBDT);

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
			printf("Date %d/%d/%d =%d RAM=%d\n",yearg,month,dia,valor,theMeters[meter].curDay);
		else
			printf("Date %d/%d/%d =%d\n",yearg,month,dia,valor);

		}
	}
	printf("===============%s\n",KBDT);

}

void framMonthSearch(string ss)
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
		cJSON *jval= cJSON_GetObjectItem(params,"MONTH");
		if(jval)
			month=jval->valueint;
		cJSON_Delete(params);
	}
	else
	{
		printf("%s=================\nFram Month Search\n",KBDT);

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
				printf("Month[%d] =%d RAM=%d\n",month,valor,theMeters[meter].curMonth);
			else
				printf("Month[%d] =%d\n",month,valor);
		}
	}
	printf("\n=================%s\n",KBDT);
}

void framMonths(string ss)
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
		cJSON_Delete(params);
	}
	else
	{
		printf("%s=================\nFram Month All\n",KBDT);
		meter=askMeter(params);
	}

	if(meter<0)
		return;

	if(xSemaphoreTake(framSem, portMAX_DELAY))
	{
		printf("Meter[%d]",meter);
		for(int a=0;a<12;a++)
		{
			fram.read_month(meter,a,(u8*)&valor);
			if(valor>0)
			{
			if(theMeters[meter].curMonth>0 && a==mesg)
				printf("%s[%d]=%d RAM=%d ",(a % 2)?CYAN:GREEN,a,valor,theMeters[meter].curMonth);
			else
				printf("%s[%d]=%d ",(a % 2)?CYAN:GREEN,a,valor);
			}
			tots+=valor;
		}
		printf(" Total=%d\n",tots);
		printf("=================%s\n",KBDT);

		xSemaphoreGive(framSem);
	}
	printf("\n");
}

void flushFram(string ss)
{
	printf("%s=============\nFlushing Fram\n",KBDT);
	for(int a=0;a<MAXDEVS;a++)
		write_to_fram(a,false);
	printf("%d meters flushed\n",MAXDEVS);
	printf("=============%s\n",KBDT);
}

void msgCount(string ss)
{
	printf("%s=============\nMessage Count\n",KBDT);
	for (int a=0;a<MAXDEVS;a++)
		//printf("TotalMsg[%d] sent msgs %d\n",a,totalMsg[a]);
	printf("TotalMsg[%d] sent msgs %d\n",a,1);
	printf("Controller Total %d\n",sentTotal);
	printf("=============%s\n",KBDT);

}

void traceFlags(string sss)
{
	char s1[60],s2[20];
	string ss;
	std::locale loc;

	printf("%sTrace Flags:%s",KBDT,CYAN);

	for (int a=0;a<NKEYS/2;a++)
	if (theConf.traceflag & (1<<a))
		printf("%s ",lookuptable[a]);
	printf("\n");

	if(sss=="")
	{
		printf("%s\nEnter TRACE FLAG:%s",RED,RESETC);
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
		strcpy(s2,sss.c_str());


	  char *ch;
	  ch = strtok(s2, " ");
	  while (ch != NULL)
	  {
		  ss=string(ch);
		//  printf("[%s]\n", ch);
		  ch = strtok(NULL, " ");

		if(strcmp(ss.c_str(),"NONE")==0)
		{
			theConf.traceflag=0;
			write_to_flash();
			return;
		}

		if(strcmp(ss.c_str(),"ALL")==0)
		{
			theConf.traceflag=0xFFFF;
			write_to_flash();
			return;
		}

		int cualf=cmdfromstring((char*)ss.c_str());
		if(cualf<0)
		{
			printf("Invalid Debug Option\n");
			return;
		}
		if(cualf<NKEYS/2 )
		{
			printf("%s%s+ ",GREEN,lookuptable[cualf]);
			theConf.traceflag |= 1<<cualf;
			write_to_flash();
		}
		else
		{
			cualf=cualf-NKEYS/2;
			printf("%s%s- ",RED,lookuptable[cualf]);
			theConf.traceflag ^= (1<<cualf);
			write_to_flash();
		}

	  }
	  printf("%s\n",KBDT);
}


bool compareCmd(cmdRecord_t r1, cmdRecord_t r2)
{
	int a=strcmp(r1.comando,r2.comando);
	if (a<0)
		return true;
	else
		return false;
}

void showHelp(string ss)
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
	}
    int n = sizeof(cmdds)/sizeof(cmdds[0]);

    std::sort(cmdds, cmdds+n, compareCmd);

	printf("%s======CMDS======\n",RED);
	for (int a=0;a<MAXCMDSK;a++)
	{
		if(a % 2)
		{
			printf("%s%s ",CYAN,cmdds[a].comando);
			if (longf)
				printf("(%s)\n",cmdds[a].help.c_str());
		}
		else
		{
			printf("%s%s ",GREEN,cmdds[a].comando);
			if (longf)
				printf("(%s)\n",cmdds[a].help.c_str());
		}
	}
	printf("\n======CMDS======%s\n",RESETC);
}

static void printControllers(string ss)
{
	char str2[INET_ADDRSTRLEN];
	int antes,son;
	cJSON *params=NULL;
	bool shortans=true;

	params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"LONG");
		if(jmeter)
			shortans=false;
		cJSON_Delete(params);
	}

	printf("%sListing %d Controllers\n",KBDT,theConf.reservedCnt);

	if(!shortans)
	{
		printf("%s============%s\n",MAGENTA,RESETC);
		printf("%sLocal Meters%s\n",MAGENTA,RESETC);
		printf("%s============%s\n",MAGENTA,RESETC);
		for (int a=0;a<MAXDEVS;a++)
			printf("%sMeter[%d]=%s KwH %6d Beats %9d\n",a%2?CYAN:GREEN,a,theMeters[a].serialNumber,theMeters[a].curLife,theMeters[a].currentBeat);
		printf("\n");
	}
	printf("===============\n");
	printf("Configured MtMs\n");
	printf("===============\n");

	for(int a=0;a<theConf.reservedCnt;a++)
	{

		time_t t = time(NULL);
		struct tm * p = localtime(&t);
		strftime(tempb, 100, "%D %T ", p);

		inet_ntop( AF_INET,(in_addr*)&losMacs[a].theIp, str2, INET_ADDRSTRLEN );
		printf("%sSlot[%d]%s=%06x %sIP=%s %sState(#%d)%s%s%s(%s) GTask=%s%s %sHW=%s %sBorn=%s%s %sSeen%s@%s",GREEN,a,CYAN,theConf.reservedMacs[a],YELLOW, str2,WHITEC,
				losMacs[a].msgCount,GREEN,stateName[losMacs[a].dState],CYAN,losMacs[a].report==REPORTED? RED "Reported" RESETC:GREEN "OK" RESETC,
				losMacs[a].theHandle?LYELLOW "Alive":MAGENTA "Dead",RESETC,GRAY,losMacs[a].hwState?LRED "FAIL":LGREEN "OK",LYELLOW,RESETC,tempb,YELLOW,RESETC,ctime(&losMacs[a].lastUpdate));
		if(!shortans)
		{
			antes=0;
			printf("StateChanges:");
			for(int b=0;b<4;b++)
			{
				son=losMacs[a].stateChangeTS[b]-antes;
				printf("[%s]=%dms(%d) ",stateName[b],losMacs[a].stateChangeTS[b],son);
				antes=losMacs[a].stateChangeTS[b];
			}
				printf("\n");

					for (int b=0;b<MAXDEVS;b++)
						printf("%sMeter[%d]=%s KwH %6d Beats %9d\n",b%2?CYAN:GREEN,b,losMacs[a].meterSerial[b],losMacs[a].controlLastKwH[b],losMacs[a].controlLastBeats[b]);
		}
	}

	printf("%s\n",KBDT);
}

static void sendTelemetry(string ss)
{
	printf("%sSend Telemetry\n",KBDT);
	xTaskCreate(&connMgr,"cnmgr",4096,(void*)123, 4, NULL);
}

static void tariffs(string ss)
{
	const char *title="YearDay:";
	int err=0;
	cJSON *params=NULL;


	params=makeJson(ss);
	int cuando=askValue(title,params);
	if(cuando<0)
		return;

	err=fram.read_tarif_day(cuando,(u8*)&tarifasDia);
	if(err!=0)
		printf("LoadTar Error %d reading Tariffs day %d...recovering from Host\n",err,cuando);
	printf("Tariffs ");
	for (int a=0;a<24;a++)
		printf("0x%04x ",tarifasDia[a]);
	printf("\n");
	err=fram.read_tarif_day(yearDay,(u8*)&tarifasDia);
		if(err!=0)
			printf("LoadTar Error %d reading Tariffs day %d...recovering from Host\n",err,yearDay);
}

static void firmware(string ss)
{
	xTaskCreate(&firmUpdate,"U571",10240,NULL, 5, NULL);

}

//static void print_auth_mode(int authmode)
//{
//    switch (authmode) {
//    case WIFI_AUTH_OPEN:
//        printf( "%sAuthmode \tWIFI_AUTH_OPEN%s\n",YELLOW,RESETC);
//        break;
//    case WIFI_AUTH_WEP:
//        printf( "%sAuthmode \tWIFI_AUTH_WEP%s\n",YELLOW,RESETC);
//        break;
//    case WIFI_AUTH_WPA_PSK:
//        printf( "%sAuthmode \tWIFI_AUTH_WPA_PSK%s\n",YELLOW,RESETC);
//        break;
//    case WIFI_AUTH_WPA2_PSK:
//        printf( "%sAuthmode \tWIFI_AUTH_WPA2_PSK%s\n",YELLOW,RESETC);
//        break;
//    case WIFI_AUTH_WPA_WPA2_PSK:
//        printf( "%sAuthmode \tWIFI_AUTH_WPA_WPA2_PSK%s\n",YELLOW,RESETC);
//        break;
//    case WIFI_AUTH_WPA2_ENTERPRISE:
//        printf( "%sAuthmode \tWIFI_AUTH_WPA2_ENTERPRISE%s\n",YELLOW,RESETC);
//        break;
//    default:
//        printf( "%sAuthmode \tWIFI_AUTH_UNKNOWN%s\n",YELLOW,RESETC);
//        break;
//    }
//}
//
//static void print_cipher_type(int pairwise_cipher, int group_cipher)
//{
//    switch (pairwise_cipher) {
//    case WIFI_CIPHER_TYPE_NONE:
//    	printf("%sPairw Cipher \tWIFI_CIPHER_TYPE_NONE%s\n",YELLOW,RESETC);
//        break;
//    case WIFI_CIPHER_TYPE_WEP40:
//    	printf("%sPairw Cipher \tWIFI_CIPHER_TYPE_WEP40%s\n",YELLOW,RESETC);
//        break;
//    case WIFI_CIPHER_TYPE_WEP104:
//    	printf("%sPairw Cipher \tWIFI_CIPHER_TYPE_WEP104%s\n",YELLOW,RESETC);
//        break;
//    case WIFI_CIPHER_TYPE_TKIP:
//    	printf("%sPairw Cipher \tWIFI_CIPHER_TYPE_TKIP%s\n",YELLOW,RESETC);
//        break;
//    case WIFI_CIPHER_TYPE_CCMP:
//    	printf("%sPairw Cipher \tWIFI_CIPHER_TYPE_CCMP%s\n",YELLOW,RESETC);
//        break;
//    case WIFI_CIPHER_TYPE_TKIP_CCMP:
//    	printf("%sPairw Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP%s\n",YELLOW,RESETC);
//        break;
//    default:
//    	printf("%sPairw Cipher \tWIFI_CIPHER_TYPE_UNKNOWN%s\n",YELLOW,RESETC);
//        break;
//    }
//
//    switch (group_cipher) {
//    case WIFI_CIPHER_TYPE_NONE:
//    	printf("%sGroup Cipher \tWIFI_CIPHER_TYPE_NONE%s\n",YELLOW,RESETC);
//        break;
//    case WIFI_CIPHER_TYPE_WEP40:
//    	printf("%sGroup Cipher \tWIFI_CIPHER_TYPE_WEP40%s\n",YELLOW,RESETC);
//        break;
//    case WIFI_CIPHER_TYPE_WEP104:
//    	printf("%sGroup Cipher \tWIFI_CIPHER_TYPE_WEP104%s\n",YELLOW,RESETC);
//        break;
//    case WIFI_CIPHER_TYPE_TKIP:
//    	printf("%sGroup Cipher \tWIFI_CIPHER_TYPE_TKIP%s\n",YELLOW,RESETC);
//        break;
//    case WIFI_CIPHER_TYPE_CCMP:
//    	printf( "%sGroup Cipher \tWIFI_CIPHER_TYPE_CCMP%s\n",YELLOW,RESETC);
//        break;
//    case WIFI_CIPHER_TYPE_TKIP_CCMP:
//    	printf( "%sGroup Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP%s\n",YELLOW,RESETC);
//        break;
//    default:
//    	printf("%sGroup Cipher \tWIFI_CIPHER_TYPE_UNKNOWN%s\n",YELLOW,RESETC);
//        break;
//    }
//}

//static void scan()
//{
//	 	uint16_t number = 10;
//	    wifi_ap_record_t ap_info[10];
//	    uint16_t ap_count = 0;
//	    memset(ap_info, 0, sizeof(ap_info));
//	    miscanf=true;
//	    ESP_ERROR_CHECK(esp_wifi_stop());
//	    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//	    esp_wifi_start();
//	   	ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));
//	    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
//	    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
//	    printf("%sTotal APs scanned = %u%s\n",RED, ap_count,RESETC);
//	    for (int i = 0; (i < 10) && (i < ap_count); i++) {
//	        printf("%sSSID%s \t\t%s\n",GREEN,RESETC, ap_info[i].ssid);
//	        printf("%sRSSI%s\t\t%d\n", CYAN,RESETC,ap_info[i].rssi);
//	        print_auth_mode(ap_info[i].authmode);
//	        if (ap_info[i].authmode != WIFI_AUTH_WEP) {
//	            print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
//	        }
//	        printf( "Channel \t%d\n", ap_info[i].primary);
//	    }
//
//	    miscanf=false;
//	    ESP_ERROR_CHECK(esp_wifi_stop());
//	    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
//	    esp_wifi_start();
//	    clientCloud = esp_mqtt_client_init(&mqtt_cfg);
//}

void eraseTariff(string ss)
{
	fram.erase_tarif();
	printf("%sTariffs erased\n",KBDT);
}

static void sha256(string ss)
{
	char s1[20];
	printf("Enter SHA256 key:");
	fflush(stdout);
	int fueron=get_string(UART_NUM_0,10,s1);
	uint8_t  shares[32];
	shaMake(s1,fueron,(uint8_t*)&shares);
	for(int a=0;a<32;a++)
		printf("%02x",shares[a]);
	printf("\n");
}

static void clearWL(string ss)
{
	char s1[20];
	printf("%sAre you sure clear Whitelist?:%s",MAGENTA,RESETC);
	fflush(stdout);
	int fueron=get_string(UART_NUM_0,10,s1);
	if(fueron<0)
		return;
	memset(theConf.reservedMacs,0,sizeof(theConf.reservedMacs));
	theConf.reservedCnt=0;
	write_to_flash();
	printf("White List Cleared\n");
}

static void WhiteList(string ss)
{
	char s1[20];
	int pos=-1;

	cJSON *params=makeJson(ss);

	if(params)
	{
		cJSON *jmeter= cJSON_GetObjectItem(params,"POS");
		if(jmeter)
			pos=jmeter->valueint;
		cJSON_Delete(params);
	}
	else
	{
		printf("%sPosition to Delete:%s",MAGENTA,RESETC);
		fflush(stdout);
		pos=get_string(UART_NUM_0,10,s1);
		if(pos<0)
			return;
	}
	if(pos<0 || pos>theConf.reservedCnt-1)
	{
		printf("Pos %d Out of Range(%d)\n",pos,theConf.reservedCnt);
		return;
	}
	int son=(MAXSTA-(pos+1))*sizeof(uint32_t);
	if(pos<MAXSTA-1)
		memmove(&theConf.reservedMacs[pos],&theConf.reservedMacs[pos+1],son);
	memset(&theConf.reservedMacs[MAXSTA-1],0,sizeof(theConf.reservedMacs[0]));

	theConf.reservedCnt--;
	write_to_flash();
	printf("WhiteList Entry %d Cleared Count %d\n",pos,theConf.reservedCnt);
}

void init_kbd_commands()
{
	strcpy((char*)&cmdds[0].comando,"Config");			cmdds[ 0].code=confStatus;			cmdds[0].help="SHORT";
	strcpy((char*)&cmdds[1].comando,"WebReset");		cmdds[ 1].code=webReset;			cmdds[1].help="";
	strcpy((char*)&cmdds[2].comando,"Controllers");		cmdds[ 2].code=printControllers;	cmdds[2].help="LONG";
	strcpy((char*)&cmdds[3].comando,"MeterStat");		cmdds[ 3].code=meterStatus;			cmdds[3].help="METER";
	strcpy((char*)&cmdds[4].comando,"EraseTariff");		cmdds[ 4].code=eraseTariff;			cmdds[4].help="";
	strcpy((char*)&cmdds[5].comando,"MeterCount");		cmdds[ 5].code=meterCount;			cmdds[5].help="";
	strcpy((char*)&cmdds[6].comando,"DumpCore");		cmdds[ 6].code=dumpCore;			cmdds[6].help="";
	strcpy((char*)&cmdds[7].comando,"FormatFram");		cmdds[ 7].code=formatFram;			cmdds[7].help="VAL";
	strcpy((char*)&cmdds[8].comando,"ReadFram");		cmdds[ 8].code=readFram;			cmdds[8].help="COUNT";
	strcpy((char*)&cmdds[9].comando,"WriteFram");		cmdds[ 9].code=writeFram;			cmdds[9].help="ADDR VAL";
	strcpy((char*)&cmdds[10].comando,"LogLevel");		cmdds[10].code=logLevel;			cmdds[10].help="None Info Err Verb Warn";
	strcpy((char*)&cmdds[11].comando,"FramDaysAll");	cmdds[11].code=framDays;			cmdds[11].help="METER MONTH";
	strcpy((char*)&cmdds[12].comando,"FramHour");		cmdds[12].code=framHourSearch;		cmdds[12].help="METER MONTH DAY HOUR";
	strcpy((char*)&cmdds[13].comando,"FramDay");		cmdds[13].code=framDaySearch;		cmdds[13].help="METER MONTH";
	strcpy((char*)&cmdds[14].comando,"FramMonth");		cmdds[14].code=framMonthSearch;		cmdds[14].help="METER MONTH";
	strcpy((char*)&cmdds[15].comando,"Flush");			cmdds[15].code=flushFram;			cmdds[15].help="";
	strcpy((char*)&cmdds[16].comando,"MessageCount");	cmdds[16].code=msgCount;			cmdds[16].help="";
	strcpy((char*)&cmdds[17].comando,"Help");			cmdds[17].code=showHelp;			cmdds[17].help="LONG";
	strcpy((char*)&cmdds[18].comando,"Trace");			cmdds[18].code=traceFlags;
	cmdds[18].help="NONE ALL BOOTD WIFID MQTTD PUBSUBD OTAD CMDD WEBD GEND MQTTT FRMCMD INTD FRAMD MSGD TIMED SIMD HOSTD";
	strcpy((char*)&cmdds[19].comando,"FramMonthsAll");	cmdds[19].code=framMonths;			cmdds[19].help="METER";
	strcpy((char*)&cmdds[20].comando,"Telemetry");		cmdds[20].code=sendTelemetry;		cmdds[20].help="";
	strcpy((char*)&cmdds[21].comando,"Tariff");			cmdds[21].code=tariffs;				cmdds[21].help="";
	strcpy((char*)&cmdds[22].comando,"Firmware");		cmdds[22].code=firmware;			cmdds[22].help="";
	strcpy((char*)&cmdds[23].comando,"Sha256");			cmdds[23].code=sha256;				cmdds[23].help="";
	strcpy((char*)&cmdds[24].comando,"ClearWL");		cmdds[24].code=clearWL;				cmdds[24].help="";
	strcpy((char*)&cmdds[25].comando,"WhiteList");		cmdds[25].code=WhiteList;			cmdds[25].help="POS";
}

void kbd(void *arg)
{
	int len,cualf;
	uart_port_t uart_num = UART_NUM_0 ;
	char kbdstr[100],oldcmd[100];
	string ss;
	std::locale loc;


	//cJSON *params;

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
		printf("Error UART Install %d\n",err);

	init_kbd_commands();

	while(1)
	{
			len=get_string(UART_NUM_0,10,kbdstr);
			if(len<=0)
				strcpy(kbdstr,oldcmd);
			ss=string(kbdstr);

			 size_t found = ss.find(" ");
			  if (found!=std::string::npos)
			  {
				  strcpy(kbdstr,ss.substr(0,found).c_str());	//Just cmd
				  ss.erase(0,found+1);
				  ss+=" ";
			  }
			  else ss="";

			  for (size_t i=0; i<ss.length(); ++i)
			  			ss[i]= std::toupper(ss[i],loc);


			cualf=kbdCmd(string(kbdstr));
			if(cualf>=0)
			{
				(*cmdds[cualf].code)(ss);
				strcpy(oldcmd,kbdstr);
			}
		//	if(params)
		//		cJSON_Delete(params);

		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}

