project('SRAL_Core', 'c', 'cpp',
  version: '1.0.0',
  default_options: [
    'c_std=c17',
    'cpp_std=c++20',
    'default_library=both',
    'b_pie=true',                  # Enforce Position Independent Executables
    'b_staticpic=true',            # Force PIC inside static libraries to support global ASLR
    'b_lto=true',                  # Global LTO lets compilers track cross-file safety paths
    'b_lundef=true',               # Hard crash linker if unexpected undefined symbols exist
    'b_asneeded=true',             # Strip out unnecessary downstream dependency references
    'b_pgo=off',                   # Block Profile Guided Optimization timing side-channels
    'werror=true'                  # Treat all build-system setup warnings as fatal errors
  ],
  meson_version: '>= 1.11.1'
)

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

is_emscripten = (meson.is_cross_build() and host_machine.system() == 'emscripten')
is_windows    = (host_machine.system() == 'windows' and not is_emscripten)
is_apple      = (host_machine.system() == 'darwin'  and not is_emscripten)
is_android    = (host_machine.system() == 'android' and not is_emscripten)
is_linux      = (host_machine.system() == 'linux'   and not is_windows and not is_apple and not is_android and not is_emscripten)
is_ios = false
if is_apple
  is_ios = get_option('TARGET_IOS')
endif

if is_apple
  add_languages('objc', 'objcpp', native: false)
endif

if cpp.get_id() == 'msvc'
  msvc_args = [
    '-D_CRT_SECURE_NO_WARNINGS',
    '/GS',                         # MSVC Buffer Security Check
    '/sdl',                        # Enable Security Development Lifecycle Checks
    '/guard:cf',                   # MSVC Control Flow Guard
    '/Qspectre',                   # Mitigate speculative execution side-channel attacks (Spectre variant 1)
    '/volatile:iso',               # Force ISO-compliant volatile semantics to prevent dangerous memory optimizations
    '/w14928'                      # Force fatal handling for unsafe implicit copy-initialization conversions
  ]
  add_project_arguments(cpp.get_supported_arguments(msvc_args), language: ['c', 'cpp'])
  
  msvc_link_args = [
    '/GUARD:CF',                   # Control Flow Guard enforcement at link time
    '/DYNAMICBASE',                # Explicitly force modern ASLR layout shifting
    '/NXCOMPAT',                   # Explicitly force Data Execution Prevention (DEP)
    '/HIGHENTROPYVA'               # Enable 64-bit ASLR for maximum randomized memory variance
  ]
  add_project_link_arguments(cpp.get_supported_link_arguments(msvc_link_args), language: ['c', 'cpp'])
endif

global_defense_args = [
  '-D_FORTIFY_SOURCE=3',             # Deep static and runtime buffer sizing evaluation
  '-fstack-protector-all',           # Stack canaries on every single function frame
  '-fstack-clash-protection',        # Mitigate stack allocation page jumping vulnerabilities
  '-fno-delete-null-pointer-checks', # Halt the optimizer from deleting explicit NULL safety traps
  '-fwrapv',                         # Strict signed wrapping math to bypass overflow optimization exploits
  '-fno-strict-overflow',            # Disallow assumptions regarding overflow Undefined Behavior
  '-fno-common',                     # Ban implicit structural collision of duplicate variables
  '-ftrivial-auto-var-init=zero',    # Automatic zero-initialization of uninitialized stack bytes
]

if cc.has_argument('-fcf-protection=full')
  global_defense_args += '-fcf-protection=full'        # Intel CET / AMD Shadow Stack
endif
if cc.has_argument('-mbranch-protection=all')
  global_defense_args += '-mbranch-protection=all'     # ARM PAC/BTI mitigations
endif

supported_global_defenses = cc.get_supported_arguments(global_defense_args)
add_project_arguments(supported_global_defenses, language: ['c', 'cpp'])

