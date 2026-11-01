#include <Geode/Geode.hpp>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>

// Tab нажат и окно игры в фокусе
bool tabKeyDown() {
	if (!(GetAsyncKeyState(VK_TAB) & 0x8000)) return false;
	DWORD pid = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &pid);
	return pid == GetCurrentProcessId();
}
#else
bool tabKeyDown() {
	return false;
}
#endif
