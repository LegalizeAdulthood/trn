<!-- Copyright (c) 2026, Richard Thomson -->

# NNTP Legacy Conversations

## Scope

This audit identifies NNTP client conversations in trn that predate, or
do not fully use, the current RFC 3977 family of behavior.  It focuses on
wire-level command sequences, not local spool behavior.

Primary references:

- RFC 3977: <https://www.rfc-editor.org/rfc/rfc3977.html>
- RFC 4642: <https://www.rfc-editor.org/rfc/rfc4642.html>
- RFC 4643: <https://www.rfc-editor.org/rfc/rfc4643.html>
- RFC 8143: <https://www.rfc-editor.org/rfc/rfc8143.html>

## Baseline

RFC 3977 replaces RFC 977, formalizes extension discovery with
`CAPABILITIES`, and standardizes commands that older clients often used
through private `X` extensions.

Modern baseline commands relevant to this code include:

- session: `CAPABILITIES`, `MODE READER`, `QUIT`
- group and article: `GROUP`, `LISTGROUP`, `NEXT`, `LAST`
- retrieval: `ARTICLE`, `HEAD`, `BODY`, `STAT`
- posting: `POST`
- information: `DATE`, `NEWGROUPS`, `LIST`, `LIST ACTIVE`,
  `LIST NEWSGROUPS`
- fields: `OVER`, `LIST OVERVIEW.FMT`, `HDR`, `LIST HEADERS`

RFC 4643 defines `AUTHINFO` capability discovery and authentication
behavior.  RFC 4642 defines `STARTTLS`; RFC 8143 updates TLS use to
current TLS best practices.

## Audit

### Session Startup

Current code:

- `nntp/nntpinit.cpp` reads the greeting.
- It then always tries `MODE READER`.
- It ignores `MODE READER` only if the server replies `500`.
- It never asks `CAPABILITIES`.

Modern concern:

- Current servers advertise commands and extensions through
  `CAPABILITIES`.
- The client should not infer support only by trial commands.
- `MODE READER` should be used when needed, then capabilities should be
  refreshed because the available command set can change.

### Capability Discovery

Current code:

- No production NNTP code sends `CAPABILITIES`.
- Support for `LIST`, `LIST ACTIVE`, `LISTGROUP`, `XOVER`, `XHDR`,
  `XGTITLE`, and `LIST SUBSCRIPTIONS` is discovered by optimistic use and
  fallback.

Modern concern:

- RFC 3977 made capability discovery central.
- Fallback can remain for old servers, but modern paths should be chosen
  from advertised capabilities first.

### Overview

Current code:

- `libtrn/rt-ov.cpp` probes `XOVER`.
- Remote overview fetches use `XOVER first-last`.
- Overview format uses `LIST overview.fmt`.

Modern concern:

- RFC 3977 formalized `XOVER` as `OVER`.
- RFC 3977 names the format command `LIST OVERVIEW.FMT`.
- `XOVER` can remain as an old-server fallback.

### Header Fetch

Current code:

- `libtrn/head.cpp` uses `XHDR field article`.
- It also uses `XHDR field first-last`.
- `XHDR Broken = y` is a configuration fallback knob.

Modern concern:

- RFC 3977 formalized this as `HDR`.
- `LIST HEADERS` can describe available header and metadata fields.
- `XHDR` can remain as an old-server fallback.

### Group Descriptions

Current code:

- `libtrn/datasrc.cpp` can fetch descriptions through `LIST NEWSGROUPS`.
- It falls back to `XGTITLE group`.

Modern concern:

- `LIST NEWSGROUPS [wildmat]` is the standardized command.
- `XGTITLE` is a historical extension and should be guarded by explicit
  extension support or old-server fallback logic.

### Subscriptions

Current code:

- `libtrn/rcstuff.cpp` tries `LIST SUBSCRIPTIONS` for remote initial
  newsrc creation.

Modern concern:

- `LIST SUBSCRIPTIONS` is not a standard RFC 3977 LIST keyword.
- It should be treated as an extension, used only if advertised or as a
  clearly marked compatibility probe.

### Authentication

Current code:

- `nntp/nntpauth.cpp` reacts to auth failures by sending
  `AUTHINFO USER` and `AUTHINFO PASS`.
- It does not consult `CAPABILITIES`.
- It has no SASL support.
- It has no TLS requirement before password authentication.

Modern concern:

- RFC 4643 advertises `AUTHINFO` variants through capabilities.
- RFC 4643 permits `USER/PASS`, but recommends against offering it unless
  a strong encryption layer is active or compatibility requires it.
- `AUTHINFO SASL` should be supported when the server advertises it and a
  suitable mechanism is available.

### TLS

Current code:

- No production NNTP code sends `STARTTLS`.
- No production NNTP code handles response `483`, which signals that a
  secure or encrypted connection is required.
