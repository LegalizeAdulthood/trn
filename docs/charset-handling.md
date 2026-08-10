# Article Charset Handling and Presentation

## Status and purpose

This document captures the current understanding of character-set handling in
`trn` and a possible long-term design for article presentation. It is a design
note, not an implementation plan or a commitment to particular class names.

The central proposal is that an article remains an immutable sequence of source
bytes. A configured presentation view interprets those bytes and produces the
UTF-8 byte stream that the reader displays. Header selection, RFC 2047 decoding,
MIME processing, charset conversion, and user-selected character substitution
all belong to that presentation transformation.

Screen paging and layout are separate concerns. Decoded presentation bytes can
be memoized as they are produced, allowing backward paging to operate on the
buffer without seeking or re-parsing the encoded article.

## Desired invariants

The design should establish the following explicit invariants:

1. Stored or fetched article data remains raw and immutable.
2. MIME structure and transfer encodings are interpreted from the raw article.
3. Text is converted to Unicode according to its declared or selected charset.
4. The presentation stream is UTF-8.
5. Character substitution consumes Unicode and produces Unicode; it is not a
   charset decoder.
6. The decoded presentation buffer contains final semantic text after character
   substitution, but before terminal-width-dependent layout.
7. Wrapping, viewport positioning, colors, cursor movement, and backward paging
   are display concerns.
8. Decoder and substitution state belongs to an article presentation instance,
   not to process-global variables.

These invariants replace the current situation in which different paths have
different encoding assumptions. In particular, RFC 2047 headers are normally
materialized as UTF-8, while non-UTF-8 MIME body text can remain in its source
charset until it is written to the terminal.

## High-level architecture

The proposed composition is:

```text
raw article bytes
        |
        v
ArticlePresentationView
  - parse and select headers
  - decode RFC 2047 encoded words
  - parse and select MIME parts
  - decode MIME transfer encodings
  - convert text charsets to Unicode
  - apply character substitution
  - encode presentation text as UTF-8
        |
        v
PresentationBuffer
  - append-only UTF-8 storage
  - memoizes bytes already produced
  - optionally indexes logical line starts
        |
        v
DisplayView
  - terminal-width wrapping
  - visual line index
  - forward and backward paging
  - colors, highlighting, and terminal output
```

The presentation decoder advances monotonically. When the display requests text
beyond the buffer's high-water mark, the buffer pulls more UTF-8 from the
presentation view. Moving backward reads only the buffer. It does not rewind the
decoder, restore MIME state, or re-parse source bytes.

Changing terminal width invalidates only display layout. Changing a setting
that affects presentation bytes--such as displayed headers, MIME selection,
charset policy, or character substitutions--requires a new presentation view
and buffer.

## Why this is a view rather than a `std::string_view`

The presentation cannot literally be a `std::string_view`. RFC 2047 decoding,
Base64 and quoted-printable decoding, charset conversion, HTML filtering, and
character substitution all change byte values and may change length. The result
is not a contiguous slice of the source storage.

There are two useful interfaces:

* A forward input range that lazily yields UTF-8 bytes. Its iterator owns the
  parser and decoder state plus a small pending-output buffer.
* A cached facade that materializes decoded output and then exposes contiguous
  string-like operations over the cache.

The long-term article presentation design naturally combines them: a
single-pass presentation producer feeds an append-only cache, and display views
refer to the cached UTF-8.

A lazy iterator may need to retain:

* The current source position.
* Header folding and header-selection state.
* An RFC 2047 encoded-word parser.
* A MIME section stack and boundary parser.
* Base64 or quoted-printable transfer-decoder state.
* The active charset decoder for the current text part.
* Stateful character-substitution state, when applicable.
* A small buffer containing pending UTF-8 bytes.

It would initially be an input iterator. It would not promise random access,
constant-time output size, or a `data()` pointer. Replay and bidirectional access
are supplied by `PresentationBuffer`, not by rewinding the iterator.

## Configuration

An article presentation configuration should be a value captured when the view
is constructed. It should not be a set of references to mutable global options.
It may include:

* The headers to display and their order.
* Header formatting and unfolding policy.
* MIME alternative-selection policy.
* Rules for inline, hidden, described, and externally viewed attachments.
* HTML-to-text behavior.
* Default and fallback charsets.
* Policies for unknown charsets and malformed encoded text.
* A selected character-substitution profile and user mapping table.

Capturing the configuration prevents a view's interpretation from changing
halfway through iteration. Interactive option changes create a newly configured
view instead.

## Header processing

The proposed header pipeline is:

