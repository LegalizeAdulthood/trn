<!-- Copyright (c) 2026, Richard Thomson -->

# Mock NNTP Server

## Purpose

Add an in-process NNTP mock server for end-to-end tests that need the
real TCP client path and deterministic validation at the end of each test
case.

The server is a test utility, not a production NNTP daemon.  It sends a
configurable NNTP welcome banner and runs explicit command expectations.
Helper builders should be added only with the tests that need those
protocol details.

## Direction

Implement a C++ RAII fixture in the test tree.  Each test owns one
server instance.  The server binds `127.0.0.1` on port `0`, records the
chosen port, runs on a worker thread, accepts a client connection, sends
the configured welcome banner, and then drains queued command
expectations.

Parallel test execution is safe because every fixture binds its own
ephemeral port and owns its own thread, socket, and state.

Avoid gMock calls from the worker thread.  The worker records failures in
thread-safe server state.  The test thread calls `verify()` to report I/O
failures and thread shutdown failures through normal test assertions.

## Example

```cpp
MockNntpServer server;
server.set_welcome_banner("200 trn-test NNTP server ready");
server.start();

ASSERT_EQ("200 trn-test NNTP server ready\r\n",
          read_one_line_from_loopback(server.port()));

server.verify();
```

## Public API

```cpp
class MockNntpServer
{
public:
    MockNntpServer();
    ~MockNntpServer();

    void expect_list(std::vector<std::string> lines);
    void expect_mode_reader();
    void expect_quit();
    void set_welcome_banner(std::string line);
    void start();
    int port() const;
    void stop();
    void verify();
};
```

Add helpers such as `expect_group()`, `expect_article()`, and
`expect_xover()` only with tests that use those helpers.

Helper methods should remain thin builders over the primitive
command/reply API.  They should not become a data-backed NNTP server.

## Protocol Rules

- Send the configured welcome banner immediately after accept.
- Read commands as CRLF-terminated lines.
- Match commands exactly unless a later test needs an explicit matcher
  type.
- Send `MODE READER`, multiline `LIST`, and `QUIT` replies through thin
  expectation helpers.
- Stop after connection close, server stop, or fatal I/O error.

Future command scripting should be added incrementally:

- Dot-stuff payload lines that begin with `.`.
- Treat `QUIT` specially only when no explicit expectation was queued.
- Add posting input capture only with a posting test.

## Threading Model

The server should use one worker thread per fixture.

- Test thread configures the welcome banner.
- `start()` binds, listens, starts the worker, and waits until ready.
- Worker thread accepts the client and sends the welcome banner.
- Shared state is protected by a mutex.
- `stop()` closes the acceptor and client socket, then joins the worker.
- Destructor calls `stop()`.
- `verify()` calls `stop()` first, then reports recorded failures.

Close the acceptor and client socket from `stop()` so a failed test does
not hang the suite.  Add explicit read or write timeouts only when a
later test proves socket shutdown is not enough.

## Files

Add:

- `tests/support/MockNntpServer.h`
- `tests/support/MockNntpServer.cpp`
- `tests/test_MockNntpServer.cpp`

Update:

- `tests/test_datasrc.cpp`

## Implementation Slices

### Slice 4: Client Integration

- Use the fixture in `tests/test_datasrc.cpp`.
- Add a test that drives `SourceFile::open()` remote active-list import
  through the real TCP path.

### Slice 5: Group And Article Helpers

- Add small helpers only when tests need common NNTP flows.
- Cover `GROUP`, `STAT`, `HEAD`, `BODY`, and `ARTICLE`.
- Keep helpers as thin expectation builders, not data-backed server
  behavior.
- Add an integration test for reading one article through `libtrn` NNTP
  functions.

### Slice 6: Overview And Header Helpers

- Add these helpers only when overview or header tests need them.
- Add helpers for `LIST OVERVIEW.FMT`, `XOVER`, and `XHDR`.
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

## Non-Goals

- Full RFC 3977 coverage.
- Python or external process orchestration.
- TLS.
- Authentication beyond explicit scripted expectations.
- Data-backed server behavior unless a later test need proves it.
- gMock assertions from the worker thread.
