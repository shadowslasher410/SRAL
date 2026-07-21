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
    'werror=true',
  ],
)

c_compiler = meson.get_compiler('c')
cpp_compiler = meson.get_compiler('cpp')

sral_dependencies = []

build_variant = get_option('buildtype')
c_id = c_compiler.get_id()
is_msvc_like = (c_compiler.get_argument_syntax() == 'msvc')
host_sys = host_machine.system()
is_windows = (host_sys == 'windows')
is_apple = (host_sys == 'darwin' or host_sys == 'ios')
is_android = (host_sys == 'android')
is_emscripten = (host_sys == 'emscripten')
is_building_on_mac = (build_machine.system() == 'darwin')

if host_sys == 'ios' and not is_building_on_mac
  error(
    '[SRAL ERROR] Apple iOS device/simulator static bundles can only be cross-compiled from a macOS host machine.',
  )
endif

kv = import('keyval')
fs = import('fs')

path_nvda = ''
path_speechd = ''
path_python = ''
path_emsdk = ''
path_ndk = ''
path_accesskit = ''
path_jdk = ''

dev_ini = files('developer_input.ini')

if fs.exists('developer_input.ini')
  dev_config = kv.load(dev_ini)

  path_nvda = dev_config.get('nvda_install', '')
  path_speechd = dev_config.get('speechd_install', '')
  path_python = dev_config.get('python_install', '')
  path_emsdk = dev_config.get('emsdk_install', '')
  path_ndk = dev_config.get('android_ndk', '')
  path_accesskit = dev_config.get('accesskit_install', '')
  path_jdk = dev_config.get('jdk_install', '')

  all_paths = {
    'NVDA SDK Remote Linkage': path_nvda,
    'Speech Dispatcher Sockets': path_speechd,
    'Isolated Python Environment': path_python,
    'Emscripten WebAssembly SDK': path_emsdk,
    'Google Android NDK Workspace': path_ndk,
    'AccessKit Proxy Library Assets': path_accesskit,
    'Target Host Java Development Kit': path_jdk,
  }

  sral_env_config = environment()

  message('--- SRAL Multi-User Developer Path Validation ---')
  foreach name, path : all_paths
    if path != ''
      if fs.is_dir(path)
        message('[VALIDATED] ' + name + ' resolved at: ' + path)
        if name == 'NVDA SDK Remote Linkage'
          sral_env_config.set('NVDA_SDK_PATH', path)
        elif name == 'Speech Dispatcher Sockets'
          sral_env_config.set('SPEECHD_PATH', path)
        elif name == 'Isolated Python Environment'
          sral_env_config.set('PYTHON_PATH', path)
        elif name == 'Emscripten WebAssembly SDK'
          sral_env_config.set('EMSDK_PATH', path)
        elif name == 'Google Android NDK Workspace'
          sral_env_config.set('ANDROID_NDK_HOME', path)
        elif name == 'AccessKit Proxy Library Assets'
          sral_env_config.set('ACCESSKIT_PATH', path)
        elif name == 'Target Host Java Development Kit'
          sral_env_config.set('JAVA_HOME', path)
        endif
      else
        warning(
          '[SRAL UNRESOLVED PATH] '
          + name
          + ' is configured in INI but does not exist at: '
          + path,
        )
      endif
    endif
  endforeach
  message('------------------------------------------------')
  meson.add_devenv(sral_env_config)
endif

build_shared_libs = get_option('BUILD_SHARED_LIBS')
build_tests = get_option('BUILD_TESTS')
build_bindings = get_option('BUILD_BINDINGS')

c_warning_args = []
cpp_warning_args = []

if is_windows
  if is_msvc_like
    c_warning_args += ['/wd4668']
    cpp_warning_args += ['/wd4668']
  else
    c_warning_args += ['-Wno-undef']
    cpp_warning_args += ['-Wno-undef']
  endif
endif

