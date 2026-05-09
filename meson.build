project('SRAL', ['c', 'cpp'],
  version : '1.0.0',
  default_options : [
    'cpp_std=c++20',
    'default_library=both',
    'warning_level=2'
  ]
)

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')
host_os = host_machine.system()

inc_dir = include_directories('Include')

# Options
build_test = get_option('build_sral_test')
disable_uia = get_option('sral_disable_uia')
disable_nsspeech = get_option('sral_disable_nsspeech')

sral_sources = files(
  'SRC/Encoding.cpp',
  'SRC/SRAL.cpp',
  'SRC/Engine.cpp'
)

sral_deps = []
sral_args = []

# Targeted MSVC Warning Suppression
if cpp.get_id() == 'msvc'
  sral_args += '-D_CRT_SECURE_NO_WARNINGS'
endif

if host_os == 'windows'
  sral_sources += files(
    'SRC/NVDA.cpp', 'SRC/SAPI.cpp',
    'Dep/blastspeak.c', 'Dep/fsapi.c', 'Dep/nvda_control.c',
    'SRC/Jaws.cpp', 'Dep/wasapi.cpp', 'SRC/ZDSR.cpp'
  )
  
  # Platform libs
  sral_deps += cpp.find_library('ole32')
  sral_deps += cpp.find_library('user32')
  
  if not disable_uia
    sral_sources += files('SRC/UIA.cpp', 'Dep/UIAProvider.cpp')
    sral_deps += cpp.find_library('uiautomationcore')
  else
    sral_args += '-DSRAL_NO_UIA'
  endif

  # AccessKit Check (Conditional)
  ak_lib = cpp.find_library('accesskit', required : false)
  if ak_lib.found()
    sral_sources += files('SRC/ACAnnouncer.cpp')
    sral_deps += ak_lib
  else
    sral_args += '-DSRAL_NO_ACCESSKIT'
  endif

elif host_os == 'darwin'
  add_languages('objcpp')
  sral_sources += files('SRC/AVSpeech.mm', 'SRC/VoiceOver.mm')
  
  if not disable_nsspeech
    sral_sources += files('SRC/NSSpeech.mm')
  else
    sral_args += '-DSRAL_NO_NSSPEECH'
  endif
  
  sral_deps += dependency('appleframeworks', modules : ['AppKit', 'Foundation', 'AVFoundation'])

elif host_os == 'android'
  sral_sources += files(
    'SRC/AndroidTextToSpeech.cpp',
    'SRC/AndroidAccessibilityManager.cpp',
    'Dep/AndroidContext.cpp'
  )
  sral_deps += cpp.find_library('log')

else # Linux / Generic
  sral_sources += files(
    'Dep/utf-8.c', 
    'SRC/SpeechDispatcher.cpp', 
    'SRC/Orca.cpp'
  )
  sral_deps += dependency('speech-dispatcher')
  sral_deps += dependency('dbus-1')
  sral_deps += cpp.find_library('brlapi')
endif

sral_lib = library('SRAL',
  sral_sources,
  include_directories : inc_dir,
  dependencies : sral_deps,
  cpp_args : sral_args,
  c_args : sral_args,
  install : true
)

install_headers('Include/SRAL.h')

# Internal dependency for easy linking in tests
sral_dep = declare_dependency(
  link_with : sral_lib,
  include_directories : inc_dir,
  compile_args : sral_args
)

if build_test
  executable('SRAL_test',
    'Examples/C/SRALExample.c',
    dependencies : [sral_dep] + sral_deps
  )

  if host_os == 'windows'
    executable('SRAL_NVDAControleExConsole',
      ['Examples/C/NVDAControlExConsole.c', 'Dep/nvda_control.c'],
      include_directories : inc_dir
    )
  endif
endif

summary({
  'Build tests': build_test,
  'UIA support disabled': disable_uia,
  'NSSpeech disabled': disable_nsspeech,
  'Library type': get_option('default_library'),
  'C++ Standard': get_option('cpp_std'),
  'AccessKit found': ak_lib.found()
}, bool_yn: true, section: 'Configuration')