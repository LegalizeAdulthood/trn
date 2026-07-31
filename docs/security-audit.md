<!-- Copyright (c) 2026, Richard Thomson -->

# Security Audit

## Scope

This note records a defensive audit of NNTP and article parsing paths for
memory-safety and related attacker-controlled input risks.  The review
focuses on code reachable from constructed Usenet articles and NNTP
server replies.

Vendored `vcpkg` code and the `support` folder are out of scope.

This document does not include exploit payloads.  It records reachable
failure modes and remediation work.

## Summary

The code does not show many direct fixed-stack-buffer overwrite
opportunities in the audited paths.  The main risks are:

- unbounded network reads
- unchecked server-provided article ranges
- unbounded storage of headers, overview fields, and references
- partial-line parser confusion
- integer narrowing around sizes and positions
- shell command construction from MIME article fields

Several of these are denial-of-service or parser-state issues rather
than classic control-flow buffer overflows.  They are still security
issues because a hostile NNTP peer or crafted article can choose the
allocation size, loop count, or command text.

## Findings

### 1. Unbounded NNTP Line Reads

Severity: high

Relevant code:

- `nntp/nntpinit.cpp`: `NNTPConnection::read_line`
- `nntp/nntpclient.cpp`: `nntp_gets`, `nntp_check`
- `nntp/include/nntp/nntpclient.h`: `nntp_get_a_line`
- `libtrn/rt-ov.cpp`: overview and XOVER readers
- `libtrn/head.cpp`: header and XHDR readers

`NNTPConnection::read_line` calls `boost::asio::read_until` with `"\r\n"`
and no project-level maximum line size.  A peer that sends a very long
line, or that never sends a line terminator, can force the stream buffer
to grow until allocation failure or process termination.

Some callers also ignore the error status.  `nntp_get_a_line` returns a
string and discards the `std::error_code`, which makes callers parse an
empty or stale-looking value as protocol data.  XOVER and XHDR loops are
especially sensitive because they wait for a list terminator.

Likely impact:

- remote denial of service through unbounded allocation
- protocol parser confusion after connection errors
- retained large string capacity in process state
- downstream allocation pressure from huge header or overview fields

Remediation:

- Add one bounded NNTP line-reading primitive.
- Give each reply kind an explicit maximum size.
- Return a structured result containing status, text, and overlong state.
- Close the connection on overlong input before parsing further data.
- Make error handling mandatory for all list-reading loops.
- Remove or replace `nntp_get_a_line`.

Suggested caps:

- status and command replies: small, for example 4 KiB
- active and list records: moderate, for example 16 KiB
- article header lines and overview records: larger but finite
- total article header bytes: separate aggregate cap
- article body copy: separate total byte cap or policy limit

### 2. Unchecked Server-Provided Article Ranges

Severity: high

Relevant code:

- `libtrn/nntp.cpp`: `nntp_group`
- `libtrn/ngdata.cpp`: `NewsgroupData::get_newsgroup_size`
- `libtrn/ngdata.cpp`: `grow_newsgroup`
- `libtrn/bits.cpp`: `find_existing_articles`
- `libtrn/datasrc.cpp`: active file parsing

The code trusts article numbers and ranges from `GROUP`, active data, and
`LISTGROUP`.  A hostile server can report very large high-water marks,
inverted ranges, negative values that cross type boundaries, or article
numbers outside the selected group range.

`find_existing_articles` parses `LISTGROUP` entries with `from_chars`,
but does not check the parse result and only rejects article numbers below
`g_abs_first`.  Entries above `g_last_art` can still create article map
entries or influence per-article state.

`grow_newsgroup` can loop toward a server-provided high article number
and allocate many `Article` objects.

Likely impact:

- remote CPU and memory exhaustion
- creation of article state outside the selected group range
- bad parser state after failed numeric conversion
- large loops driven entirely by NNTP server text

Remediation:

- Check every `from_chars` result.
- Reject negative article numbers.
- Require `0 <= low <= high`.
- Enforce a maximum group span accepted from a server reply.
- Clamp or reject `LISTGROUP` entries outside `[g_abs_first, g_last_art]`.
- Treat invalid active, group, and list records as protocol errors.
- Add tests for overflow, parse failure, inverted ranges, and huge ranges.

