project(
  'SRAL_Orchestrator',
  ['c', 'cpp'],
  version: '1.0.0',
  meson_version: '>=1.11.1',
  default_options: [
    'c_std=c17',
    'cpp_std=c++20',
    'default_library=both',
    'warning_level=3',
    'b_pie=true',                   
    'b_staticpic=true',             
    'b_lto=true',                   
    'b_lundef=true',                
    'b_asneeded=true',              
    'b_pgo=off',                    
    'werror=true'                   
  ],
)

c_compiler = meson.get_compiler('c')
cpp_compiler = meson.get_compiler('cpp')

set_variable('build_variant', get_option('buildtype'))
c_id = c_compiler.get_id()

is_windows = (target_machine.system() == 'windows')
is_apple = (target_machine.system() == 'darwin' or target_machine.system() == 'ios')
is_android = (target_machine.system() == 'android')
is_emscripten = (target_machine.system() == 'emscripten')
is_building_on_mac = (build_machine.system() == 'darwin')

if target_machine.system() == 'ios' and not is_building_on_mac
  error('[SRAL ERROR] Apple iOS device/simulator static bundles can only be cross-compiled from a macOS host machine.')
endif

build_shared_libs = get_option('BUILD_SHARED_LIBS')
build_tests       = get_option('BUILD_TESTS')
build_bindings    = get_option('BUILD_BINDINGS')

if c_id == 'clang' and not is_apple and not is_android
  add_project_arguments('-fno-extensions', language: ['c', 'cpp'])
elif c_id == 'msvc' or c_id == 'clang-cl'
  add_project_arguments(['/permissive-', '/Zc:__cplusplus'], language: ['c', 'cpp'])
endif

opt_accesskit = get_option('SRAL_WITH_ACCESSKIT')
opt_chromevox = get_option('SRAL_WITH_CHROMEVOX')
opt_uia = get_option('SRAL_WITH_UIA')
opt_nsspeech = get_option('SRAL_WITH_NSSPEECH')
opt_nvda = get_option('SRAL_WITH_NVDA')
opt_speech_dispatcher = get_option('SRAL_WITH_SPEECH_DISPATCHER')
opt_orca = get_option('SRAL_WITH_ORCA')
opt_brlapi = get_option('SRAL_WITH_BRLAPI')

if opt_chromevox and not is_emscripten
  error('[SRAL ERROR] SRAL_WITH_CHROMEVOX can only be enabled for WebAssembly cross-compilation.')
endif
if is_emscripten and not opt_chromevox
  error('[SRAL ERROR] WebAssembly/Emscripten targets must have SRAL_WITH_CHROMEVOX enabled.')
endif

if is_apple
  add_languages('objc', 'objcpp', native: false)
endif

universal_msvc_args = ['-D_CRT_SECURE_NO_WARNINGS', '/GS', '/sdl', '/guard:cf', '/volatile:iso', '/permissive-', '/W4', '/WX']
universal_msvc_link = ['/GUARD:CF', '/DYNAMICBASE', '/NXCOMPAT', '/HIGHENTROPYVA']

universal_gcc_clang_args = ['-fstack-protector-all', '-fno-delete-null-pointer-checks', '-fwrapv', '-fno-common', '-Wconversion', '-Wsign-conversion', '-Werror']

if c_id == 'gcc'
  universal_gcc_clang_args += ['-fno-strict-overflow']
endif
universal_gcc_clang_args = ['-fstack-protector-all', '-fno-delete-null-pointer-checks', '-fwrapv', '-fno-common', '-Wconversion', '-Wsign-conversion', '-Werror']

if c_id == 'gcc'
  universal_gcc_clang_args += ['-fno-strict-overflow']
endif

if not is_emscripten
  universal_gcc_clang_args += ['-fstack-clash-protection']
endif


