// Password history shared by extraction processes of the current user.
#ifndef NANAZIP_PASSWORD_HISTORY_H
#define NANAZIP_PASSWORD_HISTORY_H

#include <windows.h>
#include <shlobj.h>
#include <wincrypt.h>
#include <string>
#include <cwctype>
#include <vector>

#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Shell32.lib")

namespace NanaZipPasswordHistory
{
    inline void Erase(std::wstring& Value)
    {
        if (!Value.empty())
            ::SecureZeroMemory(&Value[0], Value.size() * sizeof(wchar_t));
        Value.clear();
    }

    struct List
    {
        bool Enabled = false;
        std::vector<std::wstring> Passwords;

        void Clear()
        {
            for (std::wstring& Password : this->Passwords)
                Erase(Password);
            this->Passwords.clear();
            this->Enabled = false;
        }

        ~List() { this->Clear(); }
    };

    class Handle
    {
    public:
        HANDLE Value;
        explicit Handle(HANDLE Value) : Value(Value) {}
        ~Handle()
        {
            if (this->Value && this->Value != INVALID_HANDLE_VALUE)
                ::CloseHandle(this->Value);
        }
        Handle(Handle const&) = delete;
        Handle& operator=(Handle const&) = delete;
    };

    class Store
    {
        std::wstring m_Path;

        class Lock
        {
            Handle m_Handle;
            bool m_Locked = false;
        public:
            HRESULT Result;
            explicit Lock(std::wstring const& Path) : m_Handle(nullptr)
            {
                // The path distinguishes users and test stores, across sessions.
                unsigned long long Hash = 14695981039346656037ULL;
                for (wchar_t Character : Path)
                {
                    Hash ^= static_cast<unsigned short>(::towlower(Character));
                    Hash *= 1099511628211ULL;
                }
                std::wstring Name = L"Global\\Nanazip-PM.PasswordHistory.";
                Name += std::to_wstring(Hash);
                this->m_Handle.Value = ::CreateMutexW(nullptr, FALSE, Name.c_str());
                this->Result = this->m_Handle.Value ? S_OK
                    : HRESULT_FROM_WIN32(::GetLastError());
                if (FAILED(this->Result))
                    return;
                DWORD Wait = ::WaitForSingleObject(this->m_Handle.Value, 10000);
                this->m_Locked = Wait == WAIT_OBJECT_0 || Wait == WAIT_ABANDONED;
                if (!this->m_Locked)
                    this->Result = HRESULT_FROM_WIN32(
                        Wait == WAIT_TIMEOUT ? ERROR_TIMEOUT : ::GetLastError());
            }
            ~Lock()
            {
                if (this->m_Locked)
                    ::ReleaseMutex(this->m_Handle.Value);
            }
        };

        static bool Consume(char const*& Cursor, char const* Token)
        {
            while (*Cursor == ' ' || *Cursor == '\t'
                || *Cursor == '\r' || *Cursor == '\n')
                ++Cursor;
            while (*Token)
                if (*Cursor++ != *Token++)
                    return false;
            return true;
        }