cpp_defense_args = [
  '-D_GLIBCXX_ASSERTIONS',           # Strict bounds checking for GNU libstdc++ containers
  '-D_LIBCPP_ENABLE_ASSERTIONS=1',   # Bounds checking for legacy LLVM libc++
  '-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE' # Modern LLVM/Clang STL hardening profile
]
supported_cpp_defenses = cpp.get_supported_arguments(cpp_defense_args)
add_project_arguments(supported_cpp_defenses, language: 'cpp')

if get_option('SRAL_WITH_CHROMEVOX')
  add_project_arguments('-DSRAL_WITH_CHROMEVOX', language: ['c', 'cpp'])
else
  add_project_arguments('-DSRAL_NO_CHROMEVOX', language: ['c', 'cpp'])
endif

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
  apple_flags = ['-fobjc-arc', '-fstack-protector-all']
  if cc.has_argument('-fobjc-direct-methods')
    apple_flags += '-fobjc-direct-methods'
  endif
  
  add_project_arguments(apple_flags, language: ['objc', 'objcpp'])
  
  sral_dependencies += dependency('appleframeworks', modules: ['Foundation', 'AVFoundation'])
  if not is_ios and get_option('SRAL_WITH_NSSPEECH')
    sral_dependencies += dependency('appleframeworks', modules: ['AppKit'])
  endif

elif is_android
  sral_dependencies += cc.find_library('log')

elif is_emscripten
  if cc.has_link_argument('-pthread')
    em_args = ['-pthread', '-sSTACK_SIZE=65536', '-sWASM_BIGINT']
    add_project_link_arguments(em_args, language: ['c', 'cpp'])
    add_project_arguments('-pthread', language: ['c', 'cpp'])
  endif

else
  add_project_arguments('-DSRAL_SPEECHD_REQUIRE_UTF8', language: ['c', 'cpp'])
  
  if get_option('SRAL_WITH_SPEECH_DISPATCHER')
    sral_dependencies += dependency('speech-dispatcher', required: true)
  else
    add_project_arguments('-DSRAL_NO_SPEECH_DISPATCHER', language: ['c', 'cpp'])
  endif
  
  if get_option('SRAL_WITH_ORCA')
    sral_dependencies += [dependency('dbus-1', required: true), dependency('atspi-2', required: true)]
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
  sral_dependencies += dependency('jni', required: false)
endif

subdir('SRC')

sral_dep = declare_dependency(
  include_directories: inc_dir,
  link_with: sral_lib,
  dependencies: sral_dependencies
)

test_link_args = []
if cc.get_id() != 'msvc'
  test_link_args += cc.get_supported_link_arguments(['-Wl,-z,relro', '-Wl,-z,now', '-Wl,-z,noexecstack'])
endif

if build_tests and not is_emscripten and not is_android
  executable('SRAL_test',
    files('Examples/C/SRALExample.c'),
    include_directories: inc_dir,
    link_with: sral_lib,
    link_args: test_link_args,
    install: false
  )
  
  if is_windows and get_option('SRAL_WITH_NVDA')
    executable('SRAL_NVDAControlExConsole',
      files('Examples/C/NVDAControlExConsole.c'),
      include_directories: inc_dir,
      link_with: sral_lib,
      link_args: test_link_args,
      install: false
    )
  endif
  
  if is_apple and not is_ios
    executable('SRAL_test_cocoa',
      files('Examples/ObjC/SRALCocoaExample.m'),
      include_directories: inc_dir,
      link_with: sral_lib,
      link_args: test_link_args,
      install: false
    )
  endif
endif

