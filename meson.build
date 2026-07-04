project('SRAL', 'c', 'cpp',
  version : '1.0.0',
  meson_version : '>= 0.62.0',
  default_options : [
    'c_std=c17',
    'cpp_std=c++20',
    'cpp_eh=default',
    'b_vscrt=from_buildtype' 
  ]
)

# ==========================================
# Environment & Platform Identification
# ==========================================
cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

is_emscripten = (host_machine.system() == 'emscripten')
is_msvc = (cc.get_id() == 'msvc' or cpp.get_id() == 'msvc')

# ==========================================
# Simplified Option Bindings Heuristics
# ==========================================
build_shared = get_option('build_shared') and not is_emscripten

if build_shared
  library_type = 'shared_library'
else
  library_type = 'static_library'
endif

build_tests    = get_option('build_tests')
build_bindings = get_option('build_bindings')

binding_csharp = get_option('binding_csharp') and build_bindings
binding_rust   = get_option('binding_rust') and build_bindings
binding_node   = get_option('binding_node') and build_bindings
# ==========================================
# Windows Feature Backends
# ==========================================
if host_machine.system() == 'windows' and not is_emscripten
  with_nvda = get_option('with_nvda')
  with_jaws = get_option('with_jaws')
  with_zdsr = get_option('with_zdsr')
  with_sapi = get_option('with_sapi')
  with_uia  = get_option('with_uia')
else
  with_nvda = false
  with_jaws = false
  with_zdsr = false
  with_sapi = false
  with_uia  = false
endif

# ==========================================
# Apple, Android, & Linux Feature Routing
# ==========================================
is_ios = false
if host_machine.system() == 'darwin' and not is_emscripten
  if host_machine.subsystem() in ['ios', 'tvos', 'watchos']
    is_ios = true
  endif

  with_voiceover = get_option('with_voiceover')
  with_avspeech  = get_option('with_avspeech')
  with_nsspeech  = not is_ios and get_option('with_nsspeech')
else
  with_voiceover = false
  with_avspeech  = false
  with_nsspeech  = false
endif

if host_machine.system() == 'android' and not is_emscripten
  with_android_tts           = get_option('with_android_tts')
  with_android_accessibility = get_option('with_android_accessibility')
else
  with_android_tts           = false
  with_android_accessibility = false
endif

if host_machine.system() == 'linux' and not is_emscripten
  with_speech_dispatcher = get_option('with_speech_dispatcher')
  with_orca              = get_option('with_orca')
  with_brlapi            = get_option('with_brlapi')
  with_chromevox         = get_option('with_chromevox')
elif is_emscripten
  with_chromevox         = get_option('with_chromevox')
  with_speech_dispatcher = false
  with_orca              = false
  with_brlapi            = false
else
  with_chromevox         = false
  with_speech_dispatcher = false
  with_orca              = false
  with_brlapi            = false
endif
# ==========================================
# Core Source File Processing Lists
# ==========================================
inc_dir = include_directories('Include')

sral_sources = files(
  'SRC/Encoding.cpp',
  'SRC/Engine.cpp',
  'SRC/SRAL.cpp'
)

sral_headers = files(
  'Include/SRAL.h',
  'Include/Sral.hpp'
)

sral_defs = []
sral_compile_args = []
sral_link_libs = []

if with_chromevox
  sral_sources += files('SRC/ChromeVox.cpp')
else
  sral_defs += '-DSRAL_NO_CHROMEVOX'
endif

# ==========================================
# Source Maps and Architecture Conditions
# ==========================================

if host_machine.system() == 'windows' and not is_emscripten
  sral_defs += '-D_CRT_SECURE_NO_WARNINGS'
  sral_link_libs += [cc.find_library('ole32'), cc.find_library('user32')]
  
  if with_nvda
    sral_sources += files('SRC/NVDA.cpp', 'Dep/nvda_control.c')
  else
    sral_defs += '-DSRAL_NO_NVDA'
  endif
  
  if with_jaws
    sral_sources += files('SRC/Jaws.cpp', 'Dep/fsapi.c')
  else
    sral_defs += '-DSRAL_NO_JAWS'
  endif
  
  if with_zdsr
    sral_sources += files('SRC/ZDSR.cpp')
  else
    sral_defs += '-DSRAL_NO_ZDSR'
  endif
  
  if with_sapi
    sral_sources += files('SRC/SAPI.cpp', 'Dep/blastspeak.c', 'Dep/wasapi.cpp')
  else
    sral_defs += '-DSRAL_NO_SAPI'
  endif
  
  if with_uia
    sral_sources += files('SRC/UIA.cpp', 'Dep/UIAProvider.cpp')
    sral_link_libs += cc.find_library('uiautomationcore')
  else
    sral_defs += '-DSRAL_NO_UIA'
  endif

