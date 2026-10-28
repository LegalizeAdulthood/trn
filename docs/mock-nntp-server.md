<!-- Copyright (c) 2026, Richard Thomson -->

# Mock NNTP Server Extensions

## Implementation Slices

### Slice 6: Overview And Header Helpers

- Add these helpers only when overview or header tests need them.
- Add helpers for `LIST OVERVIEW.FMT`, `OVER`, and `HDR`, with their
  `XOVER` and `XHDR` compatibility paths.
- Cover normal overview flow.
- Cover configured `XHDR Broken` fallback behavior.

### Slice 7: Posting Flow

- Add this only when a posting test needs it.
- Add support for command continuations, starting with `POST`.
- Read posted article data until final `.`.
- Validate posted lines against expected content.
- Add `inews` or library-level posting tests.

### Slice 8: Multi-Connection Support

- Add opt-in support for multiple sequential client connections.
- Let each connection have its own expectation block.
- Keep the default one-connection mode strict.
- Use this for reconnect and timeout recovery tests.

## Design Constraints

Add helpers only alongside tests that use them. Keep them as thin
expectation builders over the primitive command/reply API.

- Dot-stuff payload lines that begin with `.`.
- Treat `QUIT` specially only when no explicit expectation was queued.
- Add posting input capture only with a posting test.
- Add explicit read or write timeouts only when a test proves socket
  shutdown is not enough.

## Non-Goals

- Full RFC 3977 coverage.
- Python or external process orchestration.
- Authentication beyond explicit scripted expectations.
- Data-backed server behavior unless a later test need proves it.
- gMock assertions from the worker thread.
