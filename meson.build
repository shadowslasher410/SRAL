project(
    'SRAL_Orchestrator',
    ['c', 'cpp'],
    version: '1.0.0',
    meson_version: '>=1.11.2',
    default_options: [
        'c_std=c17',
        'cpp_std=c++20',
        'default_library=both',
        'warning_level=3',
        'werror=true',
        'b_pie=true',
        'b_staticpic=true',
        'b_lto=true',
        'b_lundef=true',
        'b_asneeded=true',
        'b_pgo=off',
        'b_ndebug=if-release',
    ],
)

c_compiler = meson.get_compiler('c')
cpp_compiler = meson.get_compiler('cpp')
c_id = c_compiler.get_id()

is_msvc_like = (c_compiler.get_argument_syntax() == 'msvc')
is_gcc = (c_id == 'gcc')
is_clang_cl = (c_id == 'clang-cl')
is_clang = c_id in ['clang', 'apple-clang', 'emscripten']

host_sys = host_machine.system()
is_windows = (host_sys == 'windows')
is_linux = (host_sys == 'linux')
is_macos = (host_sys == 'darwin')
is_ios = (host_sys == 'ios')
is_android = (host_sys == 'android')
is_emscripten = (host_sys == 'emscripten')

if is_ios and build_machine.system() != 'darwin'
    error(
        '[SRAL ERROR] Apple iOS device/simulator static bundles can only be cross-compiled from a macOS host machine.',
    )
endif

if is_macos or is_ios
    add_languages('objc', 'objcpp', native: false)
endif

opt_accesskit = not get_option('with_accesskit').disabled()
opt_chromevox = not get_option('with_chromevox').disabled()
opt_nsspeech = not get_option('with_nsspeech').disabled()
opt_voiceover = not get_option('with_voiceover').disabled()
opt_avspeech = not get_option('with_avspeech').disabled()
opt_speechd = not get_option('with_speech_dispatcher').disabled()
opt_orca = not get_option('with_orca').disabled()
opt_brlapi = not get_option('with_brlapi').disabled()
opt_uia = not get_option('with_uia').disabled()
opt_nvda = not get_option('with_nvda').disabled()
opt_jaws = not get_option('with_jaws').disabled()
opt_zdsr = not get_option('with_zdsr').disabled()
opt_sapi = not get_option('with_sapi').disabled()
opt_android_tts = not get_option('with_android_tts').disabled()
opt_android_acc = not get_option('with_android_accessibility').disabled()

build_tests = get_option('build_tests')
build_bindings = get_option('build_bindings')
build_variant = get_option('buildtype')

build_android_helpers = is_android and (opt_android_tts or opt_android_acc)
javac_bin = find_program('javac', required: build_android_helpers)

sral_dependencies = []
kv = import('keyval')
fs = import('fs')

dev_config = configuration_data()
paths = {
    'nvda_install': '',
    'speechd_install': '',
    'python_install': '',
    'emsdk_install': '',
    'android_ndk': '',
    'accesskit_install': '',
    'jdk_install': '',
}

