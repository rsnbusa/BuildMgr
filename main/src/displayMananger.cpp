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
extern float DS_get_temp(DS18B20_Info * cual);

static void check_delivery_date(void *pArg)
{
#define MULT 1
	time_t 		now;
	tm 			timeinfo;
	uint32_t 	nowsecs,randsecs,waitfornextW,startw,endw;
	int8_t		windNow=-1;
	int 		res=0,retcount=0;
	time_t		midnight;

	srand((unsigned) time(&now));

	bzero(&timeinfo,sizeof(timeinfo));

	if(theConf.numSlices==0)
		theConf.numSlices=24*2;		//every hour

	int slice=86400/theConf.numSlices;

	time(&now);
	localtime_r(&now,&timeinfo);
	timeinfo.tm_hour=0;
	timeinfo.tm_min=0;
	timeinfo.tm_sec=0;
	midnight=mktime(&timeinfo);

	nowsecs=now-midnight;

	windNow=nowsecs/slice;

	startw	=nowsecs;
	endw	=windNow*slice+slice-1;
	randsecs=esp_random()%(endw-startw);
	waitfornextW=slice-randsecs;

	//Effectively start loop here
	delay(randsecs*MULT);

	//first setup marks the SLICE cycle= random delay + slice-delay=SLICE

	while(true)
	{
		delay(waitfornextW*MULT);
		randsecs=esp_random()%(slice);
		waitfornextW=slice-randsecs;

#ifdef DEBUGX
		if(theConf.traceflag & (1<<TIMED))
		{
			time(&now);
			pprintf("%sRandomDelay %d wait %u MqttDelay %d %s",TIEMPOT,randsecs,waitfornextW,MQTTDelay,ctime(&now));
		}
#endif
		if(MQTTDelay<0)
			delay(randsecs*MULT);//wait within window
		else
		{
			delay(MQTTDelay*MULT);
			waitfornextW=10;
		}

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
					pprintf("Definitely Lost contact Mqtt. Restarting\n");
					delay(1000);
					esp_restart();
				}
			}
		}
	}
}

void clearScreen()
{
	display.setColor(BLACK);
	display.clear();
	display.setColor(WHITE);
}

void drawString(int x, int y, char* que, int fsize, int align,displayType showit,overType erase)
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
		int w=display.getStringWidth((char*)que);
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

	display.drawString(x,y,(char*)que);
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
			drawString(posx[a], posy[a],textt, 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
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
			drawString(posx[a], posy[a],textt, 14, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
	}
}

static void displayActivity()
{

	char textt[20];
	int yy=1,count=0;
	char *sts=(char*)malloc(120);
	char *csts=sts;
	int len;
	bzero(sts,300);

	while(count<theConf.reservedCnt)
	{
		sprintf(textt," %d ",(int)losMacs[count].msgCount);
		strcpy(sts,textt);
		len=strlen(textt);
		sts+=len;
//	//	sprintf(textt,"%d ",9999);
//		sts+=string(textt);
		count++;
		if(count %3==0)
		{
			drawString(64, yy,sts, 12, TEXT_ALIGN_CENTER,DISPLAYIT, REPLACE);
			sts=csts;
			yy+=13;
			bzero(sts,120);
		}
	}
	if(strlen(sts)>0)
		drawString(64, yy,sts, 12, TEXT_ALIGN_CENTER,DISPLAYIT, REPLACE);

}

void displayManager(void *arg) {
	time_t t = 0;
	struct tm timeinfo ;
	char textd[20],textt[20];
//	time_t cuando;
//	int pasaron;

	time(&t);
	localtime_r(&t, &timeinfo);

	xTaskCreate(&check_delivery_date,"telem",4096,NULL, 10, NULL);

	memset(oldCurBeat,0,sizeof(oldCurBeat));
	memset(oldCurLife,0,sizeof(oldCurLife));
	if(workingDevs)
		clearScreen();

	int howLong=1000;

	if(xSemaphoreTake(I2CSem, portMAX_DELAY))
	{
		display.clear();
		xSemaphoreGive(I2CSem);
	}

	while(true)
	{
		time(&mgrTime[DISPLAYMGRT]);
		theTemperature=DS_get_temp(ds18b20_info);
		vTaskDelay(howLong/portTICK_PERIOD_MS);
		if(!gpio_get_level((gpio_num_t)0))
		{
			theConf.displayMode++;
			if(theConf.displayMode>2)
				theConf.displayMode=0;
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
			drawString(0, 51, textd, 10, TEXT_ALIGN_LEFT,NODISPLAY, REPLACE);
			drawString(86, 51, textt, 10, TEXT_ALIGN_LEFT,NODISPLAY, REPLACE);
			if(framOk)
				drawString(60, 51, (char*)"FG", 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
			else
				drawString(60, 51, (char*)"OK", 10, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
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
