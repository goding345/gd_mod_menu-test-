#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

using namespace geode::prelude;

bool tabKeyDown(); // keys.cpp

static char const* kModes[3] = {"Auto bot", "Record macro", "Replay macro"};
static char const* kModesShort[3] = {"Auto", "Record", "Replay"};

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
			{0, "Autoclick", "autoclick", 0, 0, 0},
			{1, "Auto CPS", "cps", 1, 30, 1},
		}},
		{"Bot", {
			{0, "Bot on", "enabled", 0, 0, 0},
			{3, "Mode", "mode", 0, 0, 0},
			{1, "Mistakes", "legit", 0, 10, 1},
		}},
		{"Global", {
			{2, "Speed", "speed", 0.1, 5.0, 0.1},
			{0, "Safe mode", "safe-mode", 0, 0, 0},
			{0, "Indicator", "indicator", 0, 0, 0},
		}},
		{"Look", {
			{0, "Hide player", "hide-player", 0, 0, 0},
			{0, "Hide wave", "hide-wave", 0, 0, 0},
			{0, "Solid wave", "solid-wave", 0, 0, 0},
		}},
		{"Weather", {
			{1, "Snow", "fx-snow", 0, 10, 1},
			{1, "Rain", "fx-rain", 0, 10, 1},
			{1, "Fireflies", "fx-fireflies", 0, 10, 1},
		}},
		{"Screen", {
			{2, "Stretch X", "fx-stretch-x", 0.3, 3.0, 0.1},
			{2, "Stretch Y", "fx-stretch-y", 0.3, 3.0, 0.1},
			{2, "Zoom", "fx-zoom", 0.3, 3.0, 0.1},
			{1, "Tilt", "fx-tilt", -45, 45, 5},
		}},
		{"Fun", {
			{1, "Rainbow", "fx-tint", 0, 10, 1},
			{1, "Vignette", "fx-vignette", 0, 10, 1},
			{1, "Wobble", "fx-wobble", 0, 10, 1},
			{1, "Shake", "fx-shake", 0, 10, 1},
			{1, "Pulse", "fx-pulse", 0, 10, 1},
		}},
	};
	return p;
}

static std::string valueText(Row const& r) {
	auto m = Mod::get();
	switch (r.kind) {
		case 1: return std::to_string(m->getSettingValue<int64_t>(r.key));
		case 2: return fmt::format("{:.1f}x", m->getSettingValue<double>(r.key));
		case 3: {
			std::string cur = m->getSettingValue<std::string>(r.key);
			for (int i = 0; i < 3; ++i) if (cur == kModes[i]) return kModesShort[i];
			return cur;
		}
		default: return "";
	}
}

// ---------------- меню в стиле Mega Hack: окошки-панели ----------------
class HackOverlay : public CCLayer {
public:
	static inline HackOverlay* s_open = nullptr;

	static HackOverlay* create() {
		auto ret = new HackOverlay();
		if (ret->initOverlay()) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}

	~HackOverlay() override {
		if (s_open == this) s_open = nullptr;
	}

	void closeMenu() {
		this->removeFromParentAndCleanup(true);
	}

	void registerWithTouchDispatcher() override {
		CCDirector::get()->getTouchDispatcher()->addTargetedDelegate(this, -9999, true);
	}

	bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
		CCPoint p = this->convertTouchToNodeSpace(touch);
		for (int i = static_cast<int>(m_panels.size()) - 1; i >= 0; --i) {
			auto& pn = m_panels[i];
			float lx = p.x - pn.node->getPositionX();
			float ly = p.y - pn.node->getPositionY();
			if (lx < 0.f || ly < 0.f || lx > pn.w || ly > pn.h) continue;
			if (ly > pn.h - kHeader) {
				m_drag = i;
				m_dragOff = ccp(lx, ly);
				this->reorderChild(pn.node, ++m_topZ);
				return true;
			}
			for (auto& ru : pn.rows) {
				if (ly >= ru.yBottom && ly < ru.yBottom + kRow) {
					this->clickRow(ru, lx, pn.w);
					break;
				}
			}
			return true;
		}
		return true; // всё остальное проглатываем, чтобы клики не уходили в игру
	}

	void ccTouchMoved(CCTouch* touch, CCEvent*) override {
		if (m_drag < 0) return;
		auto win = CCDirector::get()->getWinSize();
		auto& pn = m_panels[m_drag];
		CCPoint p = this->convertTouchToNodeSpace(touch);
		float x = std::clamp(p.x - m_dragOff.x, 0.f, win.width - pn.w);
		float y = std::clamp(p.y - m_dragOff.y, 0.f, win.height - pn.h);
		pn.node->setPosition(ccp(x, y));
	}

	void ccTouchEnded(CCTouch*, CCEvent*) override {
		this->finishDrag();
	}

	void ccTouchCancelled(CCTouch*, CCEvent*) override {
		this->finishDrag();
	}