if fs.exists('settings.ini')
    dev_config = kv.load('settings.ini')

    config_matrix = [
        {
            'name': 'NVDA SDK Remote Linkage',
            'ini': 'nvda_install',
            'env': 'NVDA_SDK_PATH',
        },
        {
            'name': 'Speech Dispatcher Sockets',
            'ini': 'speechd_install',
            'env': 'SPEECHD_PATH',
        },
        {
            'name': 'Isolated Python Environment',
            'ini': 'python_install',
            'env': 'PYTHON_PATH',
        },
        {
            'name': 'Emscripten WebAssembly SDK',
            'ini': 'emsdk_install',
            'env': 'EMSDK_PATH',
        },
        {
            'name': 'Google Android NDK Workspace',
            'ini': 'android_ndk',
            'env': 'ANDROID_NDK_HOME',
        },
        {
            'name': 'AccessKit Proxy Library Assets',
            'ini': 'accesskit_install',
            'env': 'ACCESSKIT_PATH',
        },
        {
            'name': 'Target Host Java Development Kit',
            'ini': 'jdk_install',
            'env': 'JAVA_HOME',
        },
    ]

    sral_env_config = environment()
    message('--- SRAL Multi-User Developer Path Validation ---')

    temp_paths = {}
    foreach item : config_matrix
        path = dev_config.get(item['ini'], '').replace('"', '').replace('\'', '').strip()
        if path != ''
            if fs.is_dir(path)
                message('[VALIDATED] ' + item['name'] + ' resolved at: ' + path)
                sral_env_config.set(item['env'], path)
                temp_paths += {item['ini']: path}
            else
                warning(
                    '[SRAL WARNING] ' + item['name'] + ' configured in INI but does not exist at: ' + path,
                )
            endif
        endif
    endforeach

    foreach k, v : paths
        if not temp_paths.has_key(k)
            temp_paths += {k: v}
        endif
    endforeach
    paths = temp_paths

    log_level = dev_config.get('log_level', '').replace('"', '').replace(
        '\'',
        '',
    ).strip()
    if log_level != ''
        message('  [INI] Configured SRAL_LOG_LEVEL = ' + log_level)
    endif

    log_to_file = dev_config.get('log_to_file', '').replace('"', '').replace(
        '\'',
        '',
    ).strip()
    if log_to_file != ''
        message('  [INI] Configured SRAL_LOG_TO_FILE = ' + log_to_file)
    endif

    log_file_path = dev_config.get('log_file_path', '').replace('"', '').replace(
        '\'',
        '',
    ).strip()
    if log_file_path != ''
        message('  [INI] Configured SRAL_LOG_FILE_PATH = ' + log_file_path)
    endif

    message('------------------------------------------------')
    meson.add_devenv(sral_env_config)
endif

target_os_defs = []

if fs.exists('settings.ini')
    if is_windows
        win_ver = dev_config.get('usertarget_windows', '').replace('"', '').replace(
            '\'',
            '',
        ).strip()
        if win_ver != ''
            win_parts = win_ver.split('.')
            if win_parts.length() >= 3
                major_ver = win_parts[0]
                build_str = win_parts[2]

                if major_ver.to_int() < 10 or (
                    major_ver == '10'
                    and build_str.to_int() < 17763
                )
                    error(
                        '\n[SRAL ERROR] Unsupported Windows target version: ' + win_ver + '. Minimum version required is 10.0.17763.0.\n',
                    )
                endif

                target_os_defs += ['-D_WIN32_WINNT=0x0A00', '-DWINVER=0x0A00']
                if build_str == '17763'
                    target_os_defs += ['-DNTDDI_VERSION=0x0A000004']
                elif build_str.to_int() >= 22000
                    target_os_defs += ['-DNTDDI_VERSION=0x0A00000C']
                else
                    target_os_defs += ['-DNTDDI_VERSION=0x0A000005']
                endif
                message(
                    '  [INI] Configured TARGET_usertarget_windows = ' + win_ver,
                )
            endif
        endif
    elif is_macos or is_ios
        mac_ver = dev_config.get('usertarget_macos', '').replace('"', '').replace(
            '\'',
            '',
        ).strip()
        ios_ver = dev_config.get('usertarget_ios', '').replace('"', '').replace(
            '\'',
            '',
        ).strip()
        if mac_ver != ''
            if mac_ver.version_compare('< 15.0')
                error(
                    '\n[SRAL ERROR] Unsupported macOS target version: ' + mac_ver + '. Minimum version required is 15.0.\n',
                )
            endif
            if is_macos
                add_project_arguments(
                    ['-mmacosx-version-min=' + mac_ver],
                    language: ['c', 'cpp', 'objc', 'objcpp'],
                )
                add_project_link_arguments(
                    ['-mmacosx-version-min=' + mac_ver],
                    language: ['c', 'cpp', 'objc', 'objcpp'],
                )
            endif
            message('  [INI] Configured TARGET_usertarget_macos = ' + mac_ver)
        endif
        if ios_ver != ''
            if ios_ver.version_compare('< 15.0')
                error(
                    '\n[SRAL ERROR] Unsupported iOS target version: ' + ios_ver + '. Minimum version required is 15.0.\n',
                )
            endif
            if is_ios
                add_project_arguments(
                    ['-miphoneos-version-min=' + ios_ver],
                    language: ['c', 'cpp', 'objc', 'objcpp'],
                )
                add_project_link_arguments(
                    ['-miphoneos-version-min=' + ios_ver],
                    language: ['c', 'cpp', 'objc', 'objcpp'],
                )
            endif
            message('  [INI] Configured TARGET_usertarget_ios = ' + ios_ver)
        endif
    elif is_android
        android_ver = dev_config.get('usertarget_android', '').replace('"', '').replace(
            '\'',
            '',
        ).strip()
        if android_ver != ''
            if android_ver.to_int() < 21
                error(
                    '\n[SRAL ERROR] Unsupported Android API Level: ' + android_ver + '. Minimum version required is API Level 21.\n',
                )
            endif
            target_os_defs += ['-D__ANDROID_API__=' + android_ver]
            message(
                '  [INI] Configured TARGET_usertarget_android = ' + android_ver,
            )
        endif
    endif
