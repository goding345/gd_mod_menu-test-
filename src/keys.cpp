#include <Geode/Geode.hpp>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>

static bool keyDownInGame(int vk) {
	if (!(GetAsyncKeyState(vk) & 0x8000)) return false;
	DWORD pid = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &pid);
	return pid == GetCurrentProcessId();
}

bool tabKeyDown() {
	return keyDownInGame(VK_TAB);
}

bool restartKeyDown() {
	return keyDownInGame('R');
}

bool shiftKeyDown() {
	return keyDownInGame(VK_SHIFT);
}

bool letterKeyDown(char c) {
	return keyDownInGame(static_cast<int>(c));
}
#else
bool tabKeyDown() {
	return false;
}

bool restartKeyDown() {
	return false;
}

bool shiftKeyDown() {
	return false;
}

bool letterKeyDown(char) {
	return false;
}
#endif
