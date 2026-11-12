#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <ksmedia.h>
#endif

// WASAPI loopback: measures the sound currently playing on the Windows output device.
class SystemAudioMeter {
public:
	static SystemAudioMeter& get() {
		static SystemAudioMeter instance;
		return instance;
	}

	float level() const { return m_level.load(std::memory_order_relaxed); }

private:
	std::atomic<float> m_level{0.f};
	std::atomic<bool> m_running{true};
	std::thread m_thread;

	SystemAudioMeter() {
#ifdef _WIN32
		m_thread = std::thread([this] { capture(); });
#endif
	}

	~SystemAudioMeter() {
		m_running.store(false);
		if (m_thread.joinable()) m_thread.join();
	}

#ifdef _WIN32
	void capture() {
		HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		bool uninit = SUCCEEDED(hr);
		IMMDeviceEnumerator* enumerator = nullptr;
		IMMDevice* device = nullptr;
		IAudioClient* client = nullptr;
		IAudioCaptureClient* captureClient = nullptr;
		WAVEFORMATEX* format = nullptr;

		do {
			hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
				__uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
			if (FAILED(hr)) break;
			hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
			if (FAILED(hr)) break;
			hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
				reinterpret_cast<void**>(&client));
			if (FAILED(hr)) break;
			hr = client->GetMixFormat(&format);
			if (FAILED(hr)) break;
			hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
				AUDCLNT_STREAMFLAGS_LOOPBACK, 1000000, 0, format, nullptr);
			if (FAILED(hr)) break;
			hr = client->GetService(__uuidof(IAudioCaptureClient),
				reinterpret_cast<void**>(&captureClient));
			if (FAILED(hr)) break;
			hr = client->Start();
			if (FAILED(hr)) break;

			while (m_running.load()) {
				UINT32 packet = 0;
				if (FAILED(captureClient->GetNextPacketSize(&packet))) break;
				if (!packet) {
					std::this_thread::sleep_for(std::chrono::milliseconds(8));
					continue;
				}
				while (packet && m_running.load()) {
					BYTE* data = nullptr;
					UINT32 frames = 0;
					DWORD flags = 0;
					if (FAILED(captureClient->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) break;

					double sum = 0.0;
					size_t samples = static_cast<size_t>(frames) * format->nChannels;
					if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data && samples) {
						if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
							(format->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
								reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
							auto values = reinterpret_cast<float const*>(data);
							for (size_t i = 0; i < samples; ++i) {
								double v = std::clamp(static_cast<double>(values[i]), -1.0, 1.0);
								sum += v * v;
							}
						} else if (format->wBitsPerSample == 16) {
							auto values = reinterpret_cast<int16_t const*>(data);
							for (size_t i = 0; i < samples; ++i) {
								double v = values[i] / 32768.0;
								sum += v * v;
							}
						} else if (format->wBitsPerSample == 32) {
							auto values = reinterpret_cast<int32_t const*>(data);
							for (size_t i = 0; i < samples; ++i) {
								double v = values[i] / 2147483648.0;
								sum += v * v;
							}
						}
					}
					float rms = samples ? static_cast<float>(std::sqrt(sum / samples)) : 0.f;
					m_level.store(std::clamp(rms * 5.f, 0.f, 1.f), std::memory_order_relaxed);
					captureClient->ReleaseBuffer(frames);
					if (FAILED(captureClient->GetNextPacketSize(&packet))) packet = 0;
				}
			}
			client->Stop();
		} while (false);

		if (format) CoTaskMemFree(format);
		if (captureClient) captureClient->Release();
		if (client) client->Release();
		if (device) device->Release();
		if (enumerator) enumerator->Release();
		if (uninit) CoUninitialize();
	}
#endif
};

struct LyricLine {
	float time = 0.f;
	std::string text;
};

