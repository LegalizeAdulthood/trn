<!-- Copyright (c) 2026, Richard Thomson -->

# vcpkg Overlays

The root vcpkg-configuration.json registers ports in this directory as
overrides for the bundled vcpkg ports.

## fmt

The fmt 12.1.0 overlay provides a utf8 feature, enabled by default. trn
disables default features for fmt so its consumers do not require MSVC's
/utf-8 compiler option. The port sets FMT_UNICODE from that feature.

Upstream CMake applies FMT_UNICODE=0 to the compiled library but omits it
from the fmt-header-only target. header-only-fmt-unicode.patch adds the
missing definition so both targets honor the setting. Without it,
header-only consumers can fail fmt's MSVC checks when /utf-8 is absent.

Remove this overlay when the bundled port provides the feature control
and both exported targets correctly propagate FMT_UNICODE=0.

## OpenSSL

OpenSSL 3.6.1 uses an invalid thread ID sentinel in its thread-local
initialization recursion guard. Comparing that sentinel with a real
thread ID causes crashes on NetBSD during thread initialization.

thread-local-init.patch backports
[OpenSSL PR 31172](https://github.com/openssl/openssl/pull/31172), removing
the sentinel-based guard and arranging thread initialization before local
storage initialization. The overlay applies this patch only on NetBSD;
other platforms delegate to the bundled OpenSSL port.

Remove this overlay when the bundled OpenSSL port includes the fix.

## strong-type

strong_type 15 includes <ranges> whenever __has_include finds the header.
That check can succeed in C++17 mode even though ranges requires C++20.
MSVC reports warning STL4038 for this inclusion when building trn.

cxx20-ranges.patch requires C++20 or later before checking for and
including <ranges>. It checks __cplusplus on all compilers and also
_MSVC_LANG because MSVC can report an older __cplusplus value. The same
C++20 minimum applies in both cases.

Remove this overlay when the bundled strong-type port includes an
equivalent language-version guard.
