# QML theme examples

These are deliberately not installed or referenced by CMake. Copy one chosen
theme directory to the matching runtime directory to exercise the external
theme loader:

| Example | Destination |
| --- | --- |
| `chat/Debug chat theme` | `<profile>/chat-styles/Debug chat theme` |
| `info-panel/Debug buddy info theme` | `<profile>/info-panel-styles/Debug buddy info theme` |

Every external theme has a `theme.desc` and one entry QML file. The directory
name is its persistent identifier; `DisplayName` from `theme.desc` is the name
shown to the user. `Type` must be `chat` or `info-panel` respectively.

`System` is always supplied by Kadu and follows the desktop palette. Entries
in `[Colors]` add fixed schemes. The selected scheme is assigned to the QML
root's writable `colorScheme` property. The examples visibly print every
property that Kadu supplies, so they are also useful when debugging a changed
theme contract.