### 3. Unbounded Header, Overview, and References Storage

Severity: medium-high

Relevant code:

- `libtrn/head.cpp`: `parse_header`, `prefetch_remote_lines`
- `libtrn/rt-ov.cpp`: `ov_data`, `ov_parse`
- `libtrn/art.cpp`: `Article::thread_article`
- `libtrn/rt-process.cpp`: `get_article`
- `libtrn/hash.cpp`: hash table key storage

Remote headers, XOVER fields, XHDR fields, message IDs, and `References`
values are retained in strings and hash tables with no clear project cap.

`Article::thread_article` walks a `References` header from right to left
and can call `get_article` for many unseen message IDs.  `get_article`
allocates fake `Article` objects for unknown references.  A single crafted
article can therefore create many retained objects.

The message-id validator checks basic shape, but does not enforce a
maximum message-id length.

Likely impact:

- memory exhaustion through large headers or overview fields
- memory exhaustion through many fake reference articles
- CPU cost from very long reference chains
- very large hash keys and collision-heavy hash buckets

Remediation:

- Cap total header bytes per article.
- Cap individual header and overview field sizes.
- Cap message-id length before hashing or storing.
- Cap references processed per article.
- Cap fake reference articles per article and per group.
- Drop or truncate nonessential cached fields that exceed policy.
- Prefer `size_t` for key lengths and reject oversized hash keys.

### 4. Partial-Line Parser Confusion in `nntp_gets`

Severity: medium

Relevant code:

- `nntp/nntpclient.cpp`: `nntp_gets`
- `libtrn/datasrc.cpp`: remote active and newsgroups readers
- `libtrn/rt-ov.cpp`: overview format reader
- `libtrn/bits.cpp`: `LISTGROUP` reader

`nntp_gets` truncates long lines into caller-sized chunks.  When it
returns `len - 1` bytes, it saves the remaining text with `substr(len)`.
That drops one byte from the original line.  Several callers then parse
partial chunks as if they were complete records.

Likely impact:

- corrupted NNTP records
- active file poisoning
- incorrect overview schema parsing
- incorrect article-number parsing
- hard-to-debug state changes after overlong lines

Immediate remediation:

- Change the continuation index to `len - 1`.

Preferred remediation:

- Replace `nntp_gets` with the bounded structured line reader.
- Distinguish complete, partial, overlong, and error results.
- Make record parsers reject and drain overlong records.
- Add tests showing that overlong records are not parsed as valid data.

### 5. MIME Shell Command Construction

Severity: medium

Relevant code:

- `libtrn/mime.cpp`: `mime_exec`
- `libtrn/mime.cpp`: `mime_parse_type`
- `libtrn/mime.cpp`: `mime_find_param`

This is not a memory-safety issue, but it is reachable from crafted
article content.  MIME type names and MIME parameters can flow into
mimecap command construction.  The `%t` and `%{param}` substitutions are
wrapped in single quotes, but embedded quote characters are not escaped.

`%s` uses the decoded filename path, which is more constrained by
filename sanitization.  The MIME type and parameter paths need equivalent
hardening.

Likely impact:

- command injection when a matching mimecap entry is used
- unintended command arguments from article-controlled MIME fields

Remediation:

- Avoid shell execution and spawn viewers with an argv vector.
- If shell execution must remain, escape single quotes correctly.
- Validate MIME type tokens and parameter values before substitution.
- Reject control characters and oversized MIME parameter values.
- Add tests for quoted parameters and shell metacharacters.

### 6. Integer Narrowing and Buffer Position Arithmetic

Severity: medium

Relevant code:

- `libtrn/artio.cpp`: article buffer reads and views
- `libtrn/head.cpp`: header-line slicing
- `libtrn/hash.cpp`: hash key lengths
- `libtrn/include/trn/mime.h`: MIME `short` fields
- `libtrn/mime.cpp`: MIME part, total, and boundary length parsing

Several paths convert sizes or positions between `size_t`, `int`,
`short`, unsigned aliases, and article-position types.  These conversions
are safe only if earlier parser invariants hold.  Hostile inputs should
not be able to break those invariants.