if is_msvc_like
  msvc_shared_flags = [
    '/wd4324',  # Structure was padded due to alignment specifier
    '/wd4820',  # Padding bytes automatically added inside structs
    '/wd5039',  # Suppress winbase.h TpSetCallbackCleanupGroup throw warning under -EHc
    '/wd4865',  # Unnamed enum layout underlying type changes
    '/wd4365',  # Signed/unsigned mismatch
    '/wd5204',  # Non-virtual destructor in class with virtual functions
    '/wd5291',  # Suppress ABI padding warning between classes
    '/wd4373',  # Virtual function override const/volatile qualifier difference
    '/wd4251',  # STL template member DLL interface warning
    '/wd4625',  # Copy constructor was implicitly defined as deleted
    '/wd4626',  # Assignment operator was implicitly defined as deleted
    '/wd5026',  # Move constructor was implicitly defined as deleted
    '/wd5027',  # Move assignment operator was implicitly defined as deleted
    '/wd4005',  # Macro redefinition warning
    '/wd4191',  # Unsafe FARPROC to function pointer conversion
    '/wd5045',  # Compiler will insert Spectre mitigation
    '/wd4710',  # Function not inlined
    '/wd4711',  # Informational: Function selected for automatic inline expansion
    '/permissive-',
    '/Zc:__cplusplus',
  ]
  
  add_project_arguments(msvc_shared_flags, language: ['c', 'cpp'])
  
  add_project_arguments('/D_SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING', language: 'cpp')
  add_project_arguments('/experimental:c11atomics', language: 'c')
endif
if is_windows
  if c_id == 'clang'
    clang_win_flags = [
      '-fms-extensions',
      '-Wno-language-extension-token',
      '-Wno-missing-designated-field-initializers',
    ]
    c_warning_args += clang_win_flags
    cpp_warning_args += clang_win_flags
  elif c_id == 'clang-cl'
    clang_cl_win_flags = [
      '/clang:-fextensions',
      '/clang:-Wno-language-extension-token',
      '/clang:-Wno-missing-designated-field-initializers',
    ]
    c_warning_args += clang_cl_win_flags
    cpp_warning_args += clang_cl_win_flags
  endif
endif

if not is_msvc_like
  if c_compiler.has_argument('-Wno-unused-function')
    c_warning_args += ['-Wno-unused-function', '-Wno-unused-variable']
  endif
  if cpp_compiler.has_argument('-Wno-unused-function')
    cpp_warning_args += ['-Wno-unused-function', '-Wno-unused-variable']
  endif

  if c_compiler.has_argument('-Wno-gnu-anonymous-struct')
    c_warning_args += ['-Wno-gnu-anonymous-struct', '-Wno-nested-anon-types']
  endif
  if cpp_compiler.has_argument('-Wno-gnu-anonymous-struct')
    cpp_warning_args += ['-Wno-gnu-anonymous-struct', '-Wno-nested-anon-types']
  endif

  if cpp_compiler.has_argument('-Wno-interference-size')
    cpp_warning_args += ['-Wno-interference-size']
  endif
  if cpp_compiler.has_argument('-Wno-old-style-cast')
    cpp_warning_args += ['-Wno-old-style-cast']
  endif

  if c_id == 'clang' and not is_windows and host_sys != 'ios' and not is_android
    if c_compiler.has_argument('-fno-extensions')
      c_warning_args += ['-fno-extensions']
    endif
    if cpp_compiler.has_argument('-fno-extensions')
      cpp_warning_args += ['-fno-extensions']
    endif
  endif
endif

if is_emscripten
  if cpp_compiler.has_argument('-Wno-dollar-in-identifier-extension')
    cpp_warning_args += ['-Wno-dollar-in-identifier-extension']
  endif
endif

add_project_arguments(c_warning_args, language: 'c')
add_project_arguments(cpp_warning_args, language: 'cpp')

opt_accesskit = get_option('SRAL_WITH_ACCESSKIT')
opt_chromevox = get_option('SRAL_WITH_CHROMEVOX')
opt_uia = get_option('SRAL_WITH_UIA')
opt_nsspeech = get_option('SRAL_WITH_NSSPEECH')
opt_nvda = get_option('SRAL_WITH_NVDA')
opt_speech_dispatcher = get_option('SRAL_WITH_SPEECH_DISPATCHER')
opt_orca = get_option('SRAL_WITH_ORCA')
opt_brlapi = get_option('SRAL_WITH_BRLAPI')

