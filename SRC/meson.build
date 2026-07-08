cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

dep_sources = []
if is_windows
  dep_sources += files('../Dep/nvda_control.c', '../Dep/fsapi.c', '../Dep/blastspeak.c')
elif not is_apple and not is_android and not is_emscripten
  dep_sources += files('../Dep/utf-8.c')
endif

global_args = [
  '-D_FORTIFY_SOURCE=3',
  '-fstack-protector-all',
  '-fstack-clash-protection',
  '-fno-delete-null-pointer-checks',
  '-fwrapv',
  '-fno-strict-overflow',
  '-fno-common',
  '-ftrivial-auto-var-init=zero',
  '-Wall', '-Wextra'
]

if cc.has_argument('-fcf-protection=full')
  global_args += '-fcf-protection=full'
endif
if cc.has_argument('-mbranch-protection=all')
  global_args += '-mbranch-protection=all'
endif

supported_global_args = cc.get_supported_arguments(global_args)

sral_deps_lib = static_library('sral_deps',
  dep_sources,
  include_directories: inc_dir,
  c_args: supported_global_args,
  pic: true
)

core_args = supported_global_args + cc.get_supported_arguments([
  '-Werror',
  '-Wformat=2',                      
  '-Wformat-security',
  '-Werror=format-security',         
  '-Wconversion',                    
  '-Wsign-conversion',               
  '-Wshadow',                        
  '-Wundef',                         
  '-Wold-style-cast',                
  '-Wcast-qual',                     
  '-Wswitch-enum',                   
  '-Wnon-virtual-dtor',              
  '-Wctor-dtor-privacy',             
  '-Woverloaded-virtual',            
  '-Wnull-dereference',              
])

core_sources = files('Engine.cpp', 'SRAL.cpp')

if get_option('SRAL_WITH_CHROMEVOX')
  core_sources += files('ChromeVox.cpp')
else
  core_args += '-DSRAL_NO_CHROMEVOX'
endif

if is_windows
  core_sources += files('Encoding.cpp')
  if get_option('SRAL_WITH_NVDA')
    core_sources += files('NVDA.cpp')
  else
    core_args += '-DSRAL_NO_NVDA'
  endif
  if get_option('SRAL_WITH_JAWS')
    core_sources += files('Jaws.cpp')
  else
    core_args += '-DSRAL_NO_JAWS'
  endif
  if get_option('SRAL_WITH_ZDSR')
    core_sources += files('ZDSR.cpp')
  else
    core_args += '-DSRAL_NO_ZDSR'
  endif
  if get_option('SRAL_WITH_SAPI')
    core_sources += files('SAPI.cpp', '../Dep/wasapi.cpp')
  else
    core_args += '-DSRAL_NO_SAPI'
  endif
  if get_option('SRAL_WITH_UIA')
    core_sources += files('UIA.cpp', '../Dep/UIAProvider.cpp')
  else
    core_args += '-DSRAL_NO_UIA'
  endif

elif is_apple
  if get_option('SRAL_WITH_VOICEOVER')
    core_sources += files('VoiceOver.mm')
  else
    core_args += '-DSRAL_NO_VOICEOVER'
  endif
  if get_option('SRAL_WITH_AVSPEECH')
    core_sources += files('AVSpeech.mm')
  else
    core_args += '-DSRAL_NO_AVSPEECH'
  endif
  if not is_ios and get_option('SRAL_WITH_NSSPEECH')
    core_sources += files('NSSpeech.mm')
  else
    core_args += '-DSRAL_NO_NSSPEECH'
  endif

elif is_android
  core_sources += files('../Dep/AndroidContext.cpp')
  if get_option('SRAL_WITH_ANDROID_TTS')
    core_sources += files('AndroidTextToSpeech.cpp')
  else
    core_args += '-DSRAL_NO_ANDROID_TTS'
  endif
  if get_option('SRAL_WITH_ANDROID_ACCESSIBILITY')
    core_sources += files('AndroidAccessibilityManager.cpp')
  else
    core_args += '-DSRAL_NO_ANDROID_ACCESSIBILITY'
  endif

elif is_emscripten
else
  core_sources += files('Encoding.cpp', 'ACAnnouncer.cpp')
  if get_option('SRAL_WITH_SPEECH_DISPATCHER')
    core_sources += files('SpeechDispatcher.cpp')
  else
    core_args += '-DSRAL_NO_SPEECH_DISPATCHER'
  endif
  if get_option('SRAL_WITH_ORCA')
    core_sources += files('Orca.cpp')
  else
    core_args += '-DSRAL_NO_ORCA'
  endif
endif

desired_link_args = [
  '-Wl,-z,relro',       
  '-Wl,-z,now',         
  '-Wl,-z,noexecstack', 
  '-Wl,-z,defs',
]

supported_link_args = cc.get_supported_link_arguments(desired_link_args)

sral_lib = library('SRAL',
  core_sources,
  include_directories: inc_dir,
  dependencies: sral_dependencies,
  link_with: sral_deps_lib,          
  c_args: core_args,
  cpp_args: core_args, 
  link_args: supported_link_args,
  pic: true,                         
  gnu_symbol_visibility: 'hidden',
  
  override_options: [
    'b_lto=true',                    
    'b_lundef=true',                 
    'b_asneeded=true',               
    'b_pgo=off',                     
    'cpp_std=c++20',                 
    'werror=true'                    
  ],
  install: true
)
