//
//  MyMultipleKeyboardsPref.h
//
//  Created by Nick Zavaritsky on 9/7/12.
//  Copyright 2012 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>

@interface Org_Example_MyMultipleKeyboardsPref : NSPreferencePane {
    IBOutlet NSMatrix *modKeysMergeModeRadio;
}
- (void)mainViewDidLoad;
- (IBAction)modKeysMergeModeChanged:(id)sender;

@end
