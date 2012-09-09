//
//  MyMultipleKeyboardsPref.m
//
//  Created by Nick Zavaritsky on 9/7/12.
//  Copyright 2012 __MyCompanyName__. All rights reserved.
//

#import "MyMultipleKeyboardsPref.h"
#include "mmk-daemon.h"
#include <mach/mach.h>
#include <servers/bootstrap.h>

static kern_return_t mmk_daemon_connect(mach_port_t *server_port);

@implementation Org_Example_MyMultipleKeyboardsPref

/*
 * mainViewDidLoad  runs when preference pane is loaded
 *
 * The function retrieves the current settings from mmk-daemon and updates 
 * UI accordingly.
 */
- (void)mainViewDidLoad
{
    kern_return_t kr;
    mach_port_t server_port = MACH_PORT_NULL;
    int mode;

    kr = mmk_daemon_connect(&server_port);
    if (kr!=KERN_SUCCESS)
        goto out;
    kr = mmkd_get_mod_keys_merge_mode(server_port, &mode);
    if (kr!=KERN_SUCCESS)
        goto out;
    [modKeysMergeModeRadio selectCellWithTag:mode];
    out:
    if (server_port != MACH_PORT_NULL)
        mach_port_deallocate(mach_task_self(), server_port);
}

/*
 * modKeysMergeModeChanged  runs when user clicks on 'modifier keys merge mode' radio group
 *
 * The function sends updated settings to mmk-daemon.
 */
- (IBAction)modKeysMergeModeChanged:(id)sender
{
    kern_return_t kr;
    mach_port_t server_port = MACH_PORT_NULL;

    kr = mmk_daemon_connect(&server_port);
    if (kr!=KERN_SUCCESS)
        goto out;
    mmkd_set_mod_keys_merge_mode(server_port, [[modKeysMergeModeRadio selectedCell] tag]);
out:
    if (server_port != MACH_PORT_NULL)
        mach_port_deallocate(mach_task_self(), server_port);
}

@end

static kern_return_t mmk_daemon_connect(mach_port_t *server_port)
{
    return bootstrap_look_up(bootstrap_port, "org.example.mmk-agent", server_port);
}