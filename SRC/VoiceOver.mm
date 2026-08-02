#include "VoiceOver.h"
#include <chrono>

#if defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>
#if TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_OSX
#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>
#if TARGET_OS_IOS || TARGET_OS_TV
#import <UIKit/UIKit.h>
#elif TARGET_OS_OSX
#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#endif
#define APPLE_ACCESSIBILITY_SUPPORTED 1
#endif
#endif

namespace Sral {

VoiceOver::~VoiceOver() noexcept { (void)Uninitialize(); }

bool VoiceOver::Initialize() {
  std::lock_guard<std::mutex> lock(instanceMutex);
  if (isInitialized.load(std::memory_order_acquire)) {
    return true;
  }

  m_workerThread =
      std::jthread([this](std::stop_token st) noexcept { this->BackgroundWorkerLoop(st); });

  isInitialized.store(true, std::memory_order_release);
  return true;
}

bool VoiceOver::Uninitialize() {
  m_workerThread.request_stop();
  {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_cv.notify_all();
  }

  if (m_workerThread.joinable()) {
    m_workerThread.join();
  }

  isInitialized.store(false, std::memory_order_release);
  m_isSpeakingCache.store(false, std::memory_order_release);
  return true;
}

bool VoiceOver::GetActive() {
#if APPLE_ACCESSIBILITY_SUPPORTED
#if TARGET_OS_IOS || TARGET_OS_TV
  return UIAccessibilityIsVoiceOverRunning() == YES;
#elif TARGET_OS_OSX
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101000
  if (NSAccessibilityIsVoiceOverRunning() == YES) {
    return true;
  }
#endif
  return AXIsProcessTrusted() == YES;
#endif
#else
  return false;
#endif
}

bool VoiceOver::IsSpeaking() { return m_isSpeakingCache.load(std::memory_order_acquire); }

bool VoiceOver::Speak(const char* text, bool interrupt) {
  if (!text || !isInitialized.load(std::memory_order_acquire)) [[unlikely]] {
    return false;
  }

  std::string_view textStr(text);
  if (textStr.empty()) {
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    if (interrupt) {
      std::queue<ThreadCommand> empty;
      std::swap(m_commandQueue, empty);
      m_commandQueue.push(ThreadCommand{CommandType::Stop, "", true});
    }
    m_commandQueue.push(ThreadCommand{CommandType::Speak, std::string(textStr), interrupt});
  }
  m_cv.notify_all();
  return true;
}

bool VoiceOver::StopSpeech() {
  if (!isInitialized.load(std::memory_order_acquire)) {
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    std::queue<ThreadCommand> empty;
    std::swap(m_commandQueue, empty);
    m_commandQueue.push(ThreadCommand{CommandType::Stop, "", true});
  }
  m_cv.notify_all();
  return true;
}

void VoiceOver::BackgroundWorkerLoop(std::stop_token stop_token) noexcept {
  while (!stop_token.stop_requested()) [[likely]] {
    ThreadCommand cmd;
    bool hasCommand = false;

    {
      std::unique_lock<std::mutex> lock(m_queueMutex);

      m_cv.wait(lock, stop_token, [this] { return !m_commandQueue.empty(); });

      if (stop_token.stop_requested() && m_commandQueue.empty()) [[unlikely]] {
        break;
      }

      cmd = std::move(m_commandQueue.front());
      m_commandQueue.pop();
      hasCommand = true;
    }

    if (hasCommand) {
#if APPLE_ACCESSIBILITY_SUPPORTED
      @autoreleasepool {
        std::string rawPayload = std::move(cmd.payload);
        const bool interruptAction = cmd.interrupt;

        if (cmd.type == CommandType::Stop) {
          dispatch_async(dispatch_get_main_queue(), ^{
#if TARGET_OS_IOS || TARGET_OS_TV
              UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification, nil);
#elif TARGET_OS_OSX
			  id targetElement = NSApp ? (id)NSApp : (id)[NSApplication sharedApplication];
			  NSDictionary* userInfo = @{NSAccessibilityAnnouncementKey : @"",
										 NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh)};
			  NSAccessibilityPostNotificationWithUserInfo(targetElement, NSAccessibilityAnnouncementRequestedNotification, userInfo);
#endif
          });
        } else if (cmd.type == CommandType::Speak) {
          dispatch_async(dispatch_get_main_queue(), ^{
              @autoreleasepool {
                NSString* msg = [NSString stringWithUTF8String:rawPayload.c_str()];
                if (!msg) {
                  msg = [NSString stringWithCString:rawPayload.c_str()
                                           encoding:NSASCIIStringEncoding];
                }

                if (msg) {
#if TARGET_OS_IOS || TARGET_OS_TV
                  if (interruptAction) {
                    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification, nil);
                  }
                  UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification, msg);
#elif TARGET_OS_OSX
                      id targetElement = NSApp ? (id)NSApp : (id)[NSApplication sharedApplication];
                      NSDictionary* userInfo = @{NSAccessibilityAnnouncementKey : msg,
                                                 NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh)};
                      NSAccessibilityPostNotificationWithUserInfo(targetElement, NSAccessibilityAnnouncementRequestedNotification, userInfo);
#endif
                }
              }
          });
        }
      }
#endif
    }
    m_isSpeakingCache.store(hasCommand, std::memory_order_release);
  }
}

}  // namespace Sral