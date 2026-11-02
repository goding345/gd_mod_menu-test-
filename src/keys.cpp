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
#else
bool tabKeyDown() {
	return false;
}

bool restartKeyDown() {
	return false;
}
#endif
