#include "jadefx/stage/FolderDialog.hpp"

#include "jadefx/application/RunLater.hpp"

#include <string>
#include <utility>

#import <TargetConditionals.h>

#if !TARGET_OS_IPHONE
#import <Cocoa/Cocoa.h>
#endif

namespace jadefx {
namespace {

#if !TARGET_OS_IPHONE

NSString* Text(const std::string& value) {
    NSString* text = [NSString stringWithUTF8String:value.c_str()];
    return text != nil ? text : @"";
}

// NSOpenPanel picks an existing folder. NSSavePanel names a new one. Both run
// modal on the main thread, which is where runLater tasks run.
DialogResult RunDialog(const FolderDialogOptions& options, std::string& path) {
    @autoreleasepool {
        NSWindow* key = [NSApp keyWindow];
        NSSavePanel* panel = nil;
        if (options.save) {
            NSSavePanel* save = [NSSavePanel savePanel];
            save.canCreateDirectories = YES;
            if (!options.name.empty()) {
                save.nameFieldStringValue = Text(options.name);
            }
            panel = save;
        } else {
            NSOpenPanel* open = [NSOpenPanel openPanel];
            open.canChooseFiles = NO;
            open.canChooseDirectories = YES;
            open.allowsMultipleSelection = NO;
            open.canCreateDirectories = YES;
            open.prompt = @"Open";
            panel = open;
        }
        if (!options.title.empty()) {
            // Panels have no title bar since 10.11. The message is what shows.
            panel.title = Text(options.title);
            panel.message = Text(options.title);
        }
        if (!options.directory.empty()) {
            panel.directoryURL = [NSURL fileURLWithPath:Text(options.directory) isDirectory:YES];
        }
        const NSModalResponse response = [panel runModal];
        if (key != nil) {
            [key makeKeyAndOrderFront:nil];
        }
        if (response != NSModalResponseOK || panel.URL == nil) {
            return DialogResult::Cancelled;
        }
        const char* chosen = panel.URL.fileSystemRepresentation;
        if (chosen == nullptr || chosen[0] == '\0') {
            return DialogResult::Cancelled;
        }
        path = chosen;
        return DialogResult::Chosen;
    }
}

#endif

}  // namespace

void showFolderDialog(FolderDialogOptions options, FolderDialogHandler done) {
    if (!done) {
        return;
    }
#if TARGET_OS_IPHONE
    (void)options;
    runLater([done = std::move(done)]() { done(DialogResult::Unavailable, std::string()); });
#else
    // From the next frame, so the click that asked for it has finished.
    runLater([options = std::move(options), done = std::move(done)]() {
        std::string path;
        const DialogResult result = RunDialog(options, path);
        done(result, path);
    });
#endif
}

}  // namespace jadefx
