/*
 *  mmkd-main.c
 *  MyMultipleKeyboards
 *
 *  Created by Nick Zavaritsky on 9/8/12.
 *  Copyright 2012 __MyCompanyName__. All rights reserved.
 *
 *
 *  mmk-agent
 *
 *  The primary daemon's job is to create event tap to inspect and rewrite
 *  keyboard events.  The daemon has to run as root to achieve this.
 *  The daemon also implements a simple Mach server used by the accompaniing
 *  preference pane to retreive and update settings.
 *
 *  The daemon is first started when a user logs in.  If keyboard rewriting is
 *  disabled daemon waits for 5 secs and exits unless the settings were changed
 *  and keyboard rewriting was re-enabled withing the idle timeout.
 *
 *  The daemon is started on demand when a message is sent to the corresponding
 *  Mach port.  Hence if keyboard rewriting was disabled and the daemon did shutdown,
 *  the preference pane can start it again.  Specifically when a user enables
 *  THE FEATURE the preference pane sends 'update settings' message to the Mach port.
 *  If the daemon was not runing launchd starts it.  The daemon discovers that
 *  keyboard rewriting is still disabled (the message was not processed yet) and arranges
 *  for a 5 second idle timeout.  However long before the timeout expires the message
 *  is processed.  Idle timeout gets cancelled and normal operation resumes.
 *
 *  It's worth mentioning that the daemon has another miscelaneous responsibility.
 *  The daemon not the preference pane writes updated preferences to file (for no
 *  good reason).
 */

#include "mmk-daemonServer.h"
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <dispatch/dispatch.h>
#include <stdio.h>
#include <stdlib.h>

/*
 *
 */
static const CFStringRef prefDomainID = CFSTR("org.example.myMultipleKeyboards");
static const CFStringRef modKeysMergeModePrefName = CFSTR("MergeModifierKeys");
static int mmkd_mode;
CFMachPortRef event_tap;
dispatch_source_t idle_timer;

/*
 *
 */
struct key_kind
{
    const char     *kk_name;
    CGKeyCode       kk_code;
    CGEventFlags    kk_mask;
    int             kk_num_pressed; /* number of keys of this kind simultaneously pressed */
};

static struct key_kind ktab[] =
{
    {"left control",    59, NX_CONTROLMASK|NX_DEVICELCTLKEYMASK},
    {"right control",   62, NX_CONTROLMASK|NX_DEVICERCTLKEYMASK},
    {"left shift",      56, NX_SHIFTMASK|NX_DEVICELSHIFTKEYMASK},
    {"right shift",     60, NX_SHIFTMASK|NX_DEVICERSHIFTKEYMASK},
    {"left command",    55, NX_COMMANDMASK|NX_DEVICELCMDKEYMASK},
    {"right command",   54, NX_COMMANDMASK|NX_DEVICERCMDKEYMASK},
    {"left option",     58, NX_ALTERNATEMASK|NX_DEVICELALTKEYMASK},
    {"right option",    61, NX_ALTERNATEMASK|NX_DEVICERALTKEYMASK},

    {0,0,0,0}
};

/*
 *
 */
#ifndef __DISPATCH_SOURCE_PRIVATE__
typedef boolean_t (*dispatch_mig_callback_t)(mach_msg_header_t *message,
                                             mach_msg_header_t *reply);

extern mach_msg_return_t
dispatch_mig_server(dispatch_source_t ds, size_t maxmsgsz,
                    dispatch_mig_callback_t callback);
#endif

static CGEventRef my_cg_event_callback(CGEventTapProxy proxy, CGEventType type,
                                       CGEventRef event, void *refcon);

/*
 *
 */
static void mode_changed()
{
    switch (mmkd_mode) {
        /*
         * merge mode enabled: create event tap, cancel idle timer (if any)
         */
        case 1: {
            if (idle_timer) {
                dispatch_source_cancel(idle_timer);
                dispatch_release(idle_timer);
                idle_timer = NULL;
            }
            if (!event_tap) {

                /*
                 * reset ktab
                 */
                struct key_kind *i;
                for (i=ktab; i->kk_name; i++)
                    i->kk_num_pressed = 0;

                /*
                 * become root  (this is neccessary in order to create event tap)
                 */
                seteuid(0);

                /*
                 * based on alterkeys.c http://osxbook.com and http://dotdotcomorg.net
                 */
                CGEventMask        event_mask;
                CFRunLoopSourceRef run_loop_source;

                // Create an event tap. We are interested in key presses.
                event_mask = ((1 << kCGEventKeyDown) | (1 << kCGEventKeyUp) | (1<<kCGEventFlagsChanged));
                event_tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, 0,
                                             event_mask, my_cg_event_callback, NULL);

                /*
                 * revert to self
                 */
                seteuid(getuid());

                if (!event_tap) {
                    fprintf(stderr, "failed to create event tap\n");
                    return;
                }

                // Create a run loop source.
                run_loop_source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, event_tap, 0);

                // Add to the current run loop.
                CFRunLoopAddSource(CFRunLoopGetMain(), run_loop_source,
                                   kCFRunLoopCommonModes);

                // Enable the event tap.
                CGEventTapEnable(event_tap, true);
            }
            return;
        }
        /*
         * merge mode disabled: remove event tap (if any), setup idle timer
         */
        case 0: {
            if (event_tap) {
                CFMachPortInvalidate(event_tap);
                CFRelease(event_tap);
                event_tap = NULL;
            }
            if (!idle_timer) {
                idle_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,
                                                    0, 0, dispatch_get_main_queue());
                if (!idle_timer) {
                    fprintf(stderr, "out of memory\n");
                    return;
                }
                dispatch_source_set_event_handler(idle_timer, ^{
                    CFRunLoopStop(CFRunLoopGetMain());
                });
                dispatch_source_set_timer(idle_timer,
                                          dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC*5LL),
                                          -1, NSEC_PER_SEC*1LL);
                dispatch_resume(idle_timer);
            }
            return;
        }
    }
}

