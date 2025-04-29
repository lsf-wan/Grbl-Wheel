#include "wheel.h"
#include "../Grbl.h"

extern parser_block_t gc_block;

// $100, $101 and $901 (custom parameter)
void init_wheel_settings() {
    char command[120];
    new IntSetting(GRBL, WG, "901", "Wheel/TotalPositions", TotalXPosition, 1, 360);

    sprintf(command, "$20=0\r\n$21=0\r\n$22=1\r\n$21=3\r\n$27=1\r\n", xStepsPerMm);
    grbl_send(CLIENT_ALL, command);
    execute_line(command, CLIENT_WEBUI, WebUI::AuthenticationLevel::LEVEL_ADMIN);

    sprintf(command, "$100=%d\r\n", xStepsPerMm);
    grbl_send(CLIENT_ALL, command);
    execute_line(command, CLIENT_WEBUI, WebUI::AuthenticationLevel::LEVEL_ADMIN);

    sprintf(command, "$101=%d\r\n", yStepsPerMm);
    grbl_send(CLIENT_ALL, command);
    execute_line(command, CLIENT_WEBUI, WebUI::AuthenticationLevel::LEVEL_ADMIN);

    sprintf(command, "$901=%d\r\n", TotalXPosition);
    grbl_send(CLIENT_ALL, command);
    execute_line(command, CLIENT_WEBUI, WebUI::AuthenticationLevel::LEVEL_ADMIN);
}

// G99 Xn  => Move to position n (mod TotalXPosition) using shortest CW/CCW arc
void handle_G99(char *line) {
    char msg[120];
    // get X value from the line (position)
    sprintf(msg, "line=%s\r\n", line);
    //grbl_msg_sendf(CLIENT_ALL, MsgLevel::Info, msg);

    char *xptr = strchr(line, 'X');
    if (!xptr) {
        grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "G99 requires X position.");
        return;
    }
    int targetPosition = atoi(xptr + 1) % TotalXPosition;
    const float stepsPerPos = xStepsPerRound / TotalXPosition;
    int currentPosition = (((int)(system_get_mpos()[X_AXIS] / stepsPerPos) + TotalXPosition) % TotalXPosition);
    int clockwise = (targetPosition - currentPosition + TotalXPosition) % TotalXPosition;
    int counterClockwise = (currentPosition - targetPosition + TotalXPosition) % TotalXPosition;
    int moveAmount = (clockwise <= counterClockwise) ? clockwise : -counterClockwise;
    gc_block.values.xyz[X_AXIS] = (float)moveAmount * stepsPerPos;
    // insert G0 and modify X to the steps (pos or neg)
    sprintf(xptr, "G0X%.3f", gc_block.values.xyz[X_AXIS]);
    sprintf(msg, "[DEB] current=%d, target=%d, move=%d, %.3f steps, %s\r\n", currentPosition, targetPosition, moveAmount, gc_block.values.xyz[X_AXIS], line);
    grbl_send(CLIENT_ALL, msg);
    //sprintf(msg, "mocked line=%s", line);
    //grbl_msg_sendf(CLIENT_ALL, MsgLevel::Info, msg);
}
