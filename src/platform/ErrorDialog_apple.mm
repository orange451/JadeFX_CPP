#include "ErrorDialog.hpp"

#import <TargetConditionals.h>

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <Cocoa/Cocoa.h>
#endif

namespace jadefx {
namespace {

NSString* Text(const std::string& value, NSString* fallback) {
    NSString* text = [NSString stringWithUTF8String:value.c_str()];
    return text != nil ? text : fallback;
}

#if TARGET_OS_IPHONE

UIViewController* TopPresenter() {
    UIWindow* window = nil;
    if (@available(iOS 15.0, *)) {
        for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
            if (![scene isKindOfClass:[UIWindowScene class]]) {
                continue;
            }
            for (UIWindow* candidate in ((UIWindowScene*)scene).windows) {
                if (candidate.isKeyWindow) {
                    window = candidate;
                    break;
                }
            }
            if (window != nil) {
                break;
            }
        }
    }
    if (window == nil) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        window = UIApplication.sharedApplication.windows.firstObject;
#pragma clang diagnostic pop
    }
    UIViewController* presenter = window.rootViewController;
    while (presenter.presentedViewController != nil) {
        presenter = presenter.presentedViewController;
    }
    return presenter;
}

void Present(NSString* title, NSString* message) {
    // The quit handler runs on this same main-thread run loop.
    __block BOOL dismissed = NO;
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:title
                                                                   message:message
                                                            preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"Quit"
                                               style:UIAlertActionStyleDefault
                                             handler:^(UIAlertAction*) { dismissed = YES; }]];
    UIViewController* presenter = TopPresenter();
    if (presenter == nil) {
        return;
    }
    [presenter presentViewController:alert animated:YES completion:nil];
    while (!dismissed) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                 beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
    }
}

#else

void Present(NSString* title, NSString* message) {
    NSApplication* app = [NSApplication sharedApplication];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [app activateIgnoringOtherApps:YES];
#pragma clang diagnostic pop
    NSAlert* alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleCritical;
    alert.messageText = title;
    alert.informativeText = message;
    [alert addButtonWithTitle:@"Quit"];
    [alert runModal];
    [alert release];
}

#endif

}  // namespace

void ShowErrorDialog(const std::string& title, const std::string& message) {
    @autoreleasepool {
        NSString* titleText = Text(title, @"Cannot start");
        NSString* messageText = Text(message, @"");
        if ([NSThread isMainThread]) {
            Present(titleText, messageText);
        } else {
            dispatch_sync(dispatch_get_main_queue(), ^{ Present(titleText, messageText); });
        }
    }
}

}  // namespace jadefx