`artio.cpp` grows the article buffer by a fixed increment in some paths.
If a position ever gets far ahead of the buffer, one growth step is not
enough before pointer arithmetic.  This currently appears more likely to
produce crashes or invalid state than an easy overwrite, but it should be
hardened.

`head.cpp` computes header slices using unsigned sizes after arithmetic on
article positions.  That should be bounds checked before constructing a
view.

MIME section fields store part numbers, total part counts, and boundary
lengths in `short`.  Very large article-controlled values should be
rejected before narrowing.

Likely impact:

- out-of-bounds reads or writes if parser invariants are broken
- crashes from bad buffer positions
- incorrect MIME multipart handling after numeric overflow
- bad hash table behavior with oversized keys

Remediation:

- Use `size_t` for byte counts and string lengths.
- Check before narrowing to `int`, `short`, or article-position types.
- Grow buffers to the exact required size, or loop until sufficient.
- Bounds check every view before constructing it.
- Replace MIME `short` fields with checked wider types.
- Reject MIME part numbers, totals, and boundary lengths outside policy.

### 7. Body Copy Without Total Size Policy

Severity: medium

Relevant code:

- `libtrn/nntp.cpp`: `nntp_copy_body`
- `libtrn/nntp.cpp`: `nntp_finish_body`

Remote article bodies are copied from NNTP to a temporary file through
bounded chunks, so this is not a direct buffer overflow.  There is no
clear total body byte limit, however.  A hostile server can stream a very
large article body and consume disk space or keep the reader busy.

Likely impact:

- disk exhaustion
- long-running body downloads
- expensive decoding or MIME processing after download

Remediation:

- Add a maximum body byte policy for remote articles.
- Stop copying and close the connection when the limit is exceeded.
- Surface a clear article-too-large state to the UI.
- Apply decode and MIME limits consistently after body retrieval.

### 8. Hash Table Exposure to Attacker-Controlled Keys

Severity: medium

Relevant code:

- `libtrn/hash.cpp`
- `libtrn/rt-process.cpp`
- `libtrn/ngdata.cpp`

The internal hash function is a simple byte sum.  Newsgroup names,
subjects, message IDs, and references can be influenced by article or
server data.  The simple hash makes collision-heavy inputs cheap to
construct, which can turn lookups into large linear scans.

The hash implementation also narrows key sizes to `int`.

Likely impact:

- CPU denial of service through collision-heavy keys
- memory pressure from many retained keys
- incorrect behavior if very large key sizes are narrowed

Remediation:

- Cap all hash key lengths before insertion.
- Use `size_t` for stored key lengths.
- Replace the byte-sum hash with a stronger bounded hash.
- Consider standard containers where ownership and length handling are
  simpler.

## Remediation Order

1. Add bounded NNTP line reading and mandatory error handling.
2. Validate server-provided ranges and article numbers.
3. Fix or replace `nntp_gets` partial-line handling.
4. Add caps for headers, overview fields, message IDs, and references.
5. Harden MIME command execution.
6. Remove unsafe narrowing around sizes, positions, and MIME fields.
7. Add body-size policy for remote articles.
8. Strengthen hash table input limits and hashing.

## Test Plan

Add mock NNTP tests for:

- overlong status lines
- lines without CRLF
- XOVER and XHDR connection close before list terminator
- huge `GROUP` high-water marks
- inverted `GROUP` and active ranges
- `LISTGROUP` values below and above the selected group range
- overlong active and overview records
- overlong article headers
- excessive `References` chains
- oversized message IDs
- MIME parameters containing quotes and shell metacharacters
- MIME part numbers, totals, and boundaries outside accepted range
- article bodies that exceed the configured body limit

Add parser unit tests for:

- numeric conversion failure
- integer overflow during numeric conversion
- bounds-checked header slicing
- `nntp_gets` continuation behavior until it is replaced
- hash key length rejection

## Code Review Checklist

Before accepting remediation patches, verify that:

- no NNTP reply is read without a maximum size
- no parser treats partial data as a complete protocol record
- every server-provided number checks conversion success
- every article number is checked against the current group range
- article header and body sizes have explicit limits
- message IDs and references have explicit limits
- MIME article fields cannot reach shell syntax unsafely
- narrowing conversions are preceded by range checks
- tests cover close, timeout, overlong, and malformed reply paths
