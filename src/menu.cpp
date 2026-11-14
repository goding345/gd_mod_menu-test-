#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <unordered_map>
#include <chrono>
#include <Geode/ui/Notification.hpp>
#include <vector>

using namespace geode::prelude;

bool tabKeyDown(); // keys.cpp
bool shiftKeyDown(); // keys.cpp
bool letterKeyDown(char c); // keys.cpp
bool vkDown(int vk); // keys.cpp

static bool g_frozen = false;
static bool g_stepReq = false;
bool frameFrozen() {
	return g_frozen;
}

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
			{0, "Hazard noclip", "hazard-noclip", 0, 0, 0},
			{0, "Jump hack", "jumphack", 0, 0, 0},
			{0, "Autoclick", "autoclick", 0, 0, 0},
			{1, "Auto CPS", "cps", 1, 30, 1},
		}},
		{"Bot", {
			{0, "Bot on", "enabled", 0, 0, 0},
			{3, "Mode", "mode", 0, 0, 0},
			{1, "Mistakes", "legit", 0, 10, 1},
			{0, "Bot: cube", "bot-cube", 0, 0, 0},
			{0, "Bot: wave", "bot-wave", 0, 0, 0},
		}},
		{"Global", {
			{2, "Speed", "speed", 0.1, 5.0, 0.1},
			{0, "Slow-mo Shift", "slowmo-key", 0, 0, 0},
			{2, "Slow spd", "slowmo", 0.1, 1.0, 0.1},
			{0, "Safe mode", "safe-mode", 0, 0, 0},
			{0, "Indicator", "indicator", 0, 0, 0},
			{0, "Restart R", "restart-key", 0, 0, 0},
			{0, "Show FPS", "show-fps", 0, 0, 0},
		}},
		{"Icons", {
			{0, "Icon hack", "icon-hack", 0, 0, 0},
			{0, "Rainbow", "rainbow-icon", 0, 0, 0},
			{1, "Rainb spd", "rainbow-speed", 1, 10, 1},
			{1, "Opacity", "player-opacity", 1, 10, 1},
			{0, "Hide player", "hide-player", 0, 0, 0},
			{0, "Wave noclip", "wave-noclip", 0, 0, 0},
			{0, "Hide wave", "hide-wave", 0, 0, 0},
			{0, "Solid wave", "solid-wave", 0, 0, 0},
		}},
		{"Weather", {
			{1, "Snow", "fx-snow", 0, 10, 1},
			{1, "Rain", "fx-rain", 0, 10, 1},
			{1, "Fireflies", "fx-fireflies", 0, 10, 1},
			{1, "Petals", "fx-petals", 0, 10, 1},
			{1, "Stars", "fx-stars", 0, 10, 1},
			{1, "Wind", "fx-wind", -10, 10, 1},
			{1, "Embers", "fx-embers", 0, 10, 1},
			{1, "Confetti", "fx-confetti", 0, 10, 1},
		}},
		{"Screen", {
			{2, "Stretch X", "fx-stretch-x", 0.3, 3.0, 0.1},
			{2, "Stretch Y", "fx-stretch-y", 0.3, 3.0, 0.1},
			{2, "Zoom", "fx-zoom", 0.3, 3.0, 0.1},
			{1, "Tilt", "fx-tilt", -45, 45, 5},
			{1, "Skew X", "fx-skew", -45, 45, 5},
			{0, "Mirror", "fx-mirror", 0, 0, 0},
			{1, "Aurora", "fx-aurora", 0, 10, 1},
			{1, "Grade", "fx-grade", 0, 5, 1},
		}},
		{"Fun", {
			{1, "Rainbow", "fx-tint", 0, 10, 1},
			{1, "Vignette", "fx-vignette", 0, 10, 1},
			{1, "Scanlines", "fx-scanlines", 0, 10, 1},
			{1, "Letterbox", "fx-letterbox", 0, 10, 1},
			{1, "Wobble", "fx-wobble", 0, 10, 1},
			{1, "Shake", "fx-shake", 0, 10, 1},
			{1, "Pulse", "fx-pulse", 0, 10, 1},
		}},
		{"Tools", {
			{0, "Hitboxes", "hitboxes", 0, 0, 0},
			{0, "Layout mode", "layout-mode", 0, 0, 0},
			{1, "Path trail", "path-trail", 0, 10, 1},
			{0, "StartPos Q/E", "startpos-switch", 0, 0, 0},
			{0, "Frame step", "frame-step", 0, 0, 0},
			{0, "Lyrics", "lyrics", 0, 0, 0},
			{0, "Lyr. auto", "lyrics-auto", 0, 0, 0},
			{1, "Lyr. offset", "lyrics-offset", -20, 20, 1},
			{1, "Lyr. size", "lyrics-size", 5, 15, 1},
		}},
		{"Extra", {
			{0, "Flip screen", "fx-flip", 0, 0, 0},
			{1, "Spin", "fx-spin", 0, 10, 1},
			{0, "Show clock", "show-clock", 0, 0, 0},
			{0, "Attempt time", "show-time", 0, 0, 0},
			{1, "Glow frame", "fx-glow", 0, 10, 1},
			{1, "Grain", "fx-grain", 0, 10, 1},
			{4, "Reset FX", "", 0, 0, 0},
			{5, "Reset cheats", "", 0, 0, 0},
		}},
		{"Menu", {
			{7, "Accent", "ui-accent", 0, 0, 0},
			{1, "Opacity", "ui-opacity", 4, 10, 1},
			{1, "Anim speed", "ui-anim", 1, 10, 1},
			{0, "Round button", "menu-button", 0, 0, 0},
			{0, "Button on PC", "menu-button-desktop", 0, 0, 0},
			{6, "Reset layout", "", 0, 0, 0},
		}},
	};
	return p;
}

