
#include "../NanaZip.Universal/SevenZip/CPP/Common/MyCom.h"
#include "../NanaZip.Universal/SevenZip/CPP/Common/MyString.h"
#include "../NanaZip.Universal/SevenZip/CPP/Windows/ErrorMsg.h"
#include "../NanaZip.Universal/SevenZip/CPP/7zip/UI/Common/ArchiveOpenCallback.h"
#include "../NanaZip.Universal/SevenZip/CPP/7zip/PropID.h"
#include <cassert>
#include <iostream>
#include <vector>

static HRESULT Archive_GetItemBoolProp(IInArchive* Archive,
    UInt32 Index, PROPID Property, bool& Value)
{
    PROPVARIANT Result;
    ::PropVariantInit(&Result);
    HRESULT Status = Archive->GetProperty(Index, Property, &Result);
    Value = Result.vt == VT_BOOL && Result.boolVal != VARIANT_FALSE;
    ::PropVariantClear(&Result);
    return Status;
}

#include "../NanaZip.Universal/SevenZip/CPP/7zip/UI/FileManager/PasswordRetry.h"

class TestCallback final : public IOpenCallbackUI,
    public ICryptoGetTextPassword, public CMyUnknownImp
{
    Z7_COM_QI_BEGIN2(ICryptoGetTextPassword)
    Z7_COM_QI_END
    Z7_COM_ADDREF_RELEASE
public:
    bool PasswordWasAsked = false;
    bool PasswordIsDefined = false;
    bool PasswordArchiveStarted = false;
    bool Stop = false;
    UString Password;
    std::vector<std::wstring> Responses;
    size_t Questions = 0;
    std::vector<std::wstring> Used;
    HRESULT Open_CheckBreak() override { return this->Stop ? E_ABORT : S_OK; }
    HRESULT Open_SetTotal(UInt64 const*, UInt64 const*) override { return S_OK; }
    HRESULT Open_SetCompleted(UInt64 const*, UInt64 const*) override
    { return this->Open_CheckBreak(); }
    HRESULT Open_Finished() override { return this->Open_CheckBreak(); }
    HRESULT Open_CryptoGetTextPassword(BSTR* Value) override
    { return this->CryptoGetTextPassword(Value); }
    Z7_COM7F_IMF(CryptoGetTextPassword(BSTR* Value)) override
    {
        this->PasswordWasAsked = true;
        if (!this->PasswordIsDefined)
        {
            size_t Index = this->Questions++;
            if (Index >= this->Responses.size())
                return E_ABORT;
            this->Password = this->Responses[Index].c_str();
            this->PasswordIsDefined = true;
        }
        this->Used.emplace_back(this->Password.Ptr());
        *Value = ::SysAllocString(this->Password.Ptr());
        return S_OK;
    }
    void AddError_Message(wchar_t const*) { assert(false); }
};

class InputStream final : public IInStream, public CMyUnknownImp
{
    Z7_COM_QI_BEGIN2(IInStream)
    Z7_COM_QI_END
    Z7_COM_ADDREF_RELEASE
public:
    HANDLE File = INVALID_HANDLE_VALUE;
    ~InputStream() { ::CloseHandle(this->File); }
    Z7_COM7F_IMF(Read(void* Data, UInt32 Size, UInt32* ReadSize)) override
    {
        DWORD Actual = 0;
        BOOL Success = ::ReadFile(this->File, Data, Size, &Actual, nullptr);
        if (ReadSize) *ReadSize = Actual;
        return Success ? S_OK : HRESULT_FROM_WIN32(::GetLastError());
    }
    Z7_COM7F_IMF(Seek(Int64 Offset, UInt32 Origin, UInt64* Position)) override
    {
        LARGE_INTEGER Move, Result;
        Move.QuadPart = Offset;
        if (!::SetFilePointerEx(this->File, Move, &Result, Origin))
            return HRESULT_FROM_WIN32(::GetLastError());
        if (Position) *Position = Result.QuadPart;
        return S_OK;
    }
};

class OpenBridge final : public IArchiveOpenCallback,
    public ICryptoGetTextPassword, public CMyUnknownImp
{
    Z7_COM_QI_BEGIN2(IArchiveOpenCallback)
    Z7_COM_QI_ENTRY(ICryptoGetTextPassword)
    Z7_COM_QI_END
    Z7_COM_ADDREF_RELEASE
public:
    IOpenCallbackUI* Callback = nullptr;
    Z7_COM7F_IMF(SetTotal(UInt64 const*, UInt64 const*)) override { return S_OK; }
    Z7_COM7F_IMF(SetCompleted(UInt64 const*, UInt64 const*)) override
    { return this->Callback->Open_CheckBreak(); }
    Z7_COM7F_IMF(CryptoGetTextPassword(BSTR* Value)) override
    { return this->Callback->Open_CryptoGetTextPassword(Value); }
};