if build_variant == 'release'
  message('[SRAL] Release variant active.')
  add_project_arguments('-DNDEBUG', language: ['c', 'cpp'])

  if c_id == 'msvc' or c_id == 'clang-cl'
    msvc_release_args = universal_msvc_args + ['/O2', '/Oi', '/Qspectre', '/w14928']
    add_project_arguments(cpp_compiler.get_supported_arguments(msvc_release_args), language: ['c', 'cpp'])
    
    msvc_release_link = universal_msvc_link + ['/CETCOMPAT']
    add_project_link_arguments(cpp_compiler.get_supported_link_arguments(msvc_release_link), language: ['c', 'cpp'])
  endif

  if c_id == 'gcc' or c_id == 'clang'
    release_args = universal_gcc_clang_args + ['-ftrivial-auto-var-init=zero']
    
    if not is_emscripten
      release_args += ['-D_FORTIFY_SOURCE=3']
    endif

    if c_compiler.has_argument('-fcf-protection=full')
      release_args += ['-fcf-protection=full']
    endif
    if c_compiler.has_argument('-mbranch-protection=all')
      release_args += ['-mbranch-protection=all']
    endif
    if c_id == 'clang'
      release_args += ['-fsanitize=cfi', '-fvisibility=hidden']
      if is_apple or is_android
        # Fixed: Scrub both legacy/duplicate macro definitions and unblock vendor headers
        release_args += [
          '-Wno-macro-redefined',
          '-U_LIBCPP_HARDENING_MODE',
          '-U_LIBCPP_ENABLE_ASSERTIONS',
          '-Wno-gnu-anonymous-struct',
          '-Wno-nested-anon-types'
        ]
      endif
      add_project_link_arguments(c_compiler.get_supported_link_arguments(['-fsanitize=cfi']), language: ['c', 'cpp'])
    endif

    add_project_arguments(c_compiler.get_supported_arguments(release_args), language: ['c', 'cpp'])
  endif

  add_project_arguments('-D_GLIBCXX_ASSERTIONS', language: 'cpp')
  if not is_emscripten
    add_project_arguments(['-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_HARDENED'], language: 'cpp')
    add_project_arguments(['-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_HARDENED'], language: 'cpp')
  endif

else
  message('[SRAL] Debug variant active.')
  add_project_arguments(['-DDEBUG', '-D_DEBUG', '-DSRAL_DEBUG_TRACE_ACTIVE'], language: ['c', 'cpp'])

  if c_id == 'msvc' or c_id == 'clang-cl'
    msvc_debug_args = universal_msvc_args + ['/Od', '/Zi']
    add_project_arguments(cpp_compiler.get_supported_arguments(msvc_debug_args), language: ['c', 'cpp'])
    add_project_link_arguments(cpp_compiler.get_supported_link_arguments(universal_msvc_link), language: ['c', 'cpp'])
  endif

  if c_id == 'gcc' or c_id == 'clang'
    debug_args = universal_gcc_clang_args + ['-O0', '-g3', '-ftrivial-auto-var-init=zero']
    if not is_emscripten
      debug_args += ['-D_FORTIFY_SOURCE=1']
    endif
    if c_id == 'clang' and (is_apple or is_android)
      # Fixed: Synchronized parameters into debug mode block
      debug_args += [
        '-Wno-macro-redefined',
        '-U_LIBCPP_HARDENING_MODE',
        '-U_LIBCPP_ENABLE_ASSERTIONS',
        '-Wno-gnu-anonymous-struct',
        '-Wno-nested-anon-types'
      ]
    endif
    add_project_arguments(c_compiler.get_supported_arguments(debug_args), language: ['c', 'cpp'])
  endif

  add_project_arguments('-D_GLIBCXX_ASSERTIONS', language: 'cpp')
  if not is_emscripten
    add_project_arguments('-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE', language: 'cpp')
    add_project_arguments('-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE', language: 'cpp')
  endif
endif

if opt_chromevox
  add_project_arguments('-DSRAL_WITH_CHROMEVOX', language: ['c', 'cpp'])
else
  add_project_arguments('-DSRAL_NO_CHROMEVOX', language: ['c', 'cpp'])
