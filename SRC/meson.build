fs = import('fs')
assert(
  fs.name(meson.current_source_dir()) == 'SRC',
  'SRC/meson.build must be called via subdir() from the project root directory.',
)

root_src = meson.project_source_root()
dep_sources = []

if is_windows
	dep_sources += [
		root_src / 'Dep/nvda_control.c',
		root_src / 'Dep/fsapi.c',
		root_src / 'Dep/blastspeak.c',
	]
elif not is_apple and not is_android and not is_emscripten
	dep_sources += [root_src / 'Dep/utf-8.c']
endif

global_c_cpp_args = ['-Wall', '-Wextra']

if not is_msvc_like
	global_c_cpp_args += [
		'-fstack-protector-all',
		'-fno-delete-null-pointer-checks',
		'-fwrapv',
		'-fno-common',
		'-ftrivial-auto-var-init=zero',
	]

	if not is_emscripten
		global_c_cpp_args += ['-fstack-clash-protection']
	endif

	if c_compiler.has_argument('-fcf-protection=full')
		global_c_cpp_args += ['-fcf-protection=full']
	endif

	if c_compiler.has_argument('-mbranch-protection=all')
		global_c_cpp_args += ['-mbranch-protection=all']
	endif
endif

global_c_args = global_c_cpp_args
global_cpp_args = global_c_cpp_args

if not is_msvc_like
	global_c_args += ['-fno-strict-overflow']
endif

supported_global_c_args = c_compiler.get_supported_arguments(global_c_args)
supported_global_cpp_args = cpp_compiler.get_supported_arguments(global_cpp_args)

sral_deps_list = []
if dep_sources.length() > 0
	sral_deps_lib = static_library(
		'sral_deps',
		dep_sources,
		include_directories: inc_dir,
		c_args: supported_global_c_args + cross_platform_warning_args,
		cpp_args: supported_global_cpp_args + cross_platform_warning_args,
		pic: true,
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
	'-Wctor-dtor-privacy',
	'-Woverloaded-virtual',
]

if not is_msvc_like
	core_c_args = supported_global_c_args + c_compiler.get_supported_arguments(shared_warnings) + cross_platform_warning_args
	core_cpp_args = supported_global_cpp_args + cpp_compiler.get_supported_arguments(shared_warnings + cpp_exclusive_warnings) + cross_platform_warning_args
else
	core_c_args = supported_global_c_args + cross_platform_warning_args
	core_cpp_args = supported_global_cpp_args + cross_platform_warning_args
endif

core_sources = files('Engine.cpp', 'SRAL.cpp')

if not is_emscripten
	if opt_accesskit
		core_sources += files('ACAnnouncer.cpp')
	else
		core_c_args += ['-DSRAL_NO_ACCESSKIT']
		core_cpp_args += ['-DSRAL_NO_ACCESSKIT']
	endif
endif

if is_windows
	core_sources += files('Encoding.cpp')

	if opt_nvda
		core_sources += files('NVDA.cpp')
	else
		core_c_args += ['-DSRAL_NO_NVDA']
		core_cpp_args += ['-DSRAL_NO_NVDA']
	endif

	if get_option('SRAL_WITH_JAWS')
		core_sources += files('Jaws.cpp')
	else
		core_c_args += ['-DSRAL_NO_JAWS']
		core_cpp_args += ['-DSRAL_NO_JAWS']
	endif

	if get_option('SRAL_WITH_ZDSR')
		core_sources += files('ZDSR.cpp')
	else
		core_c_args += ['-DSRAL_NO_ZDSR']
		core_cpp_args += ['-DSRAL_NO_ZDSR']
	endif

	if get_option('SRAL_WITH_SAPI')
		core_sources += files('SAPI.cpp')
		core_sources += [root_src / 'Dep/wasapi.cpp']
	else
		core_c_args += ['-DSRAL_NO_SAPI']
		core_cpp_args += ['-DSRAL_NO_SAPI']
	endif

	if opt_uia
		core_sources += files('UIA.cpp')
		core_sources += [root_src / 'Dep/UIAProvider.cpp']
	else
		core_c_args += ['-DSRAL_NO_UIA']
		core_cpp_args += ['-DSRAL_NO_UIA']
	endif

elif is_apple
	if get_option('SRAL_WITH_VOICEOVER')
		core_sources += files('VoiceOver.mm')
	else
		core_c_args += ['-DSRAL_NO_VOICEOVER']
		core_cpp_args += ['-DSRAL_NO_VOICEOVER']
	endif

	if get_option('SRAL_WITH_AVSPEECH')
		core_sources += files('AVSpeech.mm')
	else
		core_c_args += ['-DSRAL_NO_AVSPEECH']
		core_cpp_args += ['-DSRAL_NO_AVSPEECH']
	endif

	if host_sys != 'ios' and opt_nsspeech
		core_sources += files('NSSpeech.mm')
	else
		core_c_args += ['-DSRAL_NO_NSSPEECH']
		core_cpp_args += ['-DSRAL_NO_NSSPEECH']
	endif