// kind 4: сбросить все эффекты; kind 5: выключить все читы
static void resetFx() {
	auto m = Mod::get();
	for (char const* k : {"fx-snow", "fx-rain", "fx-fireflies", "fx-petals", "fx-stars", "fx-wind",
		"fx-tilt", "fx-skew", "fx-tint", "fx-vignette", "fx-scanlines", "fx-letterbox",
		"fx-wobble", "fx-shake", "fx-pulse", "fx-spin",
		"fx-embers", "fx-confetti", "fx-aurora", "fx-grade", "fx-glow", "fx-grain"}) {
		m->setSettingValue<int64_t>(k, 0);
	}
	for (char const* k : {"fx-stretch-x", "fx-stretch-y", "fx-zoom"}) {
		m->setSettingValue<double>(k, 1.0);
	}
	m->setSettingValue<bool>("fx-mirror", false);
	m->setSettingValue<bool>("fx-flip", false);
}

static void resetCheats() {
	auto m = Mod::get();
	for (char const* k : {"noclip", "hazard-noclip", "wave-noclip", "jumphack", "autoclick", "enabled",
		"slowmo-key", "startpos-switch", "frame-step"}) {
		m->setSettingValue<bool>(k, false);
	}
	m->setSettingValue<double>("speed", 1.0);
}

static std::string valueText(Row const& r) {
	auto m = Mod::get();
	switch (r.kind) {
		case 1: return std::to_string(m->getSettingValue<int64_t>(r.key));
		case 2: return fmt::format("{:.1f}x", m->getSettingValue<double>(r.key));
		case 7: return m->getSettingValue<std::string>(r.key);
		case 3: {
			std::string cur = m->getSettingValue<std::string>(r.key);
			for (int i = 0; i < 3; ++i) if (cur == kModes[i]) return kModesShort[i];
			return cur;
		}
		default: return "";
	}
}

// ---------------- подсказки к функциям ----------------
static char const* kDefaultHint = "Hover a row for info  |  Click a header to fold  |  Drag a header to move";

static std::string descFor(Row const& r) {
	static std::unordered_map<std::string, char const*> d = {
		{"noclip", "Survive any obstacle. The start anti-cheat spike still kills."},
		{"hazard-noclip", "Ignore spikes and saws only; blocks still kill."},
		{"jumphack", "Cube can jump in mid-air."},
		{"autoclick", "Clicks by itself at the chosen speed."},
		{"cps", "Autoclicker clicks per second."},
		{"enabled", "Turn the bot on. The mode decides what it does."},
		{"mode", "Auto bot plays by itself. Record saves your run, Replay plays it."},
		{"legit", "Auto bot only: 0 = perfect, 10 = makes human mistakes."},
		{"bot-cube", "Let the bot play cube sections."},
		{"bot-wave", "Let the bot play wave sections."},
		{"speed", "Game speed multiplier inside a level."},
		{"slowmo-key", "Hold Shift to slow the game down."},
		{"slowmo", "How slow the Shift slow-motion is."},
		{"safe-mode", "Blocks level completion if cheats were used."},
		{"indicator", "Shows active cheats in the corner of the level."},
		{"restart-key", "Press R to restart the level instantly."},
		{"show-fps", "Shows FPS in the bottom-left corner."},
		{"icon-hack", "All icons and colors count as unlocked (local only)."},
		{"rainbow-icon", "Your icon cycles through all colors."},
		{"rainbow-speed", "How fast the rainbow icon changes color."},
		{"player-opacity", "Make your icon see-through (10 = normal)."},
		{"hide-player", "Hide your icon completely."},
		{"wave-noclip", "Don't die while playing as the wave."},
		{"hide-wave", "Hide the wave trail."},
		{"solid-wave", "Draw the wave trail as a solid line."},
		{"fx-snow", "Falling snow."},
		{"fx-rain", "Rain with a slight slant."},
		{"fx-fireflies", "Glowing fireflies drifting around."},
		{"fx-petals", "Pink petals falling down."},
		{"fx-stars", "Twinkling stars."},
		{"fx-wind", "Pushes snow, rain and petals sideways."},
		{"fx-embers", "Sparks rising from the bottom."},
		{"fx-confetti", "Colorful spinning confetti."},
		{"fx-stretch-x", "Stretch the picture horizontally."},
		{"fx-stretch-y", "Stretch the picture vertically."},
		{"fx-zoom", "Zoom the whole level in or out."},
		{"fx-tilt", "Rotate the picture (degrees)."},
		{"fx-skew", "Slant the picture sideways."},
		{"fx-mirror", "Flip the picture left to right."},
		{"fx-aurora", "Soft colorful northern lights on top."},
		{"fx-grade", "Color filter: 1 sepia, 2 night, 3 warm, 4 cold, 5 retro."},
		{"fx-tint", "Slowly changing rainbow color over the screen."},
		{"fx-vignette", "Darken the screen edges."},
		{"fx-scanlines", "Old monitor scan lines."},
		{"fx-letterbox", "Black cinema bars on top and bottom."},
		{"fx-wobble", "Gentle swaying of the picture."},
		{"fx-shake", "Camera shake."},
		{"fx-pulse", "Breathing zoom effect."},
		{"fx-flip", "Flip the picture upside down."},
		{"fx-spin", "Constantly rotate the picture."},
		{"show-clock", "Shows the current time."},
		{"show-time", "Shows how long this attempt lasts."},
		{"fx-glow", "Pulsing glowing frame around the screen."},
		{"fx-grain", "Film grain noise."},
		{"hitboxes", "Draw hitboxes: red hazards, blue blocks, yellow player."},
		{"layout-mode", "Hide decoration objects, keep only gameplay."},
		{"path-trail", "Draws the path you just flew."},
		{"startpos-switch", "Q / E switch between start positions."},
		{"frame-step", "F freezes the game, G advances one tick."},
		{"lyrics", "Animated lyrics at the top while the level song plays."},
		{"lyrics-auto", "Download synced lyrics from LRCLIB by song name (needs internet)."},
		{"lyrics-offset", "Shift lyrics timing (1 step = 0.1 s) if they run early or late."},
		{"lyrics-size", "Size of the lyrics text."},
		{"ui-accent", "Main color of the menu."},
		{"ui-opacity", "How see-through the panels are."},
		{"ui-anim", "Speed of menu animations."},
		{"menu-button", "Round button that opens the menu (for phones)."},
		{"menu-button-desktop", "Show that round button on PC too."},
	};
	std::string k = r.key;
	if (k.empty()) {
		if (r.kind == 4) return "Turn off every visual effect.";
		if (r.kind == 5) return "Turn off every cheat.";
		return "Put all panels back in their default places.";
	}
	auto it = d.find(k);
	return it != d.end() ? it->second : "";
}