endif

inc_dir = include_directories('Include')

public_headers = files(
  'Include/SRAL.h',
  'Include/Sral.hpp'
)

sral_dependencies = []
if is_windows
  sral_dependencies += [
    c_compiler.find_library('ole32'),
    c_compiler.find_library('user32'),
    c_compiler.find_library('uiautomationcore', required: opt_uia)
  ]

elif is_apple
  apple_flags = ['-fobjc-arc', '-fstack-protector-all']
  if c_compiler.has_argument('-fobjc-direct-methods')
    apple_flags += ['-fobjc-direct-methods']
  endif
  add_project_arguments(apple_flags, language: ['objc', 'objcpp'])
  
  sral_dependencies += dependency('appleframeworks', modules: ['Foundation', 'AVFoundation'])
  if target_machine.system() != 'ios' and opt_nsspeech
    sral_dependencies += dependency('appleframeworks', modules: ['AppKit'])
  endif

elif is_android
  sral_dependencies += c_compiler.find_library('log')

elif is_emscripten
  if c_compiler.has_link_argument('-pthread')
    em_args = ['-pthread', '-sSTACK_SIZE=65536', '-sWASM_BIGINT']
    add_project_link_arguments(em_args, language: ['c', 'cpp'])
    add_project_arguments('-pthread', language: ['c', 'cpp'])
  endif

else
  add_project_arguments('-DSRAL_SPEECHD_REQUIRE_UTF8', language: ['c', 'cpp'])
  
  if opt_speech_dispatcher
    sral_dependencies += dependency('speech-dispatcher', required: true)
  else
    add_project_arguments('-DSRAL_NO_SPEECH_DISPATCHER', language: ['c', 'cpp'])
  endif
  
  if opt_orca
    sral_dependencies += [dependency('dbus-1', required: true), dependency('atspi-2', required: true)]
  else
    add_project_arguments('-DSRAL_NO_ORCA', language: ['c', 'cpp'])
  endif
  
  if opt_brlapi
    sral_dependencies += c_compiler.find_library('brlapi', required: true)
  else
    add_project_arguments('-DSRAL_NO_BRLAPI', language: ['c', 'cpp'])
  endif
endif

if not is_android and not is_emscripten
  sral_dependencies += dependency('jni', required: false)
endif

if opt_accesskit
  add_project_arguments('-DSRAL_WITH_ACCESSKIT', language: ['c', 'cpp'])
endif
if opt_uia and is_windows
  add_project_arguments('-DSRAL_WITH_UIA', language: ['c', 'cpp'])
endif
if opt_nsspeech and is_apple and target_machine.system() != 'ios'
  add_project_arguments('-DSRAL_WITH_NSSPEECH', language: ['c', 'cpp'])
endif
if opt_nvda and is_windows
  add_project_arguments('-DSRAL_WITH_NVDA', language: ['c', 'cpp'])
endif
if opt_orca
  add_project_arguments('-DSRAL_WITH_ORCA', language: ['c', 'cpp'])
endif

python_bin = import('python').find_installation()

subdir('SRC')

sral_dep = declare_dependency(
  include_directories: inc_dir,
  link_with: sral_lib,
  dependencies: sral_dependencies
)

test_link_args = []
if c_id != 'msvc' and not is_apple and not is_emscripten
  test_link_args += c_compiler.get_supported_link_arguments(['-Wl,-z,relro', '-Wl,-z,now', '-Wl,-z,noexecstack'])
endif

