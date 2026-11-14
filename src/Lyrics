#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace geode::prelude;

ccColor3B menuAccent(); // menu.cpp

struct LyricLine {
	int ms;
	std::string text;
};

// формат .lrc:  [мин:сек.сотые] текст строки
static std::vector<LyricLine> parseLrc(std::string const& src) {
	std::vector<LyricLine> out;
	std::istringstream in(src);
	std::string line;
	while (std::getline(in, line)) {
		size_t p = 0;
		std::vector<int> times;
		while (p < line.size() && line[p] == '[') {
			size_t e = line.find(']', p);
			if (e == std::string::npos) break;
			std::string tag = line.substr(p + 1, e - p - 1);
			int mm = 0;
			double ss = 0.0;
			if (std::sscanf(tag.c_str(), "%d:%lf", &mm, &ss) == 2) {
				times.push_back(static_cast<int>(mm * 60000 + ss * 1000.0));
			}
			p = e + 1;
		}
		std::string text = line.substr(p);
		while (!text.empty() && (text.back() == '\r' || text.back() == ' ')) text.pop_back();
		size_t s = text.find_first_not_of(' ');
		if (s != std::string::npos) text = text.substr(s);
		if (times.empty() || text.empty()) continue;
		for (int t : times) out.push_back({t, text});
	}
	std::sort(out.begin(), out.end(), [](LyricLine const& a, LyricLine const& b) { return a.ms < b.ms; });
	return out;
}

static float easeOut(float t) {
	t = std::clamp(t, 0.f, 1.f);
	float u = 1.f - t;
	return 1.f - u * u * u;
}

// субтитры сверху: буквы по очереди выезжают, снизу растёт линия цвета акцента.
// Текст берётся из кэша (.lrc в папке lyrics) или скачивается с LRCLIB по названию песни.
class LyricsLayer : public CCNode {
public:
	static LyricsLayer* create(std::string const& key, std::string const& title, std::string const& artist) {
		auto ret = new LyricsLayer();
		if (ret->init()) {
			ret->autorelease();
			ret->setup(key, title, artist);
			return ret;
		}
		delete ret;
		return nullptr;
	}

	void update(float dt) override {
		auto mod = Mod::get();
		bool on = mod->getSettingValue<bool>("lyrics") && !m_lines.empty();
		this->setVisible(on);
		if (!on) return;

		int offset = static_cast<int>(mod->getSettingValue<int64_t>("lyrics-offset")) * 100;
		float size = static_cast<float>(mod->getSettingValue<int64_t>("lyrics-size")) * 0.075f;
		int ms = static_cast<int>(FMODAudioEngine::sharedEngine()->getMusicTimeMS(0)) + offset;

		auto it = std::upper_bound(m_lines.begin(), m_lines.end(), ms,
			[](int v, LyricLine const& l) { return v < l.ms; });
		int idx = static_cast<int>(it - m_lines.begin()) - 1;

		if (idx != m_idx) {
			if (m_cur.label) {
				m_cur.leaving = true;
				m_old.push_back(m_cur);
				m_cur = Line();
			}
			m_idx = idx;
			if (idx >= 0) this->makeLine(m_lines[idx].text, size);
		}

		if (m_cur.label && !m_cur.leaving && m_idx >= 0) {
			int endMs = m_lines[m_idx].ms + 7000;
			if (m_idx + 1 < static_cast<int>(m_lines.size())) endMs = std::min(endMs, m_lines[m_idx + 1].ms);
			if (ms > endMs) {
				m_cur.leaving = true;
				m_old.push_back(m_cur);
				m_cur = Line();
			}
		}

		if (m_cur.label) this->animate(m_cur, dt);
		for (auto& l : m_old) this->animate(l, dt);
		for (size_t i = 0; i < m_old.size();) {
			if (m_old[i].leave >= 0.3f) {
				m_old[i].label->removeFromParent();
				m_old[i].bar->removeFromParent();
				m_old.erase(m_old.begin() + i);
			} else {
				++i;
			}
		}
	}

private:
	struct Line {
		CCLabelBMFont* label = nullptr;
		CCLayerColor* bar = nullptr;
		std::vector<float> baseY;
		float width = 0.f;
		float t = 0.f;
		float leave = 0.f;
		bool leaving = false;
	};

