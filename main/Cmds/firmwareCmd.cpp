/*
 * firmwareCmd.cpp
 *
 *  Created on: Jan 26, 2020
 *      Author: rsn
 */

#define GLOBAL

#include "includes.h"
#include "defines.h"
#include "projTypes.h"
#include "globals.h"

extern void ConfigSystem(void *pArg);
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
//extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");


void firmUpdate(void *pArg)
{
	esp_http_client_config_t config;
	memset(&config,0,sizeof(config));

//	config.url = OTA_BIN_FILE;
	config.url = "http://192.168.100.7:8080/buildMgrOled.bin";
	config .cert_pem = (char *)server_cert_pem_start;
	config.event_handler = NULL;

	config.skip_cert_common_name_check = true;	//for testing only

	printf("Ota begin\n");

	xTaskCreate(&ConfigSystem, "cfg", 1024, (void*)50, 3, &blinkHandle);// blink to show ota process is active

	esp_err_t ret = esp_https_ota(&config);
	if (ret == ESP_OK) {
		printf("Ota ended OK\n");
		esp_restart();
	}
	else
	{
		printf("Firmware failed\n");
		if(blinkHandle)
			vTaskDelete(blinkHandle);
		gpio_set_level((gpio_num_t)WIFILED, 0);
	}

	vTaskDelete(NULL);
}


//load firmware
void firmwareCmd(parg *pArg) //called by cmdManager
{
	// use task so as to keep registering pulses
	xTaskCreate(&firmUpdate,"U571",10240,NULL, 5, NULL);
}