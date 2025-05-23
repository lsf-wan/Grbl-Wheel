/*
  Grbl.cpp - Initialization and main loop for Grbl
  Part of Grbl
  Copyright (c) 2014-2016 Sungeun K. Jeon for Gnea Research LLC

	2018 -	Bart Dring This file was modifed for use on the ESP32
					CPU. Do not use this with Grbl for atMega328P

  Grbl is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  Grbl is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with Grbl.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Grbl.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

String macAddress;
String ipAddress;
int grblId = 0;
int TotalXPosition = 160;
float xStepsPerRound = 146.8;
int xStepsPerMm = 1000;
int yStepsPerMm = 1000;

void grbl_init() {
#ifdef USE_I2S_OUT
    i2s_out_init();  // The I2S out must be initialized before it can access the expanded GPIO port
#endif
    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.enableSTA(false);
    WiFi.enableAP(false);
    WiFi.mode(WIFI_OFF);
    client_init();  // Setup serial baud rate and interrupts
    display_init();
    grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "Grbl_ESP32 Ver %s Date %s", GRBL_VERSION, GRBL_VERSION_BUILD);  // print grbl_esp32 verion info
    grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "Compiled with ESP32 SDK:%s", ESP.getSdkVersion());              // print the SDK version
// show the map name at startup
#ifdef MACHINE_NAME
    report_machine_type(CLIENT_SERIAL);
#endif
    settings_init();  // Load Grbl settings from non-volatile storage
    stepper_init();   // Configure stepper pins and interrupt timers
    system_ini();     // Configure pinout pins and pin-change interrupt (Renamed due to conflict with esp32 files)
    init_motors();
    memset(sys_position, 0, sizeof(sys_position));  // Clear machine position.
    machine_init();                                 // weak definition in Grbl.cpp does nothing
    // Initialize system state.
#ifdef FORCE_INITIALIZATION_ALARM
    // Force Grbl into an ALARM state upon a power-cycle or hard reset.
    sys.state = State::Alarm;
#else
    sys.state = State::Idle;
#endif
    // Check for power-up and set system alarm if homing is enabled to force homing cycle
    // by setting Grbl's alarm state. Alarm locks out all g-code commands, including the
    // startup scripts, but allows access to settings and internal commands. Only a homing
    // cycle '$H' or kill alarm locks '$X' will disable the alarm.
    // NOTE: The startup script will run after successful completion of the homing cycle, but
    // not after disabling the alarm locks. Prevents motion startup blocks from crashing into
    // things uncontrollably. Very bad.
#ifdef HOMING_INIT_LOCK
    if (homing_enable->get()) {
        sys.state = State::Alarm;
    }
#endif
    Spindles::Spindle::select();
#ifdef ENABLE_WIFI
    WebUI::wifi_config.begin();
#endif
#ifdef ENABLE_BLUETOOTH
    WebUI::bt_config.begin();
#endif
    WebUI::inputBuffer.begin();
    registerGrbl();
}

void registerGrbl() {
  macAddress = WiFi.macAddress();
  ipAddress = WiFi.localIP().toString();
  bool rv = false;
  String url = "http://lsf.little-shepherd.org/GrblConfig.php?mac=" + macAddress + "&ip=" + ipAddress;
  char response[200];
  sprintf(response, "[DEBUG] %s\r\n", url.c_str());
  grbl_send(CLIENT_ALL, response);
  HTTPClient http;
  url.replace(" ", "%20");  // Replace space with %20
  // Make the initial GET request
  http.begin(url.c_str());  // Begin with the current URL
  // Make the GET request
  http.addHeader("Cache-Control", "no-cache");
  http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
  int httpCode = http.GET();
  // Check the response code
  if (httpCode == HTTP_CODE_OK) {
    StaticJsonDocument<64> doc;
    String payload = http.getString();
    //Serial.println(payload);
    sprintf(response, "[DEBUG] payload=%s", payload.c_str());
    grbl_send(CLIENT_ALL, response);

    DeserializationError error = deserializeJson(doc, payload.c_str());
    if (error) {
      sprintf(response, "[ERROR] deserializeJson() failed:%s", error.f_str());
      grbl_send(CLIENT_ALL, response);
    } else {
      if (doc["success"].as<int>() == 1) {
        grblId = doc["id"].as<int>();
        TotalXPosition = doc["xpos"].as<int>();
        xStepsPerRound = doc["xstep"].as<float>();
        xStepsPerMm = doc["xstepMm"].as<int>();
        yStepsPerMm = doc["ystepMm"].as<int>();
        sprintf(response, "[DEBUG] pos=%d, step=%.3f, stepMm=%d\r\n", TotalXPosition, xStepsPerRound, xStepsPerMm);
      } else {
        strcpy(response, doc["msg"].as<String>().c_str());
      }
      //Serial.println(response);
      grbl_send(CLIENT_ALL, response);
    }
  } else {
    //Serial.printf("HTTP GET... failed, error(%d): %s, url=%s\n", httpCode, http.errorToString(httpCode).c_str(), url.c_str());
    sprintf(response, "[DEBUG] HTTP GET failed (%d)\r\n", httpCode);
    grbl_send(CLIENT_ALL, response);
  }
  http.end();  // Close connection
}

static void reset_variables() {
    // Reset system variables.
    State prior_state = sys.state;
    memset(&sys, 0, sizeof(system_t));  // Clear system struct variable.
    sys.state             = prior_state;
    sys.f_override        = FeedOverride::Default;              // Set to 100%
    sys.r_override        = RapidOverride::Default;             // Set to 100%
    sys.spindle_speed_ovr = SpindleSpeedOverride::Default;      // Set to 100%
    memset(sys_probe_position, 0, sizeof(sys_probe_position));  // Clear probe position.

    sys_probe_state                      = Probe::Off;
    sys_rt_exec_state.value              = 0;
    sys_rt_exec_accessory_override.value = 0;
    sys_rt_exec_alarm                    = ExecAlarm::None;
    cycle_stop                           = false;
    sys_rt_f_override                    = FeedOverride::Default;
    sys_rt_r_override                    = RapidOverride::Default;
    sys_rt_s_override                    = SpindleSpeedOverride::Default;

    // Reset Grbl primary systems.
    client_reset_read_buffer(CLIENT_ALL);
    gc_init();  // Set g-code parser to default state
    spindle->stop();
    coolant_init();
    limits_init();
    probe_init();
    plan_reset();  // Clear block buffer and planner variables
    st_reset();    // Clear stepper subsystem variables
    // Sync cleared gcode and planner positions to current system position.
    plan_sync_position();
    gc_sync_position();
    report_init_message(CLIENT_ALL);

    // used to keep track of a jog command sent to mc_line() so we can cancel it.
    // this is needed if a jogCancel comes along after we have already parsed a jog and it is in-flight.
    sys_pl_data_inflight = NULL;
}

void run_once() {
    reset_variables();
    // Start Grbl main loop. Processes program inputs and executes them.
    // This can exit on a system abort condition, in which case run_once()
    // is re-executed by an enclosing loop.
    protocol_main_loop();
}

void __attribute__((weak)) machine_init() {}

void __attribute__((weak)) display_init() {}

void __attribute__((weak)) user_m30() {}

void __attribute__((weak)) user_tool_change(uint8_t new_tool) {}
/*
  setup() and loop() in the Arduino .ino implements this control flow:

  void main() {
     init();          // setup()
     while (1) {      // loop()
         run_once();
     }
  }
*/
