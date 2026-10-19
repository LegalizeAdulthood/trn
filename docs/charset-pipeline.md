<!-- This software is copyrighted as detailed in the LICENSE file. -->
<!-- Copyright (c) 2026, Richard Thomson -->

# 5.0 Charset Pipeline Contract

The release charset work uses this order:

1. Retain stored and fetched article bytes unchanged.
2. Parse MIME structure and decode transfer encodings, including Base64
   and quoted-printable, into the selected text charset's bytes.
3. Decode those bytes into Unicode according to the declared or selected
   charset.  Decoded semantic text is UTF-8.
4. Apply character substitution to Unicode scalars and encode the result
   as UTF-8.  Substitution is a presentation transformation.
5. Give layout the resulting UTF-8 text.  Wrapping and terminal width
   apply after substitution, which can expand or remove characters.

RFC 2047 encoded words follow the same transfer-before-charset order.
Each encoded word and MIME text section owns its charset interpretation.
Raw storage and the decoded presentation buffer are separate objects.

The strict scalar primitives establish the boundary for Unicode
substitution and per-header/per-section charset migration.  The broader
design is described in [charset-handling.md](charset-handling.md).

## Scalar API

`decode_utf8_scalar(text)` reads the first scalar from a bounded view and
returns its `code_point` and `byte_count`.  Valid input consumes one to
four bytes.  Later bytes remain available to the caller.  Embedded NUL
is a valid scalar and consumes one byte.

`encode_utf8_scalar(scalar)` returns that scalar's UTF-8 bytes.  It
accepts U+0000 through U+10FFFF except U+D800 through U+DFFF.  Noncharacter
values such as U+FFFF remain valid Unicode scalar values.  An invalid
scalar returns an empty string; U+0000 returns one NUL byte.

Both functions are independent of `utf_init`, input/output charset
selection, and prior decoding calls.  They do not modify their input or
process-global charset state.

## Malformed Input

The decoder reports `INVALID_CODE_POINT` for an invalid lead byte,
invalid continuation, truncation, overlong encoding, surrogate, value
above U+10FFFF, or obsolete five-/six-byte encoding.  It consumes exactly
one byte on every nonempty malformed input.  This guarantees progress
without swallowing a following valid character.  Empty input reports
`INVALID_CODE_POINT` and consumes zero bytes.

Errors are reported rather than accepted as Unicode scalars.  Callers
retain the source bytes and choose recovery explicitly.  For example,
they can append `encode_utf8_scalar(0xFFFD)` for each reported error while
keeping the original article intact.  A presentation buffer must contain
valid UTF-8 after recovery.

A bounded view is treated as complete input.  A streaming producer can
retain a partial sequence and retry with an extended view before
consuming it.  The primitive keeps no partial-sequence state.

