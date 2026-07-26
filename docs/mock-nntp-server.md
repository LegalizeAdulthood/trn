<!-- Copyright (c) 2026, Richard Thomson -->

# Mock NNTP Server

## Purpose

Add an in-process NNTP mock server for end-to-end tests that need the
real TCP client path, exact protocol expectations, and deterministic
validation at the end of each test case.

The server is a test utility, not a production NNTP daemon.  It should
make each test state the commands it expects and the replies it sends.

## Direction

Implement a C++ RAII fixture in the test tree.  Each test owns one
server instance.  The server binds `127.0.0.1` on port `0`, records the
chosen port, runs on a worker thread, accepts a client connection, and
matches received NNTP command lines against an ordered expectation queue.

Parallel test execution is safe because every fixture binds its own
ephemeral port and owns its own thread, socket, state, and expectation
queue.

Avoid gMock calls from the worker thread.  The worker records failures in
thread-safe server state.  The test thread calls `verify()` to report
unexpected commands, unconsumed expectations, I/O failures, and thread
shutdown failures through normal test assertions.

## Example

```cpp
MockNntpServer server;
server.expect("MODE READER")
    .reply("200 trn-test ready");
server.expect("GROUP alt.binaries.fractals")
    .reply("211 2 623 624 alt.binaries.fractals");
server.expect("HEAD 623")
    .reply_list({
        "221 623 <id@example.test> article follows",
        "Subject: test",
        ""
    });
server.start();

g_nntp_link.port_number = server.port();
ASSERT_GT(nntp_connect("127.0.0.1", false), 0);

server.verify();
```

## Public API

Keep the first API small.

```cpp
class MockNntpServer
{
public:
    MockNntpServer();
    ~MockNntpServer();

    MockNntpExpectation &expect(std::string command);
    void start();
    int port() const;
    void stop();
    void verify();
};
```

```cpp
class MockNntpExpectation
{
public:
    MockNntpExpectation &reply(std::string line);
    MockNntpExpectation &reply_list(std::vector<std::string> lines);
    MockNntpExpectation &close_connection();
};
```

Later slices may add helpers such as `expect_group()`, `expect_article()`,
and `expect_xover()`, but the primitive API should remain available.

## Protocol Rules

- Send the configured greeting immediately after accept.
- Read commands as CRLF-terminated lines.
- Match commands exactly unless a later slice adds explicit matcher types.
- Send single-line replies as one status line.
- Send multiline replies as status line, payload lines, and final `.`.
- Dot-stuff payload lines that begin with `.`.
- Treat `QUIT` specially only when no explicit expectation was queued.
- Return `500 unexpected command` for unexpected input, then record a
  failure.
- Stop reading after connection close, server stop, or fatal I/O error.

## Threading Model

The server should use one worker thread per fixture.

- Test thread configures expectations.
- `start()` binds, listens, starts the worker, and waits until ready.
- Worker thread accepts and serves the client.
- Shared state is protected by a mutex.
- `stop()` closes the acceptor and client socket, then joins the worker.
- Destructor calls `stop()`.
- `verify()` calls `stop()` first, then reports recorded failures.

Use timeouts for accept, read, and write operations so a failed test does
not hang the suite.  Prefer short fixture-level defaults and allow tests
to override them when needed.

## Files

Add:

- `tests/support/MockNntpServer.h`
- `tests/support/MockNntpServer.cpp`
- `tests/test_MockNntpServer.cpp`

Update:

- `tests/CMakeLists.txt`

## Implementation Slices

### Slice 1: Skeleton And Smoke Test

- Add the RAII fixture class and CMake wiring.
- Bind loopback on an ephemeral port.
- Start and stop the worker thread.
- Send a configurable greeting.
- Add a smoke test that connects with a raw socket and reads the greeting.

### Slice 2: Ordered Command Expectations

- Add the expectation queue.
- Read one command line at a time.
- Match exact command strings.
- Send single-line replies.
- Record unexpected commands and unconsumed expectations.
- Add tests for success, unexpected input, and missing input.

### Slice 3: Multiline Replies

- Add `reply_list()`.
- Emit NNTP multiline replies with dot-stuffing and final `.`.
- Add tests for ordinary payloads, empty payloads, and payload lines that
  begin with `.`.

### Slice 4: Client Integration

- Use the fixture in `tests/test_nntp.cpp`.
- Add a test that drives `server_init()` through the real TCP path.
- Add tests for `MODE READER`, posting allowed, posting prohibited, and
  unavailable server greetings.

### Slice 5: Group And Article Helpers

- Add small helpers for common NNTP flows.
- Cover `GROUP`, `STAT`, `HEAD`, `BODY`, and `ARTICLE`.
- Keep helpers as thin expectation builders, not data-backed server
  behavior.
- Add an integration test for reading one article through `libtrn` NNTP
  functions.

### Slice 6: Overview And Header Helpers

- Add helpers for `LIST OVERVIEW.FMT`, `XOVER`, and `XHDR`.
- Cover normal overview flow.
- Cover configured `XHDR Broken` fallback behavior.

### Slice 7: Posting Flow

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
