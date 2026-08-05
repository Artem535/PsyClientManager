# App Lock and Clipboard Clearing Design

## Scope

Issue #35 protects an unattended PsyClientManager session from casual access.
It adds an in-memory lock screen, a credential verifier, idle detection, and
clipboard clearing. It does not encrypt the active DuckDB database or attempt
to protect against an already-unlocked operating-system account.

## Credential

The user configures one unlock credential: either a PIN of at least six digits
or a password of at least twelve characters. The setup UI asks for it twice.
Only an Argon2id hash, a random salt, and the algorithm parameters are stored
in `QSettings`. The source credential is never logged or persisted.

`AppLockService` owns the small cryptographic API:

```cpp
bool configure(std::string_view credential);
bool verify(std::string_view credential) const;
bool isConfigured() const;
void disable();
```

It uses libsodium's interactive Argon2id profile so unlocking remains
responsive. After five consecutive failures, attempts are delayed for 30
seconds; each later failure doubles the delay up to five minutes. The verifier
state resets after a successful unlock or app restart.

## Lock Lifecycle

`Application` installs a global event filter and records user input activity.
`AppLockController` evaluates it once per second. It locks when a configured
timeout has elapsed, when the user selects **Lock application**, or when the
app becomes active after having been inactive for at least one minute.

The one-minute inactive threshold covers ordinary sleep and system-lock flows
without forcing an unlock after a quick switch to another application. Native
Windows/macOS/Linux session-lock integrations are explicitly deferred.

The lock screen is an opaque, application-modal overlay. It contains no client
data and preserves the currently open page and unsaved editor state below it.
After successful verification it disappears and restores the existing focus.
Closing the lock screen is disabled; quitting remains available only through
the existing tray action.

## Settings and Actions

The existing Settings dialog gains a **Privacy** section with:

- enable, change, and disable lock credential actions;
- idle timeout in minutes, with `0` meaning manual-only;
- clipboard clearing switch and delay in seconds.

The tray menu gains **Lock application** when a credential is configured.
Disabling protection requires the current credential.

## Clipboard

`SensitiveClipboardService` replaces direct sensitive `QClipboard::setText`
calls. It records a cryptographic hash of the text it writes and starts a
single-shot timer. At expiry it clears the clipboard only when the current text
still matches the recorded value; content copied by the user is never changed.
The service is used first for meeting links and invitation text, and exposes a
single `copySensitiveText(QString)` entry point for later client-info actions.

## Failure Handling and Tests

No configured credential means no lock UI and no tray lock action. Invalid or
corrupt credential settings disable the lock for that launch and show a clear
warning in Settings; this avoids permanently locking the owner out because the
lock is not database encryption. The user must configure a new credential.
Clipboard failures are non-fatal and never log copied text.

GoogleTest coverage will verify credential hashing and verification, malformed
stored settings, timeout policy, inactive-duration policy, failed-attempt
throttling, and clipboard ownership checks. UI tests cover lock visibility and
that an unsaved event editor survives a successful unlock.
