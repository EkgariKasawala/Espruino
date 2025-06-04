/*
 * This file is part of Espruino, a JavaScript interpreter for Microcontrollers
 *
 * Copyright (C) 2025 Codex
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ----------------------------------------------------------------------------
 * JavaScript bindings for basic OpenThread functionality
 * ----------------------------------------------------------------------------
 */
#include "jswrap_openthread.h"
#include "jsvar.h"
#include "jsinteractive.h"

/*JSON{
  "type" : "class",
  "class" : "OpenThread"
}
OpenThread interface
*/

/*JSON{
  "type" : "method",
  "class" : "OpenThread",
  "name" : "init",
  "generate" : "jswrap_openthread_init"
}
Initialise the OpenThread stack
*/
void jswrap_openthread_init() {
  // Normally OpenThread initialisation would happen here
  jsiConsolePrint("OpenThread init not implemented\n");
}

/*JSON{
  "type" : "method",
  "class" : "OpenThread",
  "name" : "hello",
  "generate" : "jswrap_openthread_hello",
  "return" : ["JsVar","Greeting string"]
}
Return a greeting from OpenThread
*/
JsVar *jswrap_openthread_hello() {
  return jsvNewFromString("Hello from OpenThread!");
}

/*JSON{
  "type" : "method",
  "class" : "OpenThread",
  "name" : "sendHR",
  "generate" : "jswrap_openthread_sendHR",
  "params" : [
    ["bpm","int","Heart rate in beats per minute"]
  ]
}
Send heart rate value via OpenThread
*/
void jswrap_openthread_sendHR(JsVarInt bpm) {
  jsiConsolePrintf("OpenThread sendHR %d\n", (int)bpm);
}
