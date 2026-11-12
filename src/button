#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>

using namespace geode::prelude;

void toggleHackMenu();   // menu.cpp
ccColor3B menuAccent();  // menu.cpp

// круглая плавающая кнопка открытия меню (для телефонов и планшетов).
// Тап - открыть/закрыть меню, перетаскивание - переместить кнопку.
class MenuFab : public CCLayer {
public:
	static MenuFab* create() {
		auto ret = new MenuFab();
		if (ret->initFab()) {
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
		float dx = p.x - m_gfx->getPositionX();
		float dy = p.y - m_gfx->getPositionY();
		if (std::sqrt(dx * dx + dy * dy) > kR + 8.f) return false; // мимо кнопки - пропускаем в игру
		m_pressed = true;
		m_moved = false;
		m_start = p;
		m_offX = -dx;
		m_offY = -dy;
		m_gfx->setScale(0.9f);
		return true;
	}

	void ccTouchMoved(CCTouch* touch, CCEvent*) override {
		if (!m_pressed) return;
		CCPoint p = this->convertTouchToNodeSpace(touch);
		if (std::abs(p.x - m_start.x) + std::abs(p.y - m_start.y) > 10.f) m_moved = true;
		if (!m_moved) return;
		auto win = CCDirector::get()->getWinSize();
		m_gfx->setPosition(ccp(std::clamp(p.x + m_offX, kR, win.width - kR),
			std::clamp(p.y + m_offY, kR, win.height - kR)));
	}

	void ccTouchEnded(CCTouch*, CCEvent*) override {
		this->finish(true);
	}

	void ccTouchCancelled(CCTouch*, CCEvent*) override {
		this->finish(false);
	}

	void update(float) override {
		auto c = menuAccent();
		if (c.r != m_c.r || c.g != m_c.g || c.b != m_c.b) {
			m_c = c;
			this->redraw();
		}
	}

private:
	static constexpr float kR = 14.f;
	CCDrawNode* m_gfx = nullptr;
	ccColor3B m_c = ccc3(0, 0, 0);
	bool m_pressed = false;
	bool m_moved = false;
	CCPoint m_start = ccp(0, 0);
	float m_offX = 0.f, m_offY = 0.f;

	bool initFab() {
		if (!CCLayer::init()) return false;
		this->setTouchEnabled(true);

		auto win = CCDirector::get()->getWinSize();
		auto mod = Mod::get();
		float x = static_cast<float>(mod->getSavedValue<double>("fab-x", win.width - 28.0));
		float y = static_cast<float>(mod->getSavedValue<double>("fab-y", win.height * 0.55));

		m_gfx = CCDrawNode::create();
		m_gfx->setPosition(ccp(std::clamp(x, kR, win.width - kR), std::clamp(y, kR, win.height - kR)));
		this->addChild(m_gfx);

		m_c = menuAccent();
		this->redraw();
		this->scheduleUpdate();
		return true;
	}

	void redraw() {
		m_gfx->clear();
		float r = m_c.r / 255.f, g = m_c.g / 255.f, b = m_c.b / 255.f;
		m_gfx->drawDot(ccp(0.f, 0.f), kR + 2.f, ccc4f(0.f, 0.f, 0.f, 0.18f));   // тень
		m_gfx->drawDot(ccp(0.f, 0.f), kR, ccc4f(r, g, b, 0.55f));                // кольцо
		m_gfx->drawDot(ccp(0.f, 0.f), kR - 2.2f, ccc4f(0.08f, 0.08f, 0.11f, 0.38f));
		for (int i = -1; i <= 1; ++i) {                                           // три полоски
			float yy = i * 4.f;
			CCPoint v[4] = {ccp(-5.f, yy - 0.8f), ccp(5.f, yy - 0.8f), ccp(5.f, yy + 0.8f), ccp(-5.f, yy + 0.8f)};
			m_gfx->drawPolygon(v, 4, ccc4f(0.94f, 0.95f, 0.97f, 0.75f), 0.f, ccc4f(0.f, 0.f, 0.f, 0.f));
		}
	}

	void finish(bool allowTap) {
		if (!m_pressed) return;
		m_pressed = false;
		m_gfx->setScale(1.f);
		if (m_moved) {
			auto mod = Mod::get();
			mod->setSavedValue<double>("fab-x", m_gfx->getPositionX());
			mod->setSavedValue<double>("fab-y", m_gfx->getPositionY());
		} else if (allowTap) {
			toggleHackMenu();
		}
	}
};

CCNode* createMenuFab() {
	return MenuFab::create();
}
