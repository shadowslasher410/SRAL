fs = import('fs')
assert(
  fs.name(meson.current_source_dir()) == 'SRC',
  'SRC/meson.build must be processed via subdir() from the project root directory.',
)

root_src = meson.project_source_root()
dep_sources = []

if is_windows
  dep_sources += [
    root_src / 'Dep/nvda_control.c',
    root_src / 'Dep/fsapi.c',
    root_src / 'Dep/blastspeak.c'
  ]
elif not (is_macos or is_ios or is_android or is_emscripten)
  dep_sources += [root_src / 'Dep/utf-8.c']
endif

core_c_args = []
core_cpp_args = []

# Local warnings flags mappings
local_shared_warnings = [
  '-Wformat=2', '-Wformat-security', '-Wconversion', '-Wsign-conversion',
  '-Wshadow', '-Wundef', '-Wcast-qual', '-Wswitch-enum', '-Wnull-dereference'
]
local_cpp_warnings = ['-Wctor-dtor-privacy', '-Woverloaded-virtual']

if not is_msvc_like
  core_c_args += c_compiler.get_supported_arguments(local_shared_warnings)
  core_cpp_args += cpp_compiler.get_supported_arguments(local_shared_warnings + local_cpp_warnings)
endif

# Inherited flags from root check validations
core_c_args += c_warning_args
core_cpp_args += cpp_warning_args

core_sources = files('Engine.cpp', 'SRAL.cpp')
local_feature_defs = []

if not is_emscripten
  if opt_accesskit
    core_sources += files('ACAnnouncer.cpp')
  else
    local_feature_defs += ['-DSRAL_NO_ACCESSKIT']
  endif
endif

if is_windows
  core_sources += files('Encoding.cpp')

  if opt_nvda
    core_sources += files('NVDA.cpp')
  else
    local_feature_defs += ['-DSRAL_NO_NVDA']
  endif

  if get_option('with_jaws').allowed()
    core_sources += files('Jaws.cpp')
  else
    local_feature_defs += ['-DSRAL_NO_JAWS']
  endif

  if get_option('with_zdsr').allowed()
    core_sources += files('ZDSR.cpp')
  else
    local_feature_defs += ['-DSRAL_NO_ZDSR']
  endif

  if get_option('with_sapi').allowed()
    core_sources += [files('SAPI.cpp'), root_src / 'Dep/wasapi.cpp']
  else
    local_feature_defs += ['-DSRAL_NO_SAPI']
  endif

  if opt_uia
    core_sources += [files('UIA.cpp'), root_src / 'Dep/UIAProvider.cpp']
  else
    local_feature_defs += ['-DSRAL_NO_UIA']
  endif

elif is_macos or is_ios
  if get_option('with_voiceover').allowed()
    core_sources += files('VoiceOver.mm')
  else
    local_feature_defs += ['-DSRAL_NO_VOICEOVER']
  endif

  if get_option('with_avspeech').allowed()
    core_sources += files('AVSpeech.mm')
  else
    local_feature_defs += ['-DSRAL_NO_AVSPEECH']
  endif

  if not is_ios and opt_nsspeech
    core_sources += files('NSSpeech.mm')
  else
    local_feature_defs += ['-DSRAL_NO_NSSPEECH']
  endif

elif is_android
  core_sources += files(root_src / 'Dep/AndroidContext.cpp')
  android_java_helpers = files(root_src / 'Dep/AndroidAccessibilityManagerHelper.java', root_src / 'Dep/AndroidTTSHelper.java')

  if javac_bin.found() and paths.get('jdk_install', '') != ''
    python3 = find_program('python3', required: true)
    jar_exe = find_program('jar', required: true)
    
    custom_target('SRALAndroidHelpers',
      input: android_java_helpers,
      output: 'SRALAndroidHelpers.jar',
      command: [
        python3, '-c', 
        'import os, subprocess, sys; ' +
        'os.makedirs("java_classes", exist_ok=True); ' +
        'javac_path = sys.argv[1]; ' +
        'jar_path = sys.argv[2]; ' +
        'out_jar = sys.argv[3]; ' +
        'java_files = sys.argv[4:]; ' +
        'subprocess.run([javac_path, "-d", "java_classes"] + java_files, check=True); ' +
        'subprocess.run([jar_path, "cf", out_jar, "-C", "java_classes", "."], check=True);',
        javac_bin.full_path(),
        jar_exe.full_path(),
        '@OUTPUT@',
        '@INPUT@'
      ],
      install: true,
      install_dir: get_option('datadir') / 'sral/android',
      build_by_default: true
    )
  endif

  if get_option('with_android_tts').allowed()
    core_sources += files('AndroidTextToSpeech.cpp')
  else
    local_feature_defs += ['-DSRAL_NO_ANDROID_TTS']
  endif

  if get_option('with_android_accessibility').allowed()
    core_sources += files('AndroidAccessibilityManager.cpp')
  else
    local_feature_defs += ['-DSRAL_NO_ANDROID_ACCESSIBILITY']
  endif

