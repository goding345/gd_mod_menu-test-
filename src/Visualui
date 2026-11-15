#include <Geode/Geode.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/TextInput.hpp>
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <vector>
#include "effects.hpp"

using namespace geode::prelude;

ccColor3B menuAccent();                   // menu.cpp
void toggleHackMenu();                    // menu.cpp
bool hackMenuOpen();                      // menu.cpp
void fxBurst(float x, float y, int idx);  // visuals.cpp
bool fxAvailable();                       // visuals.cpp

// Окно "Мои визуалы": список -> редактор (имя, частицы) -> игра с частицами.
class VisualUI : public CCLayer {
public:
	static inline VisualUI* s_open = nullptr;

	// idx: -2 список, -1 новый визуал, >=0 редактировать существующий
	static VisualUI* create(int idx) {
		auto ret = new VisualUI();
		if (ret->initUI(idx)) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}

	~VisualUI() override {
		if (s_open == this) s_open = nullptr;
		modalOpen() = false;
	}

	void registerWithTouchDispatcher() override {
		CCDirector::get()->getTouchDispatcher()->addTargetedDelegate(this, -10001, true);
	}

	bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
		CCPoint p = m_content->convertTouchToNodeSpace(touch);
		for (int i = static_cast<int>(m_items.size()) - 1; i >= 0; --i) {
			auto& it = m_items[i];
			if (p.x >= it.x && p.x <= it.x + it.w && p.y >= it.y && p.y <= it.y + it.h) {
				m_active = i;
				if (it.onDown) it.onDown(p.x);
				return true;
			}
		}
		if (m_mode == Mode::Edit && m_input && m_input->boundingBox().containsPoint(p)) {
			return false; // пропускаем касание в поле ввода имени
		}
		if (m_mode == Mode::Play) {
			CCPoint sp = this->convertTouchToNodeSpace(touch);
			fxBurst(sp.x, sp.y, m_idx);
			m_burstT = 0.f;
			m_playTouch = true;
		}
		return true;
	}

	void ccTouchMoved(CCTouch* touch, CCEvent*) override {
		if (m_active >= 0 && m_active < static_cast<int>(m_items.size()) && m_items[m_active].onDrag) {
			CCPoint p = m_content->convertTouchToNodeSpace(touch);
			m_items[m_active].onDrag(p.x);
			return;
		}
		if (m_mode == Mode::Play && m_playTouch) {
			CCPoint sp = this->convertTouchToNodeSpace(touch);
			m_lastTouch = sp;
			if (m_burstT > 0.06f) {
				fxBurst(sp.x, sp.y, m_idx);
				m_burstT = 0.f;
			}
		}
	}

	void ccTouchEnded(CCTouch*, CCEvent*) override {
		m_active = -1;
		m_playTouch = false;
	}

	void ccTouchCancelled(CCTouch*, CCEvent*) override {
		m_active = -1;
		m_playTouch = false;
	}

	void update(float dt) override {
		m_burstT += dt;
	}

