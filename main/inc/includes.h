
#ifndef includes_h
#define includes_h

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <string>

extern "C"{
#include "esp_wifi.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/i2c.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"
#include "lwip/err.h"

#include "ds18b20.h"
#include "owb.h"
#include "owb_rmt.h"

#include "I2C.h"
#include "SSD1306.h"

#include "mqtt_client.h"

#include <inttypes.h>
#include "esp_sntp.h"

void app_main();
}
//#include <algorithm>
#endif
