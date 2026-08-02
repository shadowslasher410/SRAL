#include "SpeechDispatcher.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <clocale>
#include <concepts>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

#include "Encoding.h"

#if defined(__linux__) && !defined(__ANDROID__)
#include <brlapi.h>

#include <speech-dispatcher/libspeechd.h>
#endif

namespace Sral {

static std::atomic<SpeechDispatcher*> g_activeSpeechDispatcherInstance{nullptr};

std::atomic<bool> SpeechDispatcher::is_active{false};
std::mutex SpeechDispatcher::speechd_mutex;
std::atomic<size_t> SpeechDispatcher::m_activeMsgId{0};

SpeechDispatcher::~SpeechDispatcher() noexcept {
	static_cast<void>(SpeechDispatcher::Uninitialize());
	SpeechDispatcher::ClearVoiceList();
}

static bool NormalizeAndCompareLang(std::string_view system_lang, std::string_view voice_lang) noexcept {
	size_t sys_idx = 0;
	size_t voi_idx = 0;

	while (sys_idx < system_lang.size() && voi_idx < voice_lang.size()) {
		char sys_c = system_lang[sys_idx++];
		char voi_c = voice_lang[voi_idx++];

		if (sys_c == '_')
			sys_c = '-';
		if (voi_c == '_')
			voi_c = '-';

		if (sys_c != voi_c)
			return false;
	}

	return sys_idx == system_lang.size() && voi_idx == voice_lang.size();
}

int SpeechDispatcher::SetVoiceIndex() noexcept {
	RefreshVoiceList();
	if (!m_voiceList || m_voiceCount == 0) [[unlikely]] {
		return 0;
	}

#if defined(__linux__) && !defined(__ANDROID__)
	const char* system_locale = std::getenv("LC_ALL");
	if (!system_locale || std::string_view(system_locale) == "C") {
		system_locale = std::getenv("LC_CTYPE");
		if (!system_locale || std::string_view(system_locale) == "C") {
			system_locale = std::getenv("LANG");
		}
	}

	if (!system_locale)
		return 0;

	std::string_view system_lang_view(system_locale);
	const size_t spec_index = system_lang_view.find_first_of(".@");
	if (spec_index != std::string_view::npos) {
		system_lang_view = system_lang_view.substr(0, spec_index);
	}

	for (int i = 0; i < m_voiceCount; ++i) {
		if (m_voiceList[i] && m_voiceList[i]->language) {
			std::string_view voice_lang_view(m_voiceList[i]->language);
			const size_t voice_spec = voice_lang_view.find_first_of(".@");
			if (voice_spec != std::string_view::npos) {
				voice_lang_view = voice_lang_view.substr(0, voice_spec);
			}

			if (NormalizeAndCompareLang(system_lang_view, voice_lang_view)) {
				return i;
			}
		}
	}

	if (system_lang_view.size() >= 2) {
		const std::string_view base_system_lang = system_lang_view.substr(0, 2);
		for (int i = 0; i < m_voiceCount; ++i) {
			if (m_voiceList[i] && m_voiceList[i]->language) {
				const std::string_view voice_lang_view(m_voiceList[i]->language);
				if (voice_lang_view.size() >= 2 && voice_lang_view.substr(0, 2) == base_system_lang) {
					return i;
				}
			}
		}
	}
#endif

	return 0;
}

bool SpeechDispatcher::Initialize() noexcept {
	std::lock_guard lock(m_mutex);
	if (is_active.load(std::memory_order_acquire)) [[unlikely]] {
		return true;
	}

	for (size_t i = 0; i < RING_BUFFER_SIZE; ++i) {
		m_ring_queue[i].sequence.store(i, std::memory_order_relaxed);
	}

#if defined(__linux__) && !defined(__ANDROID__)
	if (speech != nullptr)
		return true;

	const auto* address = spd_get_default_address(nullptr);
	if (address == nullptr)
		return false;

	char* error_result = nullptr;
	speech = spd_open2("SRAL", nullptr, nullptr, SPD_MODE_SINGLE, address, 1, &error_result);

	if (speech == nullptr) {
		if (error_result) {
			std::free(error_result);
		}
		return false;
	}

	g_activeSpeechDispatcherInstance.store(this, std::memory_order_release);
	speech->callback_end = &SpeechDispatcher::SpeechNotificationCallback;
	speech->callback_cancel = &SpeechDispatcher::SpeechNotificationCallback;

	spd_set_data_mode(speech, SPD_DATA_SSML);

	brailleInitialized = brlapi_openConnection(nullptr, nullptr) >= 0;
	if (brailleInitialized) {
		brlapi_enterTtyMode(BRLAPI_TTY_DEFAULT, nullptr);
	}
#endif

	is_active.store(true, std::memory_order_release);
	m_worker_thread = std::jthread([this](std::stop_token st) noexcept { this->BackgroundWorkerLoop(st); });

	return true;
}

bool SpeechDispatcher::Uninitialize() noexcept {
	{
		std::lock_guard lock(m_mutex);
		if (!is_active.load(std::memory_order_acquire)) [[unlikely]] {
			return true;
		}
		is_active.store(false, std::memory_order_release);
	}

	m_worker_thread.request_stop();
	m_ring_bell.store(true, std::memory_order_release);
	m_ring_bell.notify_all();

	if (m_worker_thread.joinable()) {
		m_worker_thread.join();
	}

	std::lock_guard lock(m_mutex);
#if defined(__linux__) && !defined(__ANDROID__)
	if (speech != nullptr) {
		spd_close(speech);
		speech = nullptr;
	}
	if (brailleInitialized) {
		brlapi_leaveTtyMode();
		brlapi_closeConnection();
		brailleInitialized = false;
	}
#endif
	g_activeSpeechDispatcherInstance.store(nullptr, std::memory_order_release);
	ClearStringPool();
	return true;
}

bool SpeechDispatcher::Speak(const char* text, bool interrupt) noexcept {
	if (!text || text == '\0') [[unlikely]]
		return false;
	if (!is_active.load(std::memory_order_acquire)) [[unlikely]]
		return false;

	if (interrupt) {
		m_fastPathInterrupt.store(true, std::memory_order_release);
		m_tail.store(m_head.load(std::memory_order_relaxed), std::memory_order_release);
	}

	std::string_view sv(text);
	AsyncSpdTask* task = nullptr;
	size_t ticket = m_head.load(std::memory_order_acquire);

	while (true) {
		task = &m_ring_queue[ticket & RING_MASK];
		size_t seq = task->sequence.load(std::memory_order_acquire);
		intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

		if (difference == 0) {
			if (m_head.compare_exchange_weak(
					ticket, ticket + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
				break;
			}
		}
		else if (difference < 0) {
			return false;
		}
		else {
			ticket = m_head.load(std::memory_order_acquire);
		}
	}

	const size_t max_copy = (std::min)(sv.size(), task->payload.size() - 1);
	std::memcpy(task->payload.data(), sv.data(), max_copy);
	task->payload[max_copy] = '\0';
	task->payload_length = max_copy;

	task->type = TaskType::Speak;
	task->interrupt = interrupt;

	task->sequence.store(ticket + 1, std::memory_order_release);

	m_ring_bell.store(true, std::memory_order_release);
	m_ring_bell.notify_one();
	return true;
}

bool SpeechDispatcher::SpeakSsml(const char* ssml, bool interrupt) noexcept {
	if (!ssml || ssml == '\0') [[unlikely]]
		return false;
	if (!is_active.load(std::memory_order_acquire)) [[unlikely]]
		return false;

	if (interrupt) {
		m_fastPathInterrupt.store(true, std::memory_order_release);
		m_tail.store(m_head.load(std::memory_order_relaxed), std::memory_order_release);
	}

	std::string_view sv(ssml);
	AsyncSpdTask* task = nullptr;
	size_t ticket = m_head.load(std::memory_order_acquire);

	while (true) {
		task = &m_ring_queue[ticket & RING_MASK];
		size_t seq = task->sequence.load(std::memory_order_acquire);
		intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

		if (difference == 0) {
			if (m_head.compare_exchange_weak(
					ticket, ticket + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
				break;
			}
		}
		else if (difference < 0) {
			return false;
		}
		else {
			ticket = m_head.load(std::memory_order_acquire);
		}
	}

	const size_t max_copy = (std::min)(sv.size(), task->payload.size() - 1);
	std::memcpy(task->payload.data(), sv.data(), max_copy);
	task->payload[max_copy] = '\0';
	task->payload_length = max_copy;

	task->type = TaskType::SpeakSsml;
	task->interrupt = interrupt;

	task->sequence.store(ticket + 1, std::memory_order_release);

	m_ring_bell.store(true, std::memory_order_release);
	m_ring_bell.notify_one();
	return true;
}

bool SpeechDispatcher::StopSpeech() noexcept {
	if (!is_active.load(std::memory_order_acquire)) [[unlikely]]
		return false;

	m_fastPathInterrupt.store(true, std::memory_order_release);
	m_tail.store(m_head.load(std::memory_order_relaxed), std::memory_order_release);

	m_ring_bell.store(true, std::memory_order_release);
	m_ring_bell.notify_one();
	return true;
}

bool SpeechDispatcher::SetParameter(int param, const void* value) noexcept {
	if (!value || !is_active.load(std::memory_order_acquire)) [[unlikely]]
		return false;

	AsyncSpdTask* task = nullptr;
	size_t ticket = m_head.load(std::memory_order_acquire);

	while (true) {
		task = &m_ring_queue[ticket & RING_MASK];
		size_t seq = task->sequence.load(std::memory_order_acquire);
		intptr_t difference = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

		if (difference == 0) {
			if (m_head.compare_exchange_weak(
					ticket, ticket + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
				break;
			}
		}
		else if (difference < 0) {
			return false;
		}
		else {
			ticket = m_head.load(std::memory_order_acquire);
		}
	}

	task->payload[0] = '\0';
	task->payload_length = 0;
	task->type = TaskType::SetParam;
	task->param_id = param;

	if (param == SRAL_PARAM_ENABLE_SPELLING) {
		task->param_val = static_cast<int>(*static_cast<const bool*>(value));
	}
	else {
		task->param_val = *static_cast<const int*>(value);
	}

	task->sequence.store(ticket + 1, std::memory_order_release);

	m_ring_bell.store(true, std::memory_order_release);
	m_ring_bell.notify_one();
	return true;
}

void SpeechDispatcher::BackgroundWorkerLoop(std::stop_token stop_token) noexcept {
#if defined(__linux__) && !defined(__ANDROID__)
	(void)SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#endif

	std::array<char, 1024> stack_ssml_buf{};
	std::array<char, 5> single_char_buf{};

	constexpr std::string_view open_tag = "<speak>";
	constexpr std::string_view close_tag = "</speak>";

	while (!stop_token.stop_requested()) {
		m_ring_bell.store(false, std::memory_order_release);

		if (m_fastPathInterrupt.exchange(false, std::memory_order_acq_rel)) {
#if defined(__linux__) && !defined(__ANDROID__)
			if (speech != nullptr) {
				std::lock_guard<std::mutex> lock(speechd_mutex);
				(void)spd_stop(speech);
				(void)spd_cancel(speech);
			}
#endif
			m_isSpeakingLocal.store(false, std::memory_order_release);
		}

		size_t tail = m_tail.load(std::memory_order_relaxed);
		size_t head = m_head.load(std::memory_order_acquire);

		if (tail == head) {
			m_ring_bell.wait(false, std::memory_order_acquire);
			continue;
		}

		while (tail != head) {
			if (stop_token.stop_requested()) [[unlikely]]
				break;

			if (m_fastPathInterrupt.exchange(false, std::memory_order_acq_rel)) {
#if defined(__linux__) && !defined(__ANDROID__)
				if (speech != nullptr) {
					std::lock_guard<std::mutex> lock(speechd_mutex);
					(void)spd_stop(speech);
					(void)spd_cancel(speech);
				}
#endif
				m_isSpeakingLocal.store(false, std::memory_order_release);
				tail = m_head.load(std::memory_order_relaxed);
				m_tail.store(tail, std::memory_order_release);
				break;
			}

			AsyncSpdTask& task = m_ring_queue[tail & RING_MASK];
			size_t seq = task.sequence.load(std::memory_order_acquire);

			if (seq != (tail + 1)) {
				break;
			}

			const TaskType localType = task.type;
			const bool localInterrupt = task.interrupt;
			const int localParamId = task.param_id;
			const int localParamVal = task.param_val;

#if defined(__linux__) && !defined(__ANDROID__)
			if (speech != nullptr) {
				if (localInterrupt && (localType == TaskType::Speak || localType == TaskType::SpeakSsml)) {
					std::lock_guard<std::mutex> lock(speechd_mutex);
					(void)spd_stop(speech);
					(void)spd_cancel(speech);
					m_isSpeakingLocal.store(false, std::memory_order_relaxed);
				}

				if (localType == TaskType::Speak && task.payload_length > 0) {
					if (!enableSpelling) {
						const size_t required_space = open_tag.size() + task.payload_length + close_tag.size();
						if (required_space < stack_ssml_buf.size()) {
							char* dest = stack_ssml_buf.data();

							std::memcpy(dest, open_tag.data(), open_tag.size());
							dest += open_tag.size();

							std::memcpy(dest, task.payload.data(), task.payload_length);
							dest += task.payload_length;

							std::memcpy(dest, close_tag.data(), close_tag.size());
							dest += close_tag.size();
							*dest = '\0';

							m_isSpeakingLocal.store(true, std::memory_order_release);
							std::lock_guard<std::mutex> lock(speechd_mutex);
							int spd_msg_id = spd_say(speech, SPD_IMPORTANT, stack_ssml_buf.data());
							if (spd_msg_id > 0) {
								m_activeMsgId.store(static_cast<size_t>(spd_msg_id), std::memory_order_release);
							}
						}
					}
					else {
						utf8_iter iter;
						utf8_init(&iter, task.payload.data());

						m_isSpeakingLocal.store(true, std::memory_order_release);
						while (utf8_next(&iter)) {
							if (iter.size == 0 || iter.size > 4) [[unlikely]]
								continue;

							single_char_buf.fill(0);
							const char* raw_char_ptr = utf8_getchar(&iter);
							std::memcpy(single_char_buf.data(), raw_char_ptr, iter.size);
							single_char_buf[iter.size] = '\0';

							if (!m_isSpeakingLocal.load(std::memory_order_relaxed) || stop_token.stop_requested())
								[[unlikely]] {
								break;
							}

							{
								std::lock_guard<std::mutex> lock(speechd_mutex);
								int spd_msg_id = spd_char(speech, SPD_IMPORTANT, single_char_buf.data());
								if (spd_msg_id > 0) {
									m_activeMsgId.store(static_cast<size_t>(spd_msg_id), std::memory_order_release);
								}
							}
							std::this_thread::sleep_for(std::chrono::milliseconds(2));
						}
					}
				}
				else if (localType == TaskType::SpeakSsml && task.payload_length > 0) {
					m_isSpeakingLocal.store(true, std::memory_order_release);
					std::lock_guard<std::mutex> lock(speechd_mutex);
					int spd_msg_id = spd_say(speech, SPD_IMPORTANT, task.payload.data());
					if (spd_msg_id > 0) {
						m_activeMsgId.store(static_cast<size_t>(spd_msg_id), std::memory_order_release);
					}
				}
				else if (localType == TaskType::Stop) {
					std::lock_guard<std::mutex> lock(speechd_mutex);
					(void)spd_stop(speech);
					m_isSpeakingLocal.store(false, std::memory_order_release);
				}
				else if (localType == TaskType::SetParam) {
					std::lock_guard<std::mutex> lock(speechd_mutex);
					switch (localParamId) {
					case SRAL_PARAM_SYMBOL_LEVEL:
						(void)spd_set_punctuation(speech, static_cast<SPDPunctuation>(localParamVal));
						break;
					case SRAL_PARAM_SPEECH_RATE:
						(void)spd_set_voice_rate(speech, localParamVal);
						m_speechRate = localParamVal;
						break;
					case SRAL_PARAM_SPEECH_VOLUME:
						(void)spd_set_volume(speech, localParamVal);
						m_speechVolume = localParamVal;
						break;
					case SRAL_PARAM_ENABLE_SPELLING:
						this->enableSpelling = static_cast<bool>(localParamVal);
						break;
					case SRAL_PARAM_VOICE_INDEX: {
						if (m_voiceList && localParamVal >= 0 && localParamVal < m_voiceCount) {
							if (spd_set_synthesis_voice(speech, m_voiceList[localParamVal]->name) == 0) {
								m_voiceIndex = localParamVal;
							}
						}
						break;
					}
					}
				}
			}
#else
			(void)localParamId;
			(void)localParamVal;
#endif
			task.payload[0] = '\0';
			task.payload_length = 0;
			task.sequence.store(tail + RING_BUFFER_SIZE, std::memory_order_release);
			tail++;
			m_tail.store(tail, std::memory_order_release);
			head = m_head.load(std::memory_order_acquire);
		}
	}
}

bool SpeechDispatcher::GetActive() noexcept {
#if defined(linux) && !defined(ANDROID)
	std::lock_guardstd::mutex lock(speechd_mutex);
	return speech != nullptr;
#else
	return false;
#endif
}

bool SpeechDispatcher::IsSpeaking() noexcept {
	return m_isSpeakingLocal.load(std::memory_order_acquire);
}

bool SpeechDispatcher::GetParameter(int param, void* value) noexcept {
	if (!value) [[unlikely]]
		return false;

#if defined(__linux__) && !defined(__ANDROID__)
	if (speech == nullptr)
		return false;

	std::lock_guard<std::mutex> lock(speechd_mutex);
	switch (param) {
	case SRAL_PARAM_SPEECH_RATE:
		*static_cast<int*>(value) = this->m_speechRate;
		return true;
	case SRAL_PARAM_SPEECH_VOLUME:
		*static_cast<int*>(value) = this->m_speechVolume;
		return true;
	case SRAL_PARAM_ENABLE_SPELLING:
		*static_cast<bool*>(value) = this->enableSpelling;
		return true;
	case SRAL_PARAM_VOICE_PROPERTIES: {
		auto* voiceProperties = static_cast<SRAL_VoiceInfo*>(value);

		if (!m_voiceList) {
			RefreshVoiceList();
		}

		if (!voiceProperties || !m_voiceList) [[unlikely]]
			return false;

		if (m_voiceIndex >= m_voiceCount || m_voiceIndex < 0) {
			m_voiceIndex = (m_voiceCount > 0) ? 0 : -1;
		}

		m_string_pool.clear();

		if (m_voiceCount > 0) {
			m_string_pool.reserve(static_cast<size_t>(m_voiceCount) * 3);
		}

		for (int index = 0; index < m_voiceCount; ++index) {
			voiceProperties[index].index = index;

			if (m_voiceList[index]->name) {
				m_string_pool.emplace_back(m_voiceList[index]->name);
				voiceProperties[index].name = m_string_pool.back().c_str();
			}
			else {
				voiceProperties[index].name = nullptr;
			}

			if (m_voiceList[index]->language) {
				m_string_pool.emplace_back(m_voiceList[index]->language);
				voiceProperties[index].language = m_string_pool.back().c_str();
			}
			else {
				voiceProperties[index].language = nullptr;
			}

			if (m_voiceList[index]->variant) {
				m_string_pool.emplace_back(m_voiceList[index]->variant);
				voiceProperties[index].gender = m_string_pool.back().c_str();
			}
			else {
				voiceProperties[index].gender = nullptr;
			}

			voiceProperties[index].vendor = "Unknown";
		}
		return true;
	}
	case SRAL_PARAM_VOICE_COUNT:
		if (!m_voiceList) {
			RefreshVoiceList();
		}

		if (m_voiceIndex >= m_voiceCount || m_voiceIndex < 0) {
			m_voiceIndex = (m_voiceCount > 0) ? 0 : -1;
		}

		*static_cast<int*>(value) = m_voiceCount;
		return true;
	case SRAL_PARAM_VOICE_INDEX:
		*static_cast<int*>(value) = m_voiceIndex;
		return true;
	default:
		return false;
	}
#else
	(void)param;
	(void)value;
	return false;
#endif
}

bool SpeechDispatcher::Braille(const char* text) noexcept {
#if defined(__linux__) && !defined(__ANDROID__)
	if (!brailleInitialized || !text || text == '\0') [[unlikely]]
		return false;
	std::lock_guard<std::mutex> lock(speechd_mutex);
	return brlapi_writeText(BRLAPI_CURSOR_LEAVE, text) >= 0;
#else
	(void)text;
	return false;
#endif
}

bool SpeechDispatcher::PauseSpeech() noexcept {
#if defined(__linux__) && !defined(__ANDROID__)
	std::lock_guard<std::mutex> lock(speechd_mutex);
	if (speech == nullptr)
		return false;

	if (spd_pause(speech) == 0) {
		paused = true;
		return true;
	}
	return false;
#else
	return false;
#endif
}

bool SpeechDispatcher::ResumeSpeech() noexcept {
#if defined(__linux__) && !defined(__ANDROID__)
	std::lock_guard<std::mutex> lock(speechd_mutex);
	if (speech == nullptr)
		return false;

	if (spd_resume(speech) == 0) {
		paused = false;
		return true;
	}
	return false;
#else
	return false;
#endif
}

void SpeechDispatcher::SpeechNotificationCallback(size_t msg_id, size_t client_id, SPDNotificationType type) noexcept {
	(void)client_id;
#if defined(__linux__) && !defined(__ANDROID__)
	if (type == SPD_EVENT_END || type == SPD_EVENT_CANCEL) {
		SpeechDispatcher* instance = g_activeSpeechDispatcherInstance.load(std::memory_order_acquire);
		if (instance) [[likely]] {
			if (msg_id == instance->m_activeMsgId.load(std::memory_order_acquire)) {
				instance->m_isSpeakingLocal.store(false, std::memory_order_release);
			}
		}
	}
#else
	(void)msg_id;
	(void)type;
#endif
}

void SpeechDispatcher::RefreshVoiceList() noexcept {
#if defined(__linux__) && !defined(__ANDROID__)
	if (!speech)
		return;
	SpeechDispatcher::ClearVoiceList();

	m_voiceList = spd_list_synthesis_voices(speech);
	if (m_voiceList) [[likely]] {
		int count = 0;
		while (m_voiceList[count] != nullptr) {
			count++;
		}
		m_voiceCount = count;
	}
#endif
}

void SpeechDispatcher::ClearVoiceList() noexcept {
#if defined(__linux__) && !defined(__ANDROID__)
	if (m_voiceList) {
		free_spd_voices(m_voiceList);
		m_voiceList = nullptr;
		m_voiceCount = 0;
	}
#endif
}

void SpeechDispatcher::ClearStringPool() noexcept {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_string_pool.clear();
	m_voice_strings.clear();
}

} // namespace Sral