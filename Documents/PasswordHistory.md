# Remember passwords during extraction

Select **Remember passwords** in the extraction dialog or the extraction
password prompt. The option is off until selected. Successful passwords are
kept in recent-use order, with up to 16 distinct, case-sensitive entries.
Batch extraction and subsequent extraction processes share this history.
An explicitly supplied password is tried first for the first archive.
Otherwise, saved passwords are tried from newest to oldest. When none works,
the usual password prompt is displayed. Cancel stops the operation.

Passwords are validated without writing extracted files. Validation includes
the encrypted contents, so opening an archive with unencrypted filenames is
not mistaken for successful password validation. This extra validation can
increase extraction time for large encrypted archives. Encrypted data or CRC
errors can also indicate archive damage; trying another password does not
repair damaged archives. The command-line console keeps its existing behavior.

The shared file is `%LOCALAPPDATA%\Nanazip-PM\PasswordHistory.json` (resolved using
the Windows local application data folder). Its versioned JSON envelope holds
a Base64-encoded DPAPI blob. The decrypted data never goes into the file.
DPAPI uses the current Windows user's protection scope. This file is therefore
not a portable password backup and does not protect against programs already
running as that same user.

Updates acquire a named process mutex, reread the current history, and replace
the file atomically. Clearing **Remember passwords** deletes the shared history
and any temporary encrypted file. A pending successful extraction cannot
recreate history after the option is disabled. Corrupt or undecryptable history
is ignored; enabling the option again creates a fresh history.

## Verification

From an x64 Visual Studio developer command prompt:

```
Tests\RunPasswordHistoryTests.cmd
```

This tests DPAPI round trips, Unicode, case-sensitive deduplication, MRU order,
the entry limit, malformed data, disabling, and simultaneous updates from two
processes. All test data stays under `Output\PasswordHistoryTests`.

Build `NanaZip.Universal.Console.vcxproj` in Debug/x64, then run:

```
Tests\RunPasswordRetryTests.cmd
```

This creates real encrypted 7z and ZIP fixtures and uses `NanaZip.Core.dll` to
exercise the extraction password-selection routine, wrong-password fallback,
manual entry, and cancellation. It uses a separate test history file.