private:
	static constexpr float kPanelW = 76.f;
	static constexpr float kHeader = 15.f;
	static constexpr float kRow = 13.f;

	struct RowUI {
		Row const* row = nullptr;
		CCLayerColor* check = nullptr;
		CCLabelBMFont* value = nullptr;
		float yBottom = 0.f;
	};
	struct PanelUI {
		CCNode* node = nullptr;
		float w = 0.f, h = 0.f;
		std::vector<RowUI> rows;
	};

	std::vector<PanelUI> m_panels;
	int m_drag = -1;
	CCPoint m_dragOff = ccp(0, 0);
	int m_topZ = 10;

	bool initOverlay() {
		if (!CCLayer::init()) return false;
		this->setTouchEnabled(true);
		s_open = this;

		auto win = CCDirector::get()->getWinSize();
		auto dim = CCLayerColor::create(ccc4(0, 0, 0, 70), win.width, win.height);
		this->addChild(dim);

		auto const& pg = pages();
		int n = static_cast<int>(pg.size());
		float gap = 4.f;
		float totalW = n * kPanelW + (n - 1) * gap;
		float x0 = (win.width - totalW) / 2.f;
		auto mod = Mod::get();

		for (int p = 0; p < n; ++p) {
			PanelUI pn;
			int rows = static_cast<int>(pg[p].second.size());
			pn.w = kPanelW;
			pn.h = kHeader + rows * kRow + 3.f;

			float defX = x0 + p * (kPanelW + gap);
			float defY = win.height - 30.f - pn.h;
			float px = static_cast<float>(mod->getSavedValue<double>(fmt::format("panel-x-{}", p), defX));
			float py = static_cast<float>(mod->getSavedValue<double>(fmt::format("panel-y-{}", p), defY));
			px = std::clamp(px, 0.f, win.width - pn.w);
			py = std::clamp(py, 0.f, win.height - pn.h);

			pn.node = CCNode::create();
			pn.node->setPosition(ccp(px, py));
			this->addChild(pn.node, p);

			auto body = CCLayerColor::create(ccc4(22, 22, 26, 238), pn.w, pn.h);
			pn.node->addChild(body);

			auto head = CCLayerGradient::create(ccc4(238, 74, 156, 255), ccc4(168, 36, 104, 255), ccp(0, -1));
			head->setContentSize(CCSize(pn.w, kHeader));
			head->setPosition(ccp(0.f, pn.h - kHeader));
			pn.node->addChild(head);

			auto title = CCLabelBMFont::create(pg[p].first, "bigFont.fnt");
			title->setScale(0.3f);
			title->setPosition(ccp(pn.w / 2.f, pn.h - kHeader / 2.f));
			pn.node->addChild(title);

			for (int i = 0; i < rows; ++i) {
				RowUI ru;
				ru.row = &pg[p].second[i];
				ru.yBottom = pn.h - kHeader - (i + 1) * kRow;

				if (i % 2 == 1) {
					auto stripe = CCLayerColor::create(ccc4(255, 255, 255, 12), pn.w, kRow);
					stripe->setPosition(ccp(0.f, ru.yBottom));
					pn.node->addChild(stripe);
				}

				float cy = ru.yBottom + kRow / 2.f;
				auto label = CCLabelBMFont::create(ru.row->label, "chatFont.fnt");
				label->setScale(0.38f);
				label->setAnchorPoint(ccp(0.f, 0.5f));
				label->setPosition(ccp(4.f, cy));
				pn.node->addChild(label);

				if (ru.row->kind == 0) {
					auto box = CCLayerColor::create(ccc4(90, 90, 100, 255), 8.f, 8.f);
					box->setPosition(ccp(pn.w - 12.f, cy - 4.f));
					pn.node->addChild(box);
					auto inner = CCLayerColor::create(ccc4(238, 74, 156, 255), 6.f, 6.f);
					inner->setPosition(ccp(pn.w - 11.f, cy - 3.f));
					inner->setVisible(mod->getSettingValue<bool>(ru.row->key));
					pn.node->addChild(inner);
					ru.check = inner;
				} else {
					auto minus = CCLabelBMFont::create("<", "chatFont.fnt");
					minus->setScale(0.45f);
					minus->setPosition(ccp(pn.w - 38.f, cy));
					minus->setColor(ccc3(238, 74, 156));
					pn.node->addChild(minus);

					auto plus = CCLabelBMFont::create(">", "chatFont.fnt");
					plus->setScale(0.45f);
					plus->setPosition(ccp(pn.w - 6.f, cy));
					plus->setColor(ccc3(238, 74, 156));
					pn.node->addChild(plus);

					auto val = CCLabelBMFont::create(valueText(*ru.row).c_str(), "chatFont.fnt");
					val->setScale(0.38f);
					val->setPosition(ccp(pn.w - 22.f, cy));
					pn.node->addChild(val);
					ru.value = val;
				}
				pn.rows.push_back(ru);
			}
			m_panels.push_back(std::move(pn));
		}
		return true;
	}

	void finishDrag() {
		if (m_drag < 0) return;
		auto mod = Mod::get();
		auto& pn = m_panels[m_drag];
		mod->setSavedValue<double>(fmt::format("panel-x-{}", m_drag), pn.node->getPositionX());
		mod->setSavedValue<double>(fmt::format("panel-y-{}", m_drag), pn.node->getPositionY());
		m_drag = -1;
	}

	void clickRow(RowUI& ru, float lx, float w) {
		auto mod = Mod::get();
		auto const& r = *ru.row;

		if (r.kind == 0) {
			bool v = !mod->getSettingValue<bool>(r.key);
			mod->setSettingValue<bool>(r.key, v);
			ru.check->setVisible(v);
			return;
		}

		int dir = 0;
		if (lx >= w - 46.f && lx < w - 30.f) dir = -1;
		else if (lx >= w - 14.f) dir = 1;
		if (dir == 0) return;

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
		ru.value->setString(valueText(r).c_str());
	}
};

class $modify(MenuScheduler, CCScheduler) {
	void update(float dt) {
		static bool prevTab = false;
		bool now = tabKeyDown();
		if (now && !prevTab) {
			if (HackOverlay::s_open) {
				HackOverlay::s_open->closeMenu();
			} else if (auto scene = CCDirector::get()->getRunningScene()) {
				if (auto menu = HackOverlay::create()) scene->addChild(menu, 99999);
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
