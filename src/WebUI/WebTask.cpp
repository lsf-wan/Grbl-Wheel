#include "../Grbl.h"
#include "WebTask.h"
#include <WebServer.h>

namespace WebUI {

QueueHandle_t WebTask::commandQueue;
bool WebTask::processingCommand = false;
WebServer* WebTask::server = NULL;
TaskHandle_t WebTask::processTaskHandle = NULL;

const int cmdSize = 120;
struct CmdStruct{
  char command[cmdSize];
};

// Task to process G-code commands from the queue
void WebTask::processQueueTask(void *parameter) {
    while (true) {
        CmdStruct obj;
        char cmd[cmdSize];
        if (!processingCommand &&
            uxQueueMessagesWaiting(commandQueue) > 0 &&
            xQueueReceive(commandQueue, &obj, portMAX_DELAY) == pdTRUE) {
            // wait until it is idle
            while (sys.state != State::Idle) {
              vTaskDelay(10 / portTICK_PERIOD_MS);
            }
            processingCommand = true;
            // if it is a special move command for wheel
            if (strncmp("Move:", obj.command, 5) == 0) {
                const float factor = StepsPerRound / TotalXPosition;
                int targetPosition = std::strtol(obj.command + 5, nullptr, 10) % TotalXPosition;
                float* print_position = system_get_mpos();
                int currentPosition = (((int)(print_position[X_AXIS] / factor) + TotalXPosition) % TotalXPosition);
                int clockwise = (targetPosition - currentPosition + TotalXPosition) % TotalXPosition;
                int counterClockwise = (currentPosition - targetPosition + TotalXPosition) % TotalXPosition;
                int moveAmount = (clockwise <= counterClockwise) ? clockwise : -counterClockwise;
                sprintf(cmd, "G91G0X%.3f\r\n", (float)moveAmount * factor);

            } else {
                sprintf(cmd, "%s\r\n", obj.command);
            }
            grbl_send(CLIENT_ALL, cmd);
            report_status_message(execute_line(cmd, CLIENT_WEBUI, WebUI::AuthenticationLevel::LEVEL_ADMIN), CLIENT_ALL);

            // wait until it is done
            do  {
              vTaskDelay(10 / portTICK_PERIOD_MS);
            } while (sys.state != State::Idle);

            processingCommand = false;  // Allow next command
        } else
          vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// Handle incoming web requests for G-code
void WebTask::handleCommandRequest() {
    if (server->hasArg("cmd")) {
        char command[20];
        //sprintf(command, "X:StepsPerMm=%d, current=%.2f\r\n", StepsPerMm, x_axis_settings->steps_per_mm->get());
        //grbl_send(CLIENT_ALL, command);
        if (x_axis_settings->steps_per_mm->get() != StepsPerMm) {
            sprintf(command, "$100=%d\r\n", StepsPerMm);
            grbl_send(CLIENT_ALL, command);
            execute_line(command, CLIENT_WEBUI, WebUI::AuthenticationLevel::LEVEL_ADMIN);
        }
        int yStepsPerMm = 1000;
        //sprintf(command, "Y:StepsPerMm=%d, current=%.2f\r\n", yStepsPerMm, y_axis_settings->steps_per_mm->get());
        //grbl_send(CLIENT_ALL, command);
        if (y_axis_settings->steps_per_mm->get() != yStepsPerMm) {
            sprintf(command, "$101=%d\r\n", yStepsPerMm);
            grbl_send(CLIENT_ALL, command);
            execute_line(command, CLIENT_WEBUI, WebUI::AuthenticationLevel::LEVEL_ADMIN);
        }
        char msg[30];
        CmdStruct obj;
        strncpy(obj.command, server->arg("cmd").c_str(), sizeof(obj.command));
        if (xQueueSend(commandQueue, &obj, portMAX_DELAY) == pdTRUE) {
            sprintf(msg, "command queued: %s", obj.command);
            server->send(200, "text/plain", msg);
        } else {
            sprintf(msg, "failed to queued cmd: %s", obj.command);
            server->send(400, "text/plain", msg);
        }
    } else {
        server->send(400, "text/plain", "Missing cmd parameter");
    }
}

// Handle incoming web requests for wait until timeout or Idle
void WebTask::handleWaitUntilDone() {
    int waitMsec = 1000;      // default to wait for 1 sec
    if (server->hasArg("msec")) {
        waitMsec = server->arg("msec").toInt();
    }
    // wait until it is done
    uint32_t start = millis();
    while (millis() - start < waitMsec)  {
        if (sys.state == State::Idle) {
            server->send(200, "text/plain", "Grbl is Idle");
            return;
        }
        delay(50);
    }
    server->send(400, "text/plain", "Wait for Grbl to Idle timeout");
}

void WebTask::setup(WebServer *_server) {
    server = _server;
    server->on("/send", HTTP_ANY, handleCommandRequest);
    server->on("/wait", HTTP_ANY, handleWaitUntilDone);
    commandQueue = xQueueCreate(20, sizeof(CmdStruct));

    // Create FreeRTOS task to process commands
    xTaskCreatePinnedToCore(
        processQueueTask, "ProcessQueue", 4096, NULL, 1, &processTaskHandle, 0
    );
}
}