/*
 *
 */
static CGEventRef my_cg_event_callback(CGEventTapProxy proxy, CGEventType type,
                                       CGEventRef event, void *refcon)
{
    CGKeyCode code = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
    CGEventFlags flags = CGEventGetFlags(event), mask = 0, new_flags = 0;
    struct key_kind *i;

    switch (type)
    {
        /*
         * FlagsChanged event is generated when a modifier key is pressed or released.
         * The kCGKeyboardEventKeycode field tells us what was the key.
         * If the modifier key was released flags will have the corresponding bits cleared.
         * If all corresponding bits are set then the modifier was pressed instead.
         *
         * For each modifier key we are tracking the number of presses not yet followed
         * by key release (kk_num_pressed). Ex: if left shift key is currently pressed on
         * both keyboards kk_num_pressed=2 in the corresponding ktab entry.
         */
        case kCGEventFlagsChanged:
            for (i=ktab; i->kk_name; i++)
            {
                if (i->kk_code==code)
                {
                    int delta = ((i->kk_mask&flags)==i->kk_mask ? +1 : -1);
                    if (i->kk_num_pressed+delta < 0)
                    {
                        fprintf(stderr, "%s key released while not being pressed\n", i->kk_name);
                        break;
                    }
                    i->kk_num_pressed+=delta;
                    break;
                }
            }
            break;
        /*
         * kCGEventKeyDown and kCGEventKeyUp is generated when a regular key is pressed or released.
         * In here we rewrite flags based on ktab data.
         */
        case kCGEventKeyDown:
        case kCGEventKeyUp:
            for (i=ktab; i->kk_name; i++)
            {
                mask |= i->kk_mask;
                if (i->kk_num_pressed>0) new_flags |= i->kk_mask;
            }
            CGEventSetFlags(event, new_flags|(~mask&flags));
            break;
    }
    return event;
}

/*
 * MIG server routines
 */
kern_return_t mmkd_get_mod_keys_merge_mode (mach_port_t server_port, int *mode)
{
    // fprintf(stderr, "%s\n", __FUNCTION__);

    *mode = mmkd_mode;
    return KERN_SUCCESS;
}

kern_return_t mmkd_set_mod_keys_merge_mode (mach_port_t server_port, int mode)
{
    // fprintf(stderr, "%s (%d)\n", __FUNCTION__, mode);

    if (mode!=0 && mode!=1)
        return KERN_INVALID_VALUE;

    if (mmkd_mode != mode) {
        CFPreferencesSetAppValue(modKeysMergeModePrefName,
                                 (mode ? kCFBooleanTrue : kCFBooleanFalse),
                                 prefDomainID);
        CFPreferencesAppSynchronize(prefDomainID);
        mmkd_mode = mode;
        mode_changed();
    }

    return KERN_SUCCESS;
}

/*
 *
 */
int main()
{
    /*
     * revert to self (mmk-daemon is SUID binary)
     */
    seteuid(getuid());

    /*
     * restore saved mode
     */
    mmkd_mode = CFPreferencesGetAppBooleanValue(modKeysMergeModePrefName, prefDomainID, NULL);
    mode_changed();

    /*
     * init Mach server (used by MyMultipleKeyboards pref pane)
     */
    kern_return_t kr;
    mach_port_t server_port;

    kr = bootstrap_check_in(bootstrap_port, "org.example.mmk-agent", &server_port);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "bootstrap_check_in %x\n", kr);
        return EXIT_FAILURE;
    }

    dispatch_source_t server = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV,
                                                      server_port, 0, dispatch_get_main_queue());
    if (!server) {
        fprintf(stderr, "out of memory\n");
        return EXIT_FAILURE;
    }
    dispatch_source_set_event_handler(server, ^{
        dispatch_mig_server(server, mmkd_mmk_daemon_subsystem.maxsize, mmk_daemon_server);
    });
    dispatch_resume(server);

    /*
     * begin processing
     *
     * Runloop stops if event tap is inactive and idle timeout did expire.
     * It means that the daemon was effectively hanging around having nothing to do.
     */
    CFRunLoopRun();
    return EXIT_SUCCESS;
}
