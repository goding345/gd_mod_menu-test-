#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

using namespace geode::prelude;

struct FxParticle {
	CCLayerColor* node = nullptr;
	float x = 0.f, y = 0.f, vx = 0.f, vy = 0.f, phase = 0.f;
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

		int snow = static_cast<int>(mod->getSettingValue<int64_t>("fx-snow"));
		int rain = static_cast<int>(mod->getSettingValue<int64_t>("fx-rain"));
		int fire = static_cast<int>(mod->getSettingValue<int64_t>("fx-fireflies"));
		this->adjustPool(m_snow, snow * 12, 0);
		this->adjustPool(m_rain, rain * 15, 1);
		this->adjustPool(m_fire, fire * 5, 2);

		this->stepSnow(dt);
		this->stepRain(dt);
		this->stepFireflies(dt);

		this->updateTint(static_cast<int>(mod->getSettingValue<int64_t>("fx-tint")));
		this->updateVignette(static_cast<int>(mod->getSettingValue<int64_t>("fx-vignette")));
		this->updateScene();
	}

private:
	CCSize m_win;
	CCLayerColor* m_tint = nullptr;
	CCLayerGradient* m_vig[4] = {nullptr, nullptr, nullptr, nullptr};
	int m_lastVig = -1;
	std::vector<FxParticle> m_snow, m_rain, m_fire;
	std::mt19937 m_rng{std::random_device{}()};
	float m_t = 0.f;
	bool m_sceneActive = false;

	float frand(float a, float b) {
		std::uniform_real_distribution<float> d(a, b);
		return d(m_rng);
	}

	void setup() {
		m_win = CCDirector::get()->getWinSize();

		m_tint = CCLayerColor::create(ccc4(255, 255, 255, 0), m_win.width, m_win.height);
		this->addChild(m_tint);

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
			this->addChild(g);
			m_vig[i] = g;
		}
		this->scheduleUpdate();
	}

	// type: 0 снег, 1 дождь, 2 светлячки
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
			} else {
				w = h = frand(2.f, 4.f);
				col = ccc4(220, 255, 120, 200);
			}
			p.phase = frand(0.f, 6.28f);
			p.x = frand(0.f, m_win.width);
			p.y = frand(0.f, m_win.height);
			p.node = CCLayerColor::create(col, w, h);
			p.node->setPosition({p.x, p.y});
			this->addChild(p.node);
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

	void stepSnow(float dt) {
		for (auto& p : m_snow) {
			p.y -= p.vy * dt;
			p.x += (p.vx + std::sin(m_t * 1.3f + p.phase) * 12.f) * dt;
			this->wrap(p);
			p.node->setPosition({p.x, p.y});
		}
	}

	void stepRain(float dt) {
		for (auto& p : m_rain) {
			p.y -= p.vy * dt;
			p.x += p.vx * dt;
			this->wrap(p);
			p.node->setPosition({p.x, p.y});
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

	void updateScene() {
		auto mod = Mod::get();
		float sx = static_cast<float>(mod->getSettingValue<double>("fx-stretch-x"));
		float sy = static_cast<float>(mod->getSettingValue<double>("fx-stretch-y"));
		float zoom = static_cast<float>(mod->getSettingValue<double>("fx-zoom"));
		int tilt = static_cast<int>(mod->getSettingValue<int64_t>("fx-tilt"));
		int wobble = static_cast<int>(mod->getSettingValue<int64_t>("fx-wobble"));
		int shake = static_cast<int>(mod->getSettingValue<int64_t>("fx-shake"));
		int pulse = static_cast<int>(mod->getSettingValue<int64_t>("fx-pulse"));

		float rot = static_cast<float>(tilt) + wobble * 0.6f * std::sin(m_t * 1.7f);
		float pz = 1.f + pulse * 0.012f * std::sin(m_t * 6.f);
		float zx = sx * zoom * pz;
		float zy = sy * zoom * pz;
		CCPoint off = ccp(0, 0);
		if (shake > 0) off = ccp(frand(-1.f, 1.f) * shake * 0.6f, frand(-1.f, 1.f) * shake * 0.6f);

		bool active = std::abs(zx - 1.f) > 0.001f || std::abs(zy - 1.f) > 0.001f ||
			std::abs(rot) > 0.001f || shake > 0;

		auto scene = CCDirector::get()->getRunningScene();
		if (!scene) return;

		if (active) {
			scene->ignoreAnchorPointForPosition(false);
			scene->setAnchorPoint(ccp(0.5f, 0.5f));
			scene->setPosition(ccp(m_win.width / 2.f + off.x, m_win.height / 2.f + off.y));
			scene->setScaleX(zx);
			scene->setScaleY(zy);
			scene->setRotation(rot);
			m_sceneActive = true;
		} else if (m_sceneActive) {
			this->resetScene();
		}
	}

	void resetScene() {
		if (!m_sceneActive) return;
		m_sceneActive = false;
		auto scene = CCDirector::get()->getRunningScene();
		if (!scene) return;
		scene->setScaleX(1.f);
		scene->setScaleY(1.f);
		scene->setRotation(0.f);
		scene->setPosition(ccp(0, 0));
		scene->ignoreAnchorPointForPosition(true);
	}
};

CCNode* createFxLayer() {
	return FxLayer::create();
}