The accepted byte forms follow
[RFC 3629](https://www.rfc-editor.org/rfc/rfc3629.html#section-4).

## Charset Decoding

`CharsetDecoder(name)` retains its own charset selection.  Supported
names and aliases cover ASCII, UTF-8, ISO-8859-1, ISO-8859-15, and
Windows-1252.  `decode(bytes)` returns UTF-8 without changing global
charset selection.  Views are complete inputs; embedded NUL is retained.

The default decoder uses UTF-8.  Unknown or empty names also use strict
UTF-8 as a deterministic fallback, while `charset()` reports
`CHARSET_UNKNOWN`.  Invalid UTF-8 bytes, bytes above 0x7F in ASCII, and
undefined Windows-1252 bytes become U+FFFD.  Latin-1 decodes all byte
values, including soft hyphen and C1 controls, to their Unicode values.
Control filtering is a later presentation operation.

Each RFC 2047 word selects a decoder after transfer decoding.  Ordinary
header text uses the UTF-8 fallback.  Incomplete encoded words remain
literal text.  Whitespace is suppressed only between complete encoded
words; spaces before ordinary text remain visible.

Each MIME section retains a decoder and resets it when its content type
is parsed or cleared.  A missing charset uses the default UTF-8 decoder;
an unknown charset cannot reuse an earlier part's selection.  Body text
is transfer-decoded and converted before HTML filtering, substitution,
and layout.  The presentation cache stores UTF-8; raw article reads
continue to return the original bytes.

`MimeSection::decode_text_chunk` retains an incomplete trailing UTF-8
scalar between transfer-decoded chunks.  Finalizing a section replaces
unfinished bytes before its boundary marker is cached.  EOF also
finalizes pending text.  Clearing or reparsing a section discards its
pending bytes and selects a fresh decoder.  Complete cached lines are
already UTF-8 and are not decoded again during backward paging.

Substitution waits for complete presentation lines, so a cached partial
line cannot be transformed twice.  Changing the substitution mode clears
the presentation cache and restarts display from the article's first
page; expanded text can have different offsets and line breaks.

`utf8_visual_width_at`, `utf8_visual_length`, and `put_utf8_char_adv`
measure or output presentation text independently of legacy charset
selection.  Terminal output does not decode that text a second time.

## Historical Substitution

`CharSubstitution(mode)` consumes complete UTF-8 views and returns UTF-8.
It uses the strict scalar primitives independently of `utf_init` and
global charset selection.  Each malformed byte becomes U+FFFD; subsequent
valid characters remain available for substitution.

The plain (`p`) and off profiles preserve valid UTF-8 bytes exactly.
The historical ASCII (`a`) and monospaced ASCII (`m`) tables map Unicode
U+00A0 through U+00FF.  Their original approximations and empty mappings
remain in force.  Other scalars, including CJK, combining marks, emoji,
and U+2019, pass through unchanged.  Substitution does not normalize text.

The TeX (`t`) profile converts the historical quote-prefixed umlauts and
sharp s to Unicode.  It also accepts a backslash before the quote.
Unrecognized sequences retain their characters.  A pending quote or
backslash belongs to that instance and can span `substitute` calls.
`finish()` emits a trailing prefix and clears it; repeated calls are
empty.  Discarding an instance cannot affect another article or header.
Callers split views only at UTF-8 scalar boundaries, retaining partial
byte sequences themselves as described above.

`str_char_subst` creates a fresh instance, substitutes through the first
newline if present, and flushes its prefix.  Author and subject
compression use this wrapper before layout.  Displayed headers apply
the selected profile after encoded-word decoding.

## Optional Unicode-To-ASCII Profile

The `u` substitution mode adds Unicode punctuation approximations to the
historical `a` Latin table.  It is disabled by default: `Charset = patm`
is unchanged.  `Charset` selects substitution modes, not source decoding.
Put `u` first to start with conversion; include `p` to allow cycling back
to unchanged UTF-8 with the existing conversion-mode command.

For example, put this in the user's `trnrc`:

```text
[options]
Charset = up

[unicode substitutions]
U+2014 = "---"
U+2019 = "'"
U+1F60A = ":)"
```

User entries overlay the built-in profile and apply only in mode `u`.
Later entries and later sections override earlier entries for the same
scalar.  Loading mappings alone does not enable conversion.  Plain,
historical ASCII, monospaced ASCII, and TeX modes retain their behavior.
Unmapped scalars pass through; the profile does not promise ASCII-only
output.  Invalid input bytes follow the existing U+FFFD policy.

Keys use `U+` followed by four through six hexadecimal digits, with
case-insensitive letters.  Surrogates and values above U+10FFFF are
invalid.  Byte keys such as `0x91` are invalid: Windows-1252 byte 0x91
must first decode to U+2018, which the profile maps to a backtick.
Source charset selection still belongs to each encoded word or MIME
section; substitution never guesses it from a byte value.

The single-byte examples in [issue #34][issue34] are interpreted through
their source charset.  In Windows-1252, 0x85 decodes to U+2026 and becomes
`...`; 0x96 decodes to U+2013 and becomes `--`; 0x97 decodes to U+2014 and
becomes `---`.  Override those scalars for different approximations.
In ISO-8859-1, 0x85 instead decodes to U+0085 and becomes a newline.
An isolated 0x85 or 0x91 in UTF-8 is malformed and becomes U+FFFD.

Values follow the normal INI quoting and escape rules.  Quotes preserve
leading and trailing spaces.  `\t`, `\n`, `\r`, `\\`, and `\"` represent
tab, newline, carriage return, backslash, and double quote.  For example:

```text
[unicode substitutions]
U+2014 = "line one\nline two"
U+2019 = ""
```

An empty string deletes a scalar.  Replacement strings may contain
multiple characters or valid UTF-8 and are emitted once, without
recursive substitution.  Invalid keys or malformed UTF-8 replacement
text reject the entire section with a diagnostic; previous mappings
remain intact.  An empty section leaves previous mappings intact.
Saving options preserves the section.  Each `CharSubstitution` instance
owns a snapshot of its mappings, so later configuration cannot alter an
in-progress substitution.

The shipped additions to the historical `a` Latin table are listed
below.  U+00A0 is also shown to make the two space mappings explicit.
All other U+00A0 through U+00FF mappings retain the historical `a` table.

| Scalar | Replacement |
| --- | --- |
| U+0085 | newline (`\n`) |
| U+00A0, U+202F | space |
| U+2011 | `-` |
| U+2012, U+2013 | `--` |
| U+2014, U+2015 | `---` |
| U+2016 | two vertical bars |
| U+2017 | `_` |
| U+2018 | backtick |
| U+2019 | `'` |
| U+201C, U+201D | `"` |
| U+2020, U+2021 | `+` |
| U+2022 | `*` |
| U+2023 | `>` |
| U+2024 | `.` |
| U+2025 | `..` |
| U+2026 | `...` |
| U+2027 | `-` |
| U+2028, U+2029 | newline (`\n`) |
| U+1F60A | `:)` |

[issue34]: https://github.com/LegalizeAdulthood/trn/issues/34

## Verification

`ModernUtf8/Utf8RoundTripTest` fixes the bytes at scalar-width boundaries,
surrogate boundaries, NUL, CJK, emoji, and the maximum scalar.
`ModernUtf8/Utf8MalformedTest` covers the malformed classes above.
`Utf8ScalarTest` checks error progress, bounded retries, global-state
independence, and a header pipeline that preserves source bytes, decodes
transfer encoding before charset conversion, and gives layout UTF-8
after plain substitution.

`CharSubstTest` checks Latin approximations, unmapped Unicode, identity,
embedded NUL, malformed recovery, first-line bounds, and independent TeX
instances across calls and article lifetimes.  `CompressFromTest`,
`CompressSubjectTest`, and `TreeRenderingTest` cover substitution before
compression and decoded header display, including repeated headers.

`UnicodeSubstitutionTest` checks scalar keys, user precedence, escaped
and empty replacements, validation, and independent mapping snapshots.
`Issue34/UnicodeAsciiMappingTest` checks every scalar in the table above
and verifies that plain mode preserves the same UTF-8 text.  Recovery
tests cover malformed UTF-8, embedded NUL, unmapped scalars, and emitting
replacement text without recursively substituting it.
`UnicodeConfigurationTest` checks configuration loading, opt-in/opt-out,
diagnostics, and saving/reloading the profile without changing defaults.
`ArticleIoTest` compares RFC 2047 headers with MIME bodies after charset
decoding, including Windows-1252 examples and malformed UTF-8.  It also
checks inserted newlines, escaped whitespace, deletion, original source
retention, and cache rereads without repeated substitution.

`CharsetDecoderTest` covers aliases, supported mappings, undefined bytes,
strict UTF-8 fallback, interleaved decoders, and terminal output under a
different legacy charset selection.  `HeaderCharsetTest` covers per-word
selection, malformed and unknown words, whitespace, control filtering,
and name compression without global decoder state.  `ArticleIoTest`
covers transfer/charset ordering, raw-byte retention, cache rereads and
growth, split scalars, MIME boundaries, repeated articles, HTML, and
cache invalidation when conversion mode changes.
