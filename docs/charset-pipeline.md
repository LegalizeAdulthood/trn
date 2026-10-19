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
the selected historical profile after encoded-word decoding.

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

`CharsetDecoderTest` covers aliases, supported mappings, undefined bytes,
strict UTF-8 fallback, interleaved decoders, and terminal output under a
different legacy charset selection.  `HeaderCharsetTest` covers per-word
selection, malformed and unknown words, whitespace, control filtering,
and name compression without global decoder state.  `ArticleIoTest`
covers transfer/charset ordering, raw-byte retention, cache rereads and
growth, split scalars, MIME boundaries, repeated articles, HTML, and
cache invalidation when conversion mode changes.