```text
raw header fields
    -> parse field names and folded continuations
    -> retain structural fields needed for MIME processing
    -> select fields configured for display
    -> decode RFC 2047 where encoded words are permitted
    -> convert each encoded word's charset to Unicode
    -> apply user character substitution
    -> emit UTF-8
```

Filtering displayed headers must not prevent structural headers such as
`Content-Type` and `Content-Transfer-Encoding` from being parsed internally.
RFC 2047 decoding should apply to appropriate human-readable field components,
not blindly to every structured header token.

### Existing RFC 2047 implementation

[`decode_header_impl`](../libtrn/cache.cpp) scans a header string and recognizes
encoded words of the form:

```text
=?charset?Q?encoded-text?=
=?charset?B?encoded-text?=
```

It uses [`qp_decode_string`](../libtrn/mime.cpp) for Q encoding and
[`b64_decode_string`](../libtrn/mime.cpp) for Base64. Header-mode Q decoding
implements the rule that underscore represents space. The decoded bytes are
converted to UTF-8 with [`create_utf8_copy`](../libtrn/utf.cpp).

The charset resolver currently recognizes the following designators,
case-insensitively:

| Charset | Recognized designators |
| --- | --- |
| US-ASCII | `US`, `us-ascii`, `ascii` |
| UTF-8 | `UTF`, `utf-8`, `utf8` |
| ISO-8859-1 | `Latin1`, `iso8859-1`, `iso-8859-1` |
| ISO-8859-15 | `Latin9`, `iso8859-15`, `iso-8859-15` |
| Windows-1252 | `CP1252`, `cp1252`, `windows-1252` |

The implementation is eager: Q/Base64 helpers and charset conversion return
owned strings. That is suitable for a cached facade. A truly streaming view
would need incremental Q/Base64 decoders or would allocate once per encoded
word.

Whitespace between adjacent encoded words requires deliberate handling. RFC
2047 linear whitespace between adjacent encoded words is not displayed. A
streaming implementation can defer whitespace until it knows whether the next
item is another encoded word. If compatibility with the current implementation
is required, its precise whitespace behavior should be captured in tests first.

Malformed encoded words and unknown charsets need an explicit policy. Suitable
choices include preserving the source encoded word, emitting a replacement
character, or reporting a decoding error. An unknown charset must never silently
inherit the charset state left by earlier text.

## MIME body processing

The proposed body pipeline is:

```text
raw MIME message
    -> parse MIME structure and boundaries
    -> select the body part to present
    -> decode Content-Transfer-Encoding
    -> decode the part's declared charset to Unicode
    -> convert HTML or other textual forms when configured
    -> apply user character substitution
    -> emit UTF-8
```

Transfer decoding must occur before charset decoding. Base64 and
quoted-printable operate on encoded bytes; the resulting bytes belong to the
charset declared by the MIME part.

### Existing MIME implementation

[`MimeSection::mime_parse_type`](../libtrn/mime.cpp) parses the media type and
its charset parameter. [`MimeSection::mime_parse_encoding`](../libtrn/mime.cpp)
recognizes transfer encodings. [`read_art_buf_raw`](../libtrn/artio.cpp) handles
MIME boundaries, alternative and skipped parts, attachment descriptions,
Base64, quoted-printable, and HTML filtering.

The important current limitation is that MIME body bytes are not consistently
materialized as UTF-8 in the article buffer. `mime_parse_type` configures the
global UTF conversion state, but the body often remains in its source charset
after transfer decoding. [`put_char_adv`](../libtrn/utf.cpp) converts one
character at a time while writing to the terminal.

The present flow is therefore often:

```text
transfer decode
    -> source-charset bytes in s_art_buf
    -> charset conversion during terminal output
```

The desired flow is:

```text
transfer decode
    -> charset conversion
    -> UTF-8 in PresentationBuffer
    -> encoding-independent display
```

Each MIME text section should retain its charset or an initialized decoder. A
global current input charset is insufficient for nested or multipart messages
and makes restoration and error handling implicit.

## Character substitution

Character substitution should be the final semantic text transformation. It
must run after RFC 2047 or MIME transfer decoding and after charset conversion,
but before the decoded presentation bytes are buffered and before layout is
computed.

The required invariant is:

> Character substitution receives valid UTF-8, operates on Unicode scalar
> values, and produces UTF-8.

This is the common solution for:

