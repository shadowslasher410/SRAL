# SRAL (Screen Reader Abstraction Library)

SRAL is a cross-platform library designed to provide a unified interface for outputting speech and Braille via assistive technologies. It abstracts the complexities of various screen readers, Text-To-Speech (TTS) pipelines, and low-level speech APIs, allowing developers to implement accessibility features once and have them work across multiple engines and platforms.

## 🌍 Language Note

*Note: I am not a native English speaker. All documentation, including this README, has been written with AI assistance to ensure clarity and proper grammar while maintaining the technical accuracy of the project descriptions.*

---

## 🚀 Features

* **Unified API**: Speak text, pause, stop, and resume speech using a single interface.
* **Braille Support**: Direct output to Braille displays.
* **Parameter Control**: Adjust speech rate, volume, and voices (where supported by the engine).
* **Engine Prioritization**: Automatic detection of active screen readers with fallback to system Speech APIs.
* **Advanced Audio**: Support for "Speak to Memory" (PCM raw wave buffer extraction output) and SSML tags.
* **Keyboard Hooks**: Optional low-overhead global asynchronous hooks for interrupting (`Ctrl`) or pausing (`Shift`) active speech.

## 🛠 Supported Engines & Platforms

SRAL natively supports Windows, macOS, iOS, Android, Linux, and ChromeOS.

| Category | Supported Engines |
| --- | --- |
| **Windows Screen Readers** | NVDA, JAWS, ZDSR, Microsoft Narrator |
| **Windows Frameworks** | Microsoft UI Automation (UIA) |
| **macOS** | VoiceOver, NSSpeech, AVFoundation (AVSpeech) |
| **iOS** | VoiceOver, AVFoundation (AVSpeech) |
| **Android** | Android TextToSpeech, Android AccessibilityManager (TalkBack, etc.) |
| **Linux** | Speech Dispatcher, Orca, ChromeVox |
| **ChromeOS**| ChromeVox |
| **General APIs** | Microsoft SAPI (Windows), BRLTTY (BrlAPI Braille Display Layer) (Linux) |

---

## ❓ Why use SRAL?

SRAL is ideal for making **applications or games** accessible to blind or visually impaired users.

> [!IMPORTANT]
> **Note on Accessibility**: SRAL is for direct speech/braille output. It is **not** a UI accessibility bridge. If you need to make your GUI elements (buttons, menus, etc.) visible to screen readers, please use [AccessKit](https://github.com/AccessKit/accesskit)

---

## ⚙️ How it Works

### Initialization & Priorities

When `SRAL_Initialize()` executes, the library loads all available and supported engines.

* **Standard Functions (`SRAL_Speak`, `SRAL_StopSpeech`, etc.)**: These automatically choose the best engine based on priority. The priority order is:
 1. **Active Screen Readers** (Highest priority — avoids overlapping double-speech speech events)
 2. **System Speech Frameworks / APIs** (SAPI, Speech Dispatcher, AVFoundation)
 3. **A11y Providers** (Windows UI Automation)
* **Extended Functions (`SRAL_SpeakEx`, etc.)**: These allow you to manually target a specific engine, bypassing the automatic priority logic.

### Building the Project

SRAL uses CMake and can be built as either a static or dynamic library.

#### Linux System Requirements
The following modules are required to be installed:
```bash
# Debian / Ubuntu / Mint
sudo apt-get install libspeechd-dev libbrlapi-dev brltty pkg-config

# Fedora / RHEL / CentOS
sudo dnf install speech-dispatcher-devel brlapi-devel brltty pkgconfig
```

#### Build Execution Commands
```bash
# Configure the build directory tree
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Compile the target binaries and compilation testing scripts
cmake --build build --config Release
```
This will generate the core binaries as well as an executable testing utility  to verify SRAL functionality on your system.

---

## 💻 Usage

### C/C++ Integration

To use SRAL, simply include the central header in your source code.

#### Static Linking (Windows)
If you are linking against the static binary layout configuration of SRAL on Windows, you **must** declare `#define SRAL_STATIC` prior to including the header file to make the compiler strip explicit `__declspec(dllimport)` configuration directives from the symbol map.

```c
// If linking statically on Windows, uncomment the next line:
// #define SRAL_STATIC
#include <SRAL.h>

int main() {
    if (!SRAL_Initialize(0)) {
        return -1;
    }
    
    SRAL_Speak("Library initialized successfully.", true);
    return 0;
}
```

#### C++ Convenience Wrapper
For object-oriented C++ architectures, an exception-safe inline class implementation wrapper is provided via `#include <SRAL.hpp>`.

### NVDA Extensions
* **NVDAControlEx**:  SRAL provides enhanced speech rate monitoring and advanced runtime control adjustments when paired with the third-party [NVDAControlEx](https://github.com/m1maker/NVDAControlEx) add-on.
* **Controller Client**: For standard NVDA support, download the [Controller Client](https://www.nvaccess.org/files/nvda/releases/stable/). Please note that **Version 1 is not supported**.

### Cross-Platform Language Bindings
The core library exports strict C-linkage APIs, supporting language binding mappings across:
* **C# / .NET** (via P/Invoke Interop boundaries)
* **Dart / Flutter** (via `dart:ffi` structural channels)
* **Go** (via Cgo bindings)
* **Lua / Luau** (via foreign function modules)
* **Node.js** (via N-API / node-addon-api allocations)
* **Python** (via ctypes / native extensions)
* **Rust** (via explicit FFI bindgen bindings)
* **WebAssembly** (compiled natively via Emscripten targets)

---

## 📄 API Overview (Snippet)

For full documentation, see `Include/SRAL.h`.

```c
// Initialize library and exclude specific engines if needed
bool SRAL_Initialize(int engines_exclude);

// Speak text using the best available engine
bool SRAL_Speak(const char* text, bool interrupt);

// Output text to Braille display
bool SRAL_Braille(const char* text);

// Check if an engine is currently speaking
bool SRAL_IsSpeaking(void);

// Categorized engine bitmasks (use with SRAL_SetEnginesExclude)
int  SRAL_GetTTSEngines(void);
int  SRAL_GetAssistiveTechEngines(void);
```

### Routing only through assistive tech (with optional in-app TTS)

If your application should always speak through the user's assistive
technology, but only fall back to platform TTS when the user has
explicitly enabled it, pass a dynamic bitmask query down to clear out the target TTS providers on initial load:

```c
bool user_wants_tts_fallback = /* pull application option state flags */;

// If the user has disabled TTS fallbacks, mask out the TTS engines completely
int blocked_engines = user_wants_tts_fallback ? 0 : SRAL_GetTTSEngines();

if (SRAL_Initialize(blocked_engines)) {
    // Routes directly to active Assistive Tech; stays silent on TTS paths if opted out
    SRAL_Speak("Subsystem routing verified.", true);
}
```
