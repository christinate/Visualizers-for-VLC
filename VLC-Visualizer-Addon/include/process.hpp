#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>
namespace process {
inline std::wstring quote(const std::wstring& value){std::wstring out=L"\"";size_t slashes=0;for(wchar_t c:value){if(c==L'\\'){++slashes;continue;}if(c==L'\"')out.append(slashes*2+1,L'\\');else out.append(slashes,L'\\');slashes=0;out+=c;}out.append(slashes*2,L'\\');return out+L'\"';}
inline DWORD launch(const std::filesystem::path& exe,const std::vector<std::wstring>& args,bool wait=false){std::wstring command=quote(exe.wstring());for(const auto& arg:args)command+=L" "+quote(arg);STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION child{};if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,exe.parent_path().c_str(),&startup,&child))throw std::runtime_error("Could not launch process");CloseHandle(child.hThread);DWORD result=0;if(wait){WaitForSingleObject(child.hProcess,INFINITE);GetExitCodeProcess(child.hProcess,&result);}CloseHandle(child.hProcess);return result;}
}
