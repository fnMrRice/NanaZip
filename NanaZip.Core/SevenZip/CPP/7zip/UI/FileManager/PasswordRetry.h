#ifndef NANAZIP_PASSWORD_RETRY_H
#define NANAZIP_PASSWORD_RETRY_H

#include "../Common/PasswordHistory.h"

template<class TCallback>
class CPasswordCheck final :
    public IArchiveExtractCallback,
    public ICryptoGetTextPassword,
    public CMyUnknownImp
{
    Z7_COM_QI_BEGIN2(IArchiveExtractCallback)
    Z7_COM_QI_ENTRY(ICryptoGetTextPassword)
    Z7_COM_QI_END
    Z7_COM_ADDREF_RELEASE

public:
    TCallback* Callback = nullptr;
    bool WrongPassword = false;
    bool OtherError = false;

    Z7_COM7F_IMF(SetTotal(UInt64 Total)) override
    {
        UNREFERENCED_PARAMETER(Total);
        return static_cast<IOpenCallbackUI*>(this->Callback)->Open_CheckBreak();
    }
    Z7_COM7F_IMF(SetCompleted(UInt64 const* Completed)) override
    {
        UNREFERENCED_PARAMETER(Completed);
        return static_cast<IOpenCallbackUI*>(this->Callback)->Open_CheckBreak();
    }
    Z7_COM7F_IMF(GetStream(UInt32 Index,
        ISequentialOutStream** Stream, Int32 Mode)) override
    {
        UNREFERENCED_PARAMETER(Index);
        UNREFERENCED_PARAMETER(Mode);
        *Stream = nullptr;
        return static_cast<IOpenCallbackUI*>(this->Callback)->Open_CheckBreak();
    }
    Z7_COM7F_IMF(PrepareOperation(Int32 Mode)) override
    {
        UNREFERENCED_PARAMETER(Mode);
        return static_cast<IOpenCallbackUI*>(this->Callback)->Open_CheckBreak();
    }
    Z7_COM7F_IMF(SetOperationResult(Int32 Result)) override
    {
        switch (Result)
        {
        case NArchive::NExtract::NOperationResult::kOK:
            return S_OK;
        case NArchive::NExtract::NOperationResult::kWrongPassword:
        case NArchive::NExtract::NOperationResult::kCRCError:
        case NArchive::NExtract::NOperationResult::kDataError:
            this->WrongPassword = true;
            break;
        default:
            this->OtherError = true;
        }
        return S_FALSE;
    }
    Z7_COM7F_IMF(CryptoGetTextPassword(BSTR* Password)) override
    {
        return static_cast<ICryptoGetTextPassword*>(this->Callback)->CryptoGetTextPassword(Password);
    }
};

template<class TCallback, class TLink, class TOptions>
HRESULT OpenArchiveWithPasswordHistory(
    TCallback& Callback,
    TLink& Link,
    TOptions& Options,
    IOpenCallbackUI* OpenCallback,
    NanaZipPasswordHistory::Store& Store)
{
    if (Options.stdInMode)
        return Link.Open_Strict(Options, OpenCallback);
    NanaZipPasswordHistory::List Saved;
    Store.Load(Saved);
    if (Saved.Enabled && !Saved.Passwords.empty()
        && (!Callback.PasswordIsDefined || Callback.PasswordArchiveStarted))
    {
        Callback.Password = Saved.Passwords.front().c_str();
        Callback.PasswordIsDefined = true;
    }
    Callback.PasswordArchiveStarted = true;
    NanaZipPasswordHistory::List Attempted;
    TOptions OriginalOptions = Options;
    for (;;)
    {
        RINOK(OpenCallback->Open_CheckBreak())
        Callback.PasswordWasAsked = false;
        // Opening can replace streams and callbacks with archive-owned objects.
        Options = OriginalOptions;
        HRESULT Result = Link.Open_Strict(Options, OpenCallback);
        bool Retry = Result == S_FALSE && Callback.PasswordWasAsked;
        if (Result == S_OK)
        {
            IInArchive* Archive = Link.GetArchive();
            UInt32 Count = 0;
            RINOK(Archive->GetNumberOfItems(&Count))
            CUIntVector Encrypted;
            for (UInt32 Index = 0; Index < Count; ++Index)
            {
                if ((Index & 1023) == 0)
                {
                    RINOK(OpenCallback->Open_CheckBreak())
                }
                bool IsEncrypted = false;
                RINOK(Archive_GetItemBoolProp(
                    Archive, Index, kpidEncrypted, IsEncrypted))
                if (IsEncrypted)
                    Encrypted.Add(Index);
            }
            if (!Encrypted.IsEmpty())
            {
                if (!Callback.PasswordIsDefined)
                {
                    CMyComBSTR_Wipe Password;
                    RINOK(static_cast<ICryptoGetTextPassword*>(&Callback)->CryptoGetTextPassword(&Password))
                }
                if (!Store.IsEnabled())
                    return Result;
                CMyComPtr2_Create<IArchiveExtractCallback, CPasswordCheck<TCallback>> Check;
                Check->Callback = &Callback;
                HRESULT TestResult = Archive->Extract(
                    &Encrypted[0], Encrypted.Size(), true, Check);
                if (TestResult == E_ABORT)
                    return TestResult;
                Retry = Check->WrongPassword;
                if (!Retry && (TestResult != S_OK || Check->OtherError))
                    return Result; // Let normal extraction report non-password errors.
            }
            if (!Retry)
            {
                if (Callback.PasswordWasAsked)
                {
                    HRESULT SaveResult = Store.Remember(Callback.Password);
                    if (FAILED(SaveResult))
                        Callback.AddError_Message(NWindows::NError::MyFormatMessage(SaveResult));
                }
                return Result;
            }
        }
        if (!Retry || !Store.IsEnabled())
            return Result;
        Attempted.Passwords.emplace_back(Callback.Password.Ptr());
        Callback.Password.Empty();
        Callback.PasswordIsDefined = false;
        Store.Load(Saved);
        for (std::wstring const& Candidate : Saved.Passwords)
        {
            bool Tried = false;
            for (std::wstring const& Previous : Attempted.Passwords)
                if (Previous == Candidate)
                    Tried = true;
            if (!Tried)
            {
                Callback.Password = Candidate.c_str();
                Callback.PasswordIsDefined = true;
                break;
            }
        }
        if (!Callback.PasswordIsDefined)
        {
            CMyComBSTR_Wipe Password;
            RINOK(static_cast<ICryptoGetTextPassword*>(&Callback)->CryptoGetTextPassword(&Password))
        }
        RINOK(Link.Close())
        Link.Release();
        Link.VolumePaths.Clear();
    }
}

#endif