// ---------------- горячие клавиши ----------------
struct Bind {
	std::string key;
	std::string label;
	int vk;
};
static std::vector<Bind> g_binds;
static bool g_bindsLoaded = false;

static void ensureBinds() {
	if (g_bindsLoaded) return;
	g_bindsLoaded = true;
	for (auto const& pg : pages()) {
		for (auto const& r : pg.second) {
			if (r.kind != 0) continue;
			int vk = static_cast<int>(Mod::get()->getSavedValue<int64_t>(fmt::format("bind-{}", r.key), 0));
			if (vk > 0) g_binds.push_back({r.key, r.label, vk});
		}
	}
}

static int bindOf(char const* key) {
	ensureBinds();
	for (auto const& b : g_binds) if (b.key == key) return b.vk;
	return 0;
}

static void setBind(Row const& r, int vk) {
	ensureBinds();
	g_binds.erase(std::remove_if(g_binds.begin(), g_binds.end(),
		[&](Bind const& b) { return b.key == r.key; }), g_binds.end());
	if (vk > 0) g_binds.push_back({r.key, r.label, vk});
	Mod::get()->setSavedValue<int64_t>(fmt::format("bind-{}", r.key), vk);
}

static std::string keyName(int vk) {
	if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::string(1, static_cast<char>(vk));
	if (vk >= 0x70 && vk <= 0x7B) return fmt::format("F{}", vk - 0x6F);
	return "?";
}

static std::string rowLabel(Row const& r) {
	if (r.kind != 0) return r.label;
	int vk = bindOf(r.key);
	return vk ? fmt::format("{} [{}]", r.label, keyName(vk)) : std::string(r.label);
}

// ---------------- меню: панели с анимацией ----------------
namespace ui {
	static char const* kAccentNames[7] = {"Pink", "Blue", "Green", "Orange", "Purple", "Red", "White"};
	static ccColor3B accentFor(std::string const& n) {
		if (n == "Blue") return ccc3(86, 156, 255);
		if (n == "Green") return ccc3(72, 214, 130);
		if (n == "Orange") return ccc3(255, 160, 64);
		if (n == "Purple") return ccc3(166, 112, 255);
		if (n == "Red") return ccc3(240, 84, 84);
		if (n == "White") return ccc3(235, 236, 244);
		return ccc3(236, 72, 153);
	}
	static ccColor3B accent() {
		return accentFor(Mod::get()->getSettingValue<std::string>("ui-accent"));
	}
	static ccColor3B const kText = {208, 210, 222};
	static ccColor3B const kTrackOff = {64, 66, 80};

	static float easeOut(float t) {
		t = std::clamp(t, 0.f, 1.f);
		float u = 1.f - t;
		return 1.f - u * u * u;
	}

	static ccColor3B lerpColor(ccColor3B a, ccColor3B b, float t) {
		auto f = [&](GLubyte x, GLubyte y) { return static_cast<GLubyte>(x + (y - x) * t); };
		return ccc3(f(a.r, b.r), f(a.g, b.g), f(a.b, b.b));
	}
}

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

	// Tab: открыть / закрыть (с анимацией)
	void toggleMenu() {
		m_closing = !m_closing;
	}

	void registerWithTouchDispatcher() override {
		CCDirector::get()->getTouchDispatcher()->addTargetedDelegate(this, -9999, true);
	}

	bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
		CCPoint p = this->convertTouchToNodeSpace(touch);
		for (int i = static_cast<int>(m_panels.size()) - 1; i >= 0; --i) {
			auto& pn = m_panels[i];
			float lx = p.x - pn.node->getPositionX();
			float ly = pn.node->getPositionY() - p.y; // вниз от верхнего края
			if (lx < 0.f || lx > pn.w || ly < 0.f || ly > pn.curH) continue;

			if (ly < kHeader) {
				m_drag = i;
				m_dragOff = ccp(lx, ly);
				m_dragStart = p;
				m_dragMoved = false;
				this->reorderChild(pn.node, ++m_topZ);
				return true;
			}
			if (pn.collapse > 0.5f) return true;

			int ri = static_cast<int>((ly - kHeader) / kRow);
			if (ri >= 0 && ri < static_cast<int>(pn.rows.size())) {
				this->pressRow(i, ri, lx);
			}
			return true;
		}
		return true; // остальное проглатываем, чтобы клики не уходили в игру
	}

	void ccTouchMoved(CCTouch* touch, CCEvent*) override {
		CCPoint p = this->convertTouchToNodeSpace(touch);
		auto win = CCDirector::get()->getWinSize();

		if (m_sliderPanel >= 0) {
			auto& pn = m_panels[m_sliderPanel];
			float lx = p.x - pn.node->getPositionX();
			this->setFromNorm(pn.rows[m_sliderRow], (lx - pn.trackX) / kTrackW);
			return;
		}
		if (m_drag < 0) return;
		auto& pn = m_panels[m_drag];
		if (std::abs(p.x - m_dragStart.x) + std::abs(p.y - m_dragStart.y) > 4.f) m_dragMoved = true;
		if (!m_dragMoved) return;
		pn.x = std::clamp(p.x - m_dragOff.x, 0.f, win.width - pn.w);
		pn.topY = std::clamp(p.y + m_dragOff.y, pn.curH, win.height);
	}

	void ccTouchEnded(CCTouch*, CCEvent*) override {
		this->finishTouch();
	}

	void ccTouchCancelled(CCTouch*, CCEvent*) override {
		this->finishTouch();
	}

	void update(float) override {
		auto now = std::chrono::steady_clock::now();
		float dt = std::chrono::duration<float>(now - m_last).count();
		m_last = now;
		dt = std::min(dt, 0.05f);
		auto md = Mod::get();
		dt *= static_cast<float>(md->getSettingValue<int64_t>("ui-anim")) / 5.f;
		m_opMul = static_cast<float>(md->getSettingValue<int64_t>("ui-opacity")) / 10.f;
		auto acc = ui::accent();
		if (acc.r != m_acc.r || acc.g != m_acc.g || acc.b != m_acc.b) {
			m_acc = acc;
			this->applyAccent();
		}

		float maxT = 0.45f + m_panels.size() * 0.045f;
		m_time = std::min(m_time + (m_closing ? -dt * 1.6f : dt), maxT);
		if (m_closing && m_time <= 0.f) {
			this->unscheduleUpdate();
			this->removeFromParentAndCleanup(true);
			return;
		}
		m_shimmerT += dt;
		m_dim->setOpacity(static_cast<GLubyte>(90.f * std::clamp(m_time * 4.f, 0.f, 1.f)));

		CCPoint mouse = this->convertToNodeSpace(geode::cocos::getMousePos());
		for (int i = 0; i < static_cast<int>(m_panels.size()); ++i) {
			this->animatePanel(m_panels[i], i, dt, mouse);
		}
		float fa = std::clamp(m_time * 3.f, 0.f, 1.f);
		m_bar->setOpacity(static_cast<GLubyte>(215.f * fa));
		m_barLine->setOpacity(static_cast<GLubyte>(255.f * fa));
		m_hint->setOpacity(static_cast<GLubyte>(255.f * fa));
		m_hint2->setOpacity(static_cast<GLubyte>(255.f * fa));
		m_title->setOpacity(static_cast<GLubyte>(255.f * fa));
		m_titleLine->setOpacity(static_cast<GLubyte>(255.f * fa));
		this->handleBinding(mouse);

		// подсказка под курсором (на телефоне - после нажатия)
		Row const* hr = this->hoveredRow(mouse);
		m_pressT = std::max(0.f, m_pressT - dt);
		if (!hr && m_pressT > 0.f) hr = m_pressRow;
		if (hr != m_infoRow) {
			m_infoRow = hr;
			if (hr) m_hint->setString(fmt::format("{}  -  {}", hr->label, descFor(*hr)).c_str());
			else m_hint->setString(kDefaultHint);
		}
	}

