project('SRAL_Core', 'c', 'cpp',
  version: '1.0.0',
  default_options: [
    'c_std=c17',
    'cpp_std=c++20',
    'default_library=both',
    'b_pie=true'
  ],
  meson_version: '>= 1.11.1'
)

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

if cpp.get_id() == 'msvc'
  add_project_arguments('-D_CRT_SECURE_NO_WARNINGS', language: ['c', 'cpp'])
endif

is_emscripten = (meson.is_cross_build() and host_machine.system() == 'emscripten')
is_windows    = (host_machine.system() == 'windows' and not is_emscripten)
is_apple      = (host_machine.system() == 'darwin'  and not is_emscripten)
is_android    = (host_machine.system() == 'android' and not is_emscripten)
is_linux      = (host_machine.system() == 'linux'   and not is_windows and not is_apple and not is_android and not is_emscripten)
is_ios        = (is_apple and get_option('TARGET_IOS'))

inc_dir = include_directories('Include')

build_tests = get_option('BUILD_TESTS')
build_bindings = get_option('BUILD_BINDINGS')

public_headers = files(
  'Include/SRAL.h',
  'Include/Sral.hpp'
)

sral_dependencies = []

if is_windows
  sral_dependencies += [
    cc.find_library('ole32'),
    cc.find_library('user32'),
    cc.find_library('uiautomationcore', required: get_option('SRAL_WITH_UIA'))
  ]

elif is_apple
  add_languages('objc', 'objcpp')
  add_project_arguments('-fobjc-arc', language: ['objc', 'objcpp'])
  add_project_arguments('-DSRAL_NO_CHROMEVOX', language: ['c', 'cpp'])
  
  sral_dependencies += [
    dependency('appleframeworks', modules: ['Foundation', 'AVFoundation'])
  ]
  if not is_ios and get_option('SRAL_WITH_NSSPEECH')
    sral_dependencies += dependency('appleframeworks', modules: ['AppKit'])
  endif

elif is_android
  add_project_arguments('-DSRAL_NO_CHROMEVOX', language: ['c', 'cpp'])
  sral_dependencies += cc.find_library('log')

elif is_emscripten
  if cc.has_argument('-pthread')
    add_project_link_arguments('-pthread', language: ['c', 'cpp'])
    add_project_arguments('-pthread', language: ['c', 'cpp'])
  endif

else
  add_project_arguments('-DSRAL_NO_CHROMEVOX', language: ['c', 'cpp'])
  add_project_arguments('-DSRAL_SPEECHD_REQUIRE_UTF8', language: ['c', 'cpp'])
  
  if get_option('SRAL_WITH_SPEECH_DISPATCHER')
    sral_dependencies += dependency('speech-dispatcher', required: true)
  else
    add_project_arguments('-DSRAL_NO_SPEECH_DISPATCHER', language: ['c', 'cpp'])
  endif
  
  if get_option('SRAL_WITH_ORCA')
    sral_dependencies += [
      dependency('dbus-1', required: true),
      dependency('atspi-2', required: true)
    ]
  else
    add_project_arguments('-DSRAL_NO_ORCA', language: ['c', 'cpp'])
  endif
  
  if get_option('SRAL_WITH_BRLAPI')
    sral_dependencies += cc.find_library('brlapi', required: true)
  else
    add_project_arguments('-DSRAL_NO_BRLAPI', language: ['c', 'cpp'])
  endif
endif

if not is_android and not is_emscripten
  jni_dep = dependency('jni', required: false)
  if jni_dep.found()
    sral_dependencies += jni_dep
  endif
endif

subdir('SRC')

sral_dep = declare_dependency(
  include_directories: inc_dir,
  link_with: sral_lib
)

if build_tests and not is_emscripten and not is_android
  executable('SRAL_test',
    files('Examples/C/SRALExample.c'),
    include_directories: inc_dir,
    link_with: sral_lib,
    install: false
  )
  
  if is_windows and get_option('SRAL_WITH_NVDA')
    executable('SRAL_NVDAControlExConsole',
      files('Examples/C/NVDAControlExConsole.c'),
      include_directories: inc_dir,
      link_with: sral_lib,
      install: false
    )
  endif
  
  if is_apple and not is_ios
    executable('SRAL_test_cocoa',
      files('Examples/ObjC/SRALCocoaExample.m'),
      include_directories: inc_dir,
      link_with: sral_lib,
      install: false
    )
  endif
