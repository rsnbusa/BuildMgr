/*
 * displayMananger.cpp
 *
 *  Created on: Apr 18, 2017
 *      Author: RSN
 */
#include "defines.h"
#ifdef DISPLAY
#include "displayManager.h"

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
	uint8_t posx[MAXDEVS]={0,70,45,0,70},posy[MAXDEVS]={1,1,16,34,34};
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
	uint8_t posx[MAXDEVS]={0,70,35,0,70},posy[MAXDEVS]={1,1,16,34,34};
	uint16_t count;

	for(int a=0;a<MAXDEVS;a++)
	{
		//if(theMeters[a].curLife>oldCurLife[a])
		//{
			pcnt_get_counter_value((pcnt_unit_t)a,(short int *) &count);
			sprintf(textt," %5d-%d  ",theMeters[a].curLife,count);
			drawString(posx[a], posy[a],string(textt), 16, TEXT_ALIGN_LEFT,DISPLAYIT, REPLACE);
		//	oldCurLife[a]=theMeters[a].curLife;
		//}
	}
}

void displayManager(void *arg) {
	time_t t = 0;
	struct tm timeinfo ;
	char textd[20],textt[20];

	bool displayMode=true;// kwh

	memset(oldCurBeat,0,sizeof(oldCurBeat));
	memset(oldCurLife,0,sizeof(oldCurLife));
	clearScreen();

	while(true)
	{
		vTaskDelay(1000/portTICK_PERIOD_MS);
		if(!gpio_get_level((gpio_num_t)0))
		{
			displayMode=!displayMode;
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
			if(displayMode)
				displayBeats();
			else
				displayKwH();
			xSemaphoreGive(I2CSem);
		}
	}
}
#endif