private:
	static constexpr float kPanelW = 104.f;
	static constexpr float kHeader = 16.f;
	static constexpr float kRow = 14.f;
	static constexpr float kTrackW = 32.f;

	struct Fade {
		CCRGBAProtocol* node;
		GLubyte base;
	};
	struct RowUI {
		Row const* row = nullptr;
		CCLayerColor* hover = nullptr;
		CCLayerColor* track = nullptr;
		CCLayerColor* fill = nullptr;
		CCLayerColor* knob = nullptr;
		CCLabelBMFont* value = nullptr;
		CCLabelBMFont* label = nullptr;
		float anim = 0.f;
		float hov = 0.f;
	};
	struct PanelUI {
		CCNode* node = nullptr;
		CCNode* rowsNode = nullptr;
		CCLayerColor* body = nullptr;
		CCLayerColor* shadow = nullptr;
		CCLayerColor* shimmer = nullptr;
		CCLabelBMFont* chevron = nullptr;
		CCLabelBMFont* badge = nullptr;
		CCLayerColor* border = nullptr;
		int lastBadge = -1;
		std::vector<Fade> fades;     // шапка, фон
		std::vector<Fade> rowFades;  // содержимое строк
		std::vector<RowUI> rows;
		float w = kPanelW, bodyH = 0.f, curH = 0.f;
		float x = 0.f, topY = 0.f;
		float trackX = 0.f;
		float collapse = 0.f, collapseT = 0.f;
		float lastA = -1.f, lastRowA = -1.f, lastOp = -1.f;
		float defX = 0.f, defTop = 0.f;
		std::vector<Fade> bgFades;   // фон, тень, шапка (учитывают Opacity)
	};

	std::vector<PanelUI> m_panels;
	CCLayerColor* m_dim = nullptr;
	std::chrono::steady_clock::time_point m_last = std::chrono::steady_clock::now();
	float m_time = 0.f;
	float m_shimmerT = 0.f;
	bool m_closing = false;
	int m_drag = -1;
	CCPoint m_dragOff = ccp(0, 0);
	CCPoint m_dragStart = ccp(0, 0);
	bool m_dragMoved = false;
	int m_topZ = 10;
	int m_sliderPanel = -1;
	int m_sliderRow = -1;
	bool m_prevKey[256] = {};
	CCLabelBMFont* m_hint = nullptr;
	CCLabelBMFont* m_hint2 = nullptr;
	CCLabelBMFont* m_title = nullptr;
	CCLayerColor* m_bar = nullptr;
	CCLayerColor* m_barLine = nullptr;
	CCLayerColor* m_titleLine = nullptr;
	Row const* m_infoRow = nullptr;
	Row const* m_pressRow = nullptr;
	float m_pressT = 0.f;
	std::vector<CCRGBAProtocol*> m_accents;
	ccColor3B m_acc = ccc3(236, 72, 153);
	float m_opMul = 1.f;

	bool initOverlay() {
		if (!CCLayer::init()) return false;
		this->setTouchEnabled(true);
		s_open = this;

		auto win = CCDirector::get()->getWinSize();
		m_dim = CCLayerColor::create(ccc4(0, 0, 0, 0), win.width, win.height);
		this->addChild(m_dim);

		// нижняя панель с подсказкой
		m_bar = CCLayerColor::create(ccc4(8, 9, 13, 215), win.width, 22.f);
		this->addChild(m_bar, 99);
		m_barLine = CCLayerColor::create(ccc4(255, 255, 255, 255), win.width, 1.f);
		m_barLine->setPosition(ccp(0.f, 22.f));
		this->addChild(m_barLine, 99);
		m_accents.push_back(m_barLine);

		m_hint = CCLabelBMFont::create(kDefaultHint, "chatFont.fnt");
		m_hint->setScale(0.5f);
		m_hint->setAnchorPoint(ccp(0.f, 0.5f));
		m_hint->setPosition(ccp(10.f, 11.f));
		m_hint->setColor(ccc3(190, 194, 210));
		this->addChild(m_hint, 100);

		m_hint2 = CCLabelBMFont::create("Hover a switch + press a key = bind  |  Backspace clears", "chatFont.fnt");
		m_hint2->setScale(0.42f);
		m_hint2->setAnchorPoint(ccp(1.f, 0.5f));
		m_hint2->setPosition(ccp(win.width - 10.f, 11.f));
		m_hint2->setColor(ccc3(130, 134, 152));
		this->addChild(m_hint2, 100);

		// заголовок
		m_title = CCLabelBMFont::create("MOD MENU", "bigFont.fnt");
		m_title->setScale(0.5f);
		m_title->setAnchorPoint(ccp(0.f, 0.5f));
		m_title->setPosition(ccp(14.f, win.height - 12.f));
		this->addChild(m_title, 100);
		m_titleLine = CCLayerColor::create(ccc4(255, 255, 255, 255), 46.f, 1.5f);
		m_titleLine->setPosition(ccp(14.f, win.height - 20.f));
		this->addChild(m_titleLine, 100);
		m_accents.push_back(m_titleLine);

		auto const& pg = pages();
		int n = static_cast<int>(pg.size());
		float gap = 4.f;
		int perRow = std::max(1, static_cast<int>((win.width - 12.f + gap) / (kPanelW + gap)));
		auto mod = Mod::get();

		auto fullHeight = [&](int idx) {
			return kHeader + static_cast<float>(pg[idx].second.size()) * kRow + 4.f;
		};
		std::vector<float> rowTop;
		float top = win.height - 26.f;
		for (int r = 0; r * perRow < n; ++r) {
			rowTop.push_back(top);
			float mh = 0.f;
			for (int c = 0; c < perRow && r * perRow + c < n; ++c) mh = std::max(mh, fullHeight(r * perRow + c));
			top -= mh + 8.f;
		}

		for (int p = 0; p < n; ++p) {
			PanelUI pn;
			int rows = static_cast<int>(pg[p].second.size());
			pn.bodyH = rows * kRow + 4.f;
			pn.curH = kHeader + pn.bodyH;
			pn.trackX = kPanelW - 64.f;

			int r = p / perRow;
			int c = p % perRow;
			int cnt = std::min(perRow, n - r * perRow);
			float x0 = (win.width - (cnt * kPanelW + (cnt - 1) * gap)) / 2.f;
			float defX = x0 + c * (kPanelW + gap);
			float defTop = rowTop[r];
			pn.defX = defX;
			pn.defTop = defTop;
			pn.x = std::clamp(static_cast<float>(mod->getSavedValue<double>(fmt::format("pnl3-x-{}", p), defX)), 0.f, win.width - pn.w);
			pn.topY = std::clamp(static_cast<float>(mod->getSavedValue<double>(fmt::format("pnl3-y-{}", p), defTop)), pn.curH, win.height);
			pn.collapseT = mod->getSavedValue<bool>(fmt::format("pnl3-c-{}", p), false) ? 1.f : 0.f;
			pn.collapse = pn.collapseT;

			pn.node = CCNode::create();
			this->addChild(pn.node, p);

			// тень, фон, шапка (всё привязано к верхнему левому углу)
			pn.shadow = CCLayerColor::create(ccc4(0, 0, 0, 70), pn.w + 3.f, pn.curH + 3.f);
			pn.node->addChild(pn.shadow);
			pn.bgFades.push_back({pn.shadow, 70});

			pn.border = CCLayerColor::create(ccc4(255, 255, 255, 26), pn.w + 2.f, pn.curH + 2.f);
			pn.node->addChild(pn.border);
			pn.bgFades.push_back({pn.border, 26});

			auto grad = CCLayerGradient::create(ccc4(29, 30, 41, 255), ccc4(14, 15, 21, 255), ccp(0, -1));
			grad->setContentSize(CCSize(pn.w, pn.bodyH));
			pn.body = grad;
			pn.node->addChild(pn.body);
			pn.bgFades.push_back({pn.body, 238});

			auto head = CCLayerColor::create(ccc4(30, 31, 43, 250), pn.w, kHeader);
			head->setPosition(ccp(0.f, -kHeader));
			pn.node->addChild(head);
			pn.bgFades.push_back({head, 250});

			auto line = CCLayerColor::create(ccc4(255, 255, 255, 255), pn.w, 1.5f);
			line->setPosition(ccp(0.f, -kHeader));
			pn.node->addChild(line);
			pn.fades.push_back({line, 255});
			m_accents.push_back(line);

			pn.shimmer = CCLayerColor::create(ccc4(255, 255, 255, 14), 14.f, kHeader - 1.5f);
			pn.shimmer->setPosition(ccp(0.f, -kHeader + 1.5f));
			pn.node->addChild(pn.shimmer);
			pn.fades.push_back({pn.shimmer, 14});

			auto title = CCLabelBMFont::create(pg[p].first, "bigFont.fnt");
			title->setScale(0.3f);
			title->setAnchorPoint(ccp(0.f, 0.5f));
			title->setPosition(ccp(7.f, -kHeader / 2.f));
			pn.node->addChild(title);
			pn.fades.push_back({title, 255});

			pn.chevron = CCLabelBMFont::create(pn.collapseT > 0.5f ? "+" : "-", "chatFont.fnt");
			pn.chevron->setScale(0.55f);
			pn.chevron->setColor(ui::accent());
			pn.chevron->setPosition(ccp(pn.w - 8.f, -kHeader / 2.f));
			pn.node->addChild(pn.chevron);
			pn.fades.push_back({pn.chevron, 255});
			m_accents.push_back(pn.chevron);

			pn.badge = CCLabelBMFont::create("", "chatFont.fnt");
			pn.badge->setScale(0.42f);
			pn.badge->setPosition(ccp(pn.w - 22.f, -kHeader / 2.f));
			pn.node->addChild(pn.badge);
			pn.fades.push_back({pn.badge, 255});
			m_accents.push_back(pn.badge);

			pn.rowsNode = CCNode::create();
			pn.node->addChild(pn.rowsNode);

			for (int i = 0; i < rows; ++i) {
				RowUI ru;
				ru.row = &pg[p].second[i];
				float rowTopY = -kHeader - i * kRow;
				float cy = rowTopY - kRow / 2.f;
				auto const& rw = *ru.row;

				ru.hover = CCLayerColor::create(ccc4(255, 255, 255, 0), pn.w, kRow);
				ru.hover->setPosition(ccp(0.f, rowTopY - kRow));
				pn.rowsNode->addChild(ru.hover);

				if (i > 0) {
					auto sep = CCLayerColor::create(ccc4(255, 255, 255, 12), pn.w - 10.f, 1.f);
					sep->setPosition(ccp(5.f, rowTopY));
					pn.rowsNode->addChild(sep);
					pn.rowFades.push_back({sep, 12});
				}

				auto label = CCLabelBMFont::create(rowLabel(rw).c_str(), "chatFont.fnt");
				ru.label = label;
				label->setScale(0.4f);
				bool isBtn = rw.kind >= 4 && rw.kind != 7;
				label->setColor(isBtn ? ui::accent() : ui::kText);
				if (isBtn) m_accents.push_back(label);
				if (isBtn) {
					label->setPosition(ccp(pn.w / 2.f, cy));
				} else {
					label->setAnchorPoint(ccp(0.f, 0.5f));
					label->setPosition(ccp(7.f, cy));
				}
				pn.rowsNode->addChild(label);
				pn.rowFades.push_back({label, 255});

				if (rw.kind == 0) {
					ru.track = CCLayerColor::create(ccc4(64, 66, 80, 255), 16.f, 8.f);
					ru.track->setPosition(ccp(pn.w - 25.f, cy - 4.f));
					pn.rowsNode->addChild(ru.track);
					pn.rowFades.push_back({ru.track, 255});

					ru.knob = CCLayerColor::create(ccc4(240, 241, 247, 255), 6.f, 6.f);
					ru.knob->setPosition(ccp(pn.w - 24.f, cy - 3.f));
					pn.rowsNode->addChild(ru.knob);
					pn.rowFades.push_back({ru.knob, 255});

					ru.anim = mod->getSettingValue<bool>(rw.key) ? 1.f : 0.f;
				} else if (rw.kind == 1 || rw.kind == 2) {
					ru.track = CCLayerColor::create(ccc4(64, 66, 80, 255), kTrackW, 3.f);
					ru.track->setPosition(ccp(pn.trackX, cy - 1.5f));
					pn.rowsNode->addChild(ru.track);
					pn.rowFades.push_back({ru.track, 255});

					ru.fill = CCLayerColor::create(ccc4(255, 255, 255, 255), 1.f, 3.f);
					ru.fill->setPosition(ccp(pn.trackX, cy - 1.5f));
					pn.rowsNode->addChild(ru.fill);
					pn.rowFades.push_back({ru.fill, 255});
					m_accents.push_back(ru.fill);

					ru.knob = CCLayerColor::create(ccc4(240, 241, 247, 255), 3.f, 8.f);
					ru.knob->setPosition(ccp(pn.trackX, cy - 4.f));
					pn.rowsNode->addChild(ru.knob);
					pn.rowFades.push_back({ru.knob, 255});

					ru.anim = this->normOf(rw);

					ru.value = CCLabelBMFont::create(valueText(rw).c_str(), "chatFont.fnt");
					ru.value->setScale(0.38f);
					ru.value->setAnchorPoint(ccp(1.f, 0.5f));
					ru.value->setPosition(ccp(pn.w - 6.f, cy));
					pn.rowsNode->addChild(ru.value);
					pn.rowFades.push_back({ru.value, 255});
				} else if (rw.kind == 3 || rw.kind == 7) {
					ru.value = CCLabelBMFont::create(valueText(rw).c_str(), "chatFont.fnt");
					ru.value->setScale(0.4f);
					ru.value->setColor(ui::accent());
					m_accents.push_back(ru.value);
					ru.value->setAnchorPoint(ccp(1.f, 0.5f));
					ru.value->setPosition(ccp(pn.w - 6.f, cy));
					pn.rowsNode->addChild(ru.value);
					pn.rowFades.push_back({ru.value, 255});
				}
				pn.rows.push_back(ru);
			}
			m_panels.push_back(std::move(pn));
		}

		this->applyAccent();
		for (int i = 0; i < n; ++i) this->animatePanel(m_panels[i], i, 0.f, ccp(-999.f, -999.f));
		this->scheduleUpdate();
		return true;
	}

	Row const* hoveredRow(CCPoint mouse) {
		for (auto& pn : m_panels) {
			if (pn.collapse > 0.5f) continue;
			float lx = mouse.x - pn.x;
			float top = pn.node->getPositionY();
			if (lx < 0.f || lx > pn.w) continue;
			for (size_t i = 0; i < pn.rows.size(); ++i) {
				float rt = top - kHeader - static_cast<float>(i) * kRow;
				if (mouse.y <= rt && mouse.y > rt - kRow) return pn.rows[i].row;
			}
		}
		return nullptr;
	}

	// назначение клавиш: навёл на переключатель и нажал клавишу
	void handleBinding(CCPoint mouse) {
		RowUI* hov = nullptr;
		for (auto& pn : m_panels) {
			if (pn.collapse > 0.5f) continue;
			float lx = mouse.x - pn.x;
			float top = pn.node->getPositionY();
			if (lx < 0.f || lx > pn.w) continue;
			for (size_t i = 0; i < pn.rows.size(); ++i) {
				float rt = top - kHeader - static_cast<float>(i) * kRow;
				if (mouse.y <= rt && mouse.y > rt - kRow && pn.rows[i].row->kind == 0) hov = &pn.rows[i];
			}
		}
		auto scan = [&](int vk) {
			bool d = vkDown(vk);
			if (d && !m_prevKey[vk] && hov && !m_closing) {
				setBind(*hov->row, (vk == 0x08 || vk == 0x2E) ? 0 : vk);
				hov->label->setString(rowLabel(*hov->row).c_str());
			}
			m_prevKey[vk] = d;
		};
		for (int vk = 'A'; vk <= 'Z'; ++vk) scan(vk);
		for (int vk = '0'; vk <= '9'; ++vk) scan(vk);
		for (int vk = 0x70; vk <= 0x7B; ++vk) scan(vk);
		scan(0x08);
		scan(0x2E);
	}

	void applyAccent() {
		for (auto n : m_accents) n->setColor(m_acc);
	}

	void resetLayout() {
		auto mod = Mod::get();
		for (int i = 0; i < static_cast<int>(m_panels.size()); ++i) {
			auto& pn = m_panels[i];
			pn.x = pn.defX;
			pn.topY = pn.defTop;
			pn.collapseT = 0.f;
			pn.chevron->setString("-");
			mod->setSavedValue<double>(fmt::format("pnl3-x-{}", i), pn.x);
			mod->setSavedValue<double>(fmt::format("pnl3-y-{}", i), pn.topY);
			mod->setSavedValue<bool>(fmt::format("pnl3-c-{}", i), false);
		}
	}

	float normOf(Row const& r) {
		auto mod = Mod::get();
		double v = (r.kind == 1) ? static_cast<double>(mod->getSettingValue<int64_t>(r.key))
		                         : mod->getSettingValue<double>(r.key);
		return static_cast<float>(std::clamp((v - r.mn) / (r.mx - r.mn), 0.0, 1.0));
	}

	void animatePanel(PanelUI& pn, int idx, float dt, CCPoint mouse) {
		float a = ui::easeOut((m_time - idx * 0.045f) / 0.28f);
		float k = std::min(1.f, dt * 16.f);

		pn.collapse += (pn.collapseT - pn.collapse) * k;
		if (std::abs(pn.collapse - pn.collapseT) < 0.002f) pn.collapse = pn.collapseT;
		pn.curH = kHeader + pn.bodyH * (1.f - pn.collapse);

		pn.node->setPosition(ccp(pn.x, pn.topY + (1.f - a) * 16.f));
		pn.node->setVisible(a > 0.001f);
		pn.body->changeHeight(std::max(0.f, pn.curH - kHeader));
		pn.body->setPosition(ccp(0.f, -pn.curH));
		pn.shadow->changeHeight(pn.curH + 3.f);
		pn.shadow->setPosition(ccp(-1.5f, -pn.curH - 1.5f));
		pn.border->changeHeight(pn.curH + 2.f);
		pn.border->setPosition(ccp(-1.f, -pn.curH - 1.f));

		int cnt = 0;
		for (auto& ru : pn.rows) {
			if (ru.row->kind == 0 && Mod::get()->getSettingValue<bool>(ru.row->key)) ++cnt;
		}
		if (cnt != pn.lastBadge) {
			pn.lastBadge = cnt;
			pn.badge->setString(cnt > 0 ? fmt::format("{}", cnt).c_str() : "");
		}
		pn.rowsNode->setVisible(pn.collapse < 0.98f);

		float rowA = std::clamp(1.f - pn.collapse * 1.8f, 0.f, 1.f);
		if (std::abs(a - pn.lastA) > 0.002f || std::abs(rowA - pn.lastRowA) > 0.002f || std::abs(m_opMul - pn.lastOp) > 0.002f) {
			pn.lastA = a;
			pn.lastRowA = rowA;
			pn.lastOp = m_opMul;
			for (auto& f : pn.bgFades) f.node->setOpacity(static_cast<GLubyte>(f.base * a * m_opMul));
			for (auto& f : pn.fades) f.node->setOpacity(static_cast<GLubyte>(f.base * a));
			for (auto& f : pn.rowFades) f.node->setOpacity(static_cast<GLubyte>(f.base * a * rowA));
		}

		// блик, бегущий по шапке
		float sx = std::fmod(m_shimmerT * 38.f + idx * 29.f, pn.w + 90.f) - 14.f;
		pn.shimmer->setVisible(sx >= 0.f && sx <= pn.w - 14.f);
		pn.shimmer->setPositionX(sx);

		if (pn.collapse >= 0.98f || dt <= 0.f) return;

		float top = pn.node->getPositionY();
		float lx = mouse.x - pn.x;
		auto mod = Mod::get();
		for (int i = 0; i < static_cast<int>(pn.rows.size()); ++i) {
			auto& ru = pn.rows[i];
			float rowTopY = top - kHeader - i * kRow;
			bool inside = lx >= 0.f && lx <= pn.w && mouse.y <= rowTopY && mouse.y > rowTopY - kRow && m_sliderPanel < 0 && m_drag < 0;
			ru.hov += ((inside ? 1.f : 0.f) - ru.hov) * k;
			ru.hover->setOpacity(static_cast<GLubyte>(24.f * ru.hov * a * rowA));

			auto const& r = *ru.row;
			if (r.kind == 0) {
				float tgt = mod->getSettingValue<bool>(r.key) ? 1.f : 0.f;
				ru.anim += (tgt - ru.anim) * k;
				float cy = -kHeader - i * kRow - kRow / 2.f;
				ru.knob->setPosition(ccp(pn.w - 24.f + ru.anim * 8.f, cy - 3.f));
				ru.track->setColor(ui::lerpColor(ui::kTrackOff, m_acc, ru.anim));
			} else if (r.kind == 1 || r.kind == 2) {
				float tgt = this->normOf(r);
				ru.anim += (tgt - ru.anim) * k;
				float cy = -kHeader - i * kRow - kRow / 2.f;
				ru.fill->changeWidth(std::max(1.f, ru.anim * kTrackW));
				ru.knob->setPosition(ccp(pn.trackX + ru.anim * (kTrackW - 3.f), cy - 4.f));
			}
		}
	}

	void pressRow(int pi, int ri, float lx) {
		auto& pn = m_panels[pi];
		auto& ru = pn.rows[ri];
		auto mod = Mod::get();
		auto const& r = *ru.row;
		m_pressRow = ru.row;
		m_pressT = 2.5f;

		if (r.kind >= 4 && r.kind != 7) {
			if (r.kind == 4) resetFx();
			else if (r.kind == 5) resetCheats();
			else this->resetLayout();
			this->refreshAll();
			return;
		}
		if (r.kind == 7) {
			std::string cur = mod->getSettingValue<std::string>(r.key);
			int idx = 0;
			for (int k = 0; k < 7; ++k) if (cur == ui::kAccentNames[k]) idx = k;
			mod->setSettingValue<std::string>(r.key, ui::kAccentNames[(idx + 1) % 7]);
			ru.value->setString(valueText(r).c_str());
			return;
		}
		if (r.kind == 0) {
			mod->setSettingValue<bool>(r.key, !mod->getSettingValue<bool>(r.key));
			return;
		}
		if (r.kind == 3) {
			std::string cur = mod->getSettingValue<std::string>(r.key);
			int idx = 0;
			for (int k = 0; k < 3; ++k) if (cur == kModes[k]) idx = k;
			mod->setSettingValue<std::string>(r.key, kModes[(idx + 1) % 3]);
			ru.value->setString(valueText(r).c_str());
			return;
		}
		// ползунок
		if (lx >= pn.trackX - 8.f) {
			m_sliderPanel = pi;
			m_sliderRow = ri;
			this->setFromNorm(ru, (lx - pn.trackX) / kTrackW);
		}
	}

	void setFromNorm(RowUI& ru, float n) {
		auto mod = Mod::get();
		auto const& r = *ru.row;
		n = std::clamp(n, 0.f, 1.f);
		double v = r.mn + n * (r.mx - r.mn);
		v = std::round((v - r.mn) / r.st) * r.st + r.mn;
		v = std::clamp(v, r.mn, r.mx);
		if (r.kind == 1) {
			mod->setSettingValue<int64_t>(r.key, static_cast<int64_t>(std::llround(v)));
		} else {
			mod->setSettingValue<double>(r.key, std::round(v * 10.0) / 10.0);
		}
		ru.value->setString(valueText(r).c_str());
	}

	void refreshAll() {
		for (auto& pn : m_panels) {
			for (auto& ru : pn.rows) {
				if (ru.value) ru.value->setString(valueText(*ru.row).c_str());
			}
		}
	}

	void finishTouch() {
		auto mod = Mod::get();
		if (m_drag >= 0) {
			auto& pn = m_panels[m_drag];
			if (!m_dragMoved) {
				pn.collapseT = pn.collapseT > 0.5f ? 0.f : 1.f;
				pn.chevron->setString(pn.collapseT > 0.5f ? "+" : "-");
				mod->setSavedValue<bool>(fmt::format("pnl3-c-{}", m_drag), pn.collapseT > 0.5f);
			} else {
				mod->setSavedValue<double>(fmt::format("pnl3-x-{}", m_drag), pn.x);
				mod->setSavedValue<double>(fmt::format("pnl3-y-{}", m_drag), pn.topY);
			}
		}
		m_drag = -1;
		m_sliderPanel = -1;
		m_sliderRow = -1;
	}
};

