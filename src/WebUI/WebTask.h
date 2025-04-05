#ifndef _WEBTASK
#define _WEBTASK

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace WebUI {
    class WebTask {
    public:
        WebTask() {}
        static void setup(WebServer* _server);
        static void processQueueTask(void *parameter);
        static void handleCommandRequest();
        static void handleWaitUntilDone();
    protected:
        static QueueHandle_t commandQueue;
        static bool processingCommand;
        static WebServer* server;
        static TaskHandle_t processTaskHandle;
    };
}

#endif