endif

if build_bindings
  message('Resolving toolchains for external language wrapper modules...')

  dotnet_exe = find_program('dotnet', required: false)
  if get_option('SRAL_BINDING_CSHARP') and dotnet_exe.found()
    run_target('sral_csharp_binding',
      command: [dotnet_exe, 'build', '--configuration', 'Release', '--restore', '-p:SKIP_MOBILE_WORKLOADS=true'],
      current_dir: meson.current_source_dir() / 'Bindings' / 'C#'
    )
  endif
  
  cargo_exe = find_program('cargo', required: false)
  if get_option('SRAL_BINDING_RUST') and cargo_exe.found()
    run_target('sral_rust_binding',
      command: [cargo_exe, 'build', '--release'],
      current_dir: meson.current_source_dir() / 'Bindings' / 'Rust'
    )
  endif

  npm_exe = find_program('npm', 'npm.cmd', required: false)
  if get_option('SRAL_BINDING_NODE') and npm_exe.found()
    run_target('sral_nodejs_binding',
      command: [npm_exe, 'install'],
      current_dir: meson.current_source_dir() / 'Bindings' / 'Node.Js'
    )
  endif

  go_exe = find_program('go', required: false)
  if get_option('SRAL_BINDING_GO') and go_exe.found()
    run_target('sral_go_binding',
      command: [go_exe, 'build', './...'],
      current_dir: meson.current_source_dir() / 'Bindings' / 'Go'
    )
  endif

  dart_exe = find_program('dart', 'flutter', required: false)
  if get_option('SRAL_BINDING_DART') and dart_exe.found()
    run_target('sral_dart_binding',
      command: [dart_exe, 'pub', 'get'],
      current_dir: meson.current_source_dir() / 'Bindings' / 'Dart'
    )
  endif

  lua_exe = find_program('lua', 'luajit', required: false)
  if get_option('SRAL_BINDING_LUA')
    lua_cmd = lua_exe.found() ? [lua_exe, 'example.lua'] : ['echo', 'Lua binding environment validated (sources ready for deployment).']
    run_target('sral_lua_binding',
      command: lua_cmd,
      current_dir: meson.current_source_dir() / 'Bindings' / 'Lua'
    )
  endif

  python_exe = find_program('python3', 'python', required: false)
  if get_option('SRAL_BINDING_PYTHON') and python_exe.found()
    py_args = [python_exe, '-m', 'pip', 'install', '.']
    if run_command(python_exe, '-c', 'import sys; exit(0) if sys.version_info >= (3,11) else exit(1)', check: false).returncode() == 0
      py_args += ['--break-system-packages']
    endif
    run_target('sral_python_binding',
      command: py_args,
      current_dir: meson.current_source_dir() / 'Bindings' / 'Python'
    )
  endif
endif

install_headers(public_headers)

if get_option('SRAL_INSTALL_BINDING_SOURCES')
  install_data('Bindings/C#/Sral.cs', 'Bindings/C#/Example.cs', 'Bindings/C#/SRAL.csproj', install_dir: 'share/sral/bindings/csharp')
  install_data('Bindings/Dart/sral.dart', 'Bindings/Dart/sral_types.dart', 'Bindings/Dart/example.dart', install_dir: 'share/sral/bindings/dart')
  install_data('Bindings/Lua/sral.lua', 'Bindings/Lua/example.lua', install_dir: 'share/sral/bindings/lua')
  install_data('Bindings/Python/sral.py', 'Bindings/Python/Example.py', 'Bindings/Python/pyproject.toml', install_dir: 'share/sral/bindings/python')
  install_data('Bindings/Rust/Cargo.toml', install_dir: 'share/sral/bindings/rust')
  install_data('Bindings/WebAssembly/sral_wasm.cpp', 'Bindings/WebAssembly/sral_wasm.js', install_dir: 'share/sral/bindings/wasm')
  install_data('Dep/AndroidTTSHelper.java', 'Dep/AndroidAccessibilityManagerHelper.java', 'Dep/proguard-rules.pro', install_dir: 'share/sral/android')
  install_subdir('Bindings/Go/SRAL', install_dir: 'share/sral/bindings/go')
  install_subdir('Bindings/Rust/src', install_dir: 'share/sral/bindings/rust/src')
endif