static std::vector<LyricLine> loadLyrics() {
	std::vector<LyricLine> result;
	try {
		auto path = Mod::get()->getSaveDir() / "lyrics.lrc";
		std::ifstream file(path);
		std::string line;
		while (std::getline(file, line)) {
			size_t close = line.find(']');
			if (line.empty() || line[0] != '[' || close == std::string::npos) continue;
			std::string stamp = line.substr(1, close - 1);
			size_t colon = stamp.find(':');
			size_t dot = stamp.find_first_of(".,", colon == std::string::npos ? 0 : colon + 1);
			if (colon == std::string::npos) continue;
			try {
				float minutes = std::stof(stamp.substr(0, colon));
				float seconds = std::stof(stamp.substr(colon + 1,
					dot == std::string::npos ? std::string::npos : dot - colon - 1));
				float fraction = 0.f;
				if (dot != std::string::npos) {
					std::string part = stamp.substr(dot + 1);
					if (part.size() == 1) fraction = std::stof(part) / 10.f;
					else if (!part.empty()) fraction = std::stof(part.substr(0, 2)) / 100.f;
				}
				std::string lyric = line.substr(close + 1);
				if (!lyric.empty()) result.push_back({minutes * 60.f + seconds + fraction, lyric});
			} catch (...) {}
		}
	} catch (...) {}
	std::sort(result.begin(), result.end(), [](auto const& a, auto const& b) { return a.time < b.time; });
	return result;
}


using namespace geode::prelude;

struct FxParticle {
	CCLayerColor* node = nullptr;
	float x = 0.f, y = 0.f, vx = 0.f, vy = 0.f, phase = 0.f, rot = 0.f, vr = 0.f;
};

static ccColor3B hueColor(float h) {
	auto cl = [](float v) { return std::clamp(v, 0.f, 1.f); };
	float r = cl(std::fabs(h * 6.f - 3.f) - 1.f);
	float g = cl(2.f - std::fabs(h * 6.f - 2.f));
	float b = cl(2.f - std::fabs(h * 6.f - 4.f));
	return ccc3(static_cast<GLubyte>(r * 255.f), static_cast<GLubyte>(g * 255.f), static_cast<GLubyte>(b * 255.f));
}

class FxLayer : public CCNode {
public:
	static FxLayer* create() {
		auto ret = new FxLayer();
		if (ret->init()) {
			ret->autorelease();
			ret->setup();
			return ret;
		}
		delete ret;
		return nullptr;
	}

	void onExit() override {
		CCNode::onExit();
		this->resetScene();
	}

	void update(float dt) override {
		m_t += dt;
		auto mod = Mod::get();
		auto gi = [&](char const* key) { return static_cast<int>(mod->getSettingValue<int64_t>(key)); };

		this->adjustPool(m_snow, gi("fx-snow") * 12, 0);
		this->adjustPool(m_rain, gi("fx-rain") * 15, 1);
		this->adjustPool(m_fire, gi("fx-fireflies") * 5, 2);
		this->adjustPool(m_petal, gi("fx-petals") * 8, 3);
		this->adjustPool(m_star, gi("fx-stars") * 8, 4);

		float wind = gi("fx-wind") * 6.f;
		this->stepFalling(m_snow, dt, wind, true);
		this->stepFalling(m_rain, dt, wind, false);
		this->stepFireflies(dt);
		this->stepPetals(dt, wind);
		this->stepStars();

		this->updateTint(gi("fx-tint"));
		this->updateVignette(gi("fx-vignette"));
		this->updateLines(gi("fx-scanlines"));
		this->updateBars(gi("fx-letterbox"));
		this->updateScene();
		this->updateLyrics();
	}

private:
	CCSize m_win;
	CCLayerColor* m_tint = nullptr;
	CCLayerGradient* m_vig[4] = {nullptr, nullptr, nullptr, nullptr};
	std::vector<CCLayerColor*> m_lines;
	CCLayerColor* m_barTop = nullptr;
	CCLayerColor* m_barBottom = nullptr;
	int m_lastVig = -1;
	int m_lastLines = -1;
	std::vector<FxParticle> m_snow, m_rain, m_fire, m_petal, m_star;
	std::mt19937 m_rng{std::random_device{}()};
	float m_t = 0.f;
	CCLabelBMFont* m_lyricLabel = nullptr;
	std::vector<LyricLine> m_lyrics;
	int m_currentLyric = -1;
	bool m_sceneActive = false;
	CCNode* m_target = nullptr;
	CCPoint m_origPos = ccp(0, 0);
	CCPoint m_origAnchor = ccp(0, 0);
	bool m_origIgnore = true;

	float frand(float a, float b) {
		std::uniform_real_distribution<float> d(a, b);
		return d(m_rng);
	}

