//
//  KSCrashMonitor_Signal.c
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

#include "KSCrashMonitor_Signal.h"

#include "KSCrashMonitorContext.h"
#include "KSCrashMonitorContextHelper.h"
#include "KSCrashMonitorHelper.h"
#include "KSCrashMonitor_MachException.h"
#include "KSCrashMonitor_Memory.h"
#include "KSID.h"
#include "KSMachineContext.h"
#include "KSSignalInfo.h"
#include "KSStackCursor_MachineContext.h"
#include "KSSystemCapabilities.h"

// #define KSLogger_LocalLevel TRACE
#include "KSLogger.h"

#if KSCRASH_HAS_SIGNAL

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
#pragma mark - Globals -
// ============================================================================

static volatile bool g_isEnabled = false;
static bool g_sigterm_monitoringEnabled = false;

static KSCrash_MonitorContext g_monitorContext;
static KSStackCursor g_stackCursor;

#if KSCRASH_HAS_SIGNAL_STACK
/** Our custom signal stack. The signal handler will use this as its stack. */
static stack_t g_signalStack = { 0 };
#endif

/** Signal handlers that were installed before we installed ours. */
static struct sigaction *g_previousSignalHandlers = NULL;

static char g_eventID[37];

// ============================================================================
#pragma mark - Private -
// ============================================================================

static void uninstallSignalHandler(void);
static bool shouldHandleSignal(int sigNum) { return !(sigNum == SIGTERM && !g_sigterm_monitoringEnabled); }

// ============================================================================
#pragma mark - Callbacks -
// ============================================================================

/** Our custom signal handler.
 * Restore the default signal handlers, record the signal information, and
 * write a crash report.
 * Once we're done, re-raise the signal and let the default handlers deal with
 * it.
 *
 * @param sigNum The signal that was raised.
 *
 * @param signalInfo Information about the signal.
 *
 * @param userContext Other contextual information.
 */
static void handleSignal(int sigNum, siginfo_t *signalInfo, void *userContext)
{
    KSLOG_DEBUG("Trapped signal %d", sigNum);
    if (g_isEnabled && shouldHandleSignal(sigNum)) {
        thread_act_array_t threads = NULL;
        mach_msg_type_number_t numThreads = 0;
        ksmc_suspendEnvironment(&threads, &numThreads);
        kscm_notifyFatalExceptionCaptured(false);

        KSLOG_DEBUG("Filling out context.");
        KSMachineContext machineContext = { 0 };
        ksmc_getContextForSignal(userContext, &machineContext);
        kssc_initWithMachineContext(&g_stackCursor, KSSC_MAX_STACK_DEPTH, &machineContext);

        KSCrash_MonitorContext *crashContext = &g_monitorContext;
        memset(crashContext, 0, sizeof(*crashContext));
        ksmc_fillMonitorContext(crashContext, kscm_signal_getAPI());
        crashContext->eventID = g_eventID;
        crashContext->offendingMachineContext = &machineContext;
        crashContext->registersAreValid = true;
        crashContext->faultAddress = (uintptr_t)signalInfo->si_addr;
        crashContext->signal.userContext = userContext;
        crashContext->signal.signum = signalInfo->si_signo;
        crashContext->signal.sigcode = signalInfo->si_code;
        crashContext->stackCursor = &g_stackCursor;

        kscm_handleException(crashContext);
        ksmc_resumeEnvironment(threads, numThreads);
    } else {
        uninstallSignalHandler();
        ksmemory_notifyUnhandledFatalSignal();
    }

    KSLOG_DEBUG("Re-raising signal for regular handlers to catch.");
    raise(sigNum);
}

// ============================================================================
#pragma mark - API -
// ============================================================================

