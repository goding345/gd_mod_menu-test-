#pragma once
#include <Geode/Geode.hpp>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

// Свои визуалы пользователя (частицы), которые он создаёт в меню кнопкой "+"
struct CustomEffect {
	std::string name = "My visual";
	int shape = 0;   // 0 квадрат, 1 ромб, 2 штрих, 3 своя картинка
	int count = 5;   // 1..10
	int size = 6;    // 1..20
	int speed = 5;   // 1..10
	int spin = 3;    // 0..10
	int sway = 3;    // 0..10
	int mode = 0;    // 0 падают, 1 поднимаются, 2 плавают
	int color = 0;   // 0..99 оттенок, 100 радуга, 101 цвет фона
	bool on = true;
};

inline std::vector<CustomEffect>& customEffects() {
	static std::vector<CustomEffect> v;
	return v;
}
inline int& customEffectsVersion() {
	static int v = 0;
	return v;
}
// открыто ли окно редактора (меню в это время не должно перехватывать касания)
inline bool& modalOpen() {
	static bool v = false;
	return v;
}

inline std::string cleanName(std::string s) {
	for (auto& c : s) if (c == '\t' || c == '\n' || c == '\r') c = ' ';
	return s;
}

inline void saveEffects() {
	std::ostringstream o;
	for (auto const& e : customEffects()) {
		o << cleanName(e.name) << '\t' << e.shape << '\t' << e.count << '\t' << e.size << '\t' << e.speed << '\t'
		  << e.spin << '\t' << e.sway << '\t' << e.mode << '\t' << e.color << '\t' << (e.on ? 1 : 0) << '\n';
	}
	geode::Mod::get()->setSavedValue<std::string>("custom-effects-v1", o.str());
	++customEffectsVersion();
}

inline void loadEffects() {
	static bool done = false;
	if (done) return;
	done = true;
	std::string s = geode::Mod::get()->getSavedValue<std::string>("custom-effects-v1", "");
	std::istringstream in(s);
	std::string line;
	while (std::getline(in, line)) {
		std::vector<std::string> f;
		std::string cur;
		for (char c : line) {
			if (c == '\t') { f.push_back(cur); cur.clear(); }
			else cur += c;
		}
		f.push_back(cur);
		if (f.size() < 10) continue;
		CustomEffect e;
		e.name = f[0];
		e.shape = std::atoi(f[1].c_str());
		e.count = std::atoi(f[2].c_str());
		e.size = std::atoi(f[3].c_str());
		e.speed = std::atoi(f[4].c_str());
		e.spin = std::atoi(f[5].c_str());
		e.sway = std::atoi(f[6].c_str());
		e.mode = std::atoi(f[7].c_str());
		e.color = std::atoi(f[8].c_str());
		e.on = std::atoi(f[9].c_str()) != 0;
		customEffects().push_back(e);
	}
	++customEffectsVersion();
}