- There is no implicit TLS port handling.

Modern concern:

- RFC 4642 defines `STARTTLS`.
- RFC 8143 updates NNTP TLS handling to current TLS best practices.
- Authentication and posting should be able to require TLS by policy.

### STAT With Empty Message-ID

Current code:

- `libtrn/nntp.cpp` can call `nntp_stat_id("")`.
- That emits `STAT ` with a trailing space.

Modern concern:

- RFC 3977 syntax is `STAT`, `STAT number`, or `STAT message-id`.
- Empty message-id is not a valid message-id form.
- If the intent is current-article probing, send bare `STAT`.

### Current Commands That Are Not Legacy

These commands are current RFC 3977 commands and should remain:

- `DATE`
- `NEWGROUPS`
- `LIST`
- `LIST ACTIVE`
- `LIST NEWSGROUPS`
- `GROUP`
- `LISTGROUP`
- `NEXT`
- `ARTICLE`
- `HEAD`
- `BODY`
- `STAT`
- `POST`
- `QUIT`

Most modernization work is about discovery, secure transport, and using
the standardized names for former `X` commands.

## Modernization Slices

### Slice 1: Capability Model

- Add an `NntpCapabilities` data type.
- Parse multiline `CAPABILITIES` replies.
- Store capability state in `NNTPLink` or adjacent session state.
- Add unit tests for base capabilities, LIST variants, `OVER`, `HDR`,
  `MODE-READER`, `STARTTLS`, and `AUTHINFO`.

### Slice 2: Startup Handshake

- After greeting, send `CAPABILITIES`.
- If the server requires `MODE READER` or advertises `MODE-READER`, send
  `MODE READER`.
- Refresh capabilities after `MODE READER`.
- Preserve fallback behavior for RFC 977-era servers that return `500`.

### Slice 3: LIST Capability Selection

- Route `LIST ACTIVE`, `LIST NEWSGROUPS`, `LIST OVERVIEW.FMT`, and
  `LIST HEADERS` through capability checks.
- Keep old optimistic probes only when no capability list is available.
- Add tests with the mock NNTP server for advertised and missing LIST
  variants.

### Slice 4: OVER Before XOVER

- Add an overview command selector.
- Prefer `OVER first-last` when `OVER` is advertised.
- Use `XOVER first-last` only as an old-server fallback.
- Prefer uppercase `LIST OVERVIEW.FMT`.
- Update overview tests to cover both paths.

### Slice 5: HDR Before XHDR

- Add a header command selector.
- Prefer `HDR field article` and `HDR field first-last` when `HDR` is
  advertised.
- Use `XHDR` only as an old-server fallback.
- Add tests for `HDR`, missing `HDR`, and configured `XHDR Broken`.

### Slice 6: Replace XGTITLE Fallback

- Prefer `LIST NEWSGROUPS group` for one-group descriptions.
- Use cached description files when available.
- Keep `XGTITLE` only behind explicit compatibility fallback.
- Record when `XGTITLE` is disabled so repeated lookups do not probe it.

### Slice 7: Guard LIST SUBSCRIPTIONS

- Treat `LIST SUBSCRIPTIONS` as a private extension.
- Use it only if a capability or configured compatibility mode allows it.
- Fall back to local `SUBSCRIPTIONS` data without treating absence as an
  error.
- Add tests for advertised, rejected, and absent subscription support.

### Slice 8: Fix Empty STAT

- Replace `nntp_stat_id("")` call sites with an explicit helper for the
  intended operation.
- Send bare `STAT` for current-article status.
- Send `STAT <message-id>` only for non-empty message-id text.
- Add tests for both forms.

### Slice 9: TLS State And STARTTLS

- Add session state for cleartext, STARTTLS, and implicit TLS.
- Parse `STARTTLS` capability.
- Add a policy option for requiring TLS before auth or posting.
- Handle response `483` by attempting TLS when allowed.
- Add mock-server tests for `382`, `483`, `502`, and `580` paths.

### Slice 10: Authentication Modernization

- Drive auth from `AUTHINFO` capability data.
- Use `AUTHINFO USER/PASS` only when advertised or compatibility fallback
  is enabled.
- Refuse password auth on cleartext connections when policy requires TLS.
- Add a future seam for SASL mechanisms without implementing every
  mechanism in this slice.

### Slice 11: Mock Server Coverage

- Extend the mock NNTP server plan to cover capabilities, `OVER`, `HDR`,
  `STARTTLS`, `AUTHINFO`, and extension rejection.
- Add tests for each modern path and each legacy fallback.
- Keep tests deterministic: every command should be an explicit
  expectation.

### Slice 12: Compatibility Policy Cleanup

- Document modern, compatibility, and legacy modes.
- Default to modern commands when capabilities are available.
- Keep compatibility probes for old servers.
- Avoid repeated unsupported-command probes within one session.