static bool installSignalHandler(void)
{
    KSLOG_DEBUG("Installing signal handler.");

#if KSCRASH_HAS_SIGNAL_STACK

    if (g_signalStack.ss_size == 0) {
        KSLOG_DEBUG("Allocating signal stack area.");
        g_signalStack.ss_size = SIGSTKSZ;
        g_signalStack.ss_sp = malloc(g_signalStack.ss_size);
    }

    KSLOG_DEBUG("Setting signal stack area.");
    if (sigaltstack(&g_signalStack, NULL) != 0) {
        KSLOG_ERROR("signalstack: %s", strerror(errno));
        goto failed;
    }
#endif

    const int *fatalSignals = kssignal_fatalSignals();
    int fatalSignalsCount = kssignal_numFatalSignals();

    if (g_previousSignalHandlers == NULL) {
        KSLOG_DEBUG("Allocating memory to store previous signal handlers.");
        g_previousSignalHandlers = malloc(sizeof(*g_previousSignalHandlers) * (unsigned)fatalSignalsCount);
    }

    struct sigaction action = { { 0 } };
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
#if KSCRASH_HOST_APPLE && defined(__LP64__)
    action.sa_flags |= SA_64REGSET;
#endif
    sigemptyset(&action.sa_mask);
    action.sa_sigaction = &handleSignal;

    for (int i = 0; i < fatalSignalsCount; i++) {
        KSLOG_DEBUG("Assigning handler for signal %d", fatalSignals[i]);
        if (sigaction(fatalSignals[i], &action, &g_previousSignalHandlers[i]) != 0) {
            char sigNameBuff[30];
            const char *sigName = kssignal_signalName(fatalSignals[i]);
            if (sigName == NULL) {
                snprintf(sigNameBuff, sizeof(sigNameBuff), "%d", fatalSignals[i]);
                sigName = sigNameBuff;
            }
            KSLOG_ERROR("sigaction (%s): %s", sigName, strerror(errno));
            // Try to reverse the damage
            for (i--; i >= 0; i--) {
                sigaction(fatalSignals[i], &g_previousSignalHandlers[i], NULL);
            }
            goto failed;
        }
    }
    KSLOG_DEBUG("Signal handlers installed.");
    return true;

failed:
    KSLOG_DEBUG("Failed to install signal handlers.");
    return false;
}

static void uninstallSignalHandler(void)
{
    KSLOG_DEBUG("Uninstalling signal handlers.");

    const int *fatalSignals = kssignal_fatalSignals();
    int fatalSignalsCount = kssignal_numFatalSignals();

    for (int i = 0; i < fatalSignalsCount; i++) {
        KSLOG_DEBUG("Restoring original handler for signal %d", fatalSignals[i]);
        sigaction(fatalSignals[i], &g_previousSignalHandlers[i], NULL);
    }

#if KSCRASH_HAS_SIGNAL_STACK
    g_signalStack = (stack_t) { 0 };
#endif
    KSLOG_DEBUG("Signal handlers uninstalled.");
}

static const char *monitorId(void) { return "Signal"; }

static KSCrashMonitorFlag monitorFlags(void) { return KSCrashMonitorFlagFatal | KSCrashMonitorFlagAsyncSafe; }

static void setEnabled(bool isEnabled)
{
    if (isEnabled != g_isEnabled) {
        g_isEnabled = isEnabled;
        if (isEnabled) {
            ksid_generate(g_eventID);
            if (!installSignalHandler()) {
                return;
            }
        } else {
            uninstallSignalHandler();
        }
    }
}

static bool isEnabled(void) { return g_isEnabled; }

static void addContextualInfoToEvent(struct KSCrash_MonitorContext *eventContext)
{
    const char *machName = kscm_getMonitorId(kscm_machexception_getAPI());

    if (!(strcmp(eventContext->monitorId, monitorId()) == 0 ||
          (machName && strcmp(eventContext->monitorId, machName) == 0))) {
        eventContext->signal.signum = SIGABRT;
    }
}

#endif /* KSCRASH_HAS_SIGNAL */

#if KSCRASH_HAS_SIGNAL
void kscm_signal_sigterm_setMonitoringEnabled(bool enabled) { g_sigterm_monitoringEnabled = enabled; }
#else
void kscm_signal_sigterm_setMonitoringEnabled(__unused bool enabled) {}
#endif

KSCrashMonitorAPI *kscm_signal_getAPI(void)
{
#if KSCRASH_HAS_SIGNAL
    static KSCrashMonitorAPI api = { .monitorId = monitorId,
                                     .monitorFlags = monitorFlags,
                                     .setEnabled = setEnabled,
                                     .isEnabled = isEnabled,
                                     .addContextualInfoToEvent = addContextualInfoToEvent };
    return &api;
#else
    return NULL;
#endif
}