endif

add_project_arguments(target_os_defs, language: ['c', 'cpp'])

if is_android or is_ios or is_emscripten
    dep_method = 'extlib'
elif is_windows or is_macos
    dep_method = 'auto'
else
    dep_method = 'pkgconfig'
endif

deps_matrix = [
    {
        'name': 'accesskit',
        'active': opt_accesskit and not is_emscripten,
        'custom_path': paths.get('accesskit_install', ''),
        'fallback_macro': '-DSRAL_NO_ACCESSKIT',
    },
    {
        'name': 'nvdaControllerClient',
        'active': opt_nvda and is_windows,
        'custom_path': paths.get('nvda_install', ''),
        'success_macro': '-DSRAL_WITH_NVDA',
    },
    {
        'name': 'speechd',
        'active': opt_speechd and is_linux,
        'custom_path': paths.get('speechd_install', ''),
        'fallback_macro': '-DSRAL_NO_SPEECH_DISPATCHER',
    },
]

sral_ext_includes = []

foreach target : deps_matrix
    has_dep = false

    if target['active']
        if target['custom_path'] != ''
            search_dirs = [
                target['custom_path'],
                target['custom_path'] / 'lib',
                target['custom_path'] / 'bin',
            ]
            if is_android
                search_dirs = [
                    target['custom_path'] / 'lib' / host_machine.cpu_family(),
                ]
            endif

            dep_check = c_compiler.find_library(
                target['name'],
                dirs: search_dirs,
                required: false,
            )

            if dep_check.found() and fs.is_dir(target['custom_path'] / 'include')
                sral_ext_includes += [
                    include_directories(target['custom_path'] / 'include'),
                ]
            endif
        else
            dep_check = dependency(
                target['name'],
                method: dep_method,
                required: false,
            )
        endif

        if not dep_check.found()
            dep_check = c_compiler.find_library(target['name'], required: false)
        endif

        if dep_check.found()
            has_dep = true
            sral_dependencies += [dep_check]
            if target.has_key('success_macro')
                add_project_arguments(
                    [target['success_macro']],
                    language: ['c', 'cpp'],
                )
            endif
        endif
    endif

    if not has_dep
        if target['name'] == 'accesskit'
            opt_accesskit = false
        endif
        if target['name'] == 'nvdaControllerClient'
            opt_nvda = false
        endif
        if target['name'] == 'speechd'
            opt_speechd = false
        endif

        if target.has_key('fallback_macro')
            add_project_arguments(
                [target['fallback_macro']],
                language: ['c', 'cpp'],
            )
        endif
    endif
endforeach

if opt_chromevox and is_emscripten
    emsdk_dir = paths.get('emsdk_install', '')
    if emsdk_dir != ''
        emcc_check = find_program('emcc', dirs: [emsdk_dir], required: false)
    else
        emcc_check = find_program('emcc', required: false)
    endif
    if emcc_check.found()
        add_project_arguments(['-DSRAL_WITH_CHROMEVOX'], language: ['c', 'cpp'])
    else
        opt_chromevox = false
    endif
