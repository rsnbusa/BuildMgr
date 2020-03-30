/*
 * displayMananger.cpp
 *
 *  Created on: Apr 18, 2017
 *      Author: RSN
 */
#include "defines.h"
#ifdef DISPLAY
#include "displayManager.h"

extern void delay(uint32_t son);
extern int sendTelemetryCmd(parg *arg);
extern void pprintf(const char * format, ...);

static uint32_t getRandom(uint32_t startw, uint32_t endw)
{
	uint32_t rand;
	do
	{
		rand=esp_random();

	} while(!(rand>=startw && rand<=endw));

	return rand;
}

static void check_delivery_date(void *pArg)
{
#define MULT	1

	time_t now;
	struct tm timeinfo;
	uint32_t nowsecs,randsecs,waitfornextW;
	uint32_t windows[NUMTELSLOTS][2];
	int8_t windNow=-1;
	int res=0,retcount=0;

	int slice=86400/NUMTELSLOTS;
	for (int a=0;a<NUMTELSLOTS;a++)
	{
		windows[a][0]=res;
		res+=slice-1;
		windows[a][1]=res;
		res++;
	}

	time(&now);
	localtime_r(&now, &timeinfo);
	nowsecs=timeinfo.tm_hour*3600+timeinfo.tm_min*60+timeinfo.tm_sec;
	for(int a=0;a<NUMTELSLOTS;a++)
	{
		if(nowsecs>=windows[a][0] && nowsecs<=windows[a][1])
		{
			windNow=a;
			break;
		}
	}
	if(windNow<0)
	{
		pprintf("Internal Error Window\n");
		while(1)
			delay(1000);
	}
	while(1)
	{
		if(!(theConf.pause & (1<<PTEL)))
		{
			randsecs=getRandom(windows[windNow][0],windows[windNow][1]);
			waitfornextW=windows[windNow][1]-randsecs;
			time_t when;
			time(&when);
			when+=randsecs-windows[windNow][0];

	#ifdef DEBUGX
			if(theConf.traceflag & (1<<TIMED))
				pprintf("%sNext window [%d] Min %d Max %d delay %u wait %u heap %u MqttDelay %d Fire at %s",TIEMPOT,windNow,windows[windNow][0],windows[windNow][1],randsecs-windows[windNow][0],
					waitfornextW,esp_get_free_heap_size(),MQTTDelay,ctime(&when));
	#endif
			if(MQTTDelay<0)
				delay((randsecs-windows[windNow][0])*MULT);//wait within window
			else
				delay(MQTTDelay);

			//send telemetry
			res=sendTelemetryCmd(NULL);
			if(res!=ESP_OK)
			{
				//failed. MQTT service probable disconnected
				// retry until you can with a 30 secs delay
				retcount=0;
				while(1)
				{
					pprintf("Retrying %d\n",res);
					retcount++;
					delay(3000);
					res=sendTelemetryCmd(NULL);
					if(res==ESP_OK)
						break;
					int son=uxQueueMessagesWaiting(mqttQ);
					if(son>5)
					{
						pprintf("Freeing MqttQueue %d\n",son);
						xQueueReset(mqttQ);//try to free queue
					}
					if(retcount>30)
					{
						pprintf("Lost contact Mqtt. Restarting\n");
						delay(1000);
						esp_restart();
					}
				}
			}
			windNow++;
			if(windNow>NUMTELSLOTS-1)
				windNow=0;
			delay(waitfornextW*MULT);		//wait for Next Window
	}
		else
			delay(1000);
	}
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
	int yy=1,count=0;
	string sts="";
	while(count<theConf.reservedCnt)
	{
		sprintf(textt," %d ",(int)losMacs[count].msgCount);
	//	sprintf(textt,"%d ",9999);
		sts+=string(textt);
		count++;
		if(count %3==0)
		{
			drawString(64, yy,sts, 12, TEXT_ALIGN_CENTER,DISPLAYIT, REPLACE);
			sts="";
			yy+=13;
		}
	}
	if(sts!="")
		drawString(64, yy,sts, 12, TEXT_ALIGN_CENTER,DISPLAYIT, REPLACE);

}

void displayManager(void *arg) {
	time_t t = 0;
	struct tm timeinfo ;
	char textd[20],textt[20];
//	time_t cuando;
//	int pasaron;

	uint8_t displayMode=0;// kwh

	time(&t);
	localtime_r(&t, &timeinfo);

	xTaskCreate(&check_delivery_date,"telem",4096,NULL, 10, NULL);

//	bool ok=check_delivery_date(&pasaron,diag);
//	if(ok)
//	{
//		if(pasaron<0)
//		{
//			//in the past, Check if this month has been processed
//			fram.read_cycle(mesg,(uint8_t*)&cuando);
//			if(cuando>0 && timeinfo.tm_mon==mesg)
//			{
//				printf("Month processed %s...continuing\n",ctime((time_t*)&cuando));
//			}
//			else
//			{
//				//shit. something is wrong
//				printf("In the past and not logged\n");
//			}
//		}
//		else
//		{
//			pasaron*=1000;
//			printf("Start connmgr with time %d\n",pasaron);
//			xTaskCreate(&connMgr,"conmmgr",4096,(void*)pasaron, 10, &connHandle);// Messages from the Meters. Controller Section socket manager
//			//start connMgr task with delay pasaron
//		}
//	}// else not our day

	memset(oldCurBeat,0,sizeof(oldCurBeat));
	memset(oldCurLife,0,sizeof(oldCurLife));
	if(workingDevs)
		clearScreen();

	int howLong=1000;

	while(true)
	{
		time(&mgrTime[DISPLAYMGR]);
		vTaskDelay(howLong/portTICK_PERIOD_MS);
//		if(!gpio_get_level((gpio_num_t)0))
//		{
//			displayMode++;
//			if(displayMode>2)
//				displayMode=0;
//			memset(oldCurBeat,0,sizeof(oldCurBeat));
//			memset(oldCurLife,0,sizeof(oldCurLife));
//			clearScreen();
//		}

		time(&t);
		localtime_r(&t, &timeinfo);

		sprintf(textd,"%02d/%02d/%04d",timeinfo.tm_mday,timeinfo.tm_mon+1,1900+timeinfo.tm_year);
		sprintf(textt,"%02d:%02d:%02d",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
		if(xSemaphoreTake(I2CSem, portMAX_DELAY))
		{
			drawString(0, 51, string(textd), 10, TEXT_ALIGN_LEFT,NODISPLAY, REPLACE);
			drawString(86, 51, string(textt), 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
			switch(theConf.displayMode)
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