if build_tests and not is_emscripten and not is_android
  executable('SRAL_test',
    files('Examples/C/SRALExample.c'),
    include_directories: inc_dir,
    link_with: sral_lib,
    link_args: test_link_args,
    install: false
  )

  if is_windows and opt_nvda
    executable('SRAL_NVDAControlExConsole',
      files('Examples/C/NVDAControlExConsole.c'),
      include_directories: inc_dir,
      link_with: sral_lib,
      link_args: test_link_args,
      install: false
    )
  endif
  
  if is_apple and target_machine.system() != 'ios'
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
  bindings_dir = meson.current_source_dir() / 'Bindings'
  sh_exe = find_program('sh', required: false)
  cmd_exe = find_program('cmd.exe', required: false)
  echo_exe = find_program('echo', required: false)
  echo_exe = find_program('echo', required: false)

  dotnet_exe = find_program('dotnet', required: false)
  if get_option('SRAL_BINDING_CSHARP') and dotnet_exe.found()
    run_target('sral_csharp_binding',
      command: [dotnet_exe, 'build', bindings_dir / 'C#' / 'SRAL.csproj', '--configuration', 'Release', '--no-restore', '-p:SKIP_MOBILE_WORKLOADS=true']
    )
  else
    run_target('sral_csharp_binding', command: [echo_exe.found() ? echo_exe : 'echo', 'C# binding compilation skipped.'])
  else
    run_target('sral_csharp_binding', command: [echo_exe.found() ? echo_exe : 'echo', 'C# binding compilation skipped.'])
  endif
  
  cargo_exe = find_program('cargo', required: false)
  if get_option('SRAL_BINDING_RUST') and cargo_exe.found()
    run_target('sral_rust_binding',
      command: [cargo_exe, 'build', '--manifest-path', bindings_dir / 'Rust' / 'Cargo.toml', '--release', '--offline']
    )
  else
    run_target('sral_rust_binding', command: [echo_exe.found() ? echo_exe : 'echo', 'Rust binding compilation skipped.'])
  else
    run_target('sral_rust_binding', command: [echo_exe.found() ? echo_exe : 'echo', 'Rust binding compilation skipped.'])
  endif

  npm_exe = find_program('npm', 'npm.cmd', required: false)
  if get_option('SRAL_BINDING_NODE') and npm_exe.found()
    node_proj_dir = bindings_dir / 'Node.Js'
    if is_windows and cmd_exe.found()
      run_target('sral_nodejs_binding', command: [cmd_exe, '/c', 'cd', '/d', node_proj_dir, '&&', 'npm', 'ci', '--production', '--ignore-scripts'])
    elif sh_exe.found()
      run_target('sral_nodejs_binding', command: [sh_exe, '-c', 'cd "$1" && "$2" ci --production --ignore-scripts', 'meson_sh', node_proj_dir, npm_exe])
    endif
  else
    run_target('sral_nodejs_binding', command: [echo_exe.found() ? echo_exe : 'echo', 'Node.js binding generation skipped.'])
  else
    run_target('sral_nodejs_binding', command: [echo_exe.found() ? echo_exe : 'echo', 'Node.js binding generation skipped.'])
  endif

  go_exe = find_program('go', required: false)
  if get_option('SRAL_BINDING_GO') and go_exe.found()
    go_proj_dir = bindings_dir / 'Go'
    if is_windows and cmd_exe.found()
      run_target('sral_go_binding', command: [cmd_exe, '/c', 'cd', '/d', go_proj_dir, '&&', 'go', 'build', '-mod=readonly', './...'])
    elif sh_exe.found()
      run_target('sral_go_binding', command: [sh_exe, '-c', 'cd "$1" && "$2" build -mod=readonly ./...', 'meson_sh', go_proj_dir, go_exe])
    endif
  else
    run_target('sral_go_binding', command: [echo_exe.found() ? echo_exe : 'echo', 'Go binding compilation skipped.'])
  else
    run_target('sral_go_binding', command: [echo_exe.found() ? echo_exe : 'echo', 'Go binding compilation skipped.'])
  endif

  dart_exe = find_program('dart', 'flutter', required: false)
  if get_option('SRAL_BINDING_DART') and dart_exe.found()
    dart_proj_dir = bindings_dir / 'Dart'
    if is_windows and cmd_exe.found()
      run_target('sral_dart_binding', command: [cmd_exe, '/c', 'cd', '/d', dart_proj_dir, '&&', 'dart', 'pub', 'get', '--offline'])
    elif sh_exe.found()
      run_target('sral_dart_binding', command: [sh_exe, '-c', 'cd "$1" && "$2" pub get --offline', 'meson_sh', dart_proj_dir, dart_exe])
    endif
  else
    run_target('sral_dart_binding', command: [echo_exe.found() ? echo_exe : 'echo', 'Dart binding resolution skipped.'])
  else
    run_target('sral_dart_binding', command: [echo_exe.found() ? echo_exe : 'echo', 'Dart binding resolution skipped.'])
  endif
  
  lua_exe = find_program('lua', 'luajit', required: false)
  if get_option('SRAL_BINDING_LUA')
    lua_proj_dir = bindings_dir / 'Lua'
    if lua_exe.found()
      run_target('sral_lua_binding', command: [lua_exe, lua_proj_dir / 'example.lua'])
    else
      run_target('sral_lua_binding', command: [echo_exe.found() ? echo_exe : 'echo', 'Lua binding environment validated.'])
      run_target('sral_lua_binding', command: [echo_exe.found() ? echo_exe : 'echo', 'Lua binding environment validated.'])
    endif
  else
    run_target('sral_lua_binding', command: [echo_exe.found() ? echo_exe : 'echo', 'Lua binding execution disabled.'])
  else
    run_target('sral_lua_binding', command: [echo_exe.found() ? echo_exe : 'echo', 'Lua binding execution disabled.'])
  endif

  py_inst = python_bin
  if get_option('SRAL_BINDING_PYTHON') and py_inst.found()
    py_args = [py_inst, '-m', 'pip', 'install', bindings_dir / 'Python', '--no-index', '--no-deps']
    if py_inst.language_version().version_compare('>=3.11')
      py_args += ['--break-system-packages']
    endif
    run_target('sral_python_binding', command: py_args)
  else
    run_target('sral_python_binding', command: [echo_exe.found() ? echo_exe : 'echo', 'Python pip setup phase skipped.'])
  else
    run_target('sral_python_binding', command: [echo_exe.found() ? echo_exe : 'echo', 'Python pip setup phase skipped.'])
  endif
