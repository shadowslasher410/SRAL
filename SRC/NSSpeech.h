#ifndef NS_SPEECH_H
#define NS_SPEECH_H

#if defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>

#if TARGET_OS_OSX

namespace Sral {

constexpr int SRAL_PARAM_SPEECH_RATE = 1;
constexpr int SRAL_PARAM_SPEECH_VOLUME = 2;

class NsSpeech {
public:
	static bool Initialize();
	static bool Uninitialize();

	static bool Speak(const char* text, bool interrupt);
	static bool StopSpeech();

	static bool IsSpeaking();
	static bool GetActive();

	static bool SetParameter(int param, const void* value);
	static bool GetParameter(int param, void* value);

private:
	static void* obj;
};

} // namespace Sral

#endif /* TARGET_OS_OSX */
#endif /* defined(__APPLE__) || defined(__MACH__) */
#endif /* NS_SPEECH_H */
