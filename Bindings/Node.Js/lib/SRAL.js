const path = require('path');
const fs = require('fs');

let addon;

const platform = process.platform;
const arch = process.arch;

const isMobile = platform === 'android' || platform === 'ios';

if (isMobile) {
	addon = {};
} else {
	const searchPaths = [
		path.join(__dirname, '../build/Release/SRAL_bridge.node'),
		path.join(__dirname, `../build/${platform}-${arch}/Release/SRAL_bridge.node`),
		path.join(__dirname, '../build/Debug/SRAL_bridge.node'),
		path.join(__dirname, `../build/${platform}-${arch}/Debug/SRAL_bridge.node`)
	];

	let loaded = false;
	let errors = [];

	for (const binaryPath of searchPaths) {
		if (fs.existsSync(binaryPath)) {
			try {
				addon = require(binaryPath);
				loaded = true;
				break;
			} catch (err) {
				errors.push(`${binaryPath} found but failed: ${err.message}`);
			}
		}
	}
}

const SRALEngines = {
	NONE: 0,
	NVDA: 1 << 1,
	JAWS: 1 << 2,
	ZDSR: 1 << 3,
	NARRATOR: 1 << 4,
	UIA: 1 << 5,
	SAPI: 1 << 6,
	SPEECH_DISPATCHER: 1 << 7,
	ORCA: 1 << 8,
	VOICE_OVER: 1 << 9,
	NS_SPEECH: 1 << 10,
	AV_SPEECH: 1 << 11,
	ANDROID_ACCESSIBILITY_MANAGER: 1 << 12,
	ANDROID_TEXT_TO_SPEECH: 1 << 13,
	CHROMEVOX: 1 << 14,
	ACCESS_KIT: 1 << 15,
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
	ENGINE_IS_PAUSED: 10,
	ANDROID_JNI_ENV: 11,
	ANDROID_ACTIVITY: 12
};

const SRALVoiceInfo = {
	INDEX: "index",
	NAME: "name",
	LANGUAGE: "language",
	GENDER: "gender",
	VENDOR: "vendor"
};

const SRALSymbolLevel = {
	NONE: 0,
	SOME: 1,
	MOST: 2,
	ALL: 3
};

if (typeof global !== 'undefined') {
	global.SRALSymbolLevel = SRALSymbolLevel;
}

class SRAL {
	constructor() {
		Object.assign(this, addon);
	}

	speakSsmlEx(engine, ssml, interrupt = false) {
		if (isMobile) return false;
		if (typeof addon.speakSsmlEx === 'function') {
			return addon.speakSsmlEx(Number(engine), String(ssml), !!interrupt);
		}
		return this.speakSsml(String(ssml), !!interrupt);
	}

	brailleEx(engine, text) {
		if (isMobile) return false;
		if (typeof addon.brailleEx === 'function') {
			return addon.brailleEx(Number(engine), String(text));
		}
		return this.braille(String(text));
	}

	outputEx(engine, text, interrupt = false) {
		if (isMobile) return false;
		if (typeof addon.outputEx === 'function') {
			return addon.outputEx(Number(engine), String(text), !!interrupt);
		}
		return this.output(String(text), !!interrupt);
	}

	stopSpeechEx(engine) {
		if (isMobile) return false;
		if (typeof addon.stopSpeechEx === 'function') {
			return addon.stopSpeechEx(Number(engine));
		}
		return this.stopSpeech();
	}

	pauseSpeechEx(engine) {
		if (isMobile) return false;
		if (typeof addon.pauseSpeechEx === 'function') {
			return addon.pauseSpeechEx(Number(engine));
		}
		return this.pauseSpeech();
	}

	resumeSpeechEx(engine) {
		if (isMobile) return false;
		if (typeof addon.resumeSpeechEx === 'function') {
			return addon.resumeSpeechEx(Number(engine));
		}
		return this.resumeSpeech();
	}

	getTTSEngines() {
		if (isMobile) return 0;
		if (typeof addon.getTTSEngines === 'function') {
			return addon.getTTSEngines();
		}
		let ttsMask = 0;
		for (let key in SRALEngines) {
			let val = SRALEngines[key];
			if (val > 0 && val !== SRALEngines.CURRENT) {
				if (this.getEngineCategory(val) === SRALEngineCategory.TEXT_TO_SPEECH_ENGINE) {
					ttsMask |= val;
				}
			}
		}
		return ttsMask;
	}

	getAssistiveTechEngines() {
		if (isMobile) return 0;
		if (typeof addon.getAssistiveTechEngines === 'function') {
			return addon.getAssistiveTechEngines();
		}
		let atMask = 0;
		for (let key in SRALEngines) {
			let val = SRALEngines[key];
			if (val > 0 && val !== SRALEngines.CURRENT) {
				let cat = this.getEngineCategory(val);
				if (cat === SRALEngineCategory.SCREEN_READER || cat === SRALEngineCategory.ACCESSIBILITY_PROVIDER) {
					atMask |= val;
				}
			}
		}
		return atMask;
	}
}

module.exports = {
	SRAL,
	SRALEngines,
	SRALEngineCategory,
	SRAL_SupportedFeatures,
	SRAL_EngineParams,
	SRALVoiceInfo,
	
	SRALEngine: SRALEngines,
	SRALFeature: SRAL_SupportedFeatures,
	SRALParam: SRAL_EngineParams,
	SRALSymbolLevel
};