        HRESULT Read(List& Values) const
        {
            Values.Clear();
            Handle File(::CreateFileW(this->m_Path.c_str(), GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (File.Value == INVALID_HANDLE_VALUE)
            {
                DWORD Error = ::GetLastError();
                return Error == ERROR_FILE_NOT_FOUND || Error == ERROR_PATH_NOT_FOUND
                    ? S_OK : HRESULT_FROM_WIN32(Error);
            }
            LARGE_INTEGER Size;
            if (!::GetFileSizeEx(File.Value, &Size))
                return HRESULT_FROM_WIN32(::GetLastError());
            if (Size.QuadPart <= 0 || Size.QuadPart > 1024 * 1024)
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            std::string Json(static_cast<size_t>(Size.QuadPart), '\0');
            DWORD ReadSize = 0;
            if (!::ReadFile(File.Value, &Json[0], static_cast<DWORD>(Json.size()),
                &ReadSize, nullptr))
                return HRESULT_FROM_WIN32(::GetLastError());
            if (ReadSize != Json.size() || Json.find('\0') != std::string::npos)
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            char const* Cursor = Json.c_str();
            if (!Consume(Cursor, "{") || !Consume(Cursor, "\"version\"")
                || !Consume(Cursor, ":") || !Consume(Cursor, "1")
                || !Consume(Cursor, ",")
                || !Consume(Cursor, "\"protectedPasswords\"")
                || !Consume(Cursor, ":") || !Consume(Cursor, "\""))
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            char const* Begin = Cursor;
            while (*Cursor && *Cursor != '"')
                ++Cursor;
            std::string Encoded(Begin, Cursor);
            if (!Consume(Cursor, "\"") || !Consume(Cursor, "}")
                || !Consume(Cursor, "") || *Cursor)
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            DWORD BinarySize = 0;
            if (!::CryptStringToBinaryA(Encoded.c_str(),
                static_cast<DWORD>(Encoded.size()), CRYPT_STRING_BASE64,
                nullptr, &BinarySize, nullptr, nullptr))
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            std::vector<BYTE> Binary(BinarySize);
            if (!::CryptStringToBinaryA(Encoded.c_str(),
                static_cast<DWORD>(Encoded.size()), CRYPT_STRING_BASE64,
                Binary.data(), &BinarySize, nullptr, nullptr))
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            DATA_BLOB Protected = { BinarySize, Binary.data() };
            DATA_BLOB Plain = {};
            if (!::CryptUnprotectData(&Protected, nullptr, nullptr, nullptr,
                nullptr, CRYPTPROTECT_UI_FORBIDDEN, &Plain))
                return HRESULT_FROM_WIN32(::GetLastError());
            bool Valid = Plain.cbData >= 2 * sizeof(wchar_t)
                && Plain.cbData % sizeof(wchar_t) == 0;
            size_t Count = Plain.cbData / sizeof(wchar_t);
            wchar_t const* Text = reinterpret_cast<wchar_t const*>(Plain.pbData);
            size_t Position = 0;
            while (Valid && Position + 1 < Count && Text[Position])
            {
                size_t Start = Position;
                while (Position < Count && Text[Position])
                    ++Position;
                Valid = Position < Count && Values.Passwords.size() < 16;
                if (Valid)
                    Values.Passwords.emplace_back(Text + Start, Position++ - Start);
            }
            Valid = Valid && (Position == Count - 1
                || (Position == 0 && Count == 2 && Text[1] == 0))
                && Text[Position] == 0;
            ::SecureZeroMemory(Plain.pbData, Plain.cbData);
            ::LocalFree(Plain.pbData);
            if (!Valid)
            {
                Values.Clear();
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            }
            Values.Enabled = true;
            return S_OK;
        }

        HRESULT Write(List const& Values) const
        {
            size_t Separator = this->m_Path.find_last_of(L"\\/");
            if (Separator != std::wstring::npos)
            {
                std::wstring Directory = this->m_Path.substr(0, Separator);
                if (!::CreateDirectoryW(Directory.c_str(), nullptr)
                    && ::GetLastError() != ERROR_ALREADY_EXISTS)
                    return HRESULT_FROM_WIN32(::GetLastError());
            }
            std::wstring Plain;
            size_t Characters = 2;
            for (std::wstring const& Password : Values.Passwords)
                Characters += Password.size() + 1;
            Plain.reserve(Characters);
            for (std::wstring const& Password : Values.Passwords)
            {
                Plain += Password;
                Plain += L'\0';
            }
            Plain += L'\0';
            if (Values.Passwords.empty())
                Plain += L'\0';
            DATA_BLOB Input = { static_cast<DWORD>(Plain.size() * sizeof(wchar_t)),
                reinterpret_cast<BYTE*>(&Plain[0]) };
            DATA_BLOB Protected = {};
            BOOL Success = ::CryptProtectData(&Input, L"Nanazip-PM password history",
                nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &Protected);
            DWORD Error = ::GetLastError();
            Erase(Plain);
            if (!Success)
                return HRESULT_FROM_WIN32(Error);
            DWORD EncodedSize = 0;
            DWORD Flags = CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;
            Success = ::CryptBinaryToStringA(Protected.pbData, Protected.cbData,
                Flags, nullptr, &EncodedSize);
            std::string Encoded(EncodedSize, '\0');
            if (Success)
                Success = ::CryptBinaryToStringA(Protected.pbData,
                    Protected.cbData, Flags, &Encoded[0], &EncodedSize);
            Error = ::GetLastError();
            ::LocalFree(Protected.pbData);
            if (!Success)
                return HRESULT_FROM_WIN32(Error);
            Encoded.resize(::strlen(Encoded.c_str()));
            std::string Json = "{\r\n  \"version\": 1,\r\n  \"protectedPasswords\": \"";
            Json += Encoded;
            Json += "\"\r\n}\r\n";
            std::wstring Temporary = this->m_Path + L".tmp";
            {
                Handle File(::CreateFileW(Temporary.c_str(), GENERIC_WRITE, 0,
                    nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
                if (File.Value == INVALID_HANDLE_VALUE)
                    return HRESULT_FROM_WIN32(::GetLastError());
                DWORD Written = 0;
                Success = ::WriteFile(File.Value, Json.data(),
                    static_cast<DWORD>(Json.size()), &Written, nullptr);
                Error = Success ? ERROR_WRITE_FAULT : ::GetLastError();
                if (Success && Written == Json.size())
                {
                    Success = ::FlushFileBuffers(File.Value);
                    Error = ::GetLastError();
                }
                else
                    Success = FALSE;
            }
            if (Success)
            {
                Success = ::MoveFileExW(Temporary.c_str(), this->m_Path.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
                Error = ::GetLastError();
            }
            if (!Success)
                ::DeleteFileW(Temporary.c_str());
            return Success ? S_OK : HRESULT_FROM_WIN32(Error);
        }

    public:
        explicit Store(std::wstring const& Path = L"") : m_Path(Path) {}

        HRESULT Initialize()
        {
            if (!this->m_Path.empty())
                return S_OK;
            wchar_t Path[MAX_PATH];
            HRESULT Result = ::SHGetFolderPathW(nullptr,
                CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, Path);
            if (FAILED(Result))
                return Result;
            std::wstring Directory = std::wstring(Path) + L"\\Nanazip-PM";
            this->m_Path = Directory + L"\\PasswordHistory.json";
            return S_OK;
        }

        HRESULT Load(List& Values)
        {
            HRESULT Result = this->Initialize();
            if (FAILED(Result))
                return Result;
            Lock Guard(this->m_Path);
            return FAILED(Guard.Result) ? Guard.Result : this->Read(Values);
        }

        bool IsEnabled()
        {
            List Values;
            return SUCCEEDED(this->Load(Values)) && Values.Enabled;
        }

        HRESULT SetEnabled(bool Enabled)
        {
            HRESULT Result = this->Initialize();
            if (FAILED(Result))
                return Result;
            Lock Guard(this->m_Path);
            if (FAILED(Guard.Result))
                return Guard.Result;
            if (!Enabled)
            {
                std::wstring Temporary = this->m_Path + L".tmp";
                if (!::DeleteFileW(Temporary.c_str()))
                {
                    DWORD Error = ::GetLastError();
                    if (Error != ERROR_FILE_NOT_FOUND && Error != ERROR_PATH_NOT_FOUND)
                        return HRESULT_FROM_WIN32(Error);
                }
                if (::DeleteFileW(this->m_Path.c_str()))
                    return S_OK;
                DWORD Error = ::GetLastError();
                return Error == ERROR_FILE_NOT_FOUND || Error == ERROR_PATH_NOT_FOUND ? S_OK
                    : HRESULT_FROM_WIN32(Error);
            }
            List Values;
            Result = this->Read(Values);
            if (SUCCEEDED(Result) && Values.Enabled)
                return S_OK;
            Values.Clear();
            return this->Write(Values);
        }

        HRESULT Remember(wchar_t const* Password)
        {
            if (!Password || !*Password)
                return S_OK;
            if (::wcslen(Password) + 2 > 128 * 1024)
                return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
            HRESULT Result = this->Initialize();
            if (FAILED(Result))
                return Result;
            Lock Guard(this->m_Path);
            if (FAILED(Guard.Result))
                return Guard.Result;
            List Values;
            Result = this->Read(Values);
            if (FAILED(Result) || !Values.Enabled)
                return Result;
            if (!Values.Passwords.empty() && Values.Passwords.front() == Password)
                return S_OK;
            for (size_t Index = 0; Index < Values.Passwords.size(); ++Index)
            {
                if (Values.Passwords[Index] == Password)
                {
                    Erase(Values.Passwords[Index]);
                    Values.Passwords.erase(Values.Passwords.begin() + Index);
                    break;
                }
            }
            Values.Passwords.insert(Values.Passwords.begin(), Password);
            size_t Characters = 1;
            for (std::wstring const& Value : Values.Passwords)
                Characters += Value.size() + 1;
            while (Values.Passwords.size() > 16 || Characters > 128 * 1024)
            {
                Characters -= Values.Passwords.back().size() + 1;
                Erase(Values.Passwords.back());
                Values.Passwords.pop_back();
            }
            return this->Write(Values);
        }
    };
}

#endif
