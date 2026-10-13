#include "../NanaZip.Universal/SevenZip/CPP/7zip/UI/Common/PasswordHistory.h"
#include <cassert>
#include <fstream>
#include <iostream>

using NanaZipPasswordHistory::Store;
using NanaZipPasswordHistory::List;

int wmain(int Count, wchar_t** Arguments)
{
    assert(Count >= 2);
    std::wstring Path = Arguments[1];
    Store History(Path);
    if (Count == 3)
    {
        for (int Index = 0; Index < 8; ++Index)
        {
            std::wstring Password = std::wstring(Arguments[2])
                + std::to_wstring(Index);
            assert(SUCCEEDED(History.Remember(Password.c_str())));
        }
        return 0;
    }
    assert(SUCCEEDED(History.SetEnabled(false)));
    List Values;
    assert(SUCCEEDED(History.Load(Values)) && !Values.Enabled);
    assert(SUCCEEDED(History.Remember(L"ignored")));
    assert(!History.IsEnabled());
    assert(SUCCEEDED(History.SetEnabled(true)));
    assert(SUCCEEDED(History.Load(Values)) && Values.Enabled);
    assert(Values.Passwords.empty());
    assert(SUCCEEDED(History.Remember(L"first")));
    assert(SUCCEEDED(History.Remember(L"\x5bc6\x7801\"\\\n")));
    assert(SUCCEEDED(History.Remember(L"First")));
    assert(SUCCEEDED(History.Remember(L"first")));
    assert(SUCCEEDED(History.Load(Values)));
    assert(Values.Passwords.size() == 3);
    assert(Values.Passwords[0] == L"first");
    assert(Values.Passwords[1] == L"First");
    assert(Values.Passwords[2] == L"\x5bc6\x7801\"\\\n");
    {
        std::ifstream File(Path, std::ios::binary);
        std::string Json((std::istreambuf_iterator<char>(File)), {});
        assert(Json.find("first") == std::string::npos);
        assert(Json.find("protectedPasswords") != std::string::npos);
        assert(Json.find('\0') == std::string::npos);
    }
    for (int Index = 0; Index < 20; ++Index)
    {
        std::wstring Password = L"limit-" + std::to_wstring(Index);
        assert(SUCCEEDED(History.Remember(Password.c_str())));
    }
    assert(SUCCEEDED(History.Load(Values)));
    assert(Values.Passwords.size() == 16);
    assert(Values.Passwords.front() == L"limit-19");
    assert(Values.Passwords.back() == L"limit-4");

    // Independent processes merge updates rather than overwriting snapshots.
    assert(SUCCEEDED(History.SetEnabled(false)));
    assert(SUCCEEDED(History.SetEnabled(true)));
    wchar_t Executable[MAX_PATH];
    assert(::GetModuleFileNameW(nullptr, Executable, MAX_PATH));
    PROCESS_INFORMATION Processes[2] = {};
    for (int Index = 0; Index < 2; ++Index)
    {
        std::wstring Command = L"\"" + std::wstring(Executable)
            + L"\" \"" + Path + L"\" worker-" + std::to_wstring(Index);
        STARTUPINFOW Startup = {};
        Startup.cb = sizeof(Startup);
        assert(::CreateProcessW(nullptr, &Command[0], nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, nullptr, &Startup, &Processes[Index]));
    }
    for (PROCESS_INFORMATION& Process : Processes)
    {
        assert(::WaitForSingleObject(Process.hProcess, 30000) == WAIT_OBJECT_0);
        DWORD Result = 1;
        assert(::GetExitCodeProcess(Process.hProcess, &Result) && Result == 0);
        ::CloseHandle(Process.hThread);
        ::CloseHandle(Process.hProcess);
    }
    assert(SUCCEEDED(History.Load(Values)) && Values.Passwords.size() == 16);
    for (int Worker = 0; Worker < 2; ++Worker)
        for (int Index = 0; Index < 8; ++Index)
        {
            std::wstring Expected = L"worker-" + std::to_wstring(Worker)
                + std::to_wstring(Index);
            bool Found = false;
            for (std::wstring const& Password : Values.Passwords)
                Found = Found || Password == Expected;
            assert(Found);
        }
    // Turning the option off must not be undone by an in-flight extraction.
    assert(SUCCEEDED(History.SetEnabled(false)));
    assert(SUCCEEDED(Store(Path).Remember(L"late-success")));
    assert(SUCCEEDED(History.Load(Values)) && !Values.Enabled);
    assert(Values.Passwords.empty());

    for (char const* Invalid : {"{", "{}", "null",
        "{\"version\":1,\"protectedPasswords\":\"broken\"}",
        "{\"version\":2,\"protectedPasswords\":\"\"}"})
    {
        { std::ofstream File(Path, std::ios::binary); File << Invalid; }
        assert(FAILED(History.Load(Values)) && !Values.Enabled);
        assert(Values.Passwords.empty());
    }
    assert(SUCCEEDED(History.SetEnabled(true)));
    assert(History.IsEnabled());
    assert(SUCCEEDED(History.SetEnabled(false)));
    std::cout << "Password history tests passed\n";
}
