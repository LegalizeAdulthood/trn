<!-- Copyright (c) 2026, Richard Thomson -->

# Article Charset Handling and Presentation

## Purpose

This note proposes a long-term article presentation architecture, not a
commitment to particular class names. The existing encoding contract is
documented in [charset-pipeline.md](charset-pipeline.md).

The remaining proposal separates semantic text production, cached
presentation bytes, and terminal layout. Each presentation instance
captures its configuration and owns the state needed to continue decoding.
Layout and backward paging consume the resulting buffer.

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

The presentation decoder advances monotonically. When the display requests
text beyond the buffer's high-water mark, the buffer pulls more UTF-8 from
the presentation view. Moving backward reads only the buffer. It does not
rewind the decoder, restore MIME state, or re-parse source bytes.

Changing terminal width invalidates only display layout. Changing a setting
that affects presentation bytes--such as displayed headers, MIME selection,
charset policy, or character substitutions--requires a new presentation
view and buffer.

## Why this is a view rather than a `std::string_view`

The presentation cannot literally be a `std::string_view`. RFC 2047
decoding, Base64 and quoted-printable decoding, charset conversion, HTML
filtering, and character substitution all change byte values and may change
length. The result is not a contiguous slice of the source storage.

There are two useful interfaces:

* A forward input range that lazily yields UTF-8 bytes. Its iterator owns
  the parser and decoder state plus a small pending-output buffer.
* A cached facade that materializes decoded output and then exposes
  contiguous string-like operations over the cache.

The long-term article presentation design naturally combines them: a
single-pass presentation producer feeds an append-only cache, and display
views refer to the cached UTF-8.

A lazy iterator may need to retain:

* The current source position.
* Header folding and header-selection state.
* An RFC 2047 encoded-word parser.
* A MIME section stack and boundary parser.
* Base64 or quoted-printable transfer-decoder state.
* The active charset decoder for the current text part.
* Stateful character-substitution state, when applicable.
* A small buffer containing pending UTF-8 bytes.

It would initially be an input iterator. It would not promise random
access, constant-time output size, or a `data()` pointer. Replay and
bidirectional access are supplied by `PresentationBuffer`, not by rewinding
the iterator.

## Configuration

An article presentation configuration should be a value captured when the
view is constructed. It should not be a set of references to mutable global
options. It may include:

* The headers to display and their order.
* Header formatting and unfolding policy.
* MIME alternative-selection policy.
* Rules for inline, hidden, described, and externally viewed attachments.
* HTML-to-text behavior.
* Default and fallback charsets.
* Policies for unknown charsets and malformed encoded text.
* A selected character-substitution profile and user mapping table.

Capturing the configuration prevents a view's interpretation from changing
halfway through iteration. Interactive option changes create a newly
configured view instead.

## Header And Body Integration

Unify header selection and body presentation behind one configured
producer. Preserve structural headers needed for MIME interpretation even
when the user hides them from display.

A streaming header producer may need incremental Q/Base64 decoders, or may
allocate once per encoded word. Preserve adjacent encoded-word whitespace
behavior and the existing malformed-input policies.

For MIME bodies, preserve transfer decoding before charset decoding and
keep decoder state with each text part. Move the orchestration of MIME
selection, HTML filtering, attachment descriptions, and substitution under
the presentation instance.

## Presentation buffering and backward paging

Backward paging is purely a display concern. It does not require a
reversible decoder or a mapping back to source positions.

`PresentationBuffer` can minimally contain:

```text
utf8 bytes
logical line-start byte offsets
whether the producer reached end of input
```

When display moves forward beyond buffered data, the buffer resumes the
single-pass presentation producer. When it moves backward, it selects an
earlier range from buffered UTF-8. This avoids re-parsing RFC 2047 encoded
words, MIME boundaries, Base64, quoted-printable, or source charsets.

Terminal-width-dependent visual-line starts belong to `DisplayView`, not to
the decoded buffer. A resize can rebuild the visual-line index over the
same UTF-8 without repeating semantic decoding.

The existing `s_art_buf` is an early form of memoized presentation storage,
and `read_art_buf` exposes lines from it. The main problem is not the
existence of a buffer; it is that `read_art_buf_raw` combines source
reading, MIME parsing, transfer decoding, HTML conversion, synthetic
attachment text, byte-oriented wrapping, and buffer position management
under mutable global state.

## Layout and terminal output

The semantic presentation layer should stop at final UTF-8. The display
layer should own:

* Terminal column width.
* Word wrapping and hard wrapping.
* Visual width and grapheme handling.
* Color and highlighting.
* Cursor movement.
* Current viewport and backward paging.

This separation is especially important because character substitutions can
expand one Unicode character into several display characters. Wrapping must
see the replacement text, not the pre-substitution text.

## Remaining State Ownership

Retire process-global conversion state where legacy display helpers still
depend on it. Header and MIME decoders must remain independent when
presentation instances are interleaved.

The presentation instance should also own header parsing, MIME selection,
pending transfer-decoder output, and substitution state. Capturing settings
by value prevents an interactive option change from altering an existing
view halfway through iteration.

## Possible Migration Sequence

1. **Unify header and body presentation.** Route header filtering,
   RFC 2047, MIME selection, charset conversion, and substitution through
   one configured producer.
2. **Memoize the producer.** Let an append-only presentation buffer serve
   forward display and backward paging without decoder rewinds.
3. **Move layout out of semantic decoding.** Rebuild wrapping and
   visual-line indexing as a view over the decoded UTF-8 buffer.

Establish compatibility requirements before moving behavior.

## Refactoring Test Strategy

- Exercise transfer-decoder and multibyte sequences across producer
  chunk boundaries.
- Interleave presentation instances with different settings and MIME
  parts; verify their state remains independent.
- Verify forward reads extend the buffer exactly once and backward
  paging reads cached text without invoking the producer.
- Verify repeated reads return identical presentation bytes.
- Verify terminal resize rebuilds layout without rebuilding presentation
  bytes.
- Preserve wrapping after substitution expansion without splitting
  UTF-8.
- Verify hidden structural headers still govern MIME interpretation.

## Open Design Decisions

- Whether additional charsets are implemented internally or through a
  conversion library.
- Whether the presentation producer yields bytes, Unicode scalars, text
  spans, or richer events for attachment descriptions and styling.
- Whether `PresentationBuffer` stores only UTF-8 or also indexes logical
  lines and semantic presentation events.
- Whether display width uses Unicode scalars, grapheme clusters, or
  terminal cell sequences, and whether to use a maintained width
  implementation.
- Compatibility requirements for header whitespace, malformed MIME, and
  wrapping behavior.
