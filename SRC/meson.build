assert(meson.current_source_dir().endswith('SRC'), 'SRC/meson.build must be called via subdir() from the root directory')

root_src = meson.project_source_root()

c_id = c_compiler.get_id()

is_windows    = (target_machine.system() == 'windows')
is_apple      = (target_machine.system() == 'darwin' or target_machine.system() == 'ios')
is_android    = (target_machine.system() == 'android')
is_emscripten = (target_machine.system() == 'emscripten')

dep_sources = []
if is_windows
  dep_sources += [
    root_src / 'Dep/nvda_control.c',
    root_src / 'Dep/fsapi.c',
    root_src / 'Dep/blastspeak.c'
  ]
elif not is_apple and not is_android and not is_emscripten
  dep_sources += [root_src / 'Dep/utf-8.c']
endif

global_c_cpp_args = ['-Wall', '-Wextra']

if c_id != 'msvc' and c_id != 'clang-cl'
  global_c_cpp_args += [
    '-fstack-protector-all',
    '-fno-delete-null-pointer-checks',
    '-fwrapv',
    '-fno-strict-overflow',
    '-fno-common',
    '-ftrivial-auto-var-init=zero'
  ]
  
  if not is_emscripten
    global_c_cpp_args += ['-D_FORTIFY_SOURCE=3', '-fstack-clash-protection']
  endif

  if c_compiler.has_argument('-fcf-protection=full')
    global_c_cpp_args += ['-fcf-protection=full']
  endif
  if c_compiler.has_argument('-mbranch-protection=all')
    global_c_cpp_args += ['-mbranch-protection=all']
  endif
endif

supported_global_c_args   = c_compiler.get_supported_arguments(global_c_cpp_args)
supported_global_cpp_args = cpp_compiler.get_supported_arguments(global_c_cpp_args)

sral_deps_list = []
if dep_sources.length() > 0
  sral_deps_lib = static_library('sral_deps',
    dep_sources,
    include_directories: inc_dir,
    c_args: supported_global_c_args,
    pic: true
  )
  sral_deps_list += [sral_deps_lib]
endif

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

if c_id != 'msvc' and c_id != 'clang-cl'
  core_c_args   = supported_global_c_args   + c_compiler.get_supported_arguments(shared_warnings)
  core_cpp_args = supported_global_cpp_args + cpp_compiler.get_supported_arguments(shared_warnings + cpp_exclusive_warnings)
else
  core_c_args   = supported_global_c_args
  core_cpp_args = supported_global_cpp_args
endif

core_sources = files('Engine.cpp', 'SRAL.cpp')

if not is_emscripten
  if get_option('SRAL_WITH_ACCESSKIT')
    core_sources += files('ACAnnouncer.cpp')
  else
    core_c_args   += ['-DSRAL_NO_ACCESSKIT']
    core_cpp_args += ['-DSRAL_NO_ACCESSKIT']
  endif
endif

if is_windows
  core_sources += files('Encoding.cpp')
  if get_option('SRAL_WITH_NVDA')
    core_sources += files('NVDA.cpp')
  else
    core_c_args   += ['-DSRAL_NO_NVDA']
    core_cpp_args += ['-DSRAL_NO_NVDA']
  endif
  if get_option('SRAL_WITH_JAWS')
    core_sources += files('Jaws.cpp')
  else
    core_c_args   += ['-DSRAL_NO_JAWS']
    core_cpp_args += ['-DSRAL_NO_JAWS']
  endif
  if get_option('SRAL_WITH_ZDSR')
    core_sources += files('ZDSR.cpp')
  else
    core_c_args   += ['-DSRAL_NO_ZDSR']
    core_cpp_args += ['-DSRAL_NO_ZDSR']
  endif
  if get_option('SRAL_WITH_SAPI')
    core_sources += files('SAPI.cpp')
    core_sources += files(root_src / 'Dep/wasapi.cpp')
  else
    core_c_args   += ['-DSRAL_NO_SAPI']
    core_cpp_args += ['-DSRAL_NO_SAPI']
  endif
  if get_option('SRAL_WITH_UIA')
    core_sources += files('UIA.cpp')
    core_sources += files(root_src / 'Dep/UIAProvider.cpp')
  else
    core_c_args   += ['-DSRAL_NO_UIA']
    core_cpp_args += ['-DSRAL_NO_UIA']
  endif

elif is_apple
  if get_option('SRAL_WITH_VOICEOVER')
    core_sources += files('VoiceOver.mm')
  else
    core_c_args   += ['-DSRAL_NO_VOICEOVER']
    core_cpp_args += ['-DSRAL_NO_VOICEOVER']
  endif
  if get_option('SRAL_WITH_AVSPEECH')
    core_sources += files('AVSpeech.mm')
  else
    core_c_args   += ['-DSRAL_NO_AVSPEECH']
    core_cpp_args += ['-DSRAL_NO_AVSPEECH']
  endif
  if target_machine.system() != 'ios' and get_option('SRAL_WITH_NSSPEECH')
    core_sources += files('NSSpeech.mm')
  else
    core_c_args   += ['-DSRAL_NO_NSSPEECH']
    core_cpp_args += ['-DSRAL_NO_NSSPEECH']
  endif