elif host_machine.system() == 'darwin' and not is_emscripten
  add_languages('objc', 'objcpp', native : false)
  add_project_arguments('-fobjc-arc', language : ['objc', 'objcpp'])
  
  sral_link_libs += [dependency('appleframeworks', modules : ['Foundation', 'AVFoundation'])]
  
  if with_voiceover
    sral_sources += files('SRC/VoiceOver.mm')
  else
    sral_defs += '-DSRAL_NO_VOICEOVER'
  endif
  
  if with_avspeech
    sral_sources += files('SRC/AVSpeech.mm')
  else
    sral_defs += '-DSRAL_NO_AVSPEECH'
  endif
  
  if with_nsspeech
    sral_sources += files('SRC/NSSpeech.mm')
    sral_link_libs += dependency('appleframeworks', modules : ['AppKit'])
  else
    sral_defs += '-DSRAL_NO_NSSPEECH'
  endif

elif host_machine.system() == 'android' and not is_emscripten
  sral_sources += files('Dep/AndroidContext.cpp')
  sral_link_libs += cc.find_library('log')
  
  if with_android_tts
    sral_sources += files('SRC/AndroidTextToSpeech.cpp')
  else
    sral_defs += '-DSRAL_NO_ANDROID_TTS'
  endif
  
  if with_android_accessibility
    sral_sources += files('SRC/AndroidAccessibilityManager.cpp')
  else
    sral_defs += '-DSRAL_NO_ANDROID_ACCESSIBILITY'
  endif

elif is_emscripten
  sral_compile_args += '-pthread'
  sral_link_libs     += cc.find_library('pthread')

else # Standard Desktop Linux
  sral_sources += files('Dep/utf-8.c', 'SRC/ACAnnouncer.cpp')
  
  if with_speech_dispatcher
    sral_sources += files('SRC/SpeechDispatcher.cpp')
    sral_link_libs += dependency('speech-dispatcher', required : true)
  else
    sral_defs += '-DSRAL_NO_SPEECH_DISPATCHER'
  endif
  
  if with_orca
    sral_sources += files('SRC/Orca.cpp')
    sral_link_libs += [dependency('dbus-1', required : true), dependency('atspi-2', required : true)]
  else
    sral_defs += '-DSRAL_NO_ORCA'
  endif
  
  if with_brlapi
    sral_link_libs += cc.find_library('brlapi', required : true)
  else
    sral_defs += '-DSRAL_NO_BRLAPI'
  endif
endif

# ==========================================
# Java Resolution and Object Generation Targets
# ==========================================
if host_machine.system() == 'android' and not is_emscripten
  message('[SRAL] Android target context recognized.')
elif not is_emscripten
  sral_link_libs += dependency('jni', version : '>=1.8', required : true)
endif

sral_shared_link_args = []
if is_emscripten
  sral_shared_link_args += ['-sWASM=1', '-sSIDE_MODULE=1', '-pthread']
endif

# Main Library Compiling Pipeline
if library_type == 'shared_library'
  shared_defs = sral_defs + ['-DWASAPI_EXPORTS']
  sral_lib = shared_library('SRAL',
    sral_sources,
    include_directories : inc_dir,
    cpp_args : shared_defs,
    c_args : shared_defs,
    dependencies : sral_link_libs,
    link_args : sral_shared_link_args,
    install : true
  )
else
  static_defs = sral_defs + ['-DSRAL_STATIC']
  static_out_name = (host_machine.system() == 'windows') ? 'SRAL_static' : 'SRAL'
  sral_lib = static_library(static_out_name,
    sral_sources,
    include_directories : inc_dir,
    cpp_args : static_defs,
    c_args : static_defs,
    dependencies : sral_link_libs,
    install : true
  )
