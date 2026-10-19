<!-- Copyright (c) 2026, Richard Thomson -->

# yEnc Extraction

The `e` command recognizes yEnc headers during its existing article
prescan.  A valid `=ybegin ` line requires `line`, `size`, and a final
`name` parameter.  The attachment filename and part numbers come from
this header.  Filenames pass through the same platform policy used by
uuencode extraction: directory components and forbidden characters are
removed, and unsafe basenames receive the existing fallback name.

The decoder follows the [yEnc 1.3 draft][draft].  It operates on original
encoded bytes; display charset conversion and character substitution do
not participate.  Both LF and CRLF line endings are accepted.  Escapes
must remain on one line.  Decoded NUL and high bytes are preserved.

Single-part extraction checks the declared size against the decoded
bytes and trailer.  A supplied CRC32 must match.  Invalid input is
reported before the destination is opened, preserving any existing file.

Multipart articles require a `=ypart` range and matching trailer part
number, size, and part CRC32.  Valid articles are retained in the existing
part directory, so they may arrive out of order.  A missing total is
inferred when a part reaches the declared file end.  Extraction waits
until all numbered parts are present.

Assembly verifies matching filenames, file sizes, part counts, contiguous
nonoverlapping ranges, and any supplied whole-file CRC32.  It writes a
staging file and copies it to the destination only after validation.
Successful assembly removes saved parts.  Failed assembly reports an
error and retains them for a corrected article.

`YencPrescanTest` and `YencDecodeTest` cover metadata, critical escapes,
checksums, corrupt/truncated input, and an independent all-byte fixture.
`YencExtractionTest` covers filenames and multipart storage/assembly.
Command tests exercise `e`, ordinary MIME text wrappers, and the existing
uuencode, custom-command, and shell archive paths.  Byte-preserving local
input and NNTP cache rereads are checked on Windows.

[draft]: https://yence.sourceforge.net/docs/protocol/version1_3_draft.html
