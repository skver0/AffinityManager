#define WINVER 0x0501  // Windows XP
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <chrono>
#include <algorithm> 
#include <cctype>
#include <locale>
#include <iomanip>

const char* CONFIG_FILE = "config.ini";
const char* LOG_FILE = "affinity.log";
const UINT WM_TRAYICON = WM_USER + 1;

NOTIFYICONDATA nid;  // tray
std::map<std::string, DWORD_PTR> processList;
bool running = true;

// trim from start (in place)
inline void ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
        }));
}

// trim from end (in place)
inline void rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
        }).base(), s.end());
}

void log(const std::string& message) {
    std::ofstream log(LOG_FILE, std::ios::app);
    if (log.is_open()) {
        SYSTEMTIME st;
        GetLocalTime(&st);

        log << "["
            << st.wYear << "-"
            << std::setw(2) << std::setfill('0') << st.wMonth << "-"
            << std::setw(2) << std::setfill('0') << st.wDay << " "
            << std::setw(2) << std::setfill('0') << st.wHour << ":"
            << std::setw(2) << std::setfill('0') << st.wMinute << ":"
            << std::setw(2) << std::setfill('0') << st.wSecond << "] "
            << message << std::endl;
    }
#ifdef _DEBUG
    std::cout << message << std::endl; // output to console too
#endif
}

DWORD_PTR convert_to_affinity_mask(const std::string& coreList) {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    DWORD numCPUs = sysInfo.dwNumberOfProcessors;

    DWORD_PTR mask = 0;
    std::stringstream ss(coreList);
    std::string core;
    while (std::getline(ss, core, ',')) {
        unsigned int coreNum = static_cast<unsigned int>(std::stoi(core));
        if (coreNum < numCPUs) {
            mask |= (1ULL << coreNum);
        }
        else {
            log("warning: invalid core number " + core + " (out of range, max " + std::to_string(numCPUs - 1) + ")");
        }
    }
    return mask;
}

void load_config() {
    std::ifstream config(CONFIG_FILE);
    if (!config.is_open()) {
        log("error: unable to open config file (config.ini)");
        return;
    }

    processList.clear();
    std::string line;
    while (std::getline(config, line)) {
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos || line.empty() || line[0] == ';')
            continue;

        std::string processPath = line.substr(0, eqPos);
        std::string coreList = line.substr(eqPos + 1);

        rtrim(processPath);

        DWORD_PTR cpuMask = convert_to_affinity_mask(coreList);
        if (cpuMask > 0) {
            processList[processPath] = cpuMask;
            log("loaded config for: " + processPath + " with mask: " + std::to_string(cpuMask));
        }
    }

    log("config loaded");
}

std::string get_process_path(DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
    if (!hProcess) return "";

    char path[MAX_PATH];
    if (GetModuleFileNameExA(hProcess, nullptr, path, MAX_PATH)) {
        CloseHandle(hProcess);
        return std::string(path);
    }

    CloseHandle(hProcess);
    return "";
}

std::string get_process_name(const std::string& path) {
    size_t pos = path.find_last_of("\\/");
    return (pos != std::string::npos) ? path.substr(pos + 1) : path;
}

std::string to_lower(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
    return lowerStr;
}

void apply_affinity() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        log("error: failed to take process snapshot");
        return;
    }

    PROCESSENTRY32 entry = { sizeof(PROCESSENTRY32) };

    if (Process32First(snapshot, &entry)) {
        do {
            std::string processPath = to_lower(get_process_path(entry.th32ProcessID));
            std::string processName = to_lower(get_process_name(processPath));

            for (const auto& pair : processList) {
             //   log("pair.first: " + pair.first + " procname: " + processName + " proccompare: " + std::to_string(pair.first.compare(processName)) + " | pathname: " + processPath + " pathcompare: " + std::to_string(pair.first.compare(processPath)));
                if (!to_lower(pair.first).compare(processName) || !to_lower(pair.first).compare(processPath)) {
                    DWORD_PTR desiredAffinity = pair.second;
                    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_INFORMATION, FALSE, entry.th32ProcessID);

                    if (hProcess) {
                        DWORD_PTR processAffinity, systemAffinity;
                        if (GetProcessAffinityMask(hProcess, &processAffinity, &systemAffinity)) {
                            if (processAffinity == desiredAffinity) {
                                // already set, skip
                                CloseHandle(hProcess);
                                continue;
                            }
                        }

                        if (SetProcessAffinityMask(hProcess, desiredAffinity)) {
                            log("affinity set for " + processName + " (PID " + std::to_string(entry.th32ProcessID) + ")");
                        }
                        else {
                            log("failed to set affinity for " + processName + " (PID " + std::to_string(entry.th32ProcessID) + "), error: " + std::to_string(GetLastError()));
                        }
                        CloseHandle(hProcess);
                    }
                    else {
                        log("failed to open process " + std::to_string(entry.th32ProcessID) + " for setting affinity");
                    }
                    break;
                }
            }
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
}


void monitor_processes() {
    while (running) {
        apply_affinity();
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_TRAYICON) {
        if (lParam == WM_RBUTTONDOWN || lParam == WM_LBUTTONDOWN) {
            HMENU hMenu = CreatePopupMenu();

            AppendMenu(hMenu, MF_BYPOSITION | MF_STRING, 1, "Affinity Manager");

            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

            AppendMenu(hMenu, MF_STRING, 2, "reload config");
            AppendMenu(hMenu, MF_STRING, 3, "exit");

            POINT p;
            GetCursorPos(&p);
            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTBUTTON, p.x, p.y, 0, hwnd, NULL);

            if (cmd == 1) {
                MessageBox(hwnd, "thanks to everyone @ the w2k dev community discord", "uwu", MB_OK | MB_ICONINFORMATION);
            }
            else if (cmd == 2) {
                load_config();
            }
            else if (cmd == 3) {
                running = false;
                PostQuitMessage(0);
            }

            DestroyMenu(hMenu);
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


void create_tray_icon(HWND hwnd) {
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    strcpy_s(nid.szTip, "CPU Affinity Manager");

    Shell_NotifyIcon(NIM_ADD, &nid);
    log("tray added");
}

void run_tray() {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "AffinityManagerClass";

    RegisterClass(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, "Affinity Manager", 0, 0, 0, 0, 0, NULL, NULL, wc.hInstance, NULL);
    create_tray_icon(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) && running) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Shell_NotifyIcon(NIM_DELETE, &nid);
    log("exiting tray");
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
#ifdef _DEBUG
    AllocConsole();
    FILE* fp;

    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);

    log("debug console attached!");
#endif


    HANDLE hMutex = CreateMutex(NULL, TRUE, "AffinityManagerMutex");
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBox(NULL, "Affinity Manager is already running.", "Warning", MB_OK | MB_ICONWARNING);
        CloseHandle(hMutex);
        return 1;
    }

    std::remove(LOG_FILE);

    log("affinity manager started");
    load_config();

    std::thread monitorThread(monitor_processes);
    run_tray();

    monitorThread.join();
    return 0;
}