	void setup() {
		m_win = CCDirector::get()->getWinSize();

		m_tint = CCLayerColor::create(ccc4(255, 255, 255, 0), m_win.width, m_win.height);
		this->addChild(m_tint, 0);

		float ew = m_win.width * 0.2f;
		float eh = m_win.height * 0.25f;
		CCPoint vec[4] = {ccp(1, 0), ccp(-1, 0), ccp(0, -1), ccp(0, 1)};
		for (int i = 0; i < 4; ++i) {
			auto g = CCLayerGradient::create(ccc4(0, 0, 0, 0), ccc4(0, 0, 0, 0), vec[i]);
			bool horiz = i < 2;
			g->setContentSize(horiz ? CCSize(ew, m_win.height) : CCSize(m_win.width, eh));
			if (i == 0) g->setPosition({0.f, 0.f});
			if (i == 1) g->setPosition({m_win.width - ew, 0.f});
			if (i == 2) g->setPosition({0.f, m_win.height - eh});
			if (i == 3) g->setPosition({0.f, 0.f});
			g->setVisible(false);
			this->addChild(g, 1);
			m_vig[i] = g;
		}

		// линии развёртки (как у старого монитора)
		for (float y = 0.f; y < m_win.height; y += 3.f) {
			auto line = CCLayerColor::create(ccc4(0, 0, 0, 0), m_win.width, 1.f);
			line->setPosition({0.f, y});
			line->setVisible(false);
			this->addChild(line, 3);
			m_lines.push_back(line);
		}

		// чёрные полосы сверху и снизу (кино)
		m_barTop = CCLayerColor::create(ccc4(0, 0, 0, 255), m_win.width, 1.f);
		m_barBottom = CCLayerColor::create(ccc4(0, 0, 0, 255), m_win.width, 1.f);
		m_barTop->setVisible(false);
		m_barBottom->setVisible(false);
		this->addChild(m_barTop, 4);
		this->addChild(m_barBottom, 4);

		m_lyrics = loadLyrics();
		m_lyricLabel = CCLabelBMFont::create("", "chatFont.fnt");
		if (m_lyricLabel) {
			m_lyricLabel->setAnchorPoint(ccp(0.5f, 0.5f));
			m_lyricLabel->setPosition(ccp(m_win.width * 0.5f, m_win.height - 42.f));
			m_lyricLabel->setScale(0.72f);
			m_lyricLabel->setOpacity(0);
			this->addChild(m_lyricLabel, 20);
		}

		this->scheduleUpdate();
	}

	// type: 0 снег, 1 дождь, 2 светлячки, 3 лепестки, 4 звёзды
	void adjustPool(std::vector<FxParticle>& pool, int target, int type) {
		while (static_cast<int>(pool.size()) < target) {
			FxParticle p;
			float w = 3.f, h = 3.f;
			ccColor4B col = ccc4(255, 255, 255, 220);
			if (type == 0) {
				w = h = frand(2.f, 5.f);
				p.vy = frand(25.f, 60.f);
				p.vx = frand(-8.f, 8.f);
			} else if (type == 1) {
				w = 1.2f;
				h = frand(8.f, 14.f);
				col = ccc4(170, 200, 255, 170);
				p.vy = frand(380.f, 560.f);
				p.vx = -40.f;
			} else if (type == 2) {
				w = h = frand(2.f, 4.f);
				col = ccc4(220, 255, 120, 200);
			} else if (type == 3) {
				w = frand(3.f, 6.f);
				h = w * 0.6f;
				col = ccc4(255, 150, 190, 210);
				p.vy = frand(18.f, 40.f);
				p.vx = frand(-10.f, 10.f);
				p.vr = frand(-90.f, 90.f);
			} else {
				w = h = frand(1.5f, 3.f);
				col = ccc4(255, 255, 255, 200);
			}
			p.phase = frand(0.f, 6.28f);
			p.x = frand(0.f, m_win.width);
			p.y = frand(0.f, m_win.height);
			p.node = CCLayerColor::create(col, w, h);
			p.node->setPosition({p.x, p.y});
			this->addChild(p.node, 2);
			pool.push_back(p);
		}
		while (static_cast<int>(pool.size()) > target) {
			pool.back().node->removeFromParent();
			pool.pop_back();
		}
	}