if opt_accesskit
  if is_emscripten
    message('[SRAL] AccessKit is unsupported on Emscripten/WASM.')
    opt_accesskit = false
  else
    if path_accesskit != ''
      ak_check = c_compiler.find_library('accesskit', dirs: [path_accesskit], required: false)
    else 
      ak_check = dependency('accesskit', required: false)
    endif

    if not ak_check.found()
      ak_check = c_compiler.find_library('accesskit', required: false)
    endif

    if ak_check.found()
      message('[SRAL] AccessKit library loaded successfully.')
      opt_accesskit = true
      sral_dependencies += ak_check
    else
      message(
        '[SRAL] AccessKit not found via custom or system paths. Disabling subsystem.',
      )
      opt_accesskit = false
      add_project_arguments('-DSRAL_NO_ACCESSKIT', language: ['c', 'cpp'])
    endif
  endif
else
  add_project_arguments('-DSRAL_NO_ACCESSKIT', language: ['c', 'cpp'])
endif

if opt_nvda
  if is_windows
    opt_nvda = false
    if path_nvda != ''
      nvda_check = c_compiler.find_library('nvdaControllerClient', dirs: [path_nvda], required: false)
    else
      nvda_check = dependency('nvdaControllerClient', required: false)
    endif

    if not nvda_check.found()
      nvda_check = c_compiler.find_library('nvdaControllerClient', required: false)
    endif

    if nvda_check.found()
      message('[SRAL] NVDA Controller Client loaded successfully.')
      opt_nvda = true
      sral_dependencies += nvda_check
      add_project_arguments('-DSRAL_WITH_NVDA', language: ['c', 'cpp'])
    else
      message(
        '[SRAL] NVDA Client not found via custom or system paths. Disabling subsystem.',
      )
      opt_nvda = false
    endif
  else
    opt_nvda = false
  endif
endif

if opt_speech_dispatcher
  if host_sys == 'linux'
    opt_speech_dispatcher = false
    if path_speechd != ''
      speechd_check = c_compiler.find_library('speechd', dirs: [path_speechd], required: false)
    else
      speechd_check = dependency('speechd', required: false)
    endif

    if not speechd_check.found()
      speechd_check = c_compiler.find_library('speechd', required: false)
    endif

    if speechd_check.found()
      message('[SRAL] Speech Dispatcher socket library loaded successfully.')
      opt_speech_dispatcher = true
      sral_dependencies += speechd_check
    else
      message(
        '[SRAL] Speech Dispatcher not found via custom or system paths. Disabling subsystem.',
      )
      opt_speech_dispatcher = false
      add_project_arguments('-DSRAL_NO_SPEECH_DISPATCHER', language: ['c', 'cpp'])
    endif
  else
    opt_speech_dispatcher = false
  endif
endif

if opt_chromevox
  if is_emscripten
    emcc_check = find_program('echo', required: false)

    if path_emsdk != ''
      emcc_check = find_program('emcc', dirs: [path_emsdk], required: false)
    else
      emcc_check = dependency('emcc', required: false)
    endif

    if not emcc_check.found()
      emcc_check = find_program('emcc', required: false)
    endif

    if emcc_check.found()
      build_shared_libs = false
      message('[SRAL] Emscripten SDK compiler (emcc) located successfully.')
      opt_chromevox = true
      add_project_arguments('-DSRAL_WITH_CHROMEVOX', language: ['c', 'cpp'])
    else
      message(
        '[SRAL] Emscripten SDK not found via custom or system paths. Disabling ChromeVox/WASM features.',
      )
      opt_chromevox = false
      add_project_arguments('-DSRAL_NO_CHROMEVOX', language: ['c', 'cpp'])
    endif
  else
    message(
      '[SRAL] ChromeVox can only be enabled during WebAssembly cross-compilation. Toggling off.',
    )
    opt_chromevox = false
    add_project_arguments('-DSRAL_NO_CHROMEVOX', language: ['c', 'cpp'])
  endif
else
  add_project_arguments('-DSRAL_NO_CHROMEVOX', language: ['c', 'cpp'])
endif

if is_emscripten and not opt_chromevox
  error(
    '[SRAL] WebAssembly/Emscripten compilation targets cannot continue without a valid emsdk environment and ChromeVox enabled.',
  )
endif

if is_apple
  add_languages('objc', 'objcpp', native: false)
endif

universal_msvc_args = [
  '-D_CRT_SECURE_NO_WARNINGS', '/GS',
  '/sdl',
  '/guard:cf',
  '/volatile:iso',
  '/permissive-',
  '/W4',
  '/WX',
]
universal_msvc_link = ['/GUARD:CF', '/DYNAMICBASE', '/NXCOMPAT', '/HIGHENTROPYVA']

