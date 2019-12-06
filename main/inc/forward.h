#ifndef fw
#define fw
void set_FirmUpdateCmd(void *pArg);               // FOTA of latest firmware
void set_eraseConfig(void *pArg);                // Erase everything. Back to local AP (192.168.4.1)
void set_statusSend(void *pArg);                  // A general status condition for display. See routine for numbers.
void set_reset(void *pArg);                       // Reboot
void set_resetstats(void *pArg);                  // Clear log file and 0 Heater stats regarding power and water consumption
void set_rates(void *pArg);                       // Refresh rates. Used internally for debugging
void set_mqtt(void *pArg);;                        // Set another MQTT server. Internal
void set_internal(void *pArg);                    // Set internal variables by hand. SSID, PASSWORD and Heater name. Debuging. Should be commented at release
void set_addEmail(void *pArg);     // Change Notification On or Off for a Timer
void set_getMonth(void *pArg);
void set_getDay(void *pArg);
void set_getHour(void *pArg);
void set_getMonthAll(void *pArg);
void set_getDayAll(void *pArg);
void set_getDaysInMonth(void *pArg);
void set_getHoursInDay(void *pArg);
void set_getHoursInMonth(void *pArg);
void set_getCycle(void *pArg);
void set_getCycleDate(void *pArg);
void set_scanCmd(void *pArg);
void set_conection(void *pArg);
void set_displayMeter(void *pArg);
void set_displayManager(void *pArg);
void set_framManager(void *pArg);
void set_settingsStatus(void *pArg);
void set_payment(void *pArg);
void set_tariff(void *pArg);
void set_generalap(void *pArg);
void checkDate(void *arg);
void logManager(void *arg);
void set_clearLog(void *arg);
void set_readlog(void *arg);
void set_session(void *arg);

#endif
