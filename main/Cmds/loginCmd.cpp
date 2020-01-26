/*
 * loginCmd.cpp
 *
 *  Created on: Jan 26, 2020
 *      Author: rsn
 */

#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern uint32_t millis();


void loginCmd(parg* argument)
{
	loginT loginData;

	if(losMacs[argument->pos].dState>LOGINSTATE)
		printf("Forcing login state MAC %x state %s seen %s",(u32)argument->macn,stateName[losMacs[argument->pos].dState],ctime(&losMacs[argument->pos].lastUpdate));

	losMacs[argument->pos].dState=LOGINSTATE;
	losMacs[argument->pos].stateChangeTS[LOGINSTATE]=millis();

	time(&loginData.thedate);
	loginData.theTariff=100;
	send(argument->pComm, &loginData, sizeof(loginData), 0);
}