// открыть/закрыть меню (клавиша Tab или круглая кнопка на телефоне)
void toggleHackMenu() {
	if (HackOverlay::s_open) {
		HackOverlay::s_open->toggleMenu();
	} else if (auto scene = CCDirector::get()->getRunningScene()) {
		if (auto menu = HackOverlay::create()) scene->addChild(menu, 99999);
	}
}

ccColor3B menuAccent() {
	return ui::accent();
}

class $modify(MenuScheduler, CCScheduler) {
	void update(float dt) {
		static bool prevTab = false;
		bool now = tabKeyDown();
		if (now && !prevTab) toggleHackMenu();
		prevTab = now;

		// горячие клавиши на переключатели (пока меню закрыто)
		ensureBinds();
		static bool prevB[256] = {};
		bool nowB[256] = {};
		for (auto const& b : g_binds) {
			if (b.vk > 0 && b.vk < 256) nowB[b.vk] = !HackOverlay::s_open && vkDown(b.vk);
		}
		for (auto const& b : g_binds) {
			if (b.vk > 0 && b.vk < 256 && nowB[b.vk] && !prevB[b.vk]) {
				auto md = Mod::get();
				bool v = !md->getSettingValue<bool>(b.key);
				md->setSettingValue<bool>(b.key, v);
				Notification::create(fmt::format("{}: {}", b.label, v ? "ON" : "OFF"), NotificationIcon::None, 1.f)->show();
			}
		}
		for (int i = 0; i < 256; ++i) prevB[i] = nowB[i];

		// покадровый режим: F - пауза/пуск, G - один шаг
		static bool prevF = false, prevG = false;
		if (PlayLayer::get() && Mod::get()->getSettingValue<bool>("frame-step")) {
			bool f = letterKeyDown('F');
			bool g = letterKeyDown('G');
			if (f && !prevF) g_frozen = !g_frozen;
			if (g && !prevG) g_stepReq = true;
			prevF = f;
			prevG = g;
			if (g_frozen) {
				if (g_stepReq) {
					dt = 1.f / 240.f + 0.00001f;
					g_stepReq = false;
				} else {
					dt = 0.f;
				}
			}
		} else {
			g_frozen = false;
			g_stepReq = false;
		}

		// speedhack: масштаб времени планировщика (только внутри уровня)
		float scale = 1.f;
		if (PlayLayer::get() && !g_frozen) {
			scale = static_cast<float>(Mod::get()->getSettingValue<double>("speed"));
			if (Mod::get()->getSettingValue<bool>("slowmo-key") && shiftKeyDown()) {
				scale *= static_cast<float>(Mod::get()->getSettingValue<double>("slowmo"));
			}
		}
		this->setTimeScale(std::clamp(scale, 0.05f, 10.f));
		CCScheduler::update(dt);
	}
};
