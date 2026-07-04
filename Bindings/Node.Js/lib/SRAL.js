// lib/sral.js
const addon = require('../build/Release/sral_bridge.node');

const SRALEngines = {
	NONE: 0,
	NVDA: 1 << 1,
	JAWS: 1 << 2,
	ZDSR: 1 << 3,
	NARRATOR: 1 << 4,
	UIA: 1 << 5,
	SAPI: 1 << 6,
	SPEECH_DISPATCHER: 1 << 7,
	VOICE_OVER: 1 << 8,
	NS_SPEECH: 1 << 9,
	AV_SPEECH: 1 << 10,
	ANDROID_ACCESSIBILITY_MANAGER: 1 << 11,
	ANDROID_TEXT_TO_SPEECH: 1 << 12,
	CHROMEVOX: 1 << 13,
	ORCA: 1 << 14,
	CURRENT: -1
};

const SRALEngineCategory = {
	UNKNOWN: 0,
	SCREEN_READER: 1,
	TEXT_TO_SPEECH_ENGINE: 2,
	ACCESSIBILITY_PROVIDER: 3
};

const SRAL_SupportedFeatures = {
	SPEECH: 1 << 1,
	BRAILLE: 1 << 2,
	SPEECH_RATE: 1 << 3,
	SPEECH_VOLUME: 1 << 4,
	SELECT_VOICE: 1 << 5,
	PAUSE_SPEECH: 1 << 6,
	SSML: 1 << 7,
	SPEAK_TO_MEMORY: 1 << 8,
	SPELLING: 1 << 9
};

const SRAL_EngineParams = {
	SPEECH_RATE: 0,
	SPEECH_VOLUME: 1,
	VOICE_INDEX: 2,
	VOICE_PROPERTIES: 3,
	VOICE_COUNT: 4,
	SYMBOL_LEVEL: 5,
	SAPI_TRIM_THRESHOLD: 6,
	ENABLE_SPELLING: 7,
	USE_CHARACTER_DESCRIPTIONS: 8,
	NVDA_IS_CONTROL_EX: 9,
	ANDROID_JNI_ENV: 10,
	ANDROID_ACTIVITY: 11
};

const SRALVoiceInfo = {
	INDEX: "index",
	NAME: "name",
	LANGUAGE: "language",
	GENDER: "gender",
	VENDOR: "vendor"
};

class SRAL {
	constructor() {
		Object.assign(this, addon);
	}
}

module.exports = {
	SRAL,
	SRALEngines,
	SRALEngineCategory,
	SRAL_SupportedFeatures,
	SRAL_EngineParams,
	SRALVoiceInfo
};
