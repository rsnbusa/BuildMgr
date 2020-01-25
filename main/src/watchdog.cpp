#define GLOBAL
#include "defines.h"
#include "includes.h"
#include "globals.h"
#include "projTypes.h"

extern void delay(uint32_t cuanto);
extern uint32_t  millis();

void watchDog(void * pArg)
{

	uint32_t now;

	while(true)
	{
		delay(100);			//minimum delay
		now=millis();

		for (int a=0;a<theConf.reservedCnt;a++)
		{

			switch(whitelist[a].dState)
			{
				case BOOTSTATE:
					if((now-whitelist[a].seen)>TSTATE1)
					{
						if(whitelist[a].report!=REPORTED)
						{
							printf("NO Connection from %x Time Elapsed %d Report Host\n",theConf.reservedMacs[a],now-whitelist[a].seen);
							whitelist[a].report=REPORTED;
						}

					}
					break;
				case CONNSTATE:
					if(whitelist[a].report==REPORTED)
					{
						printf("Connection from %x \n",theConf.reservedMacs[a]);
						whitelist[a].report=NOREPORT;
						whitelist[a].seen=now;
						break;
					}

					if((now-whitelist[a].seen)>TSTATE2)
					{
						if(whitelist[a].report!=REPORTED)
						{
							printf("NO Login from %x Time Elapsed %d Report Host\n",theConf.reservedMacs[a],now-whitelist[a].seen);
							whitelist[a].report=REPORTED;
						}

					}
					break;
				case MSGSTATE:
					if(whitelist[a].report==REPORTED)
					{
						printf("Message received from %x \n",theConf.reservedMacs[a]);
						whitelist[a].report=NOREPORT;
						whitelist[a].seen=now;
						break;
					}

					if((now-whitelist[a].seen)>TSTATE3)
					{
						if(whitelist[a].report!=REPORTED)
						{
							printf("NO Msg from %x Time Elapsed %d Report Host\n",theConf.reservedMacs[a],now-whitelist[a].seen);
							whitelist[a].report=REPORTED;
							whitelist[a].dState=TOSTATE;
						}

					}
					break;
				case TOSTATE:
					break; 	// used to know its has this state
				default:
					break;
			}
		}
	}
}