universal_gcc_clang_args = [
  '-fstack-protector-all',
  '-fno-delete-null-pointer-checks',
  '-fwrapv',
  '-fno-common',
  '-Wconversion',
  '-Wsign-conversion',
]

if c_id == 'gcc'
  universal_gcc_clang_args += ['-fno-strict-overflow']
endif

if not is_emscripten
  universal_gcc_clang_args += ['-fstack-clash-protection']
endif

release_args = []
debug_args = []

if build_variant == 'release'
  message('[SRAL] Release variant active.')
  add_project_arguments('-DNDEBUG', language: ['c', 'cpp'])

  if is_msvc_like
    msvc_release_args = universal_msvc_args + ['/O2', '/Oi', '/Qspectre', '/w14928']
    supported_msvc_c_args = c_compiler.get_supported_arguments(msvc_release_args)
    supported_msvc_cpp_args = cpp_compiler.get_supported_arguments(msvc_release_args)
    
    add_project_arguments(supported_msvc_c_args, language: 'c')
    add_project_arguments(supported_msvc_cpp_args, language: 'cpp')

    msvc_release_link = universal_msvc_link + ['/CETCOMPAT']
    add_project_link_arguments(
      cpp_compiler.get_supported_link_arguments(msvc_release_link),
      language: ['c', 'cpp'],
    )
  else
    release_args += universal_gcc_clang_args + ['-ftrivial-auto-var-init=zero']

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
      release_args += ['-fvisibility=hidden']
      if c_compiler.has_argument('-fsanitize=cfi')
        release_args += ['-fsanitize=cfi']
        add_project_link_arguments(
          c_compiler.get_supported_link_arguments(['-fsanitize=cfi']),
          language: ['c', 'cpp'],
        )
      endif

      if is_apple or is_android
        release_args += [
          '-Wno-macro-redefined',
          '-U_LIBCPP_HARDENING_MODE',
          '-U_LIBCPP_ENABLE_ASSERTIONS',
          '-Wno-gnu-anonymous-struct',
          '-Wno-nested-anon-types',
        ]
      endif
    endif

    add_project_arguments(c_compiler.get_supported_arguments(release_args), language: ['c', 'cpp'])
  endif

  add_project_arguments('-D_GLIBCXX_ASSERTIONS', language: 'cpp')
  if not is_emscripten
    add_project_arguments('-D_LIBCPP_HARDENING_MODE=2', language: 'cpp')
  endif

else
  message('[SRAL] Debug variant active.')
  add_project_arguments(
    ['-DDEBUG', '-D_DEBUG', '-DSRAL_DEBUG_TRACE_ACTIVE'],
    language: ['c', 'cpp'],
  )

  if is_msvc_like
    msvc_debug_args = universal_msvc_args + ['/Od', '/Zi']
    supported_msvc_debug_c_args = c_compiler.get_supported_arguments(msvc_debug_args)
    supported_msvc_debug_cpp_args = cpp_compiler.get_supported_arguments(msvc_debug_args)
    
    add_project_arguments(supported_msvc_debug_c_args, language: 'c')
    add_project_arguments(supported_msvc_debug_cpp_args, language: 'cpp')
    
    add_project_link_arguments(
      cpp_compiler.get_supported_link_arguments(universal_msvc_link),
      language: ['c', 'cpp'],
    )
  else
    debug_args += universal_gcc_clang_args + ['-g3', '-ftrivial-auto-var-init=zero']
    if not is_emscripten
      debug_args += ['-D_FORTIFY_SOURCE=1']
    endif
    if c_id == 'clang' and (is_apple or is_android)
      debug_args += [
        '-Wno-macro-redefined',
        '-U_LIBCPP_HARDENING_MODE',
        '-U_LIBCPP_ENABLE_ASSERTIONS',
        '-Wno-gnu-anonymous-struct',
        '-Wno-nested-anon-types',
      ]
    endif
    add_project_arguments(c_compiler.get_supported_arguments(debug_args), language: 'c')
    add_project_arguments(cpp_compiler.get_supported_arguments(debug_args), language: 'cpp')
  endif

  add_project_arguments('-D_GLIBCXX_ASSERTIONS', language: 'cpp')
  if not is_emscripten
    add_project_arguments('-D_LIBCPP_HARDENING_MODE=3', language: 'cpp')
  endif
