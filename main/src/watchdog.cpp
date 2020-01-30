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
		time(&mgrTime[WATCHMGR]);

		for (int a=0;a<theConf.reservedCnt;a++)
		{

			switch(losMacs[a].dState)
			{
				case BOOTSTATE:
					if((now-losMacs[a].stateChangeTS[BOOTSTATE])>TSTATE1)
					{
						if(losMacs[a].report!=REPORTED)
						{
#ifdef DEBUGX
							if(theConf.traceflag & (1<<CMDD))
								printf("%sNO Connection from %x Time Elapsed %d Report Host\n",CMDDT,theConf.reservedMacs[a],now-losMacs[a].stateChangeTS[BOOTSTATE]);
#endif
							losMacs[a].report=REPORTED;
						}

					}
					break;
				case CONNSTATE:
					if(losMacs[a].report==REPORTED)
					{
#ifdef DEBUGX
						if(theConf.traceflag & (1<<CMDD))
							printf("%sConnection from %x \n",CMDDT,theConf.reservedMacs[a]);
#endif
						losMacs[a].report=NOREPORT;
						losMacs[a].stateChangeTS[CONNSTATE]=now;
						break;
					}

					if((now-losMacs[a].stateChangeTS[CONNSTATE])>TSTATE2)
					{
						if(losMacs[a].report!=REPORTED)
						{
#ifdef DEBUGX
							if(theConf.traceflag & (1<<CMDD))
								printf("%sNO Login from %x Time Elapsed %d Report Host\n",CMDDT,theConf.reservedMacs[a],now-losMacs[a].stateChangeTS[CONNSTATE]);
#endif
							losMacs[a].report=REPORTED;
						}

					}
					break;
				case MSGSTATE:
					if(losMacs[a].report==REPORTED)
					{
#ifdef DEBUGX
						if(theConf.traceflag & (1<<CMDD))
							printf("%sMessage received from %x \n",CMDDT,theConf.reservedMacs[a]);
#endif
						losMacs[a].report=NOREPORT;
						losMacs[a].stateChangeTS[MSGSTATE]=now;
						break;
					}

					if((now-losMacs[a].stateChangeTS[MSGSTATE])>TSTATE3)
					{
						if(losMacs[a].report!=REPORTED)
						{
#ifdef DEBUGX
							if(theConf.traceflag & (1<<CMDD))
								printf("%sNO Msg from %x Time Elapsed %d Report Host\n",CMDDT,theConf.reservedMacs[a],now-losMacs[a].stateChangeTS[MSGSTATE]);
#endif
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
