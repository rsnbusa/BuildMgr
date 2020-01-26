/*
 * dsTemp.cpp
 *
 *  Created on: Jan 26, 2020
 *      Author: rsn
 */

#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

#ifdef TEMP
float DS_get_temp(DS18B20_Info * cual)
{
    ds18b20_convert_all(owb);
    ds18b20_wait_for_conversion(ds18b20_info);
    float dtemp;
    DS18B20_ERROR err= ds18b20_read_temp(cual, &dtemp);
    if(err)
    	return -1;
    else
    return dtemp;
}


static void init_temp()
{

	  owb = owb_rmt_initialize(&rmt_driver_info, DSPIN, RMT_CHANNEL_1, RMT_CHANNEL_0);
	  owb_use_crc(owb, true);  // enable CRC check for ROM code

	  numsensors = 0;
	  OneWireBus_ROMCode rom_code;
	  owb_status status = owb_read_rom(owb, &rom_code);
	  if (status == OWB_STATUS_OK)
	  {
		char rom_code_s[OWB_ROM_CODE_STRING_LENGTH];
		owb_string_from_rom_code(rom_code, rom_code_s, sizeof(rom_code_s));
//		printf("Single device %s present\n", rom_code_s);
		ds18b20_info = ds18b20_malloc();
		ds18b20_init_solo(ds18b20_info, owb);
		ds18b20_use_crc(ds18b20_info, true);           // enable CRC check for temperature readings
		ds18b20_set_resolution(ds18b20_info, DS18B20_RESOLUTION_12_BIT);
	  }
	  float tt=DS_get_temp(ds18b20_info);
	  printf("Temp %.1f\n",tt);
	  numsensors=1;


}
#endif


