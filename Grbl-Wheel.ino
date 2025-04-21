/*
  Grbl_ESP32.ino - Header for system level commands and real-time processes
  Part of Grbl
  Copyright (c) 2014-2016 Sungeun K. Jeon for Gnea Research LLC

	2018 -	Bart Dring This file was modified for use on the ESP32
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

/*
for wheel version on axis X, we need
use esp32 version 1.0.6 for this souce code
WebTask class for extra commands, send and wait
with grbl settings:a_axis_settings
$20=0
$21=0
$22=1
$23=3
$27=1.000
add heartbeat and OTA in Protocol.cpp
the limit switch on X/Y, pull higt/trigger low
to disable z limit switch, in defaults.h, changed to
# define DEFAULT_HOMING_CYCLE_0 (bit(X_AXIS) | bit(Y_AXIS))
# define DEFAULT_HOMING_CYCLE_1 0
in Machine.h, includes
#include "Machines/TMC2209_4x.h"  // Or another machine configuration file
in Config.h, add
*/
#include "src/Grbl.h"
#include "src/custom/wheel.h"

void setup() {
    grbl_init();
    init_wheel_settings();
}

void loop() {
    run_once();
}
