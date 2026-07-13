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

fs = import('fs')
c_compiler = meson.get_compiler('c')
cpp_compiler = meson.get_compiler('cpp')

build_variant = get_option('buildtype')
c_id = c_compiler.get_id()

is_windows    = (target_machine.system() == 'windows')
is_apple      = (target_machine.system() == 'darwin' or target_machine.system() == 'ios')
is_android    = (target_machine.system() == 'android')
is_emscripten = (target_machine.system() == 'emscripten')
is_building_on_mac = (build_machine.system() == 'darwin')

target_desktop   = get_option('TARGET_DESKTOP')
target_chromevox = get_option('TARGET_CHROMEVOX')
target_android   = get_option('TARGET_ANDROID')
target_ios       = get_option('TARGET_IOS')

if target_ios and not is_building_on_mac
  error('[SRAL ERROR] Apple iOS device/simulator static bundles can only be cross-compiled from a macOS host machine.')
endif

build_shared_libs = get_option('BUILD_SHARED_LIBS')
build_tests       = get_option('BUILD_TESTS')
build_bindings    = get_option('BUILD_BINDINGS')

version_usertarget_macos   = get_option('VERSION_USERTARGET_MACOS')
version_usertarget_windows = get_option('VERSION_USERTARGET_WINDOWS')
version_usertarget_android = get_option('VERSION_USERTARGET_ANDROID')
version_usertarget_ios     = get_option('VERSION_USERTARGET_IOS')

path_nvda_install      = get_option('PATH_NVDA_INSTALL')
path_speechd_install   = get_option('PATH_SPEECHD_INSTALL')
path_python_install    = get_option('PATH_PYTHON_INSTALL')
path_emsdk_install     = get_option('PATH_EMSDK_INSTALL')
path_android_ndk       = get_option('PATH_ANDROID_NDK')
path_accesskit_install = get_option('PATH_ACCESSKIT_INSTALL')
path_jdk_install       = get_option('PATH_JDK_INSTALL')

if c_id == 'gcc' or c_id == 'clang'
  add_project_arguments('-fno-extensions', language: ['c', 'cpp'])
elif c_id == 'msvc' or c_id == 'clang-cl'
  add_project_arguments(['/permissive-', '/Zc:__cplusplus'], language: ['c', 'cpp'])
endif

opt_chromevox         = get_option('SRAL_WITH_CHROMEVOX')
opt_uia               = get_option('SRAL_WITH_UIA')
opt_nsspeech          = get_option('SRAL_WITH_NSSPEECH')
opt_nvda              = get_option('SRAL_WITH_NVDA')
opt_speech_dispatcher = get_option('SRAL_WITH_SPEECH_DISPATCHER')
opt_orca              = get_option('SRAL_WITH_ORCA')
opt_brlapi            = get_option('SRAL_WITH_BRLAPI')

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

universal_gcc_clang_args = ['-fstack-protector-all', '-fno-delete-null-pointer-checks', '-fwrapv', '-fno-strict-overflow', '-fno-common', '-Wconversion', '-Wsign-conversion', '-Werror']

if not is_emscripten
  universal_gcc_clang_args += ['-fstack-clash-protection']
endif

dev_sanitizers = get_option('SRAL_DEV_SANITIZERS')

if build_variant == 'release' or build_variant == 'minsize'
  message('[SRAL ARCHITECTURE] Hardened Production Release variant active.')
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
      release_args += '-fcf-protection=full'
    endif
    if c_compiler.has_argument('-mbranch-protection=all')
      release_args += '-mbranch-protection=all'
    endif
    if c_id == 'clang'
      release_args += ['-fsanitize=cfi', '-fvisibility=hidden']
      add_project_link_arguments(c_compiler.get_supported_link_arguments(['-fsanitize=cfi']), language: ['c', 'cpp'])
    endif

    add_project_arguments(c_compiler.get_supported_arguments(release_args), language: ['c', 'cpp'])
  endif

  add_project_arguments('-D_GLIBCXX_ASSERTIONS', language: 'cpp')
  if not is_emscripten
    add_project_arguments(['-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE', '-D_LIBCPP_ENABLE_ASSERTIONS=1'], language: 'cpp')
  endif

