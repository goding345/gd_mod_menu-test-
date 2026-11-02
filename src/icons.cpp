#include <Geode/Geode.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/GameStatsManager.hpp>

using namespace geode::prelude;

static bool iconHackOn() {
	return Mod::get()->getSettingValue<bool>("icon-hack");
}

// хак иконок: все иконки, цвета и прочее считаются открытыми
class $modify(IconHackGM, GameManager) {
	bool isIconUnlocked(int id, IconType type) {
		if (iconHackOn()) return true;
		return GameManager::isIconUnlocked(id, type);
	}

	bool isColorUnlocked(int id, UnlockType type) {
		if (iconHackOn()) return true;
		return GameManager::isColorUnlocked(id, type);
	}
};

class $modify(IconHackGSM, GameStatsManager) {
	bool isItemUnlocked(UnlockType type, int id) {
		if (iconHackOn()) return true;
		return GameStatsManager::isItemUnlocked(type, id);
	}
};