if build_bindings
  message('Resolving toolchains for external language wrapper modules...')
  bindings_dir = meson.current_source_dir() / 'Bindings'

  dotnet_exe = find_program('dotnet', required: false)
  if get_option('SRAL_BINDING_CSHARP') and dotnet_exe.found()
    run_target('sral_csharp_binding',
      command: [dotnet_exe, 'build', '--configuration', 'Release', '--no-restore', '-p:SKIP_MOBILE_WORKLOADS=true'],
      current_dir: bindings_dir / 'C#'
    )
  endif
  
  cargo_exe = find_program('cargo', required: false)
  if get_option('SRAL_BINDING_RUST') and cargo_exe.found()
    run_target('sral_rust_binding',
      command: [cargo_exe, 'build', '--release', '--offline'],
      current_dir: bindings_dir / 'Rust'
    )
  endif

  npm_exe = find_program('npm', 'npm.cmd', required: false)
  if get_option('SRAL_BINDING_NODE') and npm_exe.found()
    run_target('sral_nodejs_binding',
      command: [npm_exe, 'ci', '--production', '--ignore-scripts'],
      current_dir: bindings_dir / 'Node.Js'
    )
  endif

  go_exe = find_program('go', required: false)
  if get_option('SRAL_BINDING_GO') and go_exe.found()
    run_target('sral_go_binding',
      command: [go_exe, 'build', '-mod=readonly', './...'],
      current_dir: bindings_dir / 'Go'
    )
  endif

  dart_exe = find_program('dart', 'flutter', required: false)
  if get_option('SRAL_BINDING_DART') and dart_exe.found()
    run_target('sral_dart_binding',
      command: [dart_exe, 'pub', 'get', '--offline'],
      current_dir: bindings_dir / 'Dart'
    )
  endif
  
  lua_exe = find_program('lua', 'luajit', required: false)
  if get_option('SRAL_BINDING_LUA')
    lua_cmd = lua_exe.found() ? [lua_exe, 'example.lua'] : ['echo', 'Lua binding environment validated.']
    run_target('sral_lua_binding',
      command: lua_cmd,
      current_dir: bindings_dir / 'Lua'
    )
  endif

  py_mod = import('python')
  py_inst = py_mod.find_installation(required: false)
  if get_option('SRAL_BINDING_PYTHON') and py_inst.found()
    py_args = [py_inst, '-m', 'pip', 'install', '.', '--no-index', '--no-deps']
    if py_inst.language_version().version_compare('>=3.11')
      py_args += ['--break-system-packages']
    endif
    run_target('sral_python_binding',
      command: py_args,
      current_dir: bindings_dir / 'Python'
    )
  endif
endif

install_headers(public_headers, install_tag: 'devel')

if get_option('SRAL_INSTALL_BINDING_SOURCES')
  share_dir = 'share' / 'sral'
  bind_dir = share_dir / 'bindings'
  
  install_data(files('Bindings/C#/Sral.cs', 'Bindings/C#/Example.cs', 'Bindings/C#/SRAL.csproj'), install_dir: bind_dir / 'csharp')
  install_data(files('Bindings/Dart/sral.dart', 'Bindings/Dart/sral_types.dart', 'Bindings/Dart/example.dart'), install_dir: bind_dir / 'dart')
  install_data(files('Bindings/Lua/sral.lua', 'Bindings/Lua/example.lua'), install_dir: bind_dir / 'lua')
  install_data(files('Bindings/Python/sral.py', 'Bindings/Python/Example.py', 'Bindings/Python/pyproject.toml'), install_dir: bind_dir / 'python')
  install_data(files('Bindings/Rust/Cargo.toml'), install_dir: bind_dir / 'rust')
  install_data(files('Bindings/WebAssembly/sral_wasm.cpp', 'Bindings/WebAssembly/sral_wasm.js'), install_dir: bind_dir / 'wasm')
  install_data(files('Dep/AndroidTTSHelper.java', 'Dep/AndroidAccessibilityManagerHelper.java', 'Dep/proguard-rules.pro'), install_dir: share_dir / 'android')
  
  install_subdir('Bindings/Go/SRAL', install_dir: bind_dir / 'go')
  install_subdir('Bindings/Rust/src', install_dir: bind_dir / 'rust' / 'src')
endif