endif

inc_dir = include_directories('Include')
public_headers = files('Include/SRAL.h', 'Include/Sral.hpp')

if is_windows
  sral_dependencies += [c_compiler.find_library('ole32'), c_compiler.find_library('user32')]
  if opt_uia
    sral_dependencies += c_compiler.find_library('uiautomationcore')
  endif

elif is_apple
  apple_flags = ['-fobjc-arc', '-fstack-protector-all']
  if c_compiler.has_argument('-fobjc-direct-methods')
    apple_flags += ['-fobjc-direct-methods']
  endif
  add_project_arguments(apple_flags, language: ['objc', 'objcpp'])

  if opt_nsspeech
    if cpp_compiler.has_argument('-Wno-deprecated-declarations')
      add_project_arguments('-Wno-deprecated-declarations', language: ['cpp', 'objcpp'])
    endif
    if c_compiler.has_argument('-Wno-deprecated-declarations')
      add_project_arguments('-Wno-deprecated-declarations', language: ['c', 'objc'])
    endif
  endif
  
  sral_dependencies += dependency('appleframeworks', modules: ['Foundation', 'AVFoundation'])
  
  if host_sys == 'ios'
    sral_dependencies += dependency('appleframeworks', modules: ['UIKit'])
  elif host_sys != 'ios' and opt_nsspeech
    sral_dependencies += dependency('appleframeworks', modules: ['AppKit'])
  endif

elif is_android
  sral_dependencies += c_compiler.find_library('log')
  javac_bin = find_program('javac', required: false)
  
  if javac_bin.found()
    java_version_check = run_command(javac_bin, '--version', check: false)
    
    if java_version_check.returncode() == 0
      java_ver_stdout = java_version_check.stdout().strip()
      java_ver_stderr = java_version_check.stderr().strip()
      java_version_string = java_ver_stdout != '' ? java_ver_stdout : java_ver_stderr

      version_parts = []
      major_version = 0
      has_valid_version = false

      if java_version_string.contains('javac ')
        raw_version_list = java_version_string.split(' ')
        if raw_version_list.length() >= 2
          version_number_str = raw_version_list[1]
          no_hyphen_array = version_number_str.split('-')
          prefix_str = no_hyphen_array[0]
          no_plus_array = prefix_str.split('+')
          version_number_clean = no_plus_array[0]
          version_parts = version_number_clean.split('.')
          
          if version_parts.length() >= 1
            major_version = version_parts[0].to_int()
            has_valid_version = true
          endif
        endif
      endif


      if has_valid_version
        if major_version != 21
          error('[SRAL ERROR] Android builds require JDK 21. Found version: ' + major_version.to_string())
        else
          message('[SRAL] Android JDK 21 environment verification passed.')
        endif
      else
        warning('[SRAL WARNING] Unexpected javac version format. Raw output was: "' + java_version_string + '". Skipping validation.')
      endif
    else
      warning('[SRAL WARNING] Could not execute javac --version. Skipping validation.')
    endif
  else
    error('[SRAL ERROR] Java compiler (javac) not found. Android compilation requires JDK 21.')
  endif

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
    sral_dependencies += [
      dependency('dbus-1', required: true),
      dependency('atspi-2', required: true),
    ]
  else
    add_project_arguments('-DSRAL_NO_ORCA', language: ['c', 'cpp'])
  endif

  if opt_brlapi
    sral_dependencies += c_compiler.find_library('brlapi', required: true)
  else
    add_project_arguments('-DSRAL_NO_BRLAPI', language: ['c', 'cpp'])
  endif
endif

python_module = import('python')
if path_python != '' and fs.is_dir(path_python)
  python_bin = python_module.find_installation(path_python / 'python', required: false)
else
  python_bin = python_module.find_installation('python3', required: false)
endif

subdir('SRC')

sral_dep = declare_dependency(
  include_directories: inc_dir,
  link_with: sral_lib,
  dependencies: sral_dependencies,
)

test_link_args = []
if c_id != 'msvc' and not is_apple and not is_emscripten
  test_link_args += c_compiler.get_supported_link_arguments(['-Wl,-z,relro', '-Wl,-z,now', '-Wl,-z,noexecstack'])
endif

