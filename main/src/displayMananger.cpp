/*
 * displayMananger.cpp
 *
 *  Created on: Apr 18, 2017
 *      Author: RSN
 */
#include "defines.h"
#ifdef DISPLAY
#include "displayManager.h"

extern void connMgr(void* pArg);

bool check_delivery_date(int *cuanto,uint8_t eldia)
{
	time_t now,t;
	struct tm timeinfo;

	if (eldia==theConf.connId.dDay)
	{
		time(&now);
		localtime_r(&now, &timeinfo);
		timeinfo.tm_hour=0;
		timeinfo.tm_min=0;
		timeinfo.tm_sec=0;
		t=mktime ( &timeinfo );//Today at 0:0:0
			//its today we need to deliver Telemetry
			// time to delivery is slot_time*connSlot-faltan from 0 hours
		*cuanto=theConf.slot_Server.slot_time*theConf.connId.connSlot+t-now;
		//if negative in the Past should check if Sent
		printf("To Deliver today %d secs %d SlotTime %d connSlot %d conAlt %d conDDay %d Secsnow %d Now %d T %d\n",eldia,*cuanto,theConf.slot_Server.slot_time,theConf.connId.connSlot,
				theConf.connId.altDay,theConf.connId.dDay,(int)(now-t),(int)now,(int)t);
		return true;// its our day and *cuanto time to send metrics
	}
	return false;
}

void clearScreen()
{
	display.setColor(BLACK);
	display.clear();
	display.setColor(WHITE);
}

void drawString(int x, int y, string que, int fsize, int align,displayType showit,overType erase)
{
	if(fsize!=lastFont)
	{
		lastFont=fsize;
		switch (fsize)
		{
		case 10:
			display.setFont(ArialMT_Plain_10);
			break;
		case 12:
			display.setFont(Dialog_plain_12);
			break;
		case 14:
			display.setFont(Dialog_plain_14);
			break;
		case 16:
			display.setFont(ArialMT_Plain_16);
			break;
		case 24:
			display.setFont(ArialMT_Plain_24);
			break;
		default:
			break;
		}
	}

	if(lastalign!=align)
	{
		lastalign=align;

		switch (align) {
		case TEXT_ALIGN_LEFT:
			display.setTextAlignment(TEXT_ALIGN_LEFT);
			break;
		case TEXT_ALIGN_CENTER:
			display.setTextAlignment(TEXT_ALIGN_CENTER);
			break;
		case TEXT_ALIGN_RIGHT:
			display.setTextAlignment(TEXT_ALIGN_RIGHT);
			break;
		default:
			break;
		}
	}

	if(erase==REPLACE)
	{
		int w=display.getStringWidth((char*)que.c_str());
		int xx=0;
		switch (lastalign) {
		case TEXT_ALIGN_LEFT:
			xx=x;
			break;
		case TEXT_ALIGN_CENTER:
			xx=x-w/2;
			break;
		case TEXT_ALIGN_RIGHT:
			xx=x-w;
			break;
		default:
			break;
		}
		display.setColor(BLACK);
		display.fillRect(xx,y,w,lastFont+3);
		display.setColor(WHITE);
	}

	display.drawString(x,y,(char*)que.c_str());
	if (showit==DISPLAYIT)
		display.display();
}

static void displayBeats()
{
	uint8_t posx[MAXDEVS]={0,70,38,0,70},posy[MAXDEVS]={1,1,20,40,40};
	char textt[20];
	uint16_t count;

	for(int a=0;a<MAXDEVS;a++)
	{
		if(theMeters[a].currentBeat>oldCurBeat[a])
		{
			pcnt_get_counter_value((pcnt_unit_t)a,(short int *) &count);
			sprintf(textt," %5d-%d  ",theMeters[a].currentBeat,count);
			drawString(posx[a], posy[a],string(textt), 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
			oldCurBeat[a]=theMeters[a].currentBeat;
		}
	}
}

static void displayKwH()
{

	char textt[20];
	uint8_t posx[MAXDEVS]={0,65,35,0,65},posy[MAXDEVS]={1,1,19,36,36};
	uint16_t count;

	for(int a=0;a<MAXDEVS;a++)
	{
			pcnt_get_counter_value((pcnt_unit_t)a,(short int *) &count);
			sprintf(textt," %5d-%d  ",theMeters[a].curLife,count);
			drawString(posx[a], posy[a],string(textt), 14, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
	}
}

static void displayActivity()
{

	char textt[20];
	int yy=1;

	for(int a=0;a<theConf.reservedCnt;a++)
	{
		sprintf(textt,"%d",(int)losMacs[a].msgCount);
		drawString(64, yy,string(textt), 12, TEXT_ALIGN_CENTER,DISPLAYIT, REPLACE);
		yy+=14;
	}
}

void displayManager(void *arg) {
	time_t t = 0;
	struct tm timeinfo ;
	char textd[20],textt[20];
	time_t cuando;
	int pasaron;

	uint8_t displayMode=0;// kwh

	time(&t);
	localtime_r(&t, &timeinfo);

	bool ok=check_delivery_date(&pasaron,diag);
	if(ok)
	{
		if(pasaron<0)
		{
			//in the past, Check if this month has been processed
			fram.read_cycle(mesg,(uint8_t*)&cuando);
			if(cuando>0 && timeinfo.tm_mon==mesg)
			{
				printf("Month processed %s...continuing\n",ctime((time_t*)&cuando));
			}
			else
			{
				//shit. something is wrong
				printf("In the past and not logged\n");
			}
		}
		else
		{
			pasaron*=1000;
			printf("Start connmgr with time %d\n",pasaron);
			xTaskCreate(&connMgr,"conmmgr",4096,(void*)pasaron, 10, &connHandle);// Messages from the Meters. Controller Section socket manager
			//start connMgr task with delay pasaron
		}
	}// else not our day

	memset(oldCurBeat,0,sizeof(oldCurBeat));
	memset(oldCurLife,0,sizeof(oldCurLife));
	if(workingDevs)
		clearScreen();

	int howLong=1000;

	while(true)
	{
		time(&mgrTime[DISPLAYMGR]);

		vTaskDelay(howLong/portTICK_PERIOD_MS);
		if(!gpio_get_level((gpio_num_t)0))
		{
			displayMode++;
			if(displayMode>2)
				displayMode=0;
			memset(oldCurBeat,0,sizeof(oldCurBeat));
			memset(oldCurLife,0,sizeof(oldCurLife));
			clearScreen();
		}

		time(&t);
		localtime_r(&t, &timeinfo);

		sprintf(textd,"%02d/%02d/%04d",timeinfo.tm_mday,timeinfo.tm_mon+1,1900+timeinfo.tm_year);
		sprintf(textt,"%02d:%02d:%02d",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
		if(xSemaphoreTake(I2CSem, portMAX_DELAY))
		{
			drawString(0, 51, string(textd), 10, TEXT_ALIGN_LEFT,NODISPLAY, REPLACE);
			drawString(86, 51, string(textt), 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
			switch(displayMode)
			{
			case 0:
				howLong=1000;
				displayBeats();
				break;
			case 1:
				howLong=5000;
				displayKwH();
				break;
			case 2:
				howLong=10000;
				displayActivity();
				break;
			default:
				break;
			}
			xSemaphoreGive(I2CSem);
		}
	}
}
#endif