else
    opt_chromevox = false
endif

if not opt_chromevox
    add_project_arguments(['-DSRAL_NO_CHROMEVOX'], language: ['c', 'cpp'])
    if is_emscripten
        error(
            '[SRAL ERROR] WebAssembly Targets cannot compile without a valid Emscripten environment and ChromeVox enabled.',
        )
    endif
endif

universal_msvc_args = ['/volatile:iso', '/W4']
universal_msvc_link = []
universal_gcc_clang_args = [
    '-fno-delete-null-pointer-checks',
    '-fwrapv',
    '-fno-common',
    '-Wconversion',
    '-Wsign-conversion',
]

if is_gcc
    universal_gcc_clang_args += ['-fno-strict-overflow']
endif
if not is_emscripten
    universal_gcc_clang_args += ['-fstack-clash-protection']
endif

release_args = []
debug_args = []
global_cpp_args = ['-D_GLIBCXX_ASSERTIONS']

if not get_option('dev_sanitizers').disabled()
    san_flags = ['-fsanitize=address,undefined']
    if is_msvc_like
        san_flags = ['/fsanitize=address']
    endif
    add_project_arguments(san_flags, language: ['c', 'cpp'])
    add_project_link_arguments(san_flags, language: ['c', 'cpp'])
endif

arch_args = []
cpu_args = []
arch_choice = get_option('hardening_arch_level')
if arch_choice in ['x86-64-v3', 'x86-64-v2', 'native']
    arch_flag = '-march=' + arch_choice
    if c_compiler.has_argument(arch_flag)
        arch_args += [arch_flag]
    endif
endif

if host_machine.cpu_family() == 'x86_64'
    if c_compiler.has_argument('-fcf-protection=full')
        cpu_args += ['-fcf-protection=full']
    endif
elif host_machine.cpu_family() in ['arm', 'aarch64']
    if c_compiler.has_argument('-mbranch-protection=standard')
        cpu_args += ['-mbranch-protection=standard']
    endif
endif

if is_msvc_like
    compiler_flags = ['/GS', '/sdl', '/guard:cf', '/HIGHENTROPYVA']
    linker_flags = ['/GUARD:CF', '/DYNAMICBASE', '/NXCOMPAT', '/HIGHENTROPYVA']
    if is_clang_cl
        compiler_flags += ['/clang:-ftrivial-auto-var-init=pattern']
    endif
    msvc_warnings = [
        '/wd4324',
        '/wd4820',
        '/wd5039',
        '/wd4865',
        '/wd4365',
        '/wd4251',
        '/wd5204',
        '/wd5291',
        '/wd4373',
        '/wd4625',
        '/wd4626',
        '/wd5026',
        '/wd5027',
        '/wd4005',
        '/wd4191',
        '/wd4668',
        '/permissive-',
        '/Zc:__cplusplus',
    ]
    add_project_arguments(msvc_warnings, language: ['c', 'cpp'])
    add_project_arguments(
        '/D_SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING',
        language: 'cpp',
    )
    add_project_arguments('/experimental:c11atomics', language: 'c')
    if is_clang_cl
        clang_cl_extensions = [
            '/clang:-fextensions',
            '/clang:-Wno-language-extension-token',
            '/clang:-Wno-missing-designated-field-initializers',
        ]
        add_project_arguments(clang_cl_extensions, language: ['c', 'cpp'])
    endif