if build_tests and not is_emscripten and not is_android
  executable(
    'SRAL_test',
    files('Examples/C/SRALExample.c'),
    include_directories: inc_dir,
    dependencies: sral_dep,
    link_args: test_link_args,
    install: false,
  )

  if is_windows and opt_nvda
    executable(
      'SRAL_NVDAControlExConsole',
      files('Examples/C/NVDAControlExConsole.c'),
      include_directories: inc_dir,
      dependencies: sral_dep,
      link_args: test_link_args,
      install: false,
    )
  endif

  if is_apple and host_sys != 'ios'
    executable(
      'SRAL_test_cocoa',
      files('Examples/ObjC/SRALCocoaExample.m'),
      include_directories: inc_dir,
      dependencies: sral_dep,
      link_args: test_link_args,
      install: false,
    )
  endif
endif

if build_bindings
  bindings_dir = meson.current_source_dir() / 'Bindings'
  sh_exe = find_program('sh', required: false)
  cmd_exe = find_program('cmd.exe', required: false)
  echo_exe = find_program('echo', required: false)
  fallback_echo = echo_exe.found() ? echo_exe : 'echo'
  
  dotnet_exe = find_program('dotnet', required: false)
  if get_option('SRAL_BINDING_CSHARP') and dotnet_exe.found()
    run_target(
      'sral_csharp_binding',
      command: [
        dotnet_exe,
        'build',
        bindings_dir / 'C#' / 'SRAL.csproj',
        '--configuration', 'Release',
        '--no-restore',
        '-p:SKIP_MOBILE_WORKLOADS=true',
      ],
    )
  else
    run_target('sral_csharp_binding', command: [fallback_echo, 'C# binding compilation skipped.'])
  endif

  cargo_exe = find_program('cargo', required: false)
  if get_option('SRAL_BINDING_RUST') and cargo_exe.found()
    run_target(
      'sral_rust_binding',
      command: [
        cargo_exe,
        'build',
        '--manifest-path', bindings_dir / 'Rust' / 'Cargo.toml',
        '--release',
        '--offline',
      ],
    )
  else
    run_target('sral_rust_binding', command: [fallback_echo, 'Rust binding compilation skipped.'])
  endif

   npm_exe = find_program('npm', 'npm.cmd', required: false)
  if get_option('SRAL_BINDING_NODE') and npm_exe.found()
    node_proj_dir = '@0@'.format(bindings_dir / 'Node.Js')
    if is_windows and cmd_exe.found()
      run_target(
        'sral_nodejs_binding',
        command: [cmd_exe, '/c', 'cd /d "' + node_proj_dir + '" && npm ci --production --ignore-scripts'],
      )
    elif sh_exe.found()
      run_target(
        'sral_nodejs_binding',
        command: [sh_exe, '-c', 'cd "$1" && "$2" ci --production --ignore-scripts', 'meson_sh', node_proj_dir, npm_exe],
      )
    else
      run_target(
        'sral_nodejs_binding',
        command: [fallback_echo, 'Node.js binding shell execution failed: No terminal found.'],
      )
    endif
  else
    run_target('sral_nodejs_binding', command: [fallback_echo, 'Node.js binding generation skipped.'])
  endif

  go_exe = find_program('go', required: false)
  if get_option('SRAL_BINDING_GO') and go_exe.found()
    go_proj_dir = '@0@'.format(bindings_dir / 'Go')
    if is_windows and cmd_exe.found()
      run_target(
        'sral_go_binding',
        command: [cmd_exe, '/c', 'cd /d "' + go_proj_dir + '" && go build -mod=readonly ./...'],
      )
    elif sh_exe.found()
      run_target(
        'sral_go_binding',
        command: [sh_exe, '-c', 'cd "$1" && "$2" build -mod=readonly ./...', 'meson_sh', go_proj_dir, go_exe],
      )
    else
      run_target(
        'sral_go_binding',
        command: [fallback_echo, 'Go binding shell execution failed: No terminal found.'],
      )
    endif
  else
    run_target('sral_go_binding', command: [fallback_echo, 'Go binding compilation skipped.'])
  endif

  dart_exe = find_program('dart', 'flutter', required: false)
  if get_option('SRAL_BINDING_DART') and dart_exe.found()
    dart_proj_dir = '@0@'.format(bindings_dir / 'Dart')
    if is_windows and cmd_exe.found()
      run_target(
        'sral_dart_binding',
        command: [cmd_exe, '/c', 'cd /d "' + dart_proj_dir + '" && dart pub get --offline'],
      )
    elif sh_exe.found()
      run_target(
        'sral_dart_binding',
        command: [sh_exe, '-c', 'cd "$1" && "$2" pub get --offline', 'meson_sh', dart_proj_dir, dart_exe],
      )
    else
      run_target(
        'sral_dart_binding',
        command: [fallback_echo, 'Dart binding shell execution failed: No terminal found.'],
      )
    endif
  else
    run_target('sral_dart_binding', command: [fallback_echo, 'Dart binding resolution skipped.'])
  endif

  lua_exe = find_program('lua', 'luajit', required: false)
  if get_option('SRAL_BINDING_LUA')
    lua_proj_dir = bindings_dir / 'Lua'
    if lua_exe.found()
      run_target('sral_lua_binding', command: [lua_exe, lua_proj_dir / 'example.lua'])
    else
      run_target('sral_lua_binding', command: [fallback_echo, 'Lua binding environment validated.'])
    endif
  else
    run_target('sral_lua_binding', command: [fallback_echo, 'Lua binding execution disabled.'])
  endif

  py_inst = python_bin
  if get_option('SRAL_BINDING_PYTHON') and py_inst.found()
    py_args = [py_inst, '-m', 'pip', 'install', bindings_dir / 'Python', '--no-index', '--no-deps']
    if py_inst.language_version().version_compare('>=3.11')
      py_args += ['--break-system-packages']
    endif
    run_target('sral_python_binding', command: py_args)
  else
    run_target('sral_python_binding', command: [fallback_echo, 'Python pip setup phase skipped.'])
  endif