struct TestOptions
{
    bool stdInMode = false;
    std::wstring Path;
    BYTE Format = 7;
};

// Open real archive handlers while keeping fixture paths and the store isolated.
struct TestLink
{
    HMODULE Module;
    CMyComPtr<IInArchive> Archive;
    UStringVector VolumePaths;
    explicit TestLink(HMODULE Module) : Module(Module) {}
    HRESULT Open_Strict(TestOptions& Options, IOpenCallbackUI* Callback)
    {
        typedef HRESULT(WINAPI* CreateObjectFunction)(GUID const*, GUID const*, void**);
        auto CreateObject = reinterpret_cast<CreateObjectFunction>(
            ::GetProcAddress(this->Module, "CreateObject"));
        assert(CreateObject);
        GUID Class = { 0x23170F69, 0x40C1, 0x278A,
            { 0x10, 0, 0, 1, 0x10, Options.Format, 0, 0 } };
        RINOK(CreateObject(&Class, &IID_IInArchive,
            reinterpret_cast<void**>(&this->Archive)))
        CMyComPtr2_Create<IInStream, InputStream> Stream;
        Stream->File = ::CreateFileW(Options.Path.c_str(), GENERIC_READ,
            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        assert(Stream->File != INVALID_HANDLE_VALUE);
        CMyComPtr2_Create<IArchiveOpenCallback, OpenBridge> Bridge;
        Bridge->Callback = Callback;
        return this->Archive->Open(Stream, nullptr, Bridge);
    }
    IInArchive* GetArchive() { return this->Archive; }
    HRESULT Close() { return this->Archive ? this->Archive->Close() : S_OK; }
    void Release() { this->Archive.Release(); }
};

int wmain(int Count, wchar_t** Arguments)
{
    assert(Count == 3);
    HMODULE Module = ::LoadLibraryExW(Arguments[1], nullptr,
        LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!Module) std::cerr << "LoadLibrary error: " << ::GetLastError() << "\n";
    assert(Module);
    std::wstring Directory = Arguments[2];
    NanaZipPasswordHistory::Store Store(Directory + L"\\RetryHistory.json");
    for (wchar_t const* Name : { L"header.7z", L"content.7z", L"aes.zip", L"legacy.zip" })
    {
        assert(SUCCEEDED(Store.SetEnabled(false)));
        assert(SUCCEEDED(Store.SetEnabled(true)));
        assert(SUCCEEDED(Store.Remember(L"right")));
        assert(SUCCEEDED(Store.Remember(L"wrong")));
        TestOptions Options;
        Options.Path = Directory + L"\\" + Name;
        Options.Format = Options.Path.find(L".zip") != std::wstring::npos ? 1 : 7;
        {
            TestLink Link(Module);
            TestCallback Callback;
            assert(OpenArchiveWithPasswordHistory(
                Callback, Link, Options, &Callback, Store) == S_OK);
            assert(Callback.Questions == 0);
            assert(Callback.Used.front() == L"wrong");
            assert(Callback.Used.back() == L"right");
            NanaZipPasswordHistory::List Saved;
            assert(SUCCEEDED(Store.Load(Saved)));
            assert(Saved.Passwords.front() == L"right");
        }
        // Exhausting saved values falls back to one manual password prompt.
        assert(SUCCEEDED(Store.SetEnabled(false)));
        assert(SUCCEEDED(Store.SetEnabled(true)));
        assert(SUCCEEDED(Store.Remember(L"wrong")));
        {
            TestLink Link(Module);
            TestCallback Callback;
            Callback.Responses.emplace_back(L"right");
            assert(OpenArchiveWithPasswordHistory(
                Callback, Link, Options, &Callback, Store) == S_OK);
            assert(Callback.Questions == 1);
        }
        assert(SUCCEEDED(Store.SetEnabled(false)));
        assert(SUCCEEDED(Store.SetEnabled(true)));
        assert(SUCCEEDED(Store.Remember(L"wrong")));
        {
            TestLink Link(Module);
            TestCallback Callback;
            assert(OpenArchiveWithPasswordHistory(
                Callback, Link, Options, &Callback, Store) == E_ABORT);
            assert(Callback.Questions == 1);
        }
        {
            TestLink Link(Module);
            TestCallback Callback;
            Callback.Stop = true;
            assert(OpenArchiveWithPasswordHistory(
                Callback, Link, Options, &Callback, Store) == E_ABORT);
            assert(Callback.Questions == 0);
        }
    }
    assert(SUCCEEDED(Store.SetEnabled(false)));
    std::cout << "Real archive password retry tests passed\n";
}