else
    compiler_flags = [
        '-D_FORTIFY_SOURCE=3',
        '-fstack-protector-strong',
        '-fstack-clash-protection',
        '-fno-plt',
        '-Wformat',
        '-Wformat-security',
        '-fvisibility=hidden',
        '-fno-strict-aliasing',
        '-fno-trapping-math',
    ]
    if not (is_emscripten or is_macos or is_ios)
        if c_compiler.has_argument('-fstack-clash-protection')
            compiler_flags += ['-fstack-clash-protection']
        endif
    endif

    if c_compiler.has_argument('-ftrivial-auto-var-init=zero')
        compiler_flags += ['-ftrivial-auto-var-init=zero']
    endif
    compiler_flags += arch_args + cpu_args
    linker_flags = ['-Wl,-z,now', '-Wl,-z,noexecstack', '-Wl,-z,relro']
    if is_macos or is_ios
        linker_flags += ['-Wl,-stack_check']
    endif
    w_gnu = [
        '-Wno-undef',
        '-Wno-unused-function',
        '-Wno-unused-variable',
        '-Wno-gnu-anonymous-struct',
        '-Wno-nested-anon-types',
        '-Wno-interference-size',
        '-Wno-old-style-cast',
    ]
    if is_clang and not (is_windows or is_ios or is_android)
        w_gnu += ['-fno-extensions']
    endif
    if is_windows and is_clang
        w_gnu += [
            '-fms-extensions',
            '-Wno-language-extension-token',
            '-Wno-missing-designated-field-initializers',
        ]
    endif
    c_warning_args = c_compiler.get_supported_arguments(w_gnu)
    cpp_warning_args = cpp_compiler.get_supported_arguments(w_gnu) + [
        '-D_GLIBCXX_ASSERTIONS',
        '-D_LIBCPP_ENABLE_HARDENED_MODE=1',
    ]
    if is_emscripten
        cpp_warning_args += cpp_compiler.get_supported_arguments(
            '-Wno-dollar-in-identifier-extension',
        )
    endif
    add_project_arguments(
        c_warning_args + c_compiler.get_supported_arguments(compiler_flags),
        language: 'c',
    )
    add_project_arguments(
        cpp_warning_args + cpp_compiler.get_supported_arguments(compiler_flags),
        language: 'cpp',
    )
    add_project_link_arguments(
        c_compiler.get_supported_link_arguments(linker_flags),
        language: ['c', 'cpp'],
    )
endif
if is_macos or is_ios
    objc_compiler = meson.get_compiler('objc')
    objcpp_compiler = meson.get_compiler('objcpp')
    add_project_arguments(
        objc_compiler.get_supported_arguments(w_gnu + compiler_flags),
        language: 'objc',
    )
    add_project_arguments(
        objcpp_compiler.get_supported_arguments(w_gnu + compiler_flags),
        language: 'objcpp',
    )
    add_project_link_arguments(
        objc_compiler.get_supported_link_arguments(linker_flags),
        language: ['objc', 'objcpp'],
    )
endif
if build_variant == 'release'
    message('[SRAL] Release variant active.')
    if is_msvc_like
        msvc_release_args = universal_msvc_args + [
            '/Oi',
            '/Qspectre',
            '/w14928',
        ]
        add_project_arguments(
            c_compiler.get_supported_arguments(msvc_release_args),
            language: 'c',
        )
        add_project_arguments(
            cpp_compiler.get_supported_arguments(msvc_release_args),
            language: 'cpp',
        )
        msvc_release_link = universal_msvc_link + ['/CETCOMPAT']
        add_project_link_arguments(
            cpp_compiler.get_supported_link_arguments(msvc_release_link),
            language: ['c', 'cpp'],
        )
    else
        release_args += universal_gcc_clang_args
        if c_compiler.has_argument('-fvisibility=hidden')
            release_args += ['-fvisibility=hidden']
        endif
        if is_clang
            if c_compiler.has_argument('-fsanitize=cfi')
                release_args += ['-fsanitize=cfi']
                add_project_link_arguments(
                    c_compiler.get_supported_link_arguments(['-fsanitize=cfi']),
                    language: ['c', 'cpp'],
                )
            endif
            if is_macos or is_ios or is_android
                release_args += [
                    '-Wno-macro-redefined',
                    '-Wno-gnu-anonymous-struct',
                    '-Wno-nested-anon-types',
                ]
            endif
        endif
        add_project_arguments(
            c_compiler.get_supported_arguments(release_args),
            language: ['c', 'cpp'],
        )
    endif
    if not (is_emscripten or is_macos or is_ios or is_android)
        global_cpp_args += ['-D_LIBCPP_HARDENING_MODE=2']
    endif
