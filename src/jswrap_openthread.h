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
#ifndef JSWRAP_OPENTHREAD_H_
#define JSWRAP_OPENTHREAD_H_

#include "jsvar.h"

void jswrap_openthread_init(void);
JsVar *jswrap_openthread_hello(void);
void jswrap_openthread_sendHR(JsVarInt bpm);

#endif // JSWRAP_OPENTHREAD_H_
