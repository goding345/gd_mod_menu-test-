#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace geode::prelude;

bool tabKeyDown(); // keys.cpp

static char const* kModes[3] = {"Auto bot", "Record macro", "Replay macro"};

// kind: 0 переключатель, 1 целое число, 2 дробное число, 3 режим бота
struct Row {
	int kind;
	char const* label;
	char const* key;
	double mn, mx, st;
};

using Page = std::pair<char const*, std::vector<Row>>;

static std::vector<Page> const& pages() {
	static std::vector<Page> p = {
		{"Player", {
			{0, "Noclip", "noclip", 0, 0, 0},
			{0, "Jump hack", "jumphack", 0, 0, 0},
			{0, "Autoclicker", "autoclick", 0, 0, 0},
			{1, "Autoclicker CPS", "cps", 1, 30, 1},
		}},
		{"Bot", {
			{0, "Bot enabled", "enabled", 0, 0, 0},
			{3, "Bot mode", "mode", 0, 0, 0},
			{1, "Mistakes (0-10)", "legit", 0, 10, 1},
		}},
		{"Global", {
			{2, "Speedhack", "speed", 0.1, 5.0, 0.1},
			{0, "Safe mode", "safe-mode", 0, 0, 0},
			{0, "Cheat indicator", "indicator", 0, 0, 0},
		}},
		{"Visual", {
			{0, "Hide player", "hide-player", 0, 0, 0},
			{0, "Hide wave trail", "hide-wave", 0, 0, 0},
			{0, "Solid wave trail", "solid-wave", 0, 0, 0},
		}},
	};
	return p;
}

static std::string valueText(Row const& r) {
	auto m = Mod::get();
	switch (r.kind) {
		case 1: return std::to_string(m->getSettingValue<int64_t>(r.key));
		case 2: return fmt::format("{:.1f}x", m->getSettingValue<double>(r.key));
		case 3: return m->getSettingValue<std::string>(r.key);
		default: return "";
	}
}

class HackMenu : public Popup {
public:
	static inline HackMenu* s_open = nullptr;

	static HackMenu* create() {
		auto ret = new HackMenu();
		if (ret->initMenu()) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}

	~HackMenu() override {
		if (s_open == this) s_open = nullptr;
	}

	void closeMenu() {
		this->keyBackClicked();
	}

protected:
	int m_page = 0;
	std::vector<CCNode*> m_pageNodes;
	std::unordered_map<std::string, CCLabelBMFont*> m_valLabels;
	CCLabelBMFont* m_pageTitle = nullptr;
	CCLabelBMFont* m_pageNum = nullptr;

	bool initMenu() {
		if (!Popup::init(460.f, 300.f)) return false;
		s_open = this;
		this->setTitle("Mod Menu");

		m_pageTitle = CCLabelBMFont::create("", "goldFont.fnt");
		m_pageTitle->setScale(0.7f);
		m_pageTitle->setPosition({m_size.width / 2.f, m_size.height - 62.f});
		m_mainLayer->addChild(m_pageTitle);

		m_pageNum = CCLabelBMFont::create("", "bigFont.fnt");
		m_pageNum->setScale(0.4f);
		m_pageNum->setPosition({m_size.width / 2.f, 26.f});
		m_mainLayer->addChild(m_pageNum);

		for (int dir = 0; dir < 2; ++dir) {
			auto spr = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
			if (dir == 1) spr->setFlipX(true);
			spr->setScale(0.7f);
			auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(HackMenu::onPage));
			btn->setTag(dir);
			btn->setPosition({dir == 0 ? 50.f : m_size.width - 50.f, 26.f});
			m_buttonMenu->addChild(btn);
		}

		auto hint = CCLabelBMFont::create("Tab - open / close", "chatFont.fnt");
		hint->setScale(0.6f);
		hint->setOpacity(150);
		hint->setPosition({m_size.width - 70.f, m_size.height - 14.f});
		m_mainLayer->addChild(hint);