elif is_android
	core_sources += [root_src / 'Dep/AndroidContext.cpp']

	android_java_helpers = [
		root_src / 'Dep/AndroidAccessibilityManagerHelper.java',
		root_src / 'Dep/AndroidTTSHelper.java',
	]

	if get_option('SRAL_WITH_ANDROID_TTS')
		core_sources += files('AndroidTextToSpeech.cpp')
	else
		core_c_args += ['-DSRAL_NO_ANDROID_TTS']
		core_cpp_args += ['-DSRAL_NO_ANDROID_TTS']
	endif

	if get_option('SRAL_WITH_ANDROID_ACCESSIBILITY')
		core_sources += files('AndroidAccessibilityManager.cpp')
	else
		core_c_args += ['-DSRAL_NO_ANDROID_ACCESSIBILITY']
		core_cpp_args += ['-DSRAL_NO_ANDROID_ACCESSIBILITY']
	endif

elif is_emscripten
	if opt_chromevox
		core_sources += files('ChromeVox.cpp')
	else
		core_c_args += ['-DSRAL_NO_CHROMEVOX']
		core_cpp_args += ['-DSRAL_NO_CHROMEVOX']
	endif

	core_args_em = [
		'-DSRAL_NO_NVDA',
		'-DSRAL_NO_JAWS',
		'-DSRAL_NO_ZDSR',
		'-DSRAL_NO_SAPI',
		'-DSRAL_NO_UIA',
		'-DSRAL_NO_SPEECH_DISPATCHER',
		'-DSRAL_NO_ORCA',
		'-DSRAL_NO_BRLAPI',
	]
	core_c_args += core_args_em
	core_cpp_args += core_args_em

else
	core_sources += files('Encoding.cpp')

	if opt_speech_dispatcher
		core_sources += files('SpeechDispatcher.cpp')
	else
		core_c_args += ['-DSRAL_NO_SPEECH_DISPATCHER']
		core_cpp_args += ['-DSRAL_NO_SPEECH_DISPATCHER']
	endif

	if opt_orca
		core_sources += files('Orca.cpp')
	else
		core_c_args += ['-DSRAL_NO_ORCA']
		core_cpp_args += ['-DSRAL_NO_ORCA']
	endif
endif

supported_link_args = []

if not is_msvc_like
	desired_link_args = [
		'-Wl,-z,relro',
		'-Wl,-z,now',
		'-Wl,-z,noexecstack',
	]

	if not is_apple and not is_emscripten
		desired_link_args += ['-Wl,-z,defs']
	elif is_apple
		desired_link_args += ['-Wl,-undefined,error']
	endif
	supported_link_args = c_compiler.get_supported_link_arguments(desired_link_args)
else
	desired_msvc_link = ['/NXCOMPAT', '/DYNAMICBASE', '/HIGHENTROPYVA']
	if build_variant == 'release'
		desired_msvc_link += ['/CETCOMPAT']
	endif
	supported_link_args = c_compiler.get_supported_link_arguments(desired_msvc_link)
endif

sral_core_c_args = core_c_args
sral_core_cpp_args = core_cpp_args
sral_core_objc_args = []
sral_core_objcpp_args = []

if is_apple
	sral_core_objc_args = core_c_args
	sral_core_objcpp_args = core_cpp_args
endif

if build_shared_libs or get_option('default_library') == 'both'
	sral_core_c_args += ['-DSRAL_BUILDING_DLL']
	sral_core_cpp_args += ['-DSRAL_BUILDING_DLL']
	if is_apple
		sral_core_objc_args += ['-DSRAL_BUILDING_DLL']
		sral_core_objcpp_args += ['-DSRAL_BUILDING_DLL']
	endif
endif

if get_option('default_library') == 'both'
	sral_both_target = both_libraries(
		'SRAL',
		core_sources,
		include_directories: [inc_dir, include_directories('../Dep')],
		link_with: sral_deps_list,
		c_args: sral_core_c_args,
		cpp_args: sral_core_cpp_args,
		objc_args: sral_core_objc_args,
		objcpp_args: sral_core_objcpp_args,
		link_args: supported_link_args,
		dependencies: sral_dependencies,
		pic: true,
		install: true,
	)
	sral_lib = sral_both_target.get_shared_lib()
elif build_shared_libs
	sral_lib = shared_library(
		'SRAL',
		core_sources,
		include_directories: [inc_dir, include_directories('../Dep')],
		link_with: sral_deps_list,
		c_args: sral_core_c_args,
		cpp_args: sral_core_cpp_args,
		objc_args: sral_core_objc_args,
		objcpp_args: sral_core_objcpp_args,
		link_args: supported_link_args,
		dependencies: sral_dependencies,
		install: true,
	)
else
	sral_lib = static_library(
		'SRAL',
		core_sources,
		include_directories: [inc_dir, include_directories('../Dep')],
		link_with: sral_deps_list,
		c_args: sral_core_c_args,
		cpp_args: sral_core_cpp_args,
		objc_args: sral_core_objc_args,
		objcpp_args: sral_core_objcpp_args,
		link_args: supported_link_args,
		dependencies: sral_dependencies,
		pic: true,
		install: true,
	)
endif