	std::vector<LyricLine> m_lines;
	Line m_cur;
	std::vector<Line> m_old;
	int m_idx = -2;
	std::filesystem::path m_path;
	async::TaskHolder<web::WebResponse> m_task;

	void setup(std::string const& key, std::string const& title, std::string const& artist) {
		auto dir = Mod::get()->getSaveDir() / "lyrics";
		std::error_code ec;
		std::filesystem::create_directories(dir, ec);
		m_path = dir / (key + ".lrc");

		std::ifstream f(m_path);
		if (f) {
			std::stringstream ss;
			ss << f.rdbuf();
			m_lines = parseLrc(ss.str());
		}
		log::info("[AutoJump] lyrics '{}' ({} - {}): {} lines in cache", key, artist, title, m_lines.size());

		if (m_lines.empty() && !title.empty() && Mod::get()->getSettingValue<bool>("lyrics-auto")) {
			web::WebRequest req;
			req.userAgent("GeodeModMenu/2.9");
			req.param("track_name", title);
			if (!artist.empty()) req.param("artist_name", artist);
			m_task.spawn(req.get("https://lrclib.net/api/search"), [this](web::WebResponse res) {
				this->onResponse(res);
			});
		}
		this->scheduleUpdate();
	}

	void onResponse(web::WebResponse& res) {
		if (!res.ok()) {
			log::warn("[AutoJump] lyrics request failed: {}", res.code());
			return;
		}
		auto jr = res.json();
		if (!jr.isOk()) return;
		auto val = jr.unwrap();
		if (!val.isArray()) return;
		auto arr = val.asArray().unwrapOr(std::vector<matjson::Value>{});
		for (auto const& item : arr) {
			auto sl = item["syncedLyrics"];
			if (!sl.isString()) continue;
			std::string txt = sl.asString().unwrapOr("");
			if (txt.empty()) continue;
			std::ofstream(m_path) << txt;
			m_lines = parseLrc(txt);
			m_idx = -2;
			log::info("[AutoJump] lyrics downloaded: {} lines", m_lines.size());
			return;
		}
		log::info("[AutoJump] no synced lyrics found for this song");
	}

	void makeLine(std::string const& text, float size) {
		auto win = CCDirector::get()->getWinSize();
		Line l;
		l.label = CCLabelBMFont::create(text.c_str(), "chatFont.fnt");
		float scale = size;
		float w = l.label->getContentSize().width * scale;
		if (w > win.width * 0.9f) {
			scale *= win.width * 0.9f / w;
			w = win.width * 0.9f;
		}
		l.width = w;
		l.label->setScale(scale);
		l.label->setPosition(ccp(win.width / 2.f, win.height - 24.f));
		this->addChild(l.label, 2);

		if (auto kids = l.label->getChildren()) {
			for (auto kid : CCArrayExt<CCNode*>(kids)) l.baseY.push_back(kid->getPositionY());
		}

		l.bar = CCLayerColor::create(ccc4(255, 255, 255, 220), 1.f, 1.5f);
		l.bar->setColor(menuAccent());
		l.bar->setPosition(ccp(win.width / 2.f, win.height - 38.f));
		this->addChild(l.bar, 1);
		m_cur = l;
	}

	void animate(Line& l, float dt) {
		l.t += dt;
		if (l.leaving) l.leave += dt;
		float out = l.leaving ? 1.f - std::clamp(l.leave / 0.3f, 0.f, 1.f) : 1.f;

		int i = 0;
		if (auto kids = l.label->getChildren()) {
			for (auto kid : CCArrayExt<CCSprite*>(kids)) {
				float rev = easeOut((l.t - i * 0.03f) / 0.28f);
				kid->setOpacity(static_cast<GLubyte>(255.f * rev * out));
				if (i < static_cast<int>(l.baseY.size())) {
					kid->setPositionY(l.baseY[i] + (1.f - rev) * 4.5f / std::max(0.1f, l.label->getScale()));
				}
				++i;
			}
		}

		float grow = easeOut(l.t / 0.5f);
		float bw = std::max(1.f, l.width * 0.6f * grow);
		auto win = CCDirector::get()->getWinSize();
		l.bar->changeWidth(bw);
		l.bar->setPositionX(win.width / 2.f - bw / 2.f);
		l.bar->setOpacity(static_cast<GLubyte>(200.f * out));
	}
};

CCNode* createLyricsLayer(std::string key, std::string title, std::string artist) {
	return LyricsLayer::create(key, title, artist);
}
