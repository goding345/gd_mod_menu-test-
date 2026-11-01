#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <algorithm>
#include <cmath>
#include <string>

using namespace geode::prelude;

bool tabKeyDown(); // keys.cpp

static char const* kModes[3] = {"Auto bot", "Record macro", "Replay macro"};

class HackMenu : public Popup {
public:
	static inline HackMenu* s_open = nullptr;

	static HackMenu* create() {
		auto ret = new HackMenu();
		if (ret->init()) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}

	~HackMenu() override {
		if (s_open == this) s_open = nullptr;
	}

protected:
	CCLabelBMFont* m_speedLabel = nullptr;
	CCLabelBMFont* m_legitLabel = nullptr;
	CCLabelBMFont* m_modeLabel = nullptr;

	bool init() {
		if (!Popup::init(420.f, 290.f)) return false;
		s_open = this;
		this->setTitle("Mod Menu");

		auto mod = Mod::get();
		float top = m_size.height - 62.f;
		float stepY = 38.f;

		this->addToggleRow("Noclip", 1, mod->getSettingValue<bool>("noclip"), top);
		this->addToggleRow("Safe mode (no completion with cheats)", 3, mod->getSettingValue<bool>("safe-mode"), top - stepY);
		this->addToggleRow("Bot enabled", 2, mod->getSettingValue<bool>("enabled"), top - stepY * 2);

		m_modeLabel = this->addStepRow("Bot mode", 14,
			mod->getSettingValue<std::string>("mode"), top - stepY * 3);
		m_legitLabel = this->addStepRow("Mistakes (0-10)", 12,
			std::to_string(mod->getSettingValue<int64_t>("legit")), top - stepY * 4);
		m_speedLabel = this->addStepRow("Speedhack", 10,
			fmt::format("{:.1f}x", mod->getSettingValue<double>("speed")), top - stepY * 5);

		auto hint = CCLabelBMFont::create("Tab - open / close menu", "chatFont.fnt");
		hint->setScale(0.7f);
		hint->setOpacity(160);
		hint->setPosition({m_size.width / 2.f, 20.f});
		m_mainLayer->addChild(hint);

		return true;
	}

	void addToggleRow(char const* text, int tag, bool value, float y) {
		auto label = CCLabelBMFont::create(text, "bigFont.fnt");
		label->setScale(0.45f);
		label->setAnchorPoint({0.f, 0.5f});
		label->setPosition({24.f, y});
		m_mainLayer->addChild(label);

		auto toggler = CCMenuItemToggler::createWithStandardSprites(
			this, menu_selector(HackMenu::onToggle), 0.7f);
		toggler->setTag(tag);
		toggler->toggle(value);
		toggler->setPosition({m_size.width - 40.f, y});
		m_buttonMenu->addChild(toggler);
	}

	CCLabelBMFont* addStepRow(char const* text, int tagMinus, std::string const& value, float y) {
		auto label = CCLabelBMFont::create(text, "bigFont.fnt");
		label->setScale(0.45f);
		label->setAnchorPoint({0.f, 0.5f});
		label->setPosition({24.f, y});
		m_mainLayer->addChild(label);

		float cx = m_size.width - 105.f;

		auto left = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
		left->setScale(0.6f);
		auto btnL = CCMenuItemSpriteExtra::create(left, this, menu_selector(HackMenu::onStep));
		btnL->setTag(tagMinus);
		btnL->setPosition({cx - 55.f, y});
		m_buttonMenu->addChild(btnL);

		auto right = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
		right->setFlipX(true);
		right->setScale(0.6f);
		auto btnR = CCMenuItemSpriteExtra::create(right, this, menu_selector(HackMenu::onStep));
		btnR->setTag(tagMinus + 1);
		btnR->setPosition({cx + 55.f, y});
		m_buttonMenu->addChild(btnR);

		auto val = CCLabelBMFont::create(value.c_str(), "goldFont.fnt");
		val->setScale(0.5f);
		val->setPosition({cx, y});
		m_mainLayer->addChild(val);
		return val;
	}

	void onToggle(CCObject* sender) {
		auto t = static_cast<CCMenuItemToggler*>(sender);
		bool newVal = !t->isToggled(); // в колбэке состояние ещё старое
		switch (t->getTag()) {
			case 1: Mod::get()->setSettingValue<bool>("noclip", newVal); break;
			case 2: Mod::get()->setSettingValue<bool>("enabled", newVal); break;
			case 3: Mod::get()->setSettingValue<bool>("safe-mode", newVal); break;
			default: break;
		}
	}

	void onStep(CCObject* sender) {
		int tag = static_cast<CCNode*>(sender)->getTag();
		auto mod = Mod::get();
		if (tag == 10 || tag == 11) {
			double v = mod->getSettingValue<double>("speed");
			v += (tag == 11) ? 0.1 : -0.1;
			v = std::clamp(std::round(v * 10.0) / 10.0, 0.1, 5.0);
			mod->setSettingValue<double>("speed", v);
			m_speedLabel->setString(fmt::format("{:.1f}x", v).c_str());
		} else if (tag == 12 || tag == 13) {
			int64_t v = mod->getSettingValue<int64_t>("legit");
			v = std::clamp<int64_t>(v + ((tag == 13) ? 1 : -1), 0, 10);
			mod->setSettingValue<int64_t>("legit", v);
			m_legitLabel->setString(std::to_string(v).c_str());
		} else if (tag == 14 || tag == 15) {
			std::string cur = mod->getSettingValue<std::string>("mode");
			int idx = 0;
			for (int i = 0; i < 3; ++i) if (cur == kModes[i]) idx = i;
			idx = (idx + ((tag == 15) ? 1 : 2)) % 3;
			mod->setSettingValue<std::string>("mode", kModes[idx]);
			m_modeLabel->setString(kModes[idx]);
		}
	}
};

class $modify(MenuScheduler, CCScheduler) {
	void update(float dt) {
		static bool prevTab = false;
		bool now = tabKeyDown();
		if (now && !prevTab) {
			if (HackMenu::s_open) {
				HackMenu::s_open->keyBackClicked();
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
