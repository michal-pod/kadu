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

Chat themes can additionally declare a writable `openUrl` property on their
root item and pass it to their timeline-item component. Invoke it for links
from `TextEdit`; it routes URLs through Kadu's `UrlHandlerManager` instead of
bypassing protocol handlers with `Qt.openUrlExternally()`.

`plainText` is always literal text and must be shown with `TextEdit.PlainText`.
`formattedText`, when non-empty, is the sanitized rich-text subset supplied by
the protocol and may be shown with `TextEdit.RichText`. It contains semantic
formatting and links, never caller-controlled colours, fonts, media, scripts or
other active content.
