assert(meson.current_source_dir().endswith('SRC'), 'SRC/meson.build must be called via subdir() from the root directory')

root_src = meson.project_source_root()

is_windows_target   = (host_system == 'windows')
is_apple_target     = (host_system == 'darwin')
is_android_target   = (host_system == 'android')
is_emscripten_target = (host_system == 'emscripten')
is_ios_target       = get_option('TARGET_IOS')

dep_sources = []
if is_windows_target
  dep_sources += [
    root_src / 'Dep/nvda_control.c',
    root_src / 'Dep/fsapi.c',
    root_src / 'Dep/blastspeak.c'
  ]
elif not is_apple_target and not is_android_target and not is_emscripten_target
  dep_sources += [root_src / 'Dep/utf-8.c']
endif

global_c_cpp_args = ['-Wall', '-Wextra']

if c_id != 'msvc'
  global_c_cpp_args += [
    '-D_FORTIFY_SOURCE=3',
    '-fstack-protector-all',
    '-fstack-clash-protection',
    '-fno-delete-null-pointer-checks',
    '-fwrapv',
    '-fno-strict-overflow',
    '-fno-common',
    '-ftrivial-auto-var-init=zero'
  ]
  if c_compiler.has_argument('-fcf-protection=full')
    global_c_cpp_args += ['-fcf-protection=full']
  endif
  if c_compiler.has_argument('-mbranch-protection=all')
    global_c_cpp_args += ['-mbranch-protection=all']
  endif
endif

supported_global_c_args   = c_compiler.get_supported_arguments(global_c_cpp_args)
supported_global_cpp_args = cpp_compiler.get_supported_arguments(global_c_cpp_args)

sral_deps_lib = static_library('sral_deps',
  dep_sources,
  include_directories: inc_dir,
  c_args: supported_global_c_args,
  pic: true
)

shared_warnings = [
  '-Wformat=2',                      
  '-Wformat-security',
  '-Werror=format-security',         
  '-Wconversion',                    
  '-Wsign-conversion',               
  '-Wshadow',                        
  '-Wundef',                         
  '-Wcast-qual',                     
  '-Wswitch-enum',                   
  '-Wnull-dereference',              
]

cpp_exclusive_warnings = [
  '-Wold-style-cast',                
  '-Wnon-virtual-dtor',              
  '-Wctor-dtor-privacy',             
  '-Woverloaded-virtual',            
]

core_c_args   = supported_global_c_args   + c_compiler.get_supported_arguments(shared_warnings)
core_cpp_args = supported_global_cpp_args + cpp_compiler.get_supported_arguments(shared_warnings + cpp_exclusive_warnings)

core_sources = files('Engine.cpp', 'SRAL.cpp')

if get_option('SRAL_WITH_CHROMEVOX')
  core_sources += files('ChromeVox.cpp')
else
  core_c_args   += ['-DSRAL_NO_CHROMEVOX']
  core_cpp_args += ['-DSRAL_NO_CHROMEVOX']
endif

if is_windows_target
  core_sources += files('Encoding.cpp')
  if get_option('SRAL_WITH_NVDA')
    core_sources += files('NVDA.cpp')
  else
    core_c_args  += ['-DSRAL_NO_NVDA']
    core_cpp_args += ['-DSRAL_NO_NVDA']
  endif
  if get_option('SRAL_WITH_JAWS')
    core_sources += files('Jaws.cpp')
  else
    core_c_args  += ['-DSRAL_NO_JAWS']
    core_cpp_args += ['-DSRAL_NO_JAWS']
  endif
  if get_option('SRAL_WITH_ZDSR')
    core_sources += files('ZDSR.cpp')
  else
    core_c_args  += ['-DSRAL_NO_ZDSR']
    core_cpp_args += ['-DSRAL_NO_ZDSR']
  endif
  if get_option('SRAL_WITH_SAPI')
    core_sources += [files('SAPI.cpp'), root_src / 'Dep/wasapi.cpp']
  else
    core_c_args  += ['-DSRAL_NO_SAPI']
    core_cpp_args += ['-DSRAL_NO_SAPI']
  endif
  if get_option('SRAL_WITH_UIA')
    core_sources += [files('UIA.cpp'), root_src / 'Dep/UIAProvider.cpp']
  else
    core_c_args  += ['-DSRAL_NO_UIA']
    core_cpp_args += ['-DSRAL_NO_UIA']
  endif