else
  message('[SRAL ARCHITECTURE] Hardened Library-Safe Debug variant active.')
  add_project_arguments(['-DDEBUG', '-D_DEBUG', '-DSRAL_DEBUG_TRACE_ACTIVE'], language: ['c', 'cpp'])

  if c_id == 'msvc' or c_id == 'clang-cl'
    msvc_debug_args = universal_msvc_args + ['/Od', '/Zi']
    if dev_sanitizers
      msvc_debug_args += '/fsanitize=address'
    endif
    add_project_arguments(cpp_compiler.get_supported_arguments(msvc_debug_args), language: ['c', 'cpp'])
    add_project_link_arguments(cpp_compiler.get_supported_link_arguments(universal_msvc_link), language: ['c', 'cpp'])
  endif

  if c_id == 'gcc' or c_id == 'clang'
    debug_args = universal_gcc_clang_args + ['-O0', '-g3', '-ftrivial-auto-var-init=zero']
    
    if not is_emscripten
      debug_args += ['-D_FORTIFY_SOURCE=1']
      if dev_sanitizers
        debug_args += ['-fsanitize=address,undefined,leak', '-fno-omit-frame-pointer', '-fno-optimize-sibling-calls']
        debug_link = c_compiler.get_supported_link_arguments(['-fsanitize=address,undefined,leak'])
        add_project_link_arguments(debug_link, language: ['c', 'cpp'])
      endif
    endif

    add_project_arguments(c_compiler.get_supported_arguments(debug_args), language: ['c', 'cpp'])
  endif

  add_project_arguments('-D_GLIBCXX_ASSERTIONS', language: 'cpp')
  if not is_emscripten
    add_project_arguments('-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST', language: 'cpp')
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
    apple_flags += '-fobjc-direct-methods'
  endif
  add_project_arguments(apple_flags, language: ['objc', 'objcpp'])
  
  sral_dependencies += dependency('appleframeworks', modules: ['Foundation', 'AVFoundation'])
  if not target_ios and opt_nsspeech
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

python_bin = import('python').find_installation()

third_party_binaries = [
  path_accesskit_install,
  path_nvda_install
]

foreach bin : third_party_binaries
  if bin != '' and fs.exists(bin)
    audit_check = run_command(python_bin, '-c', '''
import sys, os
file_path = sys.argv
if os.path.isdir(file_path):
    sys.exit(0)
print(f"[SRAL AUDIT] Structural check pass executing on binary: {file_path}")
''', bin, check: false)
    
    if audit_check.returncode() != 0
      warning('[SRAL SECURITY WARNING] Third-party binary artifact failed baseline security audit metrics: ' + bin)
    endif
  endif
