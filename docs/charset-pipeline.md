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

## Verification

`ModernUtf8/Utf8RoundTripTest` fixes the bytes at scalar-width boundaries,
surrogate boundaries, NUL, CJK, emoji, and the maximum scalar.
`ModernUtf8/Utf8MalformedTest` covers the malformed classes above.
`Utf8ScalarTest` checks error progress, bounded retries, global-state
independence, and a header pipeline that preserves source bytes, decodes
transfer encoding before charset conversion, and gives layout UTF-8
after plain substitution.
