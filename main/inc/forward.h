#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

#ifndef fw
#define fw
//External declarations. Routines are on its own source file
//-------------------------------------------------------------------------------
void init_temp();
void kbd(void *pArg);
void load_from_fram(u8 meter);
void loadDefaultTariffs();
void sntpget(void *pArgs);
void start_webserver(void* pArg);
void watchDog(void* pArg);
void write_to_fram(u8 meter,bool addit);
void write_to_fram(u8 meter,bool addit);
char *sendTelemetry();

int cmd_clearwl(parg *argument);
int cmd_sendbeats(parg *argument);
int cmd_sendday(parg *argument);
int cmd_senddm(parg *argument);
int cmd_senddy(parg *argument);
int cmd_sendkwh(parg *argument);
int cmd_sendmonth(parg *argument);
int cmd_sendmy(parg *argument);
int cmd_setbpk(parg *argument);
int cmd_setdelay(parg *argument);
int cmd_setdisplay(parg *argument);
int cmd_setLock(parg *argument);
int cmd_setSlice(parg *argument);
int cmd_zerokeys(parg *argument);
int loadit(parg *pArg);
int sendHostCmd(parg *argument);
int cmd_rsvp(parg *argument);

#ifdef DISPLAY
void displayManager(void * pArg);
void drawString(int x, int y, char * que, int fsize, int align,displayType showit,overType erase);
#endif

// ------------------------------------------------------------------------------
// CMDS
int aes_decrypt(const char* src, size_t son, char *dst,const unsigned char *cualKey);
int firmwareCmd(parg *pArg);
int loginCmd(parg* argument);
int statusCmd(parg *argument);
//------------------------------------------------------------------------------

#endif
