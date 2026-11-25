#include <windows.h>
// Explorer desktop icon hooks are deliberately unavailable in the suite's
// borderless visualizer window. No old 32-bit helper DLL is loaded.
int setHook(HWND, HWND, int) { return 0; }
void removeHook() {}
void getItemData(int) {}
