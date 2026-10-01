#include "jadefx/stage/FolderDialog.hpp"

#include "jadefx/application/RunLater.hpp"

#include <string>
#include <utility>
#include <vector>

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

// NSOpenPanel picks an existing folder, or a file or several. NSSavePanel names a new one.
// Both run modal on the main thread, which is where runLater tasks run.
DialogResult RunDialog(const FolderDialogOptions& options, std::vector<std::string>& paths) {
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
            open.canChooseFiles = options.file ? YES : NO;
            open.canChooseDirectories = options.file ? NO : YES;
            open.allowsMultipleSelection = options.file && options.multiple ? YES : NO;
            open.canCreateDirectories = options.file ? NO : YES;
            open.prompt = @"Open";
            if (options.file && !options.extensions.empty()) {
                NSMutableArray<NSString*>* types = [NSMutableArray array];
                for (const std::string& extension : options.extensions) {
                    [types addObject:Text(extension)];
                }
                // allowedContentTypes replaces this from macOS 11, but needs UniformTypeIdentifiers.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
                open.allowedFileTypes = types;
#pragma clang diagnostic pop
            }
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
        // An open panel lists every pick in URLs; a save panel has only URL.
        NSArray<NSURL*>* urls = options.save ? @[ panel.URL ] : ((NSOpenPanel*)panel).URLs;
        for (NSURL* url in urls) {
            const char* chosen = url.fileSystemRepresentation;
            if (chosen != nullptr && chosen[0] != '\0') {
                paths.push_back(chosen);
            }
        }
        return paths.empty() ? DialogResult::Cancelled : DialogResult::Chosen;
    }
}

#endif

}  // namespace

void showFilesDialog(FolderDialogOptions options, FilesDialogHandler done) {
    if (!done) {
        return;
    }
#if TARGET_OS_IPHONE
    (void)options;
    runLater([done = std::move(done)]() { done(DialogResult::Unavailable, std::vector<std::string>()); });
#else
    // From the next frame, so the click that asked for it has finished.
    runLater([options = std::move(options), done = std::move(done)]() {
        std::vector<std::string> paths;
        const DialogResult result = RunDialog(options, paths);
        done(result, paths);
    });
#endif
}

void showFolderDialog(FolderDialogOptions options, FolderDialogHandler done) {
    if (!done) {
        return;
    }
    options.multiple = false;
    showFilesDialog(std::move(options), [done = std::move(done)](DialogResult result,
                                                                 const std::vector<std::string>& paths) {
        done(result, paths.empty() ? std::string() : paths.front());
    });
}

}  // namespace jadefx