elif is_android
  core_sources += files(root_src / 'Dep/AndroidContext.cpp')
  if get_option('SRAL_WITH_ANDROID_TTS')
    core_sources += files('AndroidTextToSpeech.cpp')
  else
    core_c_args   += ['-DSRAL_NO_ANDROID_TTS']
    core_cpp_args += ['-DSRAL_NO_ANDROID_TTS']
  endif
  if get_option('SRAL_WITH_ANDROID_ACCESSIBILITY')
    core_sources += files('AndroidAccessibilityManager.cpp')
  else
    core_c_args   += ['-DSRAL_NO_ANDROID_ACCESSIBILITY']
    core_cpp_args += ['-DSRAL_NO_ANDROID_ACCESSIBILITY']
  endif

elif is_emscripten
  if get_option('SRAL_WITH_CHROMEVOX')
    core_sources += files('ChromeVox.cpp')
  else
    core_c_args   += ['-DSRAL_NO_CHROMEVOX']
    core_cpp_args += ['-DSRAL_NO_CHROMEVOX']
  endif
  
  core_c_args   += ['-DSRAL_NO_NVDA', '-DSRAL_NO_JAWS', '-DSRAL_NO_ZDSR', '-DSRAL_NO_SAPI', '-DSRAL_NO_UIA', '-DSRAL_NO_SPEECH_DISPATCHER', '-DSRAL_NO_ORCA', '-DSRAL_NO_BRLAPI']
  core_cpp_args += ['-DSRAL_NO_NVDA', '-DSRAL_NO_JAWS', '-DSRAL_NO_ZDSR', '-DSRAL_NO_SAPI', '-DSRAL_NO_UIA', '-DSRAL_NO_SPEECH_DISPATCHER', '-DSRAL_NO_ORCA', '-DSRAL_NO_BRLAPI']

else
  core_sources += files('Encoding.cpp')
  if get_option('SRAL_WITH_SPEECH_DISPATCHER')
    core_sources += files('SpeechDispatcher.cpp')
  else
    core_c_args   += ['-DSRAL_NO_SPEECH_DISPATCHER']
    core_cpp_args += ['-DSRAL_NO_SPEECH_DISPATCHER']
  endif
  if get_option('SRAL_WITH_ORCA')
    core_sources += files('Orca.cpp')
  else
    core_c_args   += ['-DSRAL_NO_ORCA']
    core_cpp_args += ['-DSRAL_NO_ORCA']
  endif
endif

if not is_emscripten
  core_c_args   += ['-DSRAL_NO_CHROMEVOX']
  core_cpp_args += ['-DSRAL_NO_CHROMEVOX']
endif

supported_link_args = []

if c_id != 'msvc' and c_id != 'clang-cl'
  desired_link_args = [
    '-Wl,-z,relro',       
    '-Wl,-z,now',         
    '-Wl,-z,noexecstack', 
  ]
  if not is_apple
      desired_link_args += ['-Wl,-z,defs']
  else
    desired_link_args += ['-Wl,-undefined,error']
  endif
  supported_link_args = c_compiler.get_supported_link_arguments(desired_link_args)
else
  desired_msvc_link = ['/NXCOMPAT', '/DYNAMICBASE', '/HIGHENTROPYVA']
  if get_option('buildtype') == 'release'
    desired_msvc_link += ['/CETCOMPAT']
  endif
  supported_link_args = c_compiler.get_supported_link_arguments(desired_msvc_link)
endif

sral_target_type = build_shared_libs ? 'shared_library' : 'static_library'

if get_option('default_library') == 'both'
  sral_target_type = 'both_libraries'
endif

sral_core_c_args   = core_c_args
sral_core_cpp_args = core_cpp_args

if sral_target_type == 'shared_library' or sral_target_type == 'both_libraries'
  sral_core_c_args   += ['-DSRAL_BUILDING_DLL']
  sral_core_cpp_args += ['-DSRAL_BUILDING_DLL']
endif

sral_lib = build_target('SRAL',
  core_sources,
  target_type: sral_target_type,
  include_directories: [inc_dir, include_directories('../Dep')],
  link_with: sral_deps_list,          
  c_args: sral_core_c_args,
  cpp_args: sral_core_cpp_args, 
  link_args: supported_link_args,
  dependencies: sral_dependencies,
  pic: true,                         
  install: true,
  override_options: [
    'werror=true',
    'c_visibility=hidden',
    'cpp_visibility=hidden'
  ]
)