	void wrap(FxParticle& p) {
		if (p.y < -16.f) {
			p.y = m_win.height + 16.f;
			p.x = frand(0.f, m_win.width);
		}
		if (p.x < -16.f) p.x = m_win.width + 16.f;
		if (p.x > m_win.width + 16.f) p.x = -16.f;
	}

	void stepFalling(std::vector<FxParticle>& pool, float dt, float wind, bool sway) {
		for (auto& p : pool) {
			p.y -= p.vy * dt;
			float dx = p.vx + wind;
			if (sway) dx += std::sin(m_t * 1.3f + p.phase) * 12.f;
			p.x += dx * dt;
			this->wrap(p);
			p.node->setPosition({p.x, p.y});
		}
	}

	void stepPetals(float dt, float wind) {
		for (auto& p : m_petal) {
			p.y -= p.vy * dt;
			p.x += (p.vx + wind + std::sin(m_t * 1.1f + p.phase) * 20.f) * dt;
			p.rot += p.vr * dt;
			this->wrap(p);
			p.node->setPosition({p.x, p.y});
			p.node->setRotation(p.rot);
		}
	}

	void stepFireflies(float dt) {
		for (auto& p : m_fire) {
			p.x += std::cos(m_t * 0.7f + p.phase) * 18.f * dt;
			p.y += std::sin(m_t * 0.9f + p.phase * 2.f) * 14.f * dt;
			if (p.x < 0.f) p.x = m_win.width;
			if (p.x > m_win.width) p.x = 0.f;
			if (p.y < 0.f) p.y = m_win.height;
			if (p.y > m_win.height) p.y = 0.f;
			p.node->setPosition({p.x, p.y});
			float pulse = 0.5f + 0.5f * std::sin(m_t * 2.f + p.phase);
			p.node->setOpacity(static_cast<GLubyte>(60.f + 195.f * pulse));
		}
	}

	void stepStars() {
		for (auto& p : m_star) {
			float tw = 0.5f + 0.5f * std::sin(m_t * 1.5f + p.phase);
			p.node->setOpacity(static_cast<GLubyte>(40.f + 215.f * tw));
		}
	}

	void updateTint(int level) {
		if (level <= 0) {
			m_tint->setVisible(false);
			return;
		}
		m_tint->setVisible(true);
		m_tint->setColor(hueColor(std::fmod(m_t * 0.12f, 1.f)));
		m_tint->setOpacity(static_cast<GLubyte>(level * 7));
	}

	void updateVignette(int level) {
		if (level == m_lastVig) return;
		m_lastVig = level;
		for (int i = 0; i < 4; ++i) {
			m_vig[i]->setVisible(level > 0);
			m_vig[i]->setStartOpacity(static_cast<GLubyte>(std::clamp(level * 22, 0, 220)));
			m_vig[i]->setEndOpacity(0);
		}
	}

	void updateLines(int level) {
		if (level == m_lastLines) return;
		m_lastLines = level;
		for (auto line : m_lines) {
			line->setVisible(level > 0);
			line->setOpacity(static_cast<GLubyte>(std::clamp(level * 9, 0, 90)));
		}
	}

	void updateBars(int level) {
		bool on = level > 0;
		m_barTop->setVisible(on);
		m_barBottom->setVisible(on);
		if (!on) return;
		float h = static_cast<float>(level) * 5.f;
		m_barTop->changeHeight(h);
		m_barBottom->changeHeight(h);
		m_barTop->setPosition({0.f, m_win.height - h});
		m_barBottom->setPosition({0.f, 0.f});
	}

