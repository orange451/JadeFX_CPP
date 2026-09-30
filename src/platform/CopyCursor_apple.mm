#include "CopyCursor.hpp"

#import <TargetConditionals.h>

#if !TARGET_OS_IPHONE
#import <Cocoa/Cocoa.h>
#endif

namespace jadefx {

bool ShowSystemCopyCursor() {
#if TARGET_OS_IPHONE
    return false;
#else
    // Drawn by the system for the screen's scale, which a GLFW image cursor is not.
    @autoreleasepool {
        [[NSCursor dragCopyCursor] set];
    }
    return true;
#endif
}

}  // namespace jadefx
