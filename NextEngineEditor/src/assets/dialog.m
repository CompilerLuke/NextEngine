//
//  dialog.m
//  NextEngineEditor
//
//  Created by Antonella Calvia on 28/10/2020.
//

/*
#import <Foundation/Foundation.h>
#include <Cocoa/Cocoa.h>

void cocoa_open_file_dialog() {
    // Create the File Open Dialog class.
    NSOpenPanel* openDlg = [NSOpenPanel openPanel];

    // Enable the selection of files in the dialog.
    [openDlg setCanChooseFiles:YES];

    // Multiple files not allowed
    [openDlg setAllowsMultipleSelection:NO];

    // Can't select a directory
    [openDlg setCanChooseDirectories:NO];

    // Display the dialog. If the OK button was pressed,
    // process the files.
    if ( [openDlg runModal] == NSOKButton )
    {
        // Get an array containing the full filenames of all
        // files and directories selected.
        NSArray* urls = [openDlg URLs];

        // Loop through all the files and process them.
        for(int i = 0; i < [urls count]; i++ )
        {
            NSString* url = [urls objectAtIndex:i];
            NSLog(@"Url: %@", url);
        }
    }
}
*/