	void updateScene() {
		auto mod = Mod::get();
		float sx = static_cast<float>(mod->getSettingValue<double>("fx-stretch-x"));
		float sy = static_cast<float>(mod->getSettingValue<double>("fx-stretch-y"));
		float zoom = static_cast<float>(mod->getSettingValue<double>("fx-zoom"));
		int tilt = static_cast<int>(mod->getSettingValue<int64_t>("fx-tilt"));
		int skew = static_cast<int>(mod->getSettingValue<int64_t>("fx-skew"));
		int wobble = static_cast<int>(mod->getSettingValue<int64_t>("fx-wobble"));
		int shake = static_cast<int>(mod->getSettingValue<int64_t>("fx-shake"));
		int pulse = static_cast<int>(mod->getSettingValue<int64_t>("fx-pulse"));
		bool mirror = mod->getSettingValue<bool>("fx-mirror");
		bool flip = mod->getSettingValue<bool>("fx-flip");
		int spin = static_cast<int>(mod->getSettingValue<int64_t>("fx-spin"));

		float rot = static_cast<float>(tilt) + wobble * 0.6f * std::sin(m_t * 1.7f);
		float pz = 1.f + pulse * 0.012f * std::sin(m_t * 6.f);
		if (spin > 0) rot += std::fmod(m_t * spin * 20.f, 360.f);
		float zx = sx * zoom * pz * (mirror ? -1.f : 1.f);
		float zy = sy * zoom * pz * (flip ? -1.f : 1.f);
		CCPoint off = ccp(0, 0);
		if (shake > 0) off = ccp(frand(-1.f, 1.f) * shake * 0.6f, frand(-1.f, 1.f) * shake * 0.6f);

		bool active = std::abs(zx - 1.f) > 0.001f || std::abs(zy - 1.f) > 0.001f ||
			std::abs(rot) > 0.001f || shake > 0 || spin > 0 || skew != 0;

		// эффекты применяем только к самой игре, а не ко всей сцене:
		// меню, пауза и наши оверлеи остаются ровными
		CCNode* target = PlayLayer::get();
		if (!target) {
			m_sceneActive = false;
			m_target = nullptr;
			return;
		}
		if (target != m_target) {
			m_target = target;
			m_sceneActive = false;
		}

		if (active) {
			if (!m_sceneActive) {
				m_origPos = target->getPosition();
				m_origAnchor = target->getAnchorPoint();
				m_origIgnore = target->isIgnoreAnchorPointForPosition();
				m_sceneActive = true;
			}
			target->ignoreAnchorPointForPosition(false);
			target->setAnchorPoint(ccp(0.5f, 0.5f));
			target->setPosition(ccp(m_win.width / 2.f + off.x, m_win.height / 2.f + off.y));
			target->setScaleX(zx);
			target->setScaleY(zy);
			target->setRotation(rot);
			target->setSkewX(static_cast<float>(skew));
		} else if (m_sceneActive) {
			this->resetScene();
		}
	}

	void updateLyrics() {
		if (!m_lyricLabel) return;
		float volume = SystemAudioMeter::get().level();
		int index = -1;
		for (int i = 0; i < static_cast<int>(m_lyrics.size()); ++i) {
			if (m_t >= m_lyrics[i].time) index = i;
			else break;
		}
		if (index != m_currentLyric) {
			m_currentLyric = index;
			if (index >= 0) {
				m_lyricLabel->setString(m_lyrics[index].text.c_str());
				m_lyricLabel->stopAllActions();
				m_lyricLabel->setOpacity(0);
				m_lyricLabel->setScale(0.62f);
				m_lyricLabel->runAction(CCFadeTo::create(0.22f, 255));
				m_lyricLabel->runAction(CCScaleTo::create(0.22f, 0.72f));
			}
		}
		if (index < 0 || volume < 0.015f) {
			m_lyricLabel->setOpacity(static_cast<GLubyte>(
				std::max(0, static_cast<int>(m_lyricLabel->getOpacity()) - 12)));
			return;
		}
		float hue = std::fmod(m_t * 0.10f, 1.f);
		m_lyricLabel->setColor(hueColor(hue));
		float pulse = 1.f + volume * 0.10f;
		m_lyricLabel->setScale(0.72f * pulse);
		m_lyricLabel->setPosition(ccp(m_win.width * 0.5f,
			m_win.height - 42.f + std::sin(m_t * 3.f) * (1.5f + volume * 3.f)));
		m_lyricLabel->setOpacity(static_cast<GLubyte>(std::clamp(190.f + volume * 65.f, 0.f, 255.f)));
	}

	void resetScene() {
		if (!m_sceneActive) return;
		m_sceneActive = false;
		CCNode* target = PlayLayer::get();
		if (!target || target != m_target) return;
		target->setScaleX(1.f);
		target->setScaleY(1.f);
		target->setRotation(0.f);
		target->setSkewX(0.f);
		target->setAnchorPoint(m_origAnchor);
		target->setPosition(m_origPos);
		target->ignoreAnchorPointForPosition(m_origIgnore);
	}
};

CCNode* createFxLayer() {
	return FxLayer::create();
}