* [Issue #17: Restore original character encoding conversions](https://github.com/LegalizeAdulthood/trn/issues/17)
* [Issue #34: Add a "UTF-8 => ASCII" code conversion](https://github.com/LegalizeAdulthood/trn/issues/34)

### Why the existing substitution corrupts UTF-8

[`str_char_subst`](../libtrn/charsubst.cpp) iterates over `char` values one byte
at a time. Bytes above `0x9f` are interpreted through an ISO-8859-1-to-ASCII
table, while bytes in the range `0x80` through `0x9f` become spaces.

For example, U+2019 RIGHT SINGLE QUOTATION MARK is the UTF-8 sequence:

```text
E2 80 99
```

The current substitution code interprets those as three unrelated Latin-1
bytes. It cannot preserve the character or map it as one unit.

The older `put_subst_char` path has related problems: it accepts one byte,
assumes fixed-size replacement storage, writes directly to the terminal, and
uses shared state for TeX conversion. Under `USE_UTF_HACK`, ordinary body output
uses `put_char_adv` instead of this historical substitution path. Consequently,
body charset output can work while the original body substitutions are bypassed,
and header or selector substitutions can still corrupt UTF-8.

### Unicode-aware substitution engine

A replacement substitution engine should:

1. Decode the next valid UTF-8 sequence into a Unicode scalar value.
2. Look up that value in the selected substitution profile.
3. Emit the mapped replacement when found.
4. Otherwise copy the original UTF-8 sequence unchanged.
5. Apply an explicit malformed-UTF-8 policy.

It should be a pure transformation. It should yield or append output rather than
calling terminal functions, querying terminal width, or depending on global
charset state.

The identity/off profile must preserve valid UTF-8 byte-for-byte.

### Restoring historical conversions

The existing ISO-to-ASCII tables remain useful. Their keys should be understood
as Unicode values U+00A0 through U+00FF rather than raw byte values `0xA0`
through `0xFF`. The historical modes can then become named Unicode substitution
profiles, for example:

* ISO-derived characters to US monospaced approximations.
* ISO-derived characters to general US ASCII approximations.
* TeX notation to Unicode.
* No substitution.

With this representation, mapped Latin characters are transliterated while
unmapped CJK, emoji, and other Unicode pass through intact.

TeX conversion is stateful. Its pending escape state should belong to the
substitution instance owned by the presentation view. It should recognize ASCII
TeX sequences, emit Unicode characters, and pass unrelated Unicode through
unchanged. State must survive input chunk boundaries but must not leak between
articles or iterators.

Charset selection and substitution selection should be separate concepts. The
`USE_UTF_HACK` branch of `current_char_subst` currently reports input/output
charset names in a UI location historically associated with substitution mode.
The future design should report them independently.

### User-defined Unicode-to-ASCII mappings

Issue #34 asks for an optional, user-supplied table. It should be keyed by
Unicode scalar values, not by encoded byte sequences. A conceptual table might
contain:

```text
U+2011 -> "-"
U+2014 -> "---"
U+2018 -> "`"
U+2019 -> "'"
U+2026 -> "..."
U+1F60A -> ":)"
```

Replacement values should be allowed to contain multiple characters, escaped
whitespace or newlines, an empty string if deletion is supported, and possibly
UTF-8 text even though the initial use case is ASCII. The profile is disabled
by default. Configuration must define whether user mappings replace or overlay
a built-in profile and which mapping has precedence.

Several single-byte examples in issue #34, such as `0x91`, `0x96`, and `0x97`,
are Windows-1252 bytes rather than complete UTF-8 characters. Charset conversion
must resolve them first:

```text
Windows-1252 0x91
    -> U+2018
    -> user substitution
    -> "`"
```

Encoding-specific aliases do not belong in the substitution table.

## Presentation buffering and backward paging

Backward paging is purely a display concern. It does not require a reversible
decoder or a mapping back to source positions.

`PresentationBuffer` can minimally contain:

```text
utf8 bytes
logical line-start byte offsets
whether the producer reached end of input
```

When display moves forward beyond buffered data, the buffer resumes the
single-pass presentation producer. When it moves backward, it selects an earlier
range from buffered UTF-8. This avoids re-parsing RFC 2047 encoded words, MIME
boundaries, Base64, quoted-printable, or source charsets.

Terminal-width-dependent visual-line starts belong to `DisplayView`, not to the
decoded buffer. A resize can rebuild the visual-line index over the same UTF-8
without repeating semantic decoding.

The existing `s_art_buf` is an early form of memoized presentation storage, and
`read_art_buf` exposes lines from it. The main problem is not the existence of a
buffer; it is that `read_art_buf_raw` combines source reading, MIME parsing,
transfer decoding, HTML conversion, synthetic attachment text, byte-oriented
wrapping, and buffer position management under mutable global state.

## Layout and terminal output

The semantic presentation layer should stop at final UTF-8. The display layer
should own:

* Terminal column width.
* Word wrapping and hard wrapping.
* Visual width and grapheme handling.
* Color and highlighting.
* Cursor movement.
* Current viewport and backward paging.

This separation is especially important because character substitutions can
expand one Unicode character into several display characters. Wrapping must see
the replacement text, not the pre-substitution text.

The current code has useful UTF-aware width functions, but it also retains
byte-oriented layout. `read_art_buf_raw` performs wrapping using byte offsets,
and some selector/header paths compare or resize strings by byte count. Such
operations can split a UTF-8 sequence or confuse byte length with terminal
width.

## Existing UTF-8 support: assessment

The existing implementation is closer functionally than architecturally. For
ordinary articles using UTF-8, US-ASCII, ISO-8859-1, ISO-8859-15, or
Windows-1252, much of the decoding machinery already exists.

| Area | Existing support | Main gap |
| --- | --- | --- |
| UTF-8 primitives | Byte length, code-point decoding, encoding, approximate width | Strict validation and explicit state |
| RFC 2047 | Q/Base64 decoding and supported charset conversion | Eager/global-state design and explicit error policy |
| MIME body | Structure, alternatives, transfer decoding, HTML filtering | Body is not normalized into a UTF-8 buffer |
| Charset support | ASCII, UTF-8, Latin-1, Latin-9, CP1252 | Limited registry and global mutable decoder state |
| Character substitution | Historical mapping tables | Byte-oriented engine and incomplete integration |
| Presentation buffer | Existing `s_art_buf` memoization | Mixed semantic decoding and display layout |
| Display width | Basic combining and wide-character handling | Approximate tables and remaining byte assumptions |

A rough qualitative estimate is:

* UTF-8 display primitives: substantially present.
* RFC 2047 and MIME transfer decoding: mostly present for supported cases.
* Unicode-safe character substitution: largely missing despite reusable tables.
* The configured article presentation view: an architectural refactoring rather
  than a small wrapper, although `read_art_buf_raw` already performs many of the
  necessary transformations.

### UTF-8 correctness gaps

The current UTF-8 decoder accepts historical five- and six-byte forms. Modern
UTF-8 permits only one through four bytes. Validation should reject:

* Invalid continuation bytes.
* Truncated sequences.
* Overlong encodings.
* UTF-16 surrogate values.
* Scalar values above U+10FFFF.

The response to malformed input must be defined: replacement character,
preservation of offending bytes, or a surfaced error. Streaming decoding must
retain an incomplete sequence across chunk boundaries rather than prematurely
classifying it as invalid.

`visual_width_at` is intentionally approximate. A future display layer should
decide whether its unit is a Unicode scalar, grapheme cluster, or terminal cell
sequence and should use a maintained width implementation if full correctness is
required.

### Global-state gaps

[`utf_init`](../libtrn/utf.cpp) mutates a process-global `GraphicState`. RFC 2047
decoding saves and restores that state; MIME parsing changes it when a text
part's charset is encountered. Unknown charset names leave previously selected
state in place. This makes decoding context-dependent and prevents safe
interleaving of views.

The long-term replacement should use explicit value or state objects such as:

```text
CharsetRegistry
CharsetDecoder
Utf8Decoder
Rfc2047Decoder
MimePresentationState
CharacterSubstituter
```

These names are illustrative. The important requirement is ownership: each
article presentation instance owns the state needed to continue decoding.

## Error and fallback policy

The configured view needs consistent policies for:

* Malformed RFC 2047 encoded words.
* Unknown RFC 2047 charsets.
* Malformed Base64 and quoted-printable input.
* Missing MIME charset parameters.
* Unknown MIME charsets.
* Invalid UTF-8.
* Unsupported MIME parts.
* Invalid user substitution entries.

Preserving undecodable source text is often preferable for a newsreader, but
the policy should be explicit and testable. An error must not cause a decoder to
reuse state from a previous field, part, article, or view.

## Possible migration sequence

The design can be approached incrementally without requiring the complete view
in one change:

1. **Define strict UTF-8 scalar operations.** Introduce explicit, pure decoding
   and encoding primitives with a malformed-input policy and no global charset
   dependency.
2. **Make character substitution Unicode-aware.** Re-express the historical
   tables as Unicode mappings, add a pure substitution state object, and add the
   user mapping profile requested by issue #34.
3. **Separate charset decoding from substitution.** Represent charset choice and
   substitution profile as independent configuration values and UI state.
4. **Make charset conversion state explicit.** Replace global `GraphicState`
   dependencies with decoders owned by a header encoded word or MIME section.
5. **Normalize MIME text to UTF-8 before buffering.** Move conversion out of
   terminal output and establish UTF-8 as the article presentation invariant.
6. **Unify header and body presentation.** Route header filtering, RFC 2047,
   MIME selection, charset conversion, and substitution through one configured
   producer.
7. **Memoize the producer.** Let an append-only presentation buffer serve
   forward display and backward paging without decoder rewinds.
8. **Move layout out of semantic decoding.** Rebuild wrapping and visual-line
   indexing as a view over the decoded UTF-8 buffer.
9. **Retire scattered call-site conversions.** Selector summaries, header tree
   output, replies, and other text consumers should use the same pure charset
   and substitution components appropriate to their input invariants.

This sequence is illustrative; tests should establish current compatibility
requirements before behavior is moved.

## Test strategy

Tests should be organized around the pipeline invariant rather than individual
terminal calls.

### UTF-8 primitives

* Empty and ASCII input.
* Valid two-, three-, and four-byte sequences.
* Code points at encoding boundaries.
* Rejection of overlong, surrogate, out-of-range, truncated, and malformed
  sequences.
* UTF-8 sequences split across producer chunks.

### RFC 2047 headers

* Q and Base64 encoded words.
* Every recognized charset spelling.
* Multiple encoded words with intervening whitespace.
* Mixed literal ASCII and encoded words.
* Unknown charset and malformed-word policies.
* Header filtering that still preserves MIME structural interpretation.

### MIME bodies

* Plain UTF-8, Latin-1, Latin-9, and Windows-1252 text.
* Base64 and quoted-printable transfer encodings.
* Multipart alternatives with different charsets.
* Nested MIME messages.
* HTML text and attachment descriptions.
* Unknown or missing charset behavior.
* Transfer and multibyte characters split across input chunks.

### Character substitution

* Off mode preserves valid UTF-8 byte-for-byte.
* Historical Latin-derived mappings retain their expected ASCII output.
* Unmapped CJK, combining characters, and emoji remain intact.
* The mappings described in issue #34.
* Windows-1252 bytes become Unicode before substitution.
* Multi-character, newline, and empty replacements.
* TeX state across chunk boundaries and isolation between views.
* Identical results for equivalent text originating in an RFC 2047 header and a
  MIME body.

### Buffering and display

* Forward reads extend the buffer exactly once.
* Backward paging never invokes the decoder.
* Repeated reads return identical UTF-8.
* Terminal resize rebuilds layout without rebuilding presentation bytes.
* Wrapping never splits UTF-8 and occurs after character expansion.

## Relevant current implementation locations

The primary implementation areas discussed here are:

* [`libtrn/cache.cpp`](../libtrn/cache.cpp): RFC 2047 header decoding.
* [`libtrn/mime.cpp`](../libtrn/mime.cpp): MIME parsing, transfer decoders, HTML
  filtering, and charset parameter handling.
* [`libtrn/artio.cpp`](../libtrn/artio.cpp): article presentation buffer and the
  current combined MIME/HTML/wrapping pipeline.
* [`libtrn/utf.cpp`](../libtrn/utf.cpp): charset registry, code-point conversion,
  UTF-8 encoding/decoding, width estimation, and terminal output conversion.
* [`libtrn/charsubst.cpp`](../libtrn/charsubst.cpp): historical character
  substitution tables and modes.
* [`libtrn/art.cpp`](../libtrn/art.cpp): article display loop and final character
  output.
* [`libtrn/rt-wumpus.cpp`](../libtrn/rt-wumpus.cpp): displayed header decoding,
  substitution, wrapping, and tree integration.
* [`libtrn/rt-util.cpp`](../libtrn/rt-util.cpp): subject/author compression and
  remaining UTF-aware and byte-oriented string operations.

## Open design decisions

Before implementation, the following decisions should be made explicitly:

* Whether malformed encoded text is preserved, replaced, or reported.
* The default charset for MIME text without a charset parameter.
* Whether additional charsets are implemented internally or through a standard
  conversion library.
* The configuration syntax for Unicode substitution mappings.
* Whether user mappings overlay or replace historical profiles.
* Whether TeX conversion remains a selectable legacy profile and where it sits
  relative to general Unicode substitution.
* Whether the presentation producer yields bytes, Unicode scalars, text spans,
  or richer events for attachment descriptions and styling.
* Whether `PresentationBuffer` stores only UTF-8 or also indexes logical lines
  and semantic presentation events.
* The compatibility requirements for current header whitespace, malformed MIME,
  and wrapping behavior.

Regardless of those choices, the stable architectural boundary remains the
same: raw article bytes are interpreted once into a configured UTF-8
presentation buffer, and display views page and lay out that buffer without
re-running the decoders.
