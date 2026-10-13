// PasswordDialog.cpp

#include "StdAfx.h"

#include "PasswordDialog.h"
// **************** NanaZip Modification Start ****************
#include "../Common/PasswordHistory.h"
#include "../../../Windows/ErrorMsg.h"
// **************** NanaZip Modification End ****************

#ifdef Z7_LANG
#include "LangUtils.h"
#endif

#ifdef Z7_LANG
static const UInt32 kLangIDs[] =
{
  IDT_PASSWORD_ENTER,
  IDX_PASSWORD_SHOW,
// **************** NanaZip Modification Start ****************
  IDX_PASSWORD_REMEMBER
// **************** NanaZip Modification End ****************
};
#endif

void CPasswordDialog::ReadControls()
{
  _passwordEdit.GetText(Password);
  ShowPassword = IsButtonCheckedBool(IDX_PASSWORD_SHOW);
}

void CPasswordDialog::SetTextSpec()
{
  _passwordEdit.SetPasswordChar(ShowPassword ? 0: TEXT('*'));
  _passwordEdit.SetText(Password);
}

bool CPasswordDialog::OnInit()
{
  #ifdef Z7_LANG
  LangSetWindowText(*this, IDD_PASSWORD);
  LangSetDlgItems(*this, kLangIDs, Z7_ARRAY_SIZE(kLangIDs));
  #endif
  _passwordEdit.Attach(GetItem(IDE_PASSWORD_PASSWORD));
  CheckButton(IDX_PASSWORD_SHOW, ShowPassword);
// **************** NanaZip Modification Start ****************
  ShowItem_Bool(IDX_PASSWORD_REMEMBER, EnableRememberPassword);
  if (EnableRememberPassword)
  {
    NanaZipPasswordHistory::Store Store;
    RememberPasswords = Store.IsEnabled();
    CheckButton(IDX_PASSWORD_REMEMBER, RememberPasswords);
  }
// **************** NanaZip Modification End ****************
  SetTextSpec();
  return CModalDialog::OnInit();
}

bool CPasswordDialog::OnButtonClicked(unsigned buttonID, HWND buttonHWND)
{
  if (buttonID == IDX_PASSWORD_SHOW)
  {
    ReadControls();
    SetTextSpec();
    return true;
  }
  return CDialog::OnButtonClicked(buttonID, buttonHWND);
}

void CPasswordDialog::OnOK()
{
  ReadControls();
// **************** NanaZip Modification Start ****************
  if (EnableRememberPassword)
  {
    NanaZipPasswordHistory::Store Store;
    bool Enabled = IsButtonCheckedBool(IDX_PASSWORD_REMEMBER);
    HRESULT Result = Enabled == RememberPasswords
        ? S_OK : Store.SetEnabled(Enabled);
    if (FAILED(Result))
    {
      ::MessageBoxW(*this, NWindows::NError::MyFormatMessage(Result),
          L"Nanazip-PM", MB_OK | MB_ICONERROR);
      return;
    }
  }
// **************** NanaZip Modification End ****************
  CModalDialog::OnOK();
}
