<!-- Copyright (c) 2026, Richard Thomson -->

# NNTP Legacy Conversations

## Scope

Remaining NNTP client modernization work concerns command selection,
current-article status, and loopback integration coverage.

Primary references:

- RFC 3977: <https://www.rfc-editor.org/rfc/rfc3977.html>
- RFC 4642: <https://www.rfc-editor.org/rfc/rfc4642.html>
- RFC 4643: <https://www.rfc-editor.org/rfc/rfc4643.html>
- RFC 8143: <https://www.rfc-editor.org/rfc/rfc8143.html>

## Modernization Slices

### Slice 3: LIST Capability Selection

- Route remaining optimistic LIST requests through capability checks,
  including `LIST ACTIVE` and `LIST HEADERS` where used.
- Keep old optimistic probes only when no capability list is available.
- Add loopback tests for advertised and missing LIST variants.

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

### Slice 11: Mock Server Coverage

- Extend loopback coverage for `OVER`, `HDR`, `AUTHINFO`, and extension
  rejection as consumer tests require it.
- Cover modern paths and legacy fallbacks through the real TCP client.
- Keep every command an explicit expectation.
- Coordinate helpers with [the mock-server plan](mock-nntp-server.md).

### Slice 12: Compatibility Policy Documentation

- Document modern command selection and the remaining compatibility
  probes, including their policy controls and rejection caching.

## Deferred Authentication

SASL mechanisms remain future work. Choose suitable mechanisms before
extending the authentication dispatch.
