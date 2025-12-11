#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <vector>
#include <set>
#include <iomanip>

#pragma comment(lib, "psapi.lib")

void GetAllChildProcesses(DWORD parentPid, std::set<DWORD> &pids)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return;

    PROCESSENTRY32W pe = {sizeof(PROCESSENTRY32W)};

    if (Process32FirstW(snapshot, &pe))
    {
        do
        {
            if (pe.th32ParentProcessID == parentPid)
            {
                pids.insert(pe.th32ProcessID);
                GetAllChildProcesses(pe.th32ProcessID, pids); // Recursive
            }
        } while (Process32NextW(snapshot, &pe));
    }

    CloseHandle(snapshot);
}

DWORD FindProcessByName(const wchar_t *processName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W pe = {sizeof(PROCESSENTRY32W)};
    DWORD pid = 0;

    if (Process32FirstW(snapshot, &pe))
    {
        do
        {
            if (_wcsicmp(pe.szExeFile, processName) == 0)
            {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &pe));
    }

    CloseHandle(snapshot);
    return pid;
}

SIZE_T GetProcessMemory(DWORD pid)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess)
        return 0;

    PROCESS_MEMORY_COUNTERS pmc;
    SIZE_T memSize = 0;

    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
    {
        memSize = pmc.WorkingSetSize;
    }

    CloseHandle(hProcess);
    return memSize;
}

int main()
{
    std::cout << std::fixed << std::setprecision(2);

    while (true)
    {
        DWORD rootPid = FindProcessByName(L"hbuild.exe");

        if (rootPid == 0)
        {
            std::cout << "hbuild.exe not found\n";
            Sleep(10);
            continue;
        }

        // Collect all PIDs (root + children)
        std::set<DWORD> allPids;
        allPids.insert(rootPid);
        GetAllChildProcesses(rootPid, allPids);

        // Sum up memory
        SIZE_T totalMemory = 0;
        for (DWORD pid : allPids)
        {
            totalMemory += GetProcessMemory(pid);
        }

        // Get current time
        SYSTEMTIME st;
        GetLocalTime(&st);

        double memoryMB = totalMemory / (1024.0 * 1024.0);

        std::cout << std::setfill('0')
            << std::setw(2) << st.wHour << ":"
            << std::setw(2) << st.wMinute << ":"
            << std::setw(2) << st.wSecond << "."
            << std::setw(3) << st.wMilliseconds
            << ": " << memoryMB << " MB\n";

        Sleep(10); // 10ms
    }

    return 0;
}