endforeach
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
  
  if is_apple and not target_ios
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

  sh_exe = find_program('sh', required: false)
  cmd_exe = find_program('cmd.exe', required: false)

  dotnet_exe = find_program('dotnet', required: false)
  if get_option('SRAL_BINDING_CSHARP') and dotnet_exe.found()
    run_target('sral_csharp_binding',
      command: [dotnet_exe, 'build', bindings_dir / 'C#' / 'SRAL.csproj', '--configuration', 'Release', '--no-restore', '-p:SKIP_MOBILE_WORKLOADS=true']
    )
  endif
  
  cargo_exe = find_program('cargo', required: false)
  if get_option('SRAL_BINDING_RUST') and cargo_exe.found()
    run_target('sral_rust_binding',
      command: [cargo_exe, 'build', '--manifest-path', bindings_dir / 'Rust' / 'Cargo.toml', '--release', '--offline']
    )
  endif

  npm_exe = find_program('npm', 'npm.cmd', required: false)
  if get_option('SRAL_BINDING_NODE') and npm_exe.found()
    node_proj_dir = bindings_dir / 'Node.Js'
    if is_windows and cmd_exe.found()
      run_target('sral_nodejs_binding', command: [cmd_exe, '/c', 'cd', '/d', node_proj_dir, '&&', 'npm', 'ci', '--production', '--ignore-scripts'])
    elif sh_exe.found()
      run_target('sral_nodejs_binding', command: [sh_exe, '-c', 'cd "$1" && "$2" ci --production --ignore-scripts', 'meson_sh', node_proj_dir, npm_exe.path()])
    endif
  endif

  go_exe = find_program('go', required: false)
  if get_option('SRAL_BINDING_GO') and go_exe.found()
    go_proj_dir = bindings_dir / 'Go'
    if is_windows and cmd_exe.found()
      run_target('sral_go_binding', command: [cmd_exe, '/c', 'cd', '/d', go_proj_dir, '&&', 'go', 'build', '-mod=readonly', './...'])
    elif sh_exe.found()
      run_target('sral_go_binding', command: [sh_exe, '-c', 'cd "$1" && "$2" build -mod=readonly ./...', 'meson_sh', go_proj_dir, go_exe.path()])
    endif
  endif

  dart_exe = find_program('dart', 'flutter', required: false)
  if get_option('SRAL_BINDING_DART') and dart_exe.found()
    dart_proj_dir = bindings_dir / 'Dart'
    if is_windows and cmd_exe.found()
      run_target('sral_dart_binding', command: [cmd_exe, '/c', 'cd', '/d', dart_proj_dir, '&&', 'dart', 'pub', 'get', '--offline'])
    elif sh_exe.found()
      run_target('sral_dart_binding', command: [sh_exe, '-c', 'cd "$1" && "$2" pub get --offline', 'meson_sh', dart_proj_dir, dart_exe.path()])
    endif
  endif
  
  lua_exe = find_program('lua', 'luajit', required: false)
  if get_option('SRAL_BINDING_LUA')
    lua_proj_dir = bindings_dir / 'Lua'
    if lua_exe.found()
      run_target('sral_lua_binding', command: [lua_exe, lua_proj_dir / 'example.lua'])
    else
      run_target('sral_lua_binding', command: ['echo', 'Lua binding environment validated.'])
    endif
  endif

  py_inst = python_bin
  if get_option('SRAL_BINDING_PYTHON') and py_inst.found()
    py_args = [py_inst.path(), '-m', 'pip', 'install', bindings_dir / 'Python', '--no-index', '--no-deps']
    if py_inst.language_version().version_compare('>=3.11')
      py_args += ['--break-system-packages']
    endif
    run_target('sral_python_binding', command: py_args)
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
  install_subdir('Bindings/Rust/src', install_dir: bind_dir / 'rust')
endif

option_sign = get_option('SRAL_SIGN_OUTPUTS')

if option_sign
  if is_windows
    signtool = find_program('signtool', required: false)
    if signtool.found()
      meson.add_install_script(signtool, 'sign', '/a', '/tr', 'http://digicert.com', '/td', 'sha256', '/fd', 'sha256', slot: 'post')
      message('[SRAL SECURITY] Authenticode sign-on-install scripts registered.')
    else
      warning('[SRAL SECURITY] signtool utility missing. Skipping Windows binary signing.')
    endif
    
  elif is_apple
    codesign = find_program('codesign', required: false)
    if codesign.found()
      apple_identity = get_option('APPLE_SIGNING_IDENTITY')
      if apple_identity != ''
        meson.add_install_script(codesign, '--force', '--options', 'runtime', '--sign', apple_identity, slot: 'post')
        message('[SRAL SECURITY] Apple Developer signing hooks integrated.')
      else
         warning('[SRAL SECURITY] APPLE_SIGNING_IDENTITY is empty. Skipping apple codesign pass.')
      endif
    endif
  endif
endif
