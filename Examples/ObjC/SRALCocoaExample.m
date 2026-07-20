#import <AppKit/AppKit.h>

#define SRAL_STATIC

#ifdef __cplusplus
extern "C" {
#endif
#include <SRAL.h>
#ifdef __cplusplus
}
#endif

#define SRAL_SAFE_SPEAK(phrase_ptr, interrupt_bool) \
    ({ \
        _Generic((phrase_ptr), \
            char*:          SRAL_Speak((char*)(phrase_ptr), (interrupt_bool)), \
            const char*:    SRAL_Speak((char*)(phrase_ptr), (interrupt_bool)), \
            default:        false \
        ); \
    })

@interface AppDelegate : NSObject <NSApplicationDelegate> {
    dispatch_queue_t _speechQueue;
}

@property (nonatomic, strong) NSWindow *window;
@property (nonatomic, weak) NSTextField *engineLabel;   
@property (nonatomic, weak) NSTextField *speakingLabel; 
@property (nonatomic, strong) NSTimer *speakingTimer;

- (void)speakClicked:(id)sender;
- (void)updateStatus:(NSTimer *)timer;

@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    dispatch_queue_attr_t qosAttribute = dispatch_queue_attr_make_with_qos_class(
        DISPATCH_QUEUE_SERIAL, 
        QOS_CLASS_USER_INITIATED, 
        0
    );
    _speechQueue = dispatch_queue_create("org.sral.speechProcessingQueue", qosAttribute);

    const NSRect frame = NSMakeRect(200, 200, 400, 200);
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable)
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.title = @"SRAL Cocoa Async Example";

    NSTextField * const localEngineLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 150, 360, 20)];
    localEngineLabel.editable = NO;
    localEngineLabel.bordered = NO;
    localEngineLabel.drawsBackground = NO;
    localEngineLabel.stringValue = @"Engine: (initializing...)";
    [self.window.contentView addSubview:localEngineLabel];
    self.engineLabel = localEngineLabel;

    NSTextField * const localSpeakingLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 125, 360, 20)];
    localSpeakingLabel.editable = NO;
    localSpeakingLabel.bordered = NO;
    localSpeakingLabel.drawsBackground = NO;
    localSpeakingLabel.stringValue = @"Speaking: No";
    [self.window.contentView addSubview:localSpeakingLabel];
    self.speakingLabel = localSpeakingLabel;

    NSButton * const speakButton = [NSButton buttonWithTitle:@"Speak" target:self action:@selector(speakClicked:)];
    speakButton.frame = NSMakeRect(125, 70, 150, 40);
    speakButton.bezelStyle = NSBezelStyleRounded;
    [self.window.contentView addSubview:speakButton];

    if (!SRAL_Initialize(0)) {
        self.engineLabel.stringValue = @"Engine: Failed to initialize SRAL!";
        return;
    }

    const int engine = SRAL_GetCurrentEngine();
    const char * const name = SRAL_GetEngineName(engine);
    self.engineLabel.stringValue = [NSString stringWithFormat:@"Engine: %s", name ? name : "Unknown"];

    __weak __typeof(self) weakSelf = self;
    self.speakingTimer = [NSTimer scheduledTimerWithTimeInterval:0.1
                                                         repeats:YES
                                                           block:^(NSTimer * _Nonnull timer) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf updateStatus:timer];
        }
    }];

    [self.window makeKeyAndOrderFront:nil];
    
    [NSApp activateWithOptions:(NSApplicationActivationOptions)(NSApplicationActivationActivateAllWindows | NSApplicationActivationActivateIgnoringOtherApps)];
}

- (void)speakClicked:(id)sender {
    NSString *const textToSpeak = [@"Hello, this is a thread-safe asynchronous test of the SRAL library." copy];
    
    __weak __typeof(self) weakSelf = self;
    dispatch_async(_speechQueue, ^{
        const char *const rawPhrase = [textToSpeak UTF8String];
        if (rawPhrase) {
            const BOOL success = SRAL_SAFE_SPEAK(rawPhrase, YES);
            if (!success) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    __strong __typeof(weakSelf) strongSelf = weakSelf;
                    if (strongSelf) {
                        strongSelf.engineLabel.stringValue = @"Speech Output Pipeline Failure!";
                    }
                });
            }
        }
    });
}

- (void)updateStatus:(NSTimer *)timer {
    const int engine = SRAL_GetCurrentEngine();
    const char * const name = SRAL_GetEngineName(engine);
    self.engineLabel.stringValue = [NSString stringWithFormat:@"Engine: %s", name ? name : "None"];

    self.speakingLabel.stringValue = SRAL_IsSpeaking() ? @"Speaking: Yes" : @"Speaking: No";
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    [self.speakingTimer invalidate];
    self.speakingTimer = nil;
    
    dispatch_group_t shutdownGroup = dispatch_group_create();
    
    dispatch_group_async(shutdownGroup, _speechQueue, ^{
        SRAL_Uninitialize();
    });
    
    const dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2.0 * NSEC_PER_SEC));
    (void)dispatch_group_wait(shutdownGroup, timeout);
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication * const app = [NSApplication sharedApplication];
        app.activationPolicy = NSApplicationActivationPolicyRegular;

        AppDelegate * const delegate = [[AppDelegate alloc] init];
        app.delegate = delegate;

        [app run];
    }
    return 0;
}
