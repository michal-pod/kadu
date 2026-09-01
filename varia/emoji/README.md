# Unicode emoji source data

`emoji-test.txt` is the checked-in Unicode Emoji 17.0 test data used as the
single source for Kadu's Unicode emoji catalogue. Its copyright and terms are
kept in the file header.

`generate-emoji-catalog.py` reads that file and emits a C++ table for the
`emoji` core target. The Kadu CMake target invokes it automatically into the
build directory, so applications use compiled data and never parse this file
at runtime.

When updating Unicode, replace `emoji-test.txt` with a selected released
version, review its license header, and regenerate the C++ output through the
normal build.

## Bundled emoji font

`fonts/NotoColorEmoji_WindowsCompatible.ttf` is the Noto Color Emoji variant
recommended by Qt for bundled deployment. It is a standard TrueType/OpenType font and
works with Qt's font loader on all supported desktop platforms; unlike the
regular NotoColorEmoji build, it is compatible with Windows too. It is installed
as a regular Kadu data file rather than embedded in `libkadu`. Its SHA-256 is
`19473341d23f8fdf90e91ffca381d727c43f7bc05b2758dec9687a58fbb81150`.

`fonts/LICENSE-Noto-Emoji.txt` contains the upstream SIL Open Font License 1.1.
