// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <fcntl.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach/mach.h>
#include <sys/resource.h>
#endif
#endif
namespace visualizers::testing {
inline nlohmann::json resourceMetrics(){
    uint64_t descriptors=0,resident=0;
#ifdef _WIN32
    DWORD count=0;if(!GetProcessHandleCount(GetCurrentProcess(),&count))throw std::runtime_error("Cannot count process handles");descriptors=count;
    PROCESS_MEMORY_COUNTERS memory{};memory.cb=sizeof(memory);if(!GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory)))throw std::runtime_error("Cannot query process memory");resident=memory.WorkingSetSize;
#elif defined(__APPLE__)
    const int maximum=getdtablesize();for(int fd=0;fd<maximum;++fd)if(fcntl(fd,F_GETFD)>=0)++descriptors;
    mach_task_basic_info_data_t memory{};mach_msg_type_number_t count=MACH_TASK_BASIC_INFO_COUNT;
    if(task_info(mach_task_self(),MACH_TASK_BASIC_INFO,reinterpret_cast<task_info_t>(&memory),&count)!=KERN_SUCCESS)throw std::runtime_error("Cannot query process memory");resident=memory.resident_size;
#else
    for(const auto& entry:std::filesystem::directory_iterator("/proc/self/fd")){(void)entry;++descriptors;}
    std::ifstream memory("/proc/self/statm");uint64_t total=0,pages=0;if(!(memory>>total>>pages))throw std::runtime_error("Cannot query process memory");resident=pages*uint64_t(sysconf(_SC_PAGESIZE));
#endif
    return {{"handles_or_fds",descriptors},{"resident_bytes",resident}};
}
}