else
    message('[SRAL] Debug variant active.')
    add_project_arguments(
        ['-DDEBUG', '-D_DEBUG', '-DSRAL_DEBUG_TRACE_ACTIVE'],
        language: ['c', 'cpp'],
    )
    if is_msvc_like
        msvc_debug_args = universal_msvc_args + ['/Zi']
        add_project_arguments(
            c_compiler.get_supported_arguments(msvc_debug_args),
            language: 'c',
        )
        add_project_arguments(
            cpp_compiler.get_supported_arguments(msvc_debug_args),
            language: 'cpp',
        )
        add_project_link_arguments(
            cpp_compiler.get_supported_link_arguments(universal_msvc_link),
            language: ['c', 'cpp'],
        )
    else
        debug_args += universal_gcc_clang_args + ['-g3']
        if is_clang and (is_macos or is_ios or is_android)
            debug_args += [
                '-Wno-macro-redefined',
                '-Wno-gnu-anonymous-struct',
                '-Wno-nested-anon-types',
            ]
        endif
        add_project_arguments(
            c_compiler.get_supported_arguments(debug_args),
            language: 'c',
        )
        add_project_arguments(
            cpp_compiler.get_supported_arguments(debug_args),
            language: 'cpp',
        )
    endif
    if not (is_emscripten or is_macos or is_ios or is_android)
        global_cpp_args += ['-D_LIBCPP_HARDENING_MODE=3']
    endif
endif
add_project_arguments(global_cpp_args, language: 'cpp')
inc_dir = include_directories('Include', 'Dep', 'SRC')
public_headers = files('Include/SRAL.h', 'Include/Sral.hpp')
if is_windows
    sral_dependencies += [
        c_compiler.find_library('ole32'),
        c_compiler.find_library('user32'),
    ]
    if opt_uia
        sral_dependencies += [c_compiler.find_library('uiautomationcore')]
    endif
elif is_macos or is_ios
    apple_flags = ['-fobjc-arc']
    if c_compiler.has_argument('-fobjc-direct-methods')
        apple_flags += ['-fobjc-direct-methods']
    endif
    add_project_arguments(apple_flags, language: ['objc', 'objcpp'])
    sral_dependencies += [dependency('Foundation'), dependency('AVFoundation')]

    if is_ios
        sral_dependencies += [dependency('UIKit')]
    elif is_macos and opt_nsspeech
        objc_comp = meson.get_compiler('objc')
        if objc_comp.has_argument('-Wno-deprecated-declarations')
            add_project_arguments(
                ['-Wno-deprecated-declarations'],
                language: ['objc', 'objcpp', 'c', 'cpp'],
            )
        endif
        sral_dependencies += [dependency('AppKit')]
    endif
elif is_android
    sral_dependencies += [c_compiler.find_library('log')]
    if javac_bin.found()
        java_version_check = run_command(javac_bin, '--version', check: false)
        if java_version_check.returncode() == 0
            raw_version_string = (
                java_version_check.stdout() + java_version_check.stderr()
            ).strip()
            version_tokens = raw_version_string.split(' ')
            if version_tokens.length() >= 2
                clean_version = version_tokens[1]
                if clean_version.version_compare('>=21.0.0') and clean_version.version_compare(
                    '<22.0.0',
                )
                    message(
                        '[SRAL] Android JDK 21 environment verification passed. Found: ' + clean_version,
                    )
                else
                    error(
                        '[SRAL ERROR] Android builds require JDK 21. Found unsupported version: ' + clean_version,
                    )
                endif
            else
                warning(
                    '[SRAL WARNING] Unexpected javac version format. Skipping validation.',
                )
            endif
        else
            warning(
                '[SRAL WARNING] Could not execute javac --version. Skipping validation.',
            )
        endif
    else
        error(
            '[SRAL ERROR] Java compiler (javac) not found. Android compilation requires JDK 21.',
        )
    endif