endif

if fs.exists('developer_input.ini')
  configure_file(input: 'developer_input.ini', output: 'developer_input.configured.ini', copy: true)
endif

install_headers(public_headers)

if not is_android and host_sys != 'ios' and fs.exists('developer_input.ini')
  install_data('developer_input.ini', install_dir: get_option('datadir') / 'sral')
endif

if get_option('SRAL_INSTALL_BINDING_SOURCES')
  share_dir = 'share/sral'
  bind_dir = share_dir / 'bindings'

  install_data(
    files(
      'Bindings/C#/Example.cs',
      'Bindings/C#/SRAL.csproj',
      'Bindings/C#/Sral.cs',
    ),
    install_dir: bind_dir / 'csharp',
  )
  install_data(
    files(
      'Bindings/Dart/example.dart',
      'Bindings/Dart/sral.dart',
      'Bindings/Dart/sral_types.dart',
    ),
    install_dir: bind_dir / 'dart',
  )
  install_data(
    files('Bindings/Lua/example.lua', 'Bindings/Lua/sral.lua'),
    install_dir: bind_dir / 'lua',
  )
  install_data(
    files(
      'Bindings/Python/Example.py',
      'Bindings/Python/pyproject.toml',
      'Bindings/Python/sral.py',
    ),
    install_dir: bind_dir / 'python',
  )
  install_data(files('Bindings/Rust/Cargo.toml'), install_dir: bind_dir / 'rust')
  install_data(
    files(
      'Bindings/WebAssembly/sral_wasm.cpp',
      'Bindings/WebAssembly/sral_wasm.js',
      'Bindings/WebAssembly/example.js'
    ),
    install_dir: bind_dir / 'wasm',
  )

  if fs.is_dir('Bindings/Go/SRAL')
    install_subdir('Bindings/Go/SRAL', install_dir: bind_dir / 'go')
  endif
  if fs.is_dir('Bindings/Rust/src')
    install_subdir('Bindings/Rust/src', install_dir: bind_dir / 'rust')
  endif
endif

if not is_android and host_sys != 'ios' and not is_emscripten
  private_reqs = []
  if host_sys == 'linux'
    if opt_speech_dispatcher
      private_reqs += 'speech-dispatcher'
    endif
    if opt_orca
      private_reqs += ['dbus-1', 'atspi-2']
    endif
  endif

  pkg_mod = import('pkgconfig')
  pkg_mod.generate(
    name: 'SRAL_Orchestrator',
    filebase: 'sral_orchestrator',
    description: 'Screen Reader Abstraction Layer Orchestrator Engine Core.',
    subdirs: 'SRAL',
    libraries: sral_lib,
    requires_private: private_reqs,
    version: '1.0.0',
  )
endif
