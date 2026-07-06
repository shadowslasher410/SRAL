core_sources = files(
  'Engine.cpp',
  'SRAL.cpp'
)

local_c_cpp_args = []

if get_option('SRAL_WITH_CHROMEVOX')
  core_sources += files('ChromeVox.cpp')
else
  local_c_cpp_args += '-DSRAL_NO_CHROMEVOX'
endif

if is_windows
  core_sources += files('Encoding.cpp')

  if get_option('SRAL_WITH_NVDA')
    core_sources += files('NVDA.cpp', '../Dep/nvda_control.c')
  else
    local_c_cpp_args += '-DSRAL_NO_NVDA'
  endif
  
  if get_option('SRAL_WITH_JAWS')
    core_sources += files('Jaws.cpp', '../Dep/fsapi.c')
  else
    local_c_cpp_args += '-DSRAL_NO_JAWS'
  endif
  
  if get_option('SRAL_WITH_ZDSR')
    core_sources += files('ZDSR.cpp')
  else
    local_c_cpp_args += '-DSRAL_NO_ZDSR'
  endif
  
  if get_option('SRAL_WITH_SAPI')
    core_sources += files('SAPI.cpp', '../Dep/blastspeak.c', '../Dep/wasapi.cpp')
  else
    local_c_cpp_args += '-DSRAL_NO_SAPI'
  endif
  
  if get_option('SRAL_WITH_UIA')
    core_sources += files('UIA.cpp', '../Dep/UIAProvider.cpp')
  else
    local_c_cpp_args += '-DSRAL_NO_UIA'
  endif

elif is_apple
  if get_option('SRAL_WITH_VOICEOVER')
    core_sources += files('VoiceOver.mm')
  else
    local_c_cpp_args += '-DSRAL_NO_VOICEOVER'
  endif
  
  if get_option('SRAL_WITH_AVSPEECH')
    core_sources += files('AVSpeech.mm')
  else
    local_c_cpp_args += '-DSRAL_NO_AVSPEECH'
  endif
  
  if not is_ios and get_option('SRAL_WITH_NSSPEECH')
    core_sources += files('NSSpeech.mm')
  else
    local_c_cpp_args += '-DSRAL_NO_NSSPEECH'
  endif

elif is_android
  core_sources += files('../Dep/AndroidContext.cpp')
  
  if get_option('SRAL_WITH_ANDROID_TTS')
    core_sources += files('AndroidTextToSpeech.cpp')
  else
    local_c_cpp_args += '-DSRAL_NO_ANDROID_TTS'
  endif
  
  if get_option('SRAL_WITH_ANDROID_ACCESSIBILITY')
    core_sources += files('AndroidAccessibilityManager.cpp')
  else
    local_c_cpp_args += '-DSRAL_NO_ANDROID_ACCESSIBILITY'
  endif

elif is_emscripten
else
  core_sources += files(
    'Encoding.cpp',
    '../Dep/utf-8.c',
    'ACAnnouncer.cpp'
  )
  
  if get_option('SRAL_WITH_SPEECH_DISPATCHER')
    core_sources += files('SpeechDispatcher.cpp')
  else
    local_c_cpp_args += '-DSRAL_NO_SPEECH_DISPATCHER'
  endif
  
  if get_option('SRAL_WITH_ORCA')
    core_sources += files('Orca.cpp')
  else
    local_c_cpp_args += '-DSRAL_NO_ORCA'
  endif
endif

sral_lib = library('SRAL',
  core_sources,
  include_directories: inc_dir,
  dependencies: sral_dependencies,
  c_args: local_c_cpp_args,
  cpp_args: local_c_cpp_args, 
  install: true
)
