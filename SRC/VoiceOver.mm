#if defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>

#if TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_OSX

#include "VoiceOver.h"

#if TARGET_OS_IOS || TARGET_OS_TV
#import <UIKit/UIKit.h>
#elif TARGET_OS_OSX
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <ApplicationServices/ApplicationServices.h>
#endif

namespace Sral {

bool VoiceOver::Initialize() {
    return true;
}

bool VoiceOver::Uninitialize() {
    return true;
}

bool VoiceOver::Speak(const char* text, bool interrupt) {
    if (!text) {
        return false;
    }
    
    NSString* msg = [NSString stringWithUTF8String:text];
    if (!msg) {
        msg = [NSString stringWithCString:text encoding:NSASCIIStringEncoding];
        if (!msg) return false;
    }

#if TARGET_OS_IOS || TARGET_OS_TV
    if (interrupt) {
        UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification, nil);
    }
    UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification, msg);
#elif TARGET_OS_OSX
    id targetElement = NSApp ? (id)NSApp : (id)[NSApplication sharedApplication];
    
    NSDictionary* userInfo = @{NSAccessibilityAnnouncementKey : msg,
                               NSAccessibilityAnnouncementPriorityKey : @(NSAccessibilityPriorityHigh)};
    
    NSAccessibilityPostNotificationWithUserInfo(targetElement, NSAccessibilityAnnouncementRequestedNotification, userInfo);
    (void)interrupt; 
#endif

    return true;
}

bool VoiceOver::StopSpeech() {
#if TARGET_OS_IOS || TARGET_OS_TV
    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification, nil);
    return true;
#elif TARGET_OS_OSX
    NSDictionary* userInfo = @{NSAccessibilityAnnouncementKey : @"",
                               NSAccessibilityAnnouncementPriorityKey : @(NSAccessibilityPriorityHigh)};
    id targetElement = NSApp ? (id)NSApp : (id)[NSApplication sharedApplication];
    NSAccessibilityPostNotificationWithUserInfo(targetElement, NSAccessibilityAnnouncementRequestedNotification, userInfo);
    return true;
#endif
}

bool VoiceOver::GetActive() {
#if TARGET_OS_IOS || TARGET_OS_TV
    return UIAccessibilityIsVoiceOverRunning() == YES;
#elif TARGET_OS_OSX
    if ([[NSWorkspace sharedWorkspace] isVoiceOverEnabled]) {
        return true;
    }
    
    return AXIsProcessTrusted() == YES;
#endif
}

} // namespace Sral

#endif /* TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_OSX */
#endif /* defined(__APPLE__) || defined(__MACH__) */