endif

install_headers(sral_headers, subdir : 'SRAL')

# ==========================================
# Test Target Suites & Language Custom targets
# ==========================================
if build_tests and not is_emscripten and host_machine.system() != 'android'
  executable('SRAL_test', 'Examples/C/SRALExample.c',
    include_directories : inc_dir, link_with : sral_lib, install : false)

  if host_machine.system() == 'windows' and with_nvda
    executable('SRAL_NVDAControlExConsole', 'Examples/C/NVDAControlExConsole.c',
      include_directories : inc_dir, link_with : sral_lib, install : false)
  endif

  if host_machine.system() == 'darwin' and not is_ios
    executable('SRAL_test_cocoa', 'Examples/ObjC/SRALCocoaExample.m',
      include_directories : inc_dir, link_with : sral_lib, objc_args : ['-fobjc-arc'], install : false)
  endif
endif

if build_bindings
  if binding_csharp
    dotnet_exe = find_program('dotnet', required : false)
    if dotnet_exe.found()
      custom_target('sral_csharp_binding',
        command : [dotnet_exe, 'build', '--configuration', 'Release', '--restore', '-p:SKIP_MOBILE_WORKLOADS=true'],
        working_directory : meson.current_source_dir() / 'Bindings/C#',
        output : 'sral_csharp_assembly_placeholder', build_by_default : true)
    endif
  endif

  if binding_rust
    cargo_exe = find_program('cargo', required : false)
    if cargo_exe.found()
      custom_target('sral_rust_binding', command : [cargo_exe, 'build', '--release'],
        working_directory : meson.current_source_dir() / 'Bindings/Rust',
        output : 'sral_rust_crate_placeholder', build_by_default : true)
    endif
  endif

  if binding_node
    npm_exe = find_program('npm', 'npm.cmd', required : false)
    if npm_exe.found()
      custom_target('sral_nodejs_binding', command : [npm_exe, 'install'],
        working_directory : meson.current_source_dir() / 'Bindings/Node.Js',
        output : 'sral_node_addon_placeholder', build_by_default : true)
    endif
  endif
endif

# ==========================================
# Installation Data Staging Arrays
# ==========================================
if get_option('binding_sources')
  install_data(['Bindings/C#/Sral.cs', 'Bindings/C#/Example.cs', 'Bindings/C#/SRAL.csproj'], install_dir : get_option('datadir') / 'sral/bindings/csharp')
  install_data(['Bindings/Dart/sral.dart', 'Bindings/Dart/sral_types.dart', 'Bindings/Dart/example.dart'], install_dir : get_option('datadir') / 'sral/bindings/dart')
  install_subdir('Bindings/go', install_dir : get_option('datadir') / 'sral/bindings')
  install_data(['Bindings/Lua/sral.lua', 'Bindings/Lua/example.lua'], install_dir : get_option('datadir') / 'sral/bindings/lua')
  install_data(['Bindings/Python/sral.py', 'Bindings/Python/Example.py', 'Bindings/Python/pyproject.toml'], install_dir : get_option('datadir') / 'sral/bindings/python')
  install_subdir('Bindings/Rust/src', install_dir : get_option('datadir') / 'sral/bindings/rust')
  install_data('Bindings/Rust/Cargo.toml', install_dir : get_option('datadir') / 'sral/bindings/rust')
  install_data(['Bindings/WebAssembly/sral_wasm.cpp', 'Bindings/WebAssembly/sral_wasm.js'], install_dir : get_option('datadir') / 'sral/bindings/wasm')
  install_data(['Dep/AndroidTTSHelper.java', 'Dep/AndroidAccessibilityManagerHelper.java', 'Dep/proguard-rules.pro'], install_dir : get_option('datadir') / 'sral/android')
endif

pkg_mod = import('pkgconfig')
pkg_mod.generate(
  name : 'SRAL',
  filebase : 'sral',
  description : 'Screen Reader Abstraction Layer Core API Library Wrapper',
  libraries : sral_lib,
  subdirs : 'SRAL'
)