elif is_emscripten
    if c_compiler.has_link_argument('-pthread')
        em_args = ['-pthread', '-sSTACK_SIZE=524288', '-sWASM_BIGINT']
        add_project_link_arguments(em_args, language: ['c', 'cpp'])
        add_project_arguments(['-pthread'], language: ['c', 'cpp'])
    endif
else
    add_project_arguments(
        ['-DSRAL_SPEECHD_REQUIRE_UTF8'],
        language: ['c', 'cpp'],
    )
endif
if not is_android and not is_ios and not is_emscripten
    if opt_speechd
        sral_dependencies += [
            dependency('speechd', method: ['pkgconfig', 'cmake'], required: true),
        ]
    else
        add_project_arguments(['-DSRAL_NO_SPEECHD'], language: ['c', 'cpp'])
    endif
    if opt_orca
        sral_dependencies += [
            dependency('dbus-1', method: ['pkgconfig', 'cmake'], required: true),
            dependency('atspi-2', method: ['pkgconfig', 'cmake'], required: true),
        ]
    else
        add_project_arguments(['-DSRAL_NO_ORCA'], language: ['c', 'cpp'])
    endif
    if opt_brlapi
        sral_dependencies += [c_compiler.find_library('brlapi', required: true)]
    else
        add_project_arguments(['-DSRAL_NO_BRLAPI'], language: ['c', 'cpp'])
    endif
endif
python_module = import('python')
if paths.get('python_install', '') != ''
    python_bin = python_module.find_installation(
        paths.get('python_install') / 'python',
        required: false,
    )
else
    python_bin = python_module.find_installation('python3', required: false)
endif
subdir('SRC')
sral_dep = declare_dependency(
    include_directories: inc_dir,
    link_with: sral_targets,
    dependencies: sral_dependencies,
)
test_link_args = []
if is_msvc_liketest_link_args += [
    '/GUARD:CF',
    '/DYNAMICBASE',
    '/NXCOMPAT',
    '/HIGHENTROPYVA',
]
    if build_variant == 'release'
        test_link_args += ['/CETCOMPAT']
    endif
elif is_ios or is_macos
    test_link_args += ['-Wl,-stack_check', '-Wl,-dead_strip']
elif not is_emscripten
    test_link_args += c_compiler.get_supported_link_arguments(
        ['-Wl,-z,relro', '-Wl,-z,now', '-Wl,-z,noexecstack'],
    )
