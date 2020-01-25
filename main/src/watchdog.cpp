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

			switch(losMacs[a].dState)
			{
				case BOOTSTATE:
					if((now-losMacs[a].stateChangeTS[BOOTSTATE])>TSTATE1)
					{
						if(losMacs[a].report!=REPORTED)
						{
							printf("NO Connection from %x Time Elapsed %d Report Host\n",theConf.reservedMacs[a],now-losMacs[a].stateChangeTS[BOOTSTATE]);
							losMacs[a].report=REPORTED;
						}

					}
					break;
				case CONNSTATE:
					if(losMacs[a].report==REPORTED)
					{
						printf("Connection from %x \n",theConf.reservedMacs[a]);
						losMacs[a].report=NOREPORT;
						losMacs[a].stateChangeTS[CONNSTATE]=now;
						break;
					}

					if((now-losMacs[a].stateChangeTS[CONNSTATE])>TSTATE2)
					{
						if(losMacs[a].report!=REPORTED)
						{
							printf("NO Login from %x Time Elapsed %d Report Host\n",theConf.reservedMacs[a],now-losMacs[a].stateChangeTS[CONNSTATE]);
							losMacs[a].report=REPORTED;
						}

					}
					break;
				case MSGSTATE:
					if(losMacs[a].report==REPORTED)
					{
						printf("Message received from %x \n",theConf.reservedMacs[a]);
						losMacs[a].report=NOREPORT;
						losMacs[a].stateChangeTS[MSGSTATE]=now;
						break;
					}

					if((now-losMacs[a].stateChangeTS[MSGSTATE])>TSTATE3)
					{
						if(losMacs[a].report!=REPORTED)
						{
							printf("NO Msg from %x Time Elapsed %d Report Host\n",theConf.reservedMacs[a],now-losMacs[a].stateChangeTS[MSGSTATE]);
							losMacs[a].report=REPORTED;
							losMacs[a].dState=TOSTATE;
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