private:
	enum class Mode { List, Edit, Play };

	struct Item {
		float x, y, w, h;
		std::function<void(float)> onDown;
		std::function<void(float)> onDrag;
	};

	Mode m_mode = Mode::List;
	CCNode* m_content = nullptr;
	CCLayerColor* m_dim = nullptr;
	TextInput* m_input = nullptr;
	std::vector<Item> m_items;
	int m_active = -1;
	int m_idx = -1;
	bool m_isNew = false;
	bool m_fromList = true;
	bool m_confirmDelete = false;
	bool m_reopenMenu = false;
	bool m_playTouch = false;
	int m_page = 0;
	float m_w = 0.f, m_h = 0.f;
	float m_burstT = 0.f;
	CustomEffect m_backup;
	CCPoint m_lastTouch = ccp(0, 0);

	static constexpr int kMaxEffects = 12;

	bool initUI(int idx) {
		if (!CCLayer::init()) return false;
		this->setTouchEnabled(true);
		s_open = this;
		modalOpen() = true;
		loadEffects();

		auto win = CCDirector::get()->getWinSize();
		m_dim = CCLayerColor::create(ccc4(0, 0, 0, 130), win.width, win.height);
		this->addChild(m_dim);
		m_content = CCNode::create();
		this->addChild(m_content, 5);

		m_fromList = idx == -2;
		if (idx == -2) this->showList();
		else this->showEdit(idx);
		this->scheduleUpdate();
		return true;
	}

	// ---------- общие кусочки ----------
	void clearScreen() {
		m_content->removeAllChildrenWithCleanup(true);
		m_items.clear();
		m_input = nullptr;
		m_active = -1;
	}

	void makeWindow(float w, float h, char const* title) {
		auto win = CCDirector::get()->getWinSize();
		m_w = w;
		m_h = h;
		m_content->setPosition(ccp((win.width - w) / 2.f, (win.height - h) / 2.f));

		auto shadow = CCLayerColor::create(ccc4(0, 0, 0, 90), w + 6.f, h + 6.f);
		shadow->setPosition(ccp(-3.f, -4.f));
		m_content->addChild(shadow);
		auto border = CCLayerColor::create(ccc4(255, 255, 255, 30), w + 2.f, h + 2.f);
		border->setPosition(ccp(-1.f, -1.f));
		m_content->addChild(border);
		auto body = CCLayerGradient::create(ccc4(30, 31, 42, 252), ccc4(15, 16, 22, 252), ccp(0, -1));
		body->setContentSize(CCSize(w, h));
		m_content->addChild(body);

		auto bar = CCLayerColor::create(ccc4(255, 255, 255, 255), w, 1.5f);
		bar->setColor(menuAccent());
		bar->setPosition(ccp(0.f, h - 26.f));
		m_content->addChild(bar, 1);

		auto t = CCLabelBMFont::create(title, "bigFont.fnt");
		t->setScale(0.42f);
		t->setAnchorPoint(ccp(0.f, 0.5f));
		t->setPosition(ccp(12.f, h - 13.f));
		m_content->addChild(t, 2);
	}

	CCLabelBMFont* addLabel(std::string const& text, float x, float y, float scale, ccColor3B col, float ax = 0.f) {
		auto l = CCLabelBMFont::create(text.c_str(), "chatFont.fnt");
		l->setScale(scale);
		l->setColor(col);
		l->setAnchorPoint(ccp(ax, 0.5f));
		l->setPosition(ccp(x, y));
		m_content->addChild(l, 3);
		return l;
	}

	CCLabelBMFont* addButton(std::string const& text, float x, float y, float w, float h,
		std::function<void()> cb, bool accent = false) {
		auto bg = CCLayerColor::create(ccc4(255, 255, 255, 255), w, h);
		bg->setColor(accent ? menuAccent() : ccc3(52, 54, 70));
		bg->setPosition(ccp(x, y));
		m_content->addChild(bg, 2);
		auto l = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
		l->setScale(0.3f);
		l->setPosition(ccp(x + w / 2.f, y + h / 2.f));
		m_content->addChild(l, 3);
		m_items.push_back({x, y, w, h, [cb](float) { cb(); }, nullptr});
		return l;
	}

	void addToggle(float x, float y, bool value, std::function<bool()> flip) {
		auto box = CCLayerColor::create(ccc4(70, 72, 88, 255), 12.f, 12.f);
		box->setPosition(ccp(x, y));
		m_content->addChild(box, 2);
		auto fill = CCLayerColor::create(ccc4(255, 255, 255, 255), 8.f, 8.f);
		fill->setColor(menuAccent());
		fill->setPosition(ccp(x + 2.f, y + 2.f));
		fill->setVisible(value);
		m_content->addChild(fill, 3);
		m_items.push_back({x - 4.f, y - 4.f, 20.f, 20.f, [fill, flip](float) { fill->setVisible(flip()); }, nullptr});
	}

	void addSlider(char const* label, int CustomEffect::* ptr, int mn, int mx, float y,
		std::function<std::string(int)> txt) {
		float tx = 92.f, tw = 128.f;
		addLabel(label, 12.f, y, 0.42f, ccc3(208, 210, 222));
		auto track = CCLayerColor::create(ccc4(64, 66, 80, 255), tw, 3.f);
		track->setPosition(ccp(tx, y - 1.5f));
		m_content->addChild(track, 2);
		auto fill = CCLayerColor::create(ccc4(255, 255, 255, 255), 1.f, 3.f);
		fill->setColor(menuAccent());
		fill->setPosition(ccp(tx, y - 1.5f));
		m_content->addChild(fill, 3);
		auto knob = CCLayerColor::create(ccc4(240, 241, 247, 255), 3.f, 9.f);
		m_content->addChild(knob, 4);
		auto val = addLabel("", m_w - 12.f, y, 0.4f, menuAccent(), 1.f);

		auto apply = [=](int v) {
			float n = (mx > mn) ? static_cast<float>(v - mn) / static_cast<float>(mx - mn) : 0.f;
			fill->changeWidth(std::max(1.f, n * tw));
			knob->setPosition(ccp(tx + n * (tw - 3.f), y - 4.5f));
			val->setString(txt(v).c_str());
		};
		apply(customEffects()[m_idx].*ptr);

		auto set = [=, this](float x) {
			if (m_idx < 0 || m_idx >= static_cast<int>(customEffects().size())) return;
			float n = std::clamp((x - tx) / tw, 0.f, 1.f);
			int v = mn + static_cast<int>(std::lround(n * static_cast<float>(mx - mn)));
			customEffects()[m_idx].*ptr = v;
			++customEffectsVersion();
			apply(v);
		};
		m_items.push_back({tx - 6.f, y - 8.f, tw + 12.f, 16.f, set, set});
	}

	// ---------- экран 1: список ----------
	void showList() {
		m_mode = Mode::List;
		m_dim->setVisible(true);
		clearScreen();
		makeWindow(260.f, 218.f, "My visuals");

		auto& fx = customEffects();
		int perPage = 5;
		int pages = std::max(1, (static_cast<int>(fx.size()) + perPage - 1) / perPage);
		m_page = std::clamp(m_page, 0, pages - 1);

		if (fx.empty()) {
			addLabel("Nothing here yet.", m_w / 2.f, m_h - 80.f, 0.5f, ccc3(150, 154, 172), 0.5f);
			addLabel("Press + New to create your first visual.", m_w / 2.f, m_h - 96.f, 0.42f, ccc3(120, 124, 142), 0.5f);
		}
		for (int k = 0; k < perPage; ++k) {
			int i = m_page * perPage + k;
			if (i >= static_cast<int>(fx.size())) break;
			float y = m_h - 52.f - k * 26.f;
			addLabel(fx[i].name, 14.f, y + 8.f, 0.5f, ccc3(220, 222, 234));
			addButton("Edit", m_w - 84.f, y, 38.f, 16.f, [this, i]() { m_fromList = true; showEdit(i); });
			addToggle(m_w - 32.f, y + 2.f, fx[i].on, [i]() {
				auto& e = customEffects()[i];
				e.on = !e.on;
				saveEffects();
				return e.on;
			});
		}
		if (pages > 1) {
			addButton("<", 14.f, 44.f, 22.f, 16.f, [this]() { --m_page; showList(); });
			addButton(">", 40.f, 44.f, 22.f, 16.f, [this]() { ++m_page; showList(); });
			addLabel(fmt::format("{}/{}", m_page + 1, pages), 74.f, 52.f, 0.42f, ccc3(150, 154, 172));
		}
		addButton("+ New", 14.f, 12.f, 110.f, 22.f, [this]() { m_fromList = true; showEdit(-1); }, true);
		addButton("Close", m_w - 124.f, 12.f, 110.f, 22.f, [this]() { closeUI(); });
	}

	// ---------- экран 2: редактор ----------
	void showEdit(int idx) {
		auto& fx = customEffects();
		if (idx < 0) {
			if (static_cast<int>(fx.size()) >= kMaxEffects) {
				Notification::create("Too many visuals (max 12). Delete one first.", NotificationIcon::Warning, 2.f)->show();
				if (m_fromList) showList(); else closeUI();
				return;
			}
			CustomEffect e;
			e.name = fmt::format("Visual {}", fx.size() + 1);
			fx.push_back(e);
			m_idx = static_cast<int>(fx.size()) - 1;
			m_isNew = true;
			++customEffectsVersion();
		} else {
			m_idx = idx;
			m_isNew = false;
			m_backup = fx[idx];
		}
		m_confirmDelete = false;
		buildEdit();
	}

	void buildEdit() {
		m_mode = Mode::Edit;
		m_dim->setVisible(true);
		clearScreen();
		makeWindow(300.f, 240.f, m_isNew ? "New visual" : "Edit visual");

		auto grey = ccc3(130, 134, 152);
		addLabel("1  NAME", 12.f, m_h - 38.f, 0.4f, grey);
		m_input = TextInput::create(170.f, "Visual name", "bigFont.fnt");
		m_input->setString(customEffects()[m_idx].name);
		m_input->setMaxCharCount(16);
		m_input->setPosition(ccp(m_w / 2.f + 10.f, m_h - 58.f));
		m_input->setCallback([this](std::string const& s) {
			if (m_idx >= 0 && m_idx < static_cast<int>(customEffects().size())) {
				customEffects()[m_idx].name = cleanName(s);
			}
		});
		m_content->addChild(m_input, 5);

		addLabel("2  PARTICLES", 12.f, m_h - 82.f, 0.4f, grey);
		float y = m_h - 98.f;
		float st = 15.f;
		static char const* shapes[4] = {"Square", "Diamond", "Streak", "Image"};
		static char const* modes[3] = {"Fall", "Rise", "Drift"};
		addSlider("Shape", &CustomEffect::shape, 0, 3, y, [](int v) { return std::string(shapes[std::clamp(v, 0, 3)]); });
		addSlider("Count", &CustomEffect::count, 1, 10, y - st, [](int v) { return std::to_string(v); });
		addSlider("Size", &CustomEffect::size, 1, 20, y - st * 2, [](int v) { return std::to_string(v); });
		addSlider("Speed", &CustomEffect::speed, 1, 10, y - st * 3, [](int v) { return std::to_string(v); });
		addSlider("Spin", &CustomEffect::spin, 0, 10, y - st * 4, [](int v) { return std::to_string(v); });
		addSlider("Sway", &CustomEffect::sway, 0, 10, y - st * 5, [](int v) { return std::to_string(v); });
		addSlider("Move", &CustomEffect::mode, 0, 2, y - st * 6, [](int v) { return std::string(modes[std::clamp(v, 0, 2)]); });
		addSlider("Color", &CustomEffect::color, 0, 101, y - st * 7, [](int v) {
			if (v == 100) return std::string("Rainbow");
			if (v == 101) return std::string("Background");
			return fmt::format("Hue {}", v);
		});

		addLabel("3  PLAY", 12.f, 42.f, 0.4f, grey);
		float bw = 66.f;
		addButton("Play", 12.f, 10.f, bw, 20.f, [this]() { showPlay(); });
		addButton("Save", 12.f + (bw + 5.f), 10.f, bw, 20.f, [this]() { saveEdit(); }, true);
		addButton("Cancel", 12.f + (bw + 5.f) * 2, 10.f, bw, 20.f, [this]() { cancelEdit(); });
		if (!m_isNew) {
			m_delLabel = addButton("Delete", 12.f + (bw + 5.f) * 3, 10.f, 58.f, 20.f, [this]() { deleteEdit(); });
		} else {
			m_delLabel = nullptr;
		}
	}

	CCLabelBMFont* m_delLabel = nullptr;

	void saveEdit() {
		auto& fx = customEffects();
		if (m_idx >= 0 && m_idx < static_cast<int>(fx.size())) {
			if (fx[m_idx].name.empty()) fx[m_idx].name = fmt::format("Visual {}", m_idx + 1);
			fx[m_idx].on = true;
		}
		saveEffects();
		afterEdit();
	}

	// отмена: новый визуал удаляется, у старого возвращаются прежние настройки
	void cancelEdit() {
		auto& fx = customEffects();
		if (m_idx >= 0 && m_idx < static_cast<int>(fx.size())) {
			if (m_isNew) fx.erase(fx.begin() + m_idx);
			else fx[m_idx] = m_backup;
		}
		++customEffectsVersion();
		afterEdit();
	}

	void deleteEdit() {
		if (!m_confirmDelete) {
			m_confirmDelete = true;
			if (m_delLabel) m_delLabel->setString("Sure?");
			return;
		}
		auto& fx = customEffects();
		if (m_idx >= 0 && m_idx < static_cast<int>(fx.size())) fx.erase(fx.begin() + m_idx);
		saveEffects();
		afterEdit();
	}

	void afterEdit() {
		m_idx = -1;
		if (m_fromList) showList();
		else closeUI();
	}

	// ---------- экран 3: играть ----------
	void showPlay() {
		m_mode = Mode::Play;
		m_dim->setVisible(false);
		clearScreen();
		auto win = CCDirector::get()->getWinSize();
		m_content->setPosition(ccp(0.f, 0.f));
		m_reopenMenu = hackMenuOpen();
		if (m_reopenMenu) toggleHackMenu(); // прячем меню, чтобы видеть весь экран

		bool ok = fxAvailable();
		addLabel(ok ? "Tap or drag anywhere to spawn bursts" : "Open a level to play with visuals",
			win.width / 2.f, win.height - 22.f, 0.55f, ccc3(235, 236, 244), 0.5f);
		addButton("Back", win.width / 2.f - 36.f, 30.f, 72.f, 20.f, [this]() {
			if (m_reopenMenu) {
				m_reopenMenu = false;
				toggleHackMenu();
			}
			buildEdit();
		}, true);
	}

	void closeUI() {
		this->unscheduleUpdate();
		this->removeFromParentAndCleanup(true);
	}
};

static void openVisualUI(int idx) {
	if (VisualUI::s_open) return;
	if (auto scene = CCDirector::get()->getRunningScene()) {
		if (auto ui = VisualUI::create(idx)) scene->addChild(ui, 100000);
	}
}

void openVisualManager() {
	openVisualUI(-2);
}

void openVisualEditor(int idx) {
	openVisualUI(idx);
}
