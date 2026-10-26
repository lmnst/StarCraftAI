#define UNICODE
#define _UNICODE
#include <windows.h>
#include <string>
#include <fstream>
#include <sstream>

static HANDLE demoJob = nullptr;
static HWND statusBox = nullptr;
static std::wstring selectedBot;
// Locate the project relative to this launcher's tools directory.
static std::wstring FindProjectRoot() {
    wchar_t path[32768]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, 32768);
    std::wstring result(path, length);
    const auto fileSeparator = result.find_last_of(L"\\/");
    result.resize(fileSeparator);
    const auto toolsSeparator = result.find_last_of(L"\\/");
    return result.substr(0, toolsSeparator + 1);
}
static const std::wstring root = FindProjectRoot();

static void StopDemo() {
    if (demoJob) { TerminateJobObject(demoJob, 0); CloseHandle(demoJob); demoJob = nullptr; }
}

static bool StartOwnedProcess(const std::wstring& exe, std::wstring command,
                              const std::wstring& cwd, const std::wstring& logPath) {
    SECURITY_ATTRIBUTES security{ sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    HANDLE log = CreateFileW(logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log == INVALID_HANDLE_VALUE || input == INVALID_HANDLE_VALUE) {
        if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
        if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
        return false;
    }
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input; startup.hStdOutput = log; startup.hStdError = log;
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(exe.c_str(), &command[0], nullptr, nullptr, TRUE,
        CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, cwd.c_str(), &startup, &process);
    const DWORD creationError = created ? ERROR_SUCCESS : GetLastError();
    CloseHandle(log); CloseHandle(input);
    if (!created) { SetLastError(creationError); return false; }
    if (!AssignProcessToJobObject(demoJob, process.hProcess)) {
        const DWORD jobError = GetLastError(); TerminateProcess(process.hProcess, 1);
        CloseHandle(process.hThread); CloseHandle(process.hProcess); SetLastError(jobError); return false;
    }
    ResumeThread(process.hThread);
    CloseHandle(process.hThread); CloseHandle(process.hProcess); return true;
}

static void StartDemo(bool useUab) {
    StopDemo();
    selectedBot = useUab ? L"UAlbertaBot" : L"StarterBot";
    demoJob = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!demoJob || !SetInformationJobObject(demoJob, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        SetWindowTextW(statusBox, L"Could not create the local demo process group."); return;
    }
    const std::wstring botDirectory = std::wstring(root) + (useUab ? L"src\\UAlbertaBot\\UAlbertaBot\\bin" : L"src\\STARTcraft\\bin");
    const std::wstring botExe = botDirectory + L"\\" + selectedBot + L".exe";
    if (!StartOwnedProcess(botExe, L"\"" + botExe + L"\"", botDirectory,
                           std::wstring(root) + L"logs\\" + selectedBot + L"-process.log")) {
        const auto error = std::to_wstring(GetLastError()); StopDemo();
        SetWindowTextW(statusBox, (L"Bot could not start. Windows error: " + error).c_str()); return;
    }
    const std::wstring gameDirectory = std::wstring(root) + L"StarCraft1161";
    const std::wstring injectorExe = gameDirectory + L"\\injectory.x86.exe";
    const std::wstring injectorCommand = L"\"" + injectorExe + L"\" --launch StarCraft.exe --inject bwapi-data\\BWAPI.dll --wait-for-exit";
    if (!StartOwnedProcess(injectorExe, injectorCommand, gameDirectory,
                           std::wstring(root) + L"logs\\game-launch.log")) {
        const auto error = std::to_wstring(GetLastError()); StopDemo();
        SetWindowTextW(statusBox, (L"Game could not start. Windows error: " + error).c_str()); return;
    }
    SetWindowTextW(statusBox, (selectedBot + L" started. Waiting for game telemetry...").c_str());
}

static void RefreshTelemetry() {
    if (!demoJob || selectedBot.empty()) return;
    std::wstring path = std::wstring(root) + L"logs\\" + selectedBot + L"-telemetry.csv";
    std::ifstream stream(path.c_str());
    if (!stream) return;
    std::string header, line, last;
    std::getline(stream, header);
    while (std::getline(stream, line)) if (!line.empty()) last = line;
    if (last.empty()) return;
    std::wstring text = selectedBot + L" - live game state\r\n\r\n";
    text += std::wstring(header.begin(), header.end()) + L"\r\n";
    text += std::wstring(last.begin(), last.end());
    text += L"\r\n\r\nCSV and process logs: " + root + L"logs\r\nClose this launcher or click Stop to stop only this demo.";
    SetWindowTextW(statusBox, text.c_str());
}

static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        CreateWindowW(L"STATIC", L"Local StarCraft 1.16.1 + BWAPI 4.4.0", WS_CHILD | WS_VISIBLE,
                      15, 15, 550, 24, window, nullptr, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Run StarterBot", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                      15, 48, 175, 32, window, (HMENU)100, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Run UAlbertaBot", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                      200, 48, 175, 32, window, (HMENU)101, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Stop demo", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                      385, 48, 175, 32, window, (HMENU)102, nullptr, nullptr);
        statusBox = CreateWindowW(L"EDIT", L"Ready. Choose one bot. No Battle.net account is required.",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
            15, 95, 545, 190, window, (HMENU)103, nullptr, nullptr);
        SetTimer(window, 1, 1000, nullptr); return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == 100) StartDemo(false);
        if (LOWORD(wParam) == 101) StartDemo(true);
        if (LOWORD(wParam) == 102) { StopDemo(); SetWindowTextW(statusBox, L"Demo stopped."); }
        return 0;
    case WM_TIMER: RefreshTelemetry(); return 0;
    case WM_DESTROY: StopDemo(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    WNDCLASSW windowClass{}; windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance; windowClass.lpszClassName = L"StarBenchLocalLauncher";
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&windowClass);
    HWND window = CreateWindowW(windowClass.lpszClassName, L"StarCraft AI - Local Demo",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, 650, 60, 595, 335,
        nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0)) { TranslateMessage(&message); DispatchMessageW(&message); }
    return 0;
}
