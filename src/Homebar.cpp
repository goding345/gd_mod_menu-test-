#include <Geode/Geode.hpp>
#include <cmath>

using namespace geode::prelude;

void toggleHackMenu(); // menu.cpp

// тонкая полоска внизу экрана, как индикатор "домой" на iPhone:
// тап или свайп вверх открывает меню
class HomeBar : public CCLayer {
public:
	static HomeBar* create() {
		auto ret = new HomeBar();
		if (ret->initBar()) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}

	void registerWithTouchDispatcher() override {
		CCDirector::get()->getTouchDispatcher()->addTargetedDelegate(this, -10000, true);
	}

	bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
		if (!this->isVisible()) return false;
		CCPoint p = this->convertTouchToNodeSpace(touch);
		auto win = CCDirector::get()->getWinSize();
		if (p.y > 26.f || std::abs(p.x - win.width / 2.f) > 90.f) return false; // мимо полоски - в игру
		m_pressed = true;
		m_fired = false;
		m_start = p;
		m_pill->setOpacity(235);
		return true;
	}

	void ccTouchMoved(CCTouch* touch, CCEvent*) override {
		if (!m_pressed || m_fired) return;
		CCPoint p = this->convertTouchToNodeSpace(touch);
		if (p.y - m_start.y > 12.f) {
			m_fired = true;
			toggleHackMenu();
		}
	}

	void ccTouchEnded(CCTouch*, CCEvent*) override {
		this->finish(true);
	}

	void ccTouchCancelled(CCTouch*, CCEvent*) override {
		this->finish(false);
	}

private:
	CCLayerColor* m_pill = nullptr;
	bool m_pressed = false;
	bool m_fired = false;
	CCPoint m_start = ccp(0, 0);

	bool initBar() {
		if (!CCLayer::init()) return false;
		this->setTouchEnabled(true);
		auto win = CCDirector::get()->getWinSize();
		m_pill = CCLayerColor::create(ccc4(255, 255, 255, 120), 84.f, 3.5f);
		m_pill->setPosition(ccp(win.width / 2.f - 42.f, 6.f));
		this->addChild(m_pill);
		return true;
	}

	void finish(bool allowTap) {
		if (!m_pressed) return;
		m_pressed = false;
		m_pill->setOpacity(120);
		if (!m_fired && allowTap) toggleHackMenu();
	}
};

CCNode* createHomeBar() {
	return HomeBar::create();
}
