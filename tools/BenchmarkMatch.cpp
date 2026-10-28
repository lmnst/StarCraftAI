// Use exactly the same process ownership, PID binding and match completion as the GUI.
#define STARBENCH_CONSOLE
#include "MatchLauncher.cpp"
#include <iostream>

static bool ReplaySaved(const wchar_t* bot) {
    WIN32_FIND_DATAW entry{};
    HANDLE search = FindFirstFileW((runDir + L"\\" + bot + L"*.rep").c_str(), &entry);
    if (search == INVALID_HANDLE_VALUE) return false;
    FindClose(search);
    return entry.nFileSizeHigh || entry.nFileSizeLow > 512;
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 5) {
        std::cerr << "Usage: BenchmarkMatch.exe candidate-bin seed timeout-seconds receipt.json\n";
        return 2;
    }
    const auto timeout = wcstoul(argv[3], nullptr, 10);
    if (!timeout) return 2;
    const auto began = GetTickCount64();
    // Skip the results screen so BWAPI's automatic replay save can complete.
    StartMatch(10, argv[1], wcstoul(argv[2], nullptr, 10), true);
    while (!failed && !finished && GetTickCount64() - began < timeout * 1000ULL) {
        Refresh();
        Sleep(250);
    }
    // MatchEnd can precede the engine's replay write. Do not kill the game first.
    const auto ended = GetTickCount64();
    while (finished && !(ReplaySaved(L"StarterBot") && ReplaySaved(L"UAlbertaBot"))
           && GetTickCount64() - ended < 30000) Sleep(250);
    const bool complete = finished && ReplaySaved(L"StarterBot") && ReplaySaved(L"UAlbertaBot");
    std::ofstream receipt(argv[4]);
    const auto escape = [](const std::wstring& value) {
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), nullptr, 0, nullptr, nullptr);
        std::string utf8(bytes, '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), &utf8[0], bytes, nullptr, nullptr);
        std::string result;
        for (const auto ch : utf8) { if (ch == '\\' || ch == '"') result += '\\'; result += ch; }
        return result;
    };
    receipt << "{\"status\":\"" << (complete ? "complete" : failed ? "failed" : "timeout_or_missing_replay")
            << "\",\"match_directory\":\"" << escape(runDir)
            << "\",\"elapsed_seconds\":" << (GetTickCount64() - began) / 1000.0 << "}\n";
    receipt.close();
    StopMatch();
    std::cout << narrow(message) << '\n';
    return complete ? 0 : 1;
}