		this->buildPage();
		return true;
	}

	void buildPage() {
		for (auto n : m_pageNodes) n->removeFromParent();
		m_pageNodes.clear();
		m_valLabels.clear();

		auto const& pg = pages()[m_page];
		m_pageTitle->setString(pg.first);
		m_pageNum->setString(fmt::format("{} / {}", m_page + 1, pages().size()).c_str());

		float y = m_size.height - 105.f;
		int i = 0;
		for (auto const& r : pg.second) {
			this->addRow(r, i, y);
			y -= 42.f;
			++i;
		}
	}

	void addRow(Row const& r, int i, float y) {
		auto label = CCLabelBMFont::create(r.label, "bigFont.fnt");
		label->setScale(0.45f);
		label->setAnchorPoint({0.f, 0.5f});
		label->setPosition({28.f, y});
		m_mainLayer->addChild(label);
		m_pageNodes.push_back(label);

		if (r.kind == 0) {
			auto toggler = CCMenuItemToggler::createWithStandardSprites(
				this, menu_selector(HackMenu::onToggle), 0.7f);
			toggler->setTag(i);
			toggler->toggle(Mod::get()->getSettingValue<bool>(r.key));
			toggler->setPosition({m_size.width - 45.f, y});
			m_buttonMenu->addChild(toggler);
			m_pageNodes.push_back(toggler);
			return;
		}

		float cx = m_size.width - 120.f;
		for (int dir = 0; dir < 2; ++dir) {
			auto spr = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
			if (dir == 1) spr->setFlipX(true);
			spr->setScale(0.6f);
			auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(HackMenu::onStep));
			btn->setTag(i * 2 + dir);
			btn->setPosition({dir == 0 ? cx - 65.f : cx + 65.f, y});
			m_buttonMenu->addChild(btn);
			m_pageNodes.push_back(btn);
		}
		auto val = CCLabelBMFont::create(valueText(r).c_str(), "goldFont.fnt");
		val->setScale(0.5f);
		val->setPosition({cx, y});
		m_mainLayer->addChild(val);
		m_pageNodes.push_back(val);
		m_valLabels[r.key] = val;
	}

	void onPage(CCObject* sender) {
		int dir = static_cast<CCNode*>(sender)->getTag();
		int n = static_cast<int>(pages().size());
		m_page = (m_page + (dir == 1 ? 1 : n - 1)) % n;
		this->buildPage();
	}

	void onToggle(CCObject* sender) {
		auto t = static_cast<CCMenuItemToggler*>(sender);
		auto const& r = pages()[m_page].second[t->getTag()];
		bool newVal = !t->isToggled(); // в колбэке состояние ещё старое
		Mod::get()->setSettingValue<bool>(r.key, newVal);
	}

	void onStep(CCObject* sender) {
		int tag = static_cast<CCNode*>(sender)->getTag();
		int i = tag / 2;
		int dir = (tag % 2) ? 1 : -1;
		auto const& r = pages()[m_page].second[i];
		auto mod = Mod::get();

		if (r.kind == 1) {
			int64_t v = mod->getSettingValue<int64_t>(r.key) + dir * static_cast<int64_t>(r.st);
			v = std::clamp<int64_t>(v, static_cast<int64_t>(r.mn), static_cast<int64_t>(r.mx));
			mod->setSettingValue<int64_t>(r.key, v);
		} else if (r.kind == 2) {
			double v = mod->getSettingValue<double>(r.key) + dir * r.st;
			v = std::clamp(std::round(v * 10.0) / 10.0, r.mn, r.mx);
			mod->setSettingValue<double>(r.key, v);
		} else if (r.kind == 3) {
			std::string cur = mod->getSettingValue<std::string>(r.key);
			int idx = 0;
			for (int k = 0; k < 3; ++k) if (cur == kModes[k]) idx = k;
			idx = (idx + (dir == 1 ? 1 : 2)) % 3;
			mod->setSettingValue<std::string>(r.key, kModes[idx]);
		}
		auto it = m_valLabels.find(r.key);
		if (it != m_valLabels.end()) it->second->setString(valueText(r).c_str());
	}
};

class $modify(MenuScheduler, CCScheduler) {
	void update(float dt) {
		static bool prevTab = false;
		bool now = tabKeyDown();
		if (now && !prevTab) {
			if (HackMenu::s_open) {
				HackMenu::s_open->closeMenu();
			} else if (auto menu = HackMenu::create()) {
				menu->show();
			}
		}
		prevTab = now;

		// speedhack только внутри уровня
		if (PlayLayer::get()) {
			double sp = Mod::get()->getSettingValue<double>("speed");
			if (std::abs(sp - 1.0) > 0.001) dt *= static_cast<float>(sp);
		}
		CCScheduler::update(dt);
	}
};
