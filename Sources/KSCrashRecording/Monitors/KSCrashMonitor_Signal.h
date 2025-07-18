//
//  KSCrashMonitor_Signal.h
//
//  Created by Karl Stenerud on 2012-01-28.
//
//  Copyright (c) 2012 Karl Stenerud. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall remain in place
// in this source code.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

/* Catches fatal unix signals.
 */

#ifndef HDR_KSCrashMonitor_Signal_h
#define HDR_KSCrashMonitor_Signal_h

#include "KSCrashMonitor.h"
#include "KSCrashNamespace.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Access the Monitor API.
 */
KSCrashMonitorAPI *kscm_signal_getAPI(void);

/** Enables or disables SIGTERM monitoring.
 *  Default to false.
 *
 * @param enabled if true, SIGTERM signals will be monitored and reported.
 */
void kscm_signal_sigterm_setMonitoringEnabled(bool enabled);

#ifdef __cplusplus
}
#endif

#endif  // HDR_KSCrashMonitor_Signal_h