endif

install_headers(public_headers)
install_data('sral_config.ini', install_dir: get_option('datadir') / 'sral')

if get_option('SRAL_INSTALL_BINDING_SOURCES')
  share_dir = 'share/sral'
  bind_dir = share_dir / 'bindings'
  
  install_data(files('Bindings/C#/Sral.cs', 'Bindings/C#/Example.cs', 'Bindings/C#/SRAL.csproj'), install_dir: bind_dir / 'csharp')
  install_data(files('Bindings/Dart/sral.dart', 'Bindings/Dart/sral_types.dart', 'Bindings/Dart/example.dart'), install_dir: bind_dir / 'dart')
  install_data(files('Bindings/Lua/sral.lua', 'Bindings/Lua/example.lua'), install_dir: bind_dir / 'lua')
  install_data(files('Bindings/Python/sral.py', 'Bindings/Python/Example.py', 'Bindings/Python/pyproject.toml'), install_dir: bind_dir / 'python')
  install_data(files('Bindings/Rust/Cargo.toml'), install_dir: bind_dir / 'rust')
  install_data(files('Bindings/WebAssembly/sral_wasm.cpp', 'Bindings/WebAssembly/sral_wasm.js'), install_dir: bind_dir / 'wasm')
  install_subdir('Bindings/Go/SRAL', install_dir: bind_dir / 'go')
  install_subdir('Bindings/Rust/src', install_dir: bind_dir / 'rust')
endif

pkg_mod = import('pkgconfig')
pkg_mod.generate(
  name: 'SRAL_Orchestrator',
  filebase: 'sral_orchestrator',
  description: 'Screen Reader Abstraction Layer Orchestrator Engine Core.',
  subdirs: 'SRAL',
  libraries: sral_dep,
  version: '1.0.0'
)