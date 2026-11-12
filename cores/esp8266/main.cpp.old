#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "Arduino.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#ifndef CONFIG_ARDUINO_LOOP_STACK_SIZE
#define CONFIG_ARDUINO_LOOP_STACK_SIZE 4096
#endif
#pragma message("ARDUINO_LOOP_STACK_SIZE effectif = " STR(CONFIG_ARDUINO_LOOP_STACK_SIZE))


TaskHandle_t loopTaskHandle = NULL;
void loopTask(void *pvParameters)
{
    setup();
    while(true) {
        loop();
        if (serialEventRun) serialEventRun();
        vTaskDelay(1); // yield cpu to avoid watchdog timeout
    }
}


extern "C" void app_main()
{   
 xTaskCreateUniversal(loopTask, "loopTask", CONFIG_ARDUINO_LOOP_STACK_SIZE, NULL, 1, &loopTaskHandle, 0);
}