elif is_apple_target
  if get_option('SRAL_WITH_VOICEOVER')
    core_sources += files('VoiceOver.mm')
  else
    core_c_args  += ['-DSRAL_NO_VOICEOVER']
    core_cpp_args += ['-DSRAL_NO_VOICEOVER']
  endif
  if get_option('SRAL_WITH_AVSPEECH')
    core_sources += files('AVSpeech.mm')
  else
    core_c_args  += ['-DSRAL_NO_AVSPEECH']
    core_cpp_args += ['-DSRAL_NO_AVSPEECH']
  endif
  if not is_ios_target and get_option('SRAL_WITH_NSSPEECH')
    core_sources += files('NSSpeech.mm')
  else
    core_c_args  += ['-DSRAL_NO_NSSPEECH']
    core_cpp_args += ['-DSRAL_NO_NSSPEECH']
  endif

elif is_android_target
  core_sources += [root_src / 'Dep/AndroidContext.cpp']
  if get_option('SRAL_WITH_ANDROID_TTS')
    core_sources += files('AndroidTextToSpeech.cpp')
  else
    core_c_args  += ['-DSRAL_NO_ANDROID_TTS']
    core_cpp_args += ['-DSRAL_NO_ANDROID_TTS']
  endif
  if get_option('SRAL_WITH_ANDROID_ACCESSIBILITY')
    core_sources += files('AndroidAccessibilityManager.cpp')
  else
    core_c_args  += ['-DSRAL_NO_ANDROID_ACCESSIBILITY']
    core_cpp_args += ['-DSRAL_NO_ANDROID_ACCESSIBILITY']
  endif

elif is_emscripten_target
else
  core_sources += files('Encoding.cpp', 'ACAnnouncer.cpp')
  if get_option('SRAL_WITH_SPEECH_DISPATCHER')
    core_sources += files('SpeechDispatcher.cpp')
  else
    core_c_args  += ['-DSRAL_NO_SPEECH_DISPATCHER']
    core_cpp_args += ['-DSRAL_NO_SPEECH_DISPATCHER']
  endif
  if get_option('SRAL_WITH_ORCA')
    core_sources += files('Orca.cpp')
  else
    core_c_args  += ['-DSRAL_NO_ORCA']
    core_cpp_args += ['-DSRAL_NO_ORCA']
  endif
endif

supported_link_args = []

if c_id != 'msvc'
  desired_link_args = [
    '-Wl,-z,relro',       
    '-Wl,-z,now',         
    '-Wl,-z,noexecstack', 
    '-Wl,-z,defs',
  ]
  supported_link_args = c_compiler.get_supported_link_arguments(desired_link_args)
else
  desired_msvc_link = ['/NXCOMPAT', '/DYNAMICBASE', '/HIGHENTROPYVA']
  supported_link_args = c_compiler.get_supported_link_arguments(desired_msvc_link)
endif

sral_lib = library('SRAL',
  core_sources,
  include_directories: inc_dir,
  dependencies: sral_dependencies,
  link_with: sral_deps_lib,          
  c_args: core_c_args,
  cpp_args: core_cpp_args, 
  link_args: supported_link_args,
  pic: true,                         
  werror: true,
  install: true,
  sral_lib = library('SRAL',
  core_sources,
  include_directories: inc_dir,
  dependencies: sral_dependencies,
  link_with: sral_deps_lib,          
  c_args: core_c_args,
  cpp_args: core_cpp_args, 
  link_args: supported_link_args,
  pic: true,                         
  install: true,
  override_options: [
    'werror=true',
    'c_visibility=hidden',
    'cpp_visibility=hidden'
  ]
)