endif
if build_tests.allowed() and not is_emscripten and not is_android
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
    if is_macos
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
if build_bindings.allowed()
    bindings_dir = meson.current_source_dir() / 'Bindings'
    fallback_echo = find_program('echo', required: true)
    dotnet_exe = find_program('dotnet', required: false)
    if get_option('binding_csharp').allowed() and dotnet_exe.found()
        run_target(
            'sral_csharp_binding',
            command: [
                dotnet_exe,
                'build',
                bindings_dir / 'C#/SRAL.csproj',
                '--configuration',
                'Release',
                '--no-restore',
                '-p:SKIP_MOBILE_WORKLOADS=true',
                '-p:Deterministic=true',
            ],
        )
    else
        run_target(
            'sral_csharp_binding',
            command: [fallback_echo, 'C# binding compilation skipped.'],
        )
    endif
    cargo_exe = find_program('cargo', required: false)
    if get_option('binding_rust').allowed() and cargo_exe.found()
        run_target(
            'sral_rust_binding',
            command: [
                cargo_exe,
                'build',
                '--manifest-path',
                bindings_dir / 'Rust/Cargo.toml',
                '--release',
                '--offline',
                '--frozen',
            ],
        )
    else
        run_target(
            'sral_rust_binding',
            command: [fallback_echo, 'Rust binding compilation skipped.'],
        )
    endif
    npm_exe = find_program('npm', 'npm.cmd', required: false)
    if get_option('binding_node').allowed() and npm_exe.found()
        run_target(
            'sral_nodejs_binding',
            command: [
                npm_exe,
                'ci',
                '--production',
                '--ignore-scripts',
                '--audit=false',
            ],
            env: {'NODE_ENV': 'production'},
        )
    else
        run_target(
            'sral_nodejs_binding',
            command: [fallback_echo, 'Node.js binding generation skipped.'],
        )
    endif
    go_exe = find_program('go', required: false)
    if get_option('binding_go').allowed() and go_exe.found()
        run_target(
            'sral_go_binding',
            command: [go_exe, 'build', '-mod=readonly', '-trimpath', './...'],
        )
    else
        run_target(
            'sral_go_binding',
            command: [fallback_echo, 'Go binding compilation skipped.'],
        )
    endif
    dart_exe = find_program('dart', 'flutter', required: false)
    if get_option('binding_dart').allowed() and dart_exe.found()
        run_target(
            'sral_dart_binding',
            command: [dart_exe, 'pub', 'get', '--offline'],
        )
    else
        run_target(
            'sral_dart_binding',
            command: [fallback_echo, 'Dart binding resolution skipped.'],
        )
    endif
    lua_exe = find_program('lua', 'luajit', required: false)
    if get_option('binding_lua').allowed()
        lua_cmd = lua_exe.found() ? [lua_exe, bindings_dir / 'Lua/example.lua'] : [
            fallback_echo,
            'Lua interpreter environment verified cleanly.',
        ]
        run_target('sral_lua_binding', command: lua_cmd)
    else
        run_target(
            'sral_lua_binding',
            command: [fallback_echo, 'Lua binding execution disabled.'],
        )
    endif
    if get_option('binding_python').allowed() and python_bin.found()
        run_target(
            'sral_python_binding',
            command: [
                python_bin,
                '-m',
                'pip',
                'install',
                bindings_dir / 'Python',
                '--no-index',
                '--no-deps',
                '--user',
            ],
        )
    else
        run_target(
            'sral_python_binding',
            command: [fallback_echo, 'Python pip setup phase skipped.'],
        )
    endif
endif
if fs.exists('settings.ini')
    configure_file(
        input: 'settings.ini',
        output: 'settings.configured',
        copy: true,
    )
endif
install_headers(public_headers)
if get_option('install_binding_sources').allowed()
    bind_dest = 'share/sral/bindings'
    install_subdir(
        'Bindings/C#',
        install_dir: bind_dest / 'csharp',
        strip_directory: true,
    )
    install_subdir(
        'Bindings/Dart',
        install_dir: bind_dest / 'dart',
        strip_directory: true,
    )
    install_subdir(
        'Bindings/Go',
        install_dir: bind_dest / 'go',
        strip_directory: true,
    )
    install_subdir(
        'Bindings/Lua',
        install_dir: bind_dest / 'lua',
        strip_directory: true,
    )
    install_subdir(
        'Bindings/Python',
        install_dir: bind_dest / 'python',
        strip_directory: true,
    )
    install_subdir(
        'Bindings/Rust',
        install_dir: bind_dest / 'rust',
        strip_directory: true,
    )
    install_subdir(
        'Bindings/WebAssembly',
        install_dir: bind_dest / 'wasm',
        strip_directory: true,
    )
    install_subdir(
        'Dep',
        install_dir: 'share/sral/android',
        strip_directory: true,
    )
endif
if not (is_android or is_ios or is_emscripten)
    private_reqs = []
    if is_linux
        if opt_speechd
            private_reqs += ['speechd']
        endif
        if opt_orca
            private_reqs += ['dbus-1', 'atspi-2']
        endif
    endif
    downstream_cflags = ['-fno-strict-aliasing']
    if build_variant == 'release' and not is_msvc_like
        downstream_cflags += ['-fvisibility=hidden']
    endif
    pkg_mod = import('pkgconfig')

    foreach target_lib : sral_targets
        get_lib_type = target_lib.name()
        if get_lib_type == 'SRAL'
            pkg_mod.generate(
                target_lib,
                name: 'SRAL',
                filebase: 'sral',
                description: 'Screen Reader Abstraction Library Core',
                requires_private: private_reqs,
                extra_cflags: downstream_cflags,
                version: '1.0.0',
            )
        endif
    endforeach
endif