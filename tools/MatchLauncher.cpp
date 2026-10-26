#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include "../src/STARTcraft/src/bwapi/include/BWAPI/Client/GameTable.h"

#include "RuntimePaths.h"
static const std::wstring root = StarBench::ProjectRoot();
static HANDLE matchJob = nullptr;
static HWND statusBox = nullptr;
static std::wstring runDir, message;
static bool failed = false, finished = false, replaying = false;
static ULONGLONG startedAt = 0;
struct Side { std::wstring bot, botDir, ini; DWORD injectorPID = 0, gamePID = 0; bool botStarted = false; };
static Side sides[2];

static std::string narrow(const std::wstring& s) { return StarBench::Narrow(s); }
static std::string readFile(const std::wstring& path) { std::ifstream f(path); return std::string(std::istreambuf_iterator<char>(f), {}); }
static std::wstring widen(const std::string& s) { return StarBench::Widen(s); }
static std::string lastLine(const std::wstring& path) {
    std::ifstream f(path); std::string line, last;
    while (std::getline(f, line)) if (!line.empty()) last = line;
    return last;
}
static void StopMatch() {
    if (matchJob) { TerminateJobObject(matchJob, 0); CloseHandle(matchJob); matchJob = nullptr; }
}
using StarBench::EnvironmentOverride;
static bool Spawn(const std::wstring& exe, std::wstring command, const std::wstring& cwd,
                  const std::wstring& logPath, DWORD& pid) {
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE log = CreateFileW(logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log == INVALID_HANDLE_VALUE || input == INVALID_HANDLE_VALUE) {
        if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
        if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
        return false;
    }
    STARTUPINFOW si{}; si.cb = sizeof(si); si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = input; si.hStdOutput = log; si.hStdError = log;
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(exe.c_str(), &command[0], nullptr, nullptr, TRUE,
        CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, cwd.c_str(), &si, &pi);
    DWORD error = ok ? 0 : GetLastError();
    CloseHandle(log); CloseHandle(input);
    if (!ok) { SetLastError(error); return false; }
    if (!AssignProcessToJobObject(matchJob, pi.hProcess)) {
        error = GetLastError(); TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess); SetLastError(error); return false;
    }
    pid = pi.dwProcessId; ResumeThread(pi.hThread);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return true;
}
static DWORD FindGameChild(DWORD parent) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W p{}; p.dwSize = sizeof(p); DWORD result = 0;
    if (Process32FirstW(snapshot, &p)) do {
        if (p.th32ParentProcessID == parent && _wcsicmp(p.szExeFile, L"StarCraft.exe") == 0) {
            result = p.th32ProcessID; break;
        }
    } while (Process32NextW(snapshot, &p));
    CloseHandle(snapshot); return result;
}
static bool WriteIni(Side& side, bool host, int frameMs) {
    std::ofstream f(side.ini);
    if (!f) return false;
    f << "[ai]\nai=NULL\nai_dbg=NULL\n[auto_menu]\nauto_menu=LAN\n"
      << "character_name=" << narrow(side.bot) << "\nlan_mode=Local PC\nauto_restart=OFF\n"
      << "map=" << (host ? "maps/BroodWar/AIIDE/(2)Heartbreak Ridge.scx" : "") << "\n"
      << "game=StarterBot\nrace=Protoss\ngame_type=MELEE\n"
      << "wait_for_min_players=2\nwait_for_max_players=2\nwait_for_time=2000\n"
      << "save_replay=" << narrow(runDir) << "\\" << narrow(side.bot) << ".rep\n"
      << "[config]\nshared_memory=ON\nconsole_attach_on_startup=FALSE\nconsole_alloc_on_startup=FALSE\nconsole_attach_auto=FALSE\nconsole_alloc_auto=FALSE\n"
      << "[window]\nwindowed=ON\nleft=" << (host ? 10 : 685) << "\ntop=40\nwidth=640\nheight=480\n"
      << "[starcraft]\nsound=OFF\nseed_override=20261026\nspeed_override=" << frameMs << "\n";
    return bool(f);
}
static void Fail(const std::wstring& what) {
    failed = true; message = what + L" (Windows error " + std::to_wstring(GetLastError()) + L").";
    std::ofstream f(runDir + L"\\launcher-error.txt"); f << narrow(message); StopMatch();
}
static void StartMatch(int frameMs) {
    StopMatch(); failed = false; finished = false; replaying = false;
    SYSTEMTIME now; GetLocalTime(&now); wchar_t stamp[80];
    swprintf_s(stamp, L"%04u%02u%02u-%02u%02u%02u-%03u", now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds);
    CreateDirectoryW((root + L"matches").c_str(), nullptr);
    runDir = root + L"matches\\" + stamp;
    if (!CreateDirectoryW(runDir.c_str(), nullptr)) { message = L"Could not create match directory."; failed = true; return; }
    std::ofstream(root + L"matches\\latest.txt") << narrow(stamp);
    sides[0] = {L"StarterBot", root + L"src\\STARTcraft\\bin", runDir + L"\\StarterBot.ini"};
    sides[1] = {L"UAlbertaBot", root + L"src\\UAlbertaBot\\UAlbertaBot\\bin", runDir + L"\\UAlbertaBot.ini"};
    matchJob = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!matchJob || !SetInformationJobObject(matchJob, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) { Fail(L"Could not create match process group"); return; }
    // First create both configs, then launch the host. Guest starts after host BWAPI is connected.
    for (int i = 0; i < 2; ++i) {
        if (!WriteIni(sides[i], i == 0, frameMs)) { Fail(L"Could not write match configuration"); return; }
    }
    EnvironmentOverride config(L"BWAPI_CONFIG_INI", sides[0].ini);
    std::wstring gameDir = root + L"StarCraft1161";
    std::wstring exe = gameDir + L"\\injectory.x86.exe";
    if (!Spawn(exe, L"\"" + exe + L"\" --launch StarCraft.exe --inject bwapi-data\\BWAPI.dll --wait-for-exit",
               gameDir, runDir + L"\\StarterBot-game-launch.log", sides[0].injectorPID)) {
        Fail(L"Could not launch host game"); return;
    }
    startedAt = GetTickCount64(); message = L"Starting two Local PC game instances...";
}
static void StartReplay() {
    if (runDir.empty()) {
        std::wstring saved = widen(readFile(root + L"matches\\latest.txt"));
        while (!saved.empty() && (saved.back() == L'\n' || saved.back() == L'\r')) saved.pop_back();
        const auto separator = saved.find_last_of(L"\\/");
        if (separator != std::wstring::npos) saved = saved.substr(separator + 1);
        if (saved.empty()) { message = L"Finish a match before opening a replay."; return; }
        runDir = root + L"matches\\" + saved;
    }
    WIN32_FIND_DATAW found{};
    HANDLE search = FindFirstFileW((runDir + L"\\StarterBot*.rep").c_str(), &found);
    if (search == INVALID_HANDLE_VALUE) { message = L"No completed replay found. Finish a match first."; return; }
    FindClose(search);
    std::wstring replay = runDir + L"\\" + found.cFileName;
    StopMatch(); replaying = true; failed = false; finished = false;
    matchJob = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!matchJob || !SetInformationJobObject(matchJob, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) { Fail(L"Could not create replay process group"); return; }
    std::wstring ini = runDir + L"\\replay.ini";
    {
        std::ofstream f(ini);
        f << "[ai]\nai=NULL\nai_dbg=NULL\n[auto_menu]\nauto_menu=SINGLE_PLAYER\ncharacter_name=ReplayViewer\nauto_restart=OFF\nmap="
          << "..\\matches\\" << narrow(runDir.substr((root + L"matches\\").size())) << "\\" << narrow(found.cFileName) << "\nsave_replay=\n[config]\nshared_memory=OFF\nconsole_attach_auto=FALSE\nconsole_alloc_auto=FALSE\n"
          << "[window]\nwindowed=ON\nleft=20\ntop=40\nwidth=640\nheight=480\n[starcraft]\nsound=OFF\nspeed_override=42\n";
    }
    EnvironmentOverride config(L"BWAPI_CONFIG_INI", ini);
    std::wstring gameDir = root + L"StarCraft1161", exe = gameDir + L"\\injectory.x86.exe"; DWORD pid;
    if (!Spawn(exe, L"\"" + exe + L"\" --launch StarCraft.exe --inject bwapi-data\\BWAPI.dll --wait-for-exit", gameDir, runDir + L"\\replay-launch.log", pid)) { Fail(L"Could not start replay"); return; }
    message = L"REPLAY VIEWER - recorded match, normal speed. No bots are issuing commands.";
}
static void Refresh() {
    if (matchJob && !replaying && !failed && !sides[1].injectorPID && sides[0].botStarted) {
        bool hostConnected = false;
        HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, L"Local\\bwapi_shared_memory_game_list");
        if (mapping) {
            auto table = static_cast<const BWAPI::GameTable*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(BWAPI::GameTable)));
            if (table) {
                for (const auto& instance : table->gameInstances)
                    if (instance.serverProcessID == sides[0].gamePID && instance.isConnected) hostConnected = true;
                UnmapViewOfFile(table);
            }
            CloseHandle(mapping);
        }
        if (hostConnected) {
            EnvironmentOverride config(L"BWAPI_CONFIG_INI", sides[1].ini);
            std::wstring gameDir = root + L"StarCraft1161", exe = gameDir + L"\\injectory.x86.exe";
            if (!Spawn(exe, L"\"" + exe + L"\" --launch StarCraft.exe --inject bwapi-data\\BWAPI.dll --wait-for-exit",
                       gameDir, runDir + L"\\UAlbertaBot-game-launch.log", sides[1].injectorPID)) Fail(L"Could not launch guest game");
        }
    }
    if (matchJob && !replaying && !failed && !finished) {
        for (auto& side : sides) if (!side.botStarted) {
            if (!side.gamePID) side.gamePID = FindGameChild(side.injectorPID);
            if (side.gamePID) {
                EnvironmentOverride run(L"STARBENCH_RUN_DIR", runDir);
                EnvironmentOverride pid(L"STARBENCH_SERVER_PID", std::to_wstring(side.gamePID));
                std::wstring exe = side.botDir + L"\\" + side.bot + L".exe"; DWORD botPID;
                if (!Spawn(exe, L"\"" + exe + L"\"", side.botDir, runDir + L"\\" + side.bot + L"-process.log", botPID)) { Fail(L"Could not launch " + side.bot); break; }
                side.botStarted = true;
                std::ofstream binding(runDir + L"\\" + side.bot + L"-binding.json");
                binding << "{\"game_pid\":" << side.gamePID << ",\"bot_pid\":" << botPID << ",\"injector_pid\":" << side.injectorPID << "}\n";
            }
        }
        if (!failed && GetTickCount64() - startedAt > 45000 && (!sides[0].botStarted || !sides[1].botStarted)) Fail(L"Timed out finding the launched game processes");
        if (!failed && sides[0].botStarted && sides[1].botStarted) message = L"Both bots bound to their own game processes. Waiting for / playing Local PC match.";
        std::string a = readFile(runDir + L"\\StarterBot-result.json"), b = readFile(runDir + L"\\UAlbertaBot-result.json");
        if (!a.empty() && !b.empty()) {
            const bool aw = a.find("\"winner\": true") != std::string::npos;
            const bool bw = b.find("\"winner\": true") != std::string::npos;
            if (aw != bw) { finished = true; message = L"MATCH COMPLETE. Winner: " + std::wstring(aw ? L"StarterBot" : L"UAlbertaBot"); }
            else { message = L"Both games ended; inspect result files (no unique winner)."; }
        }
    }
    std::wstring text = message + L"\r\n\r\n";
    if (replaying) { SetWindowTextW(statusBox, (text + L"Replay and original CSV/event/result files:\r\n" + runDir + L"\r\n\r\nThe recorded game is playing at normal speed. Camera controls do not alter the recorded match.").c_str()); return; }
    if (!runDir.empty()) {
        text += L"frame, minerals, gas, supply_used, supply_total, workers, combat_units, buildings\r\n\r\n";
        for (const auto& side : sides) {
            text += side.bot + L"  [game PID " + std::to_wstring(side.gamePID) + L"]\r\n";
            std::string row = lastLine(runDir + L"\\" + side.bot + L"-telemetry.csv");
            text += row.empty() ? L"Waiting for telemetry...\r\n\r\n" : widen(row) + L"\r\n\r\n";
        }
        text += L"Match files:\r\n" + runDir;
        text += L"\r\n\r\nEach side: CSV samples + unit events + start/result JSON + .rep replay.\r\nUnit input disabled. Camera viewing is allowed.\r\nClosing this launcher or Stop ends only its own match processes.";
    }
    SetWindowTextW(statusBox, text.c_str());
}
static LRESULT CALLBACK WindowProcedure(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        CreateWindowW(L"STATIC", L"StarterBot vs UAlbertaBot - Local PC / BWAPI 4.4.0", WS_CHILD | WS_VISIBLE, 15, 12, 690, 24, window, nullptr, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Start match (normal)", WS_CHILD | WS_VISIBLE | WS_TABSTOP,15,45,210,32,window,(HMENU)100,nullptr,nullptr);
        CreateWindowW(L"BUTTON", L"Start match (fast)", WS_CHILD | WS_VISIBLE | WS_TABSTOP,235,45,210,32,window,(HMENU)101,nullptr,nullptr);
        CreateWindowW(L"BUTTON", L"Stop match", WS_CHILD | WS_VISIBLE | WS_TABSTOP,455,45,210,32,window,(HMENU)102,nullptr,nullptr);
        CreateWindowW(L"BUTTON",L"Replay last completed match",WS_CHILD|WS_VISIBLE|WS_TABSTOP,15,85,330,32,window,(HMENU)104,nullptr,nullptr);
        statusBox=CreateWindowW(L"EDIT",L"Ready. Choose normal or fast speed. No Battle.net account required.",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_MULTILINE|ES_READONLY|WS_VSCROLL,15,130,680,330,window,(HMENU)103,nullptr,nullptr);
        message=L"Ready. Choose normal (42 ms/frame) or fast (10 ms/frame).";
        SetTimer(window,1,1000,nullptr); return 0;
    case WM_COMMAND:
        if(LOWORD(wp)==100)StartMatch(42);
        if(LOWORD(wp)==101)StartMatch(10);
        if(LOWORD(wp)==104)StartReplay();
        if(LOWORD(wp)==102){StopMatch();message=L"Match stopped. Existing files retained (stopping early is not a completed game).";}
        Refresh();return 0;
    case WM_TIMER:Refresh();return 0;
    case WM_DESTROY:StopMatch();PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(window,msg,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
    WNDCLASSW wc{};wc.lpfnWndProc=WindowProcedure;wc.hInstance=instance;wc.lpszClassName=L"StarBenchMatchLauncher";
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassW(&wc);
    HWND w=CreateWindowW(wc.lpszClassName,L"StarCraft AI - Bot vs Bot",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,300,520,730,510,nullptr,nullptr,instance,nullptr);
    if(!w)return 1;ShowWindow(w,show);MSG m{};while(GetMessageW(&m,nullptr,0,0)){TranslateMessage(&m);DispatchMessageW(&m);}return 0;
}