elif is_emscripten
  if opt_chromevox
    core_sources += files('ChromeVox.cpp')
  else
    local_feature_defs += ['-DSRAL_NO_CHROMEVOX']
  endif

  core_args_em = [
    '-DSRAL_NO_NVDA', '-DSRAL_NO_JAWS', '-DSRAL_NO_ZDSR', '-DSRAL_NO_SAPI', '-DSRAL_NO_UIA',
    '-DSRAL_NO_SPEECHD', '-DSRAL_NO_ORCA', '-DSRAL_NO_BRLAPI'
  ]
  core_c_args += core_args_em
  core_cpp_args += core_args_em

else
  core_sources += files('Encoding.cpp')

  if opt_speechd
    core_sources += files('SpeechDispatcher.cpp')
  else
    local_feature_defs += ['-DSRAL_NO_SPEECHD']
  endif

  if opt_orca
    core_sources += files('Orca.cpp')
  else
    local_feature_defs += ['-DSRAL_NO_ORCA']
  endif
endif

if not is_emscripten
  local_feature_defs += ['-DSRAL_NO_CHROMEVOX']
endif

sral_deps_list = []
if dep_sources.length() > 0
  sral_deps_lib = static_library('sral_deps', dep_sources,
    include_directories: [inc_dir, include_directories('../Dep')],
    c_args: core_c_args + local_feature_defs,
    cpp_args: core_cpp_args + local_feature_defs,
    pic: true
  )
  sral_deps_list += [sral_deps_lib]
endif

sral_core_objc_args = []
sral_core_objcpp_args = []
if is_macos or is_ios
  sral_core_objc_args = core_c_args
  sral_core_objcpp_args = core_cpp_args
endif

lib_type = get_option('default_library')
shared_macro_flags = local_feature_defs + ['-DSRAL_BUILDING_DLL', '-DWASAPI_EXPORTS']
static_macro_flags = local_feature_defs + ['-DSRAL_STATIC']

sral_targets = []

if lib_type == 'both'
  sral_shared = shared_library('SRAL', core_sources,
    include_directories: [inc_dir, include_directories('../Dep')],
    link_with: sral_deps_list,
    dependencies: sral_dependencies,
    c_args: core_c_args + shared_macro_flags,
    cpp_args: core_cpp_args + shared_macro_flags,
    objc_args: sral_core_objc_args + shared_macro_flags,
    objcpp_args: sral_core_objcpp_args + shared_macro_flags,
    install: true,
    gnu_symbol_visibility: 'hidden'
  )
  
  sral_static = static_library('SRAL_static', core_sources,
    include_directories: [inc_dir, include_directories('../Dep')],
    link_with: sral_deps_list,
    dependencies: sral_dependencies,
    c_args: core_c_args + static_macro_flags,
    cpp_args: core_cpp_args + static_macro_flags,
    objc_args: sral_core_objc_args + static_macro_flags,
    objcpp_args: sral_core_objcpp_args + static_macro_flags,
    install: true,
    pic: true
  )
  sral_targets = [sral_shared, sral_static]

elif lib_type == 'shared'
  sral_shared = shared_library('SRAL', core_sources,
    include_directories: [inc_dir, include_directories('../Dep')],
    link_with: sral_deps_list,
    dependencies: sral_dependencies,
    c_args: core_c_args + shared_macro_flags,
    cpp_args: core_cpp_args + shared_macro_flags,
    objc_args: sral_core_objc_args + shared_macro_flags,
    objcpp_args: sral_core_objcpp_args + shared_macro_flags,
    install: true,
    gnu_symbol_visibility: 'hidden'
  )
  sral_targets = [sral_shared]

else
  sral_static = static_library('SRAL', core_sources,
    include_directories: [inc_dir, include_directories('../Dep')],
    link_with: sral_deps_list,
    dependencies: sral_dependencies,
    c_args: core_c_args + static_macro_flags,
    cpp_args: core_cpp_args + static_macro_flags,
    objc_args: sral_core_objc_args + static_macro_flags,
    objcpp_args: sral_core_objcpp_args + static_macro_flags,
    install: true,
    pic: true
  )
  sral_targets = [sral_static]
endif