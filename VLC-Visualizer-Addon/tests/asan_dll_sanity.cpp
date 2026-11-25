#ifdef SANITY_DLL
#include <windows.h>
static wchar_t* correctArray=new wchar_t[1];
extern "C" __declspec(dllexport) void FreeCorrectArray(){delete[] correctArray;correctArray=nullptr;}
#else
#include <windows.h>
int wmain(int argc,wchar_t** argv){if(argc!=2)return 2;auto dll=LoadLibraryW(argv[1]);if(!dll)return 3;auto release=(void(*)())GetProcAddress(dll,"FreeCorrectArray");if(!release)return 4;release();FreeLibrary(dll);return 0;}
#endif
