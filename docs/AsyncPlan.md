# Async I/O Migration Plan for TRN Using Boost.Asio 1.89 (C++17)

## Executive Summary

Transform the TRN newsreader from synchronous blocking I/O to fully
asynchronous operations using Boost.Asio 1.89 with C+\+17. This
approach uses callback-based async patterns and completion token
composition instead of C++20 coroutines, making the code accessible
to developers familiar with traditional async programming.

---

## Key Boost.Asio 1.89 Features (C++17 Compatible)

### 1. Completion Token Flexibility
- **Types**: use_future, callbacks, deferred
- **Benefit**: Choose async style per operation
- **Use Case**: Callbacks for simple operations, futures for complex chains

### 2. Completion Token Composition
- **Functions**: deferred, append, prepend, consign
- **Benefit**: Chain operations without nested callbacks
- **Use Case**: Sequential NNTP command flows

### 3. Native File I/O
- **Classes**: basic_file, stream_file, random_access_file
- **Benefit**: True async file operations (IOCP on Windows, io_uring on Linux)
- **Use Case**: Article caching, configuration file reads

### 4. Enhanced Cancellation
- **Classes**: cancellation_signal, cancellation_slot
- **Benefit**: Proper operation cancellation and cleanup
- **Use Case**: Timeout handling, user interruptions

### 5. Composed Operations
- **Function**: async_compose
- **Benefit**: Build reusable async state machines
- **Use Case**: Multi-step NNTP protocol sequences

---

## Phase 1: Core Async Infrastructure

### 1.1 IO Context Manager

**File**: async_core.h

**Purpose**: Centralized event loop and thread pool management

**Implementation**:
- Meyer's singleton pattern (thread-safe)
- Manages single io_context instance
- Manages thread_pool for CPU-bound work (default 4 threads)
- Provides run() and stop() methods

**Key Methods**:

    static IOContextManager& instance()
    io_context& io_context()
    thread_pool& thread_pool()
    void run(size_t thread_count = hardware_concurrency())
    void stop()

**Rationale**: All async operations need access to the same io_context to ensure proper scheduling and event handling.

**Example Implementation**:

    // async_core.h
    #pragma once
    #include <boost/asio.hpp>
    #include <memory>
    #include <thread>
    #include <vector>
    
    namespace trn::async {
    
    class IOContextManager {
    public:
        static IOContextManager& instance() {
            static IOContextManager mgr;
            return mgr;
        }
    
        boost::asio::io_context& io_context() { 
            return io_ctx_; 
        }
        
        boost::asio::thread_pool& thread_pool() { 
            return thread_pool_; 
        }
        
        void run(size_t thread_count = std::thread::hardware_concurrency()) {
            std::vector<std::thread> threads;
            threads.reserve(thread_count);
            
            for (size_t i = 0; i < thread_count; ++i) {
                threads.emplace_back([this] { 
                    io_ctx_.run(); 
                });
            }
            
            for (auto& t : threads) {
                if (t.joinable()) {
                    t.join();
                }
            }
        }
        
        void stop() {
            io_ctx_.stop();
            thread_pool_.join();
        }
    
    private:
        IOContextManager() 
            : thread_pool_(4)
        {}
        
        ~IOContextManager() = default;
        IOContextManager(const IOContextManager&) = delete;
        IOContextManager& operator=(const IOContextManager&) = delete;
    
        boost::asio::io_context io_ctx_;
        boost::asio::thread_pool thread_pool_;
    };
    
    } // namespace trn::async

---

## Phase 2: Async Console I/O

### 2.1 Platform-Specific Console Wrapper

**Files**: async_console.h, async_console.cpp

**Platform Detection**:
- Windows: `boost::asio::windows::stream_handle` for stdin/stdout
- POSIX/Linux: `boost::asio::posix::stream_descriptor` for stdin/stdout

**Public Interface**:

    class AsyncConsole {
        void async_read_line(ReadLineHandler handler);
        void async_write(string text, WriteHandler handler);
        void write_sync(string_view text);
        void cancel();
    };

**Handler Types**:

    using ReadLineHandler = function<void(error_code, string)>;
    using WriteHandler = function<void(error_code)>;

**Implementation Details**:
- Use `boost::asio::streambuf` for buffered reading
- `async_read_until()` with newline delimiter
- Write queue with sequential processing (prevent interleaving)
- Normalize line endings (handle CR/LF across platforms)
- Support cancellation via `handle.cancel()`

**Example Implementation**:

    // async_console.h
    #pragma once
    #include <boost/asio.hpp>
    #include <functional>
    #include <string>
    #include <deque>
    
    namespace trn::io {
    
    using ReadLineHandler = std::function<void(boost::system::error_code, std::string)>;
    using WriteHandler = std::function<void(boost::system::error_code)>;
    
    class AsyncConsole {
    public:
        explicit AsyncConsole(boost::asio::io_context& ioc);
        
        void async_read_line(ReadLineHandler handler);
        void async_write(std::string text, WriteHandler handler);
        void write_sync(std::string_view text);
        void cancel();
    
    private:
        void handle_read(
            const boost::system::error_code& ec,
            std::size_t bytes_transferred,
            ReadLineHandler handler
        );
        
        void do_write();
        void handle_write(
            const boost::system::error_code& ec,
            std::size_t bytes_transferred
        );
    
    #ifdef _WIN32
        boost::asio::windows::stream_handle stdin_;
        boost::asio::windows::stream_handle stdout_;
    #else
        boost::asio::posix::stream_descriptor stdin_;
        boost::asio::posix::stream_descriptor stdout_;
    #endif
    
        boost::asio::streambuf read_buffer_;
        
        struct WriteOp {
            std::string data;
            WriteHandler handler;
        };
        std::deque<WriteOp> write_queue_;
        bool write_in_progress_{false};
    };
    
    } // namespace trn::io

    // async_console.cpp
    #include "async_console.h"
    #include <iostream>
    
    namespace trn::io {
    
    #ifdef _WIN32
    AsyncConsole::AsyncConsole(boost::asio::io_context& ioc)
        : stdin_(ioc, ::GetStdHandle(STD_INPUT_HANDLE))
        , stdout_(ioc, ::GetStdHandle(STD_OUTPUT_HANDLE))
    {}
    #else
    AsyncConsole::AsyncConsole(boost::asio::io_context& ioc)
        : stdin_(ioc, ::dup(STDIN_FILENO))
        , stdout_(ioc, ::dup(STDOUT_FILENO))
    {}
    #endif
    
    void AsyncConsole::async_read_line(ReadLineHandler handler) {
        boost::asio::async_read_until(
            stdin_,
            read_buffer_,
            '\n',
            [this, handler = std::move(handler)](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                this->handle_read(ec, bytes_transferred, std::move(handler));
            }
        );
    }
    
    void AsyncConsole::handle_read(
        const boost::system::error_code& ec,
        std::size_t /*bytes_transferred*/,
        ReadLineHandler handler
    ) {
        if (ec) {
            handler(ec, "");
            return;
        }
        
        std::istream is(&read_buffer_);
        std::string line;
        std::getline(is, line);
        
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        handler(boost::system::error_code{}, std::move(line));
    }
    
    void AsyncConsole::async_write(std::string text, WriteHandler handler) {
        write_queue_.push_back({std::move(text), std::move(handler)});
        
        if (!write_in_progress_) {
            do_write();
        }
    }
    
    void AsyncConsole::do_write() {
        if (write_queue_.empty()) {
            write_in_progress_ = false;
            return;
        }
        
        write_in_progress_ = true;
        auto& op = write_queue_.front();
        
        boost::asio::async_write(
            stdout_,
            boost::asio::buffer(op.data),
            [this](const boost::system::error_code& ec, std::size_t bytes) {
                this->handle_write(ec, bytes);
            }
        );
    }
    
    void AsyncConsole::handle_write(
        const boost::system::error_code& ec,
        std::size_t /*bytes_transferred*/
    ) {
        auto handler = std::move(write_queue_.front().handler);
        write_queue_.pop_front();
        
        if (handler) {
            handler(ec);
        }
        
        do_write();
    }
    
    void AsyncConsole::write_sync(std::string_view text) {
        boost::system::error_code ec;
        boost::asio::write(stdout_, boost::asio::buffer(text), ec);
    }
    
    void AsyncConsole::cancel() {
        stdin_.cancel();
        stdout_.cancel();
    }
    
    } // namespace trn::io

---

## Phase 3: Async NNTP Client

### 3.1 NNTP Client Design

**Files**: async_nntp.h, async_nntp.cpp

**Class Structure**:

    class AsyncNNTPClient : public enable_shared_from_this<AsyncNNTPClient> {
    public:
        static shared_ptr<AsyncNNTPClient> create(io_context& ioc);
        
        void async_connect(string host, uint16_t port, 
                          ConnectHandler handler,
                          chrono::seconds timeout = 30s);
        
        void async_send_command(string command,
                               CommandHandler handler,
                               chrono::seconds timeout = 10s);
        
        void async_fetch_article(long article_num,
                                ArticleHandler handler);
        
        void async_post_article(const string& article,
                               CommandHandler handler);
        
        bool is_connected() const;
        void close();
    };

**Handler Types**:

    using ConnectHandler = function<void(error_code)>;
    using CommandHandler = function<void(error_code, string)>;
    using ArticleHandler = function<void(error_code, vector<string>)>;

### 3.2 Connection Flow

**Step 1: DNS Resolution**
- `async_resolve()` with timeout
- Handle both IPv4 and IPv6

**Step 2: TCP Connection**
- `async_connect()` to resolved endpoints
- Timeout using steady_timer
- Cancel socket on timeout

**Step 3: Server Greeting**
- Read server response (200 or 201)
- Validate connection success
- Report error if server rejects

### 3.3 Timeout Pattern

**Implementation**:

1. Create `shared_ptr<steady_timer>`
2. Set `timer.expires_after(timeout)`
3. `timer.async_wait([socket] { socket.cancel(); })`
4. Start actual operation
5. On completion, cancel timer
6. Check error_code for `operation_aborted`

**Rationale**: Prevents indefinite blocking on network operations

### 3.4 Example Implementation

    // async_nntp.h
    #pragma once
    #include <boost/asio.hpp>
    #include <boost/asio/steady_timer.hpp>
    #include <functional>
    #include <string>
    #include <vector>
    #include <memory>
    
    namespace trn::nntp {
    
    using ConnectHandler = std::function<void(boost::system::error_code)>;
    using CommandHandler = std::function<void(boost::system::error_code, std::string)>;
    using ArticleHandler = std::function<void(boost::system::error_code, std::vector<std::string>)>;
    
    class AsyncNNTPClient : public std::enable_shared_from_this<AsyncNNTPClient> {
    public:
        static std::shared_ptr<AsyncNNTPClient> create(boost::asio::io_context& ioc) {
            return std::shared_ptr<AsyncNNTPClient>(new AsyncNNTPClient(ioc));
        }
        
        void async_connect(
            std::string host, 
            uint16_t port,
            ConnectHandler handler,
            std::chrono::seconds timeout = std::chrono::seconds(30)
        );
        
        void async_send_command(
            std::string command,
            CommandHandler handler,
            std::chrono::seconds timeout = std::chrono::seconds(10)
        );
        
        void async_fetch_article(
            long article_num,
            ArticleHandler handler
        );
        
        void async_post_article(
            const std::string& article,
            CommandHandler handler
        );
        
        bool is_connected() const { return socket_.is_open(); }
        void close();
    
    private:
        explicit AsyncNNTPClient(boost::asio::io_context& ioc);
        
        void start_connect(
            boost::asio::ip::tcp::resolver::results_type endpoints,
            ConnectHandler handler,
            std::shared_ptr<boost::asio::steady_timer> timeout_timer
        );
        
        void handle_connect(
            const boost::system::error_code& ec,
            ConnectHandler handler,
            std::shared_ptr<boost::asio::steady_timer> timeout_timer
        );
        
        void async_read_line(
            std::function<void(boost::system::error_code, std::string)> handler
        );
        
        void async_write_line(
            std::string line,
            std::function<void(boost::system::error_code)> handler
        );
    
        boost::asio::io_context& ioc_;
        boost::asio::ip::tcp::socket socket_;
        boost::asio::streambuf read_buffer_;
        boost::asio::cancellation_signal cancel_signal_;
    };
    
    } // namespace trn::nntp

    // async_nntp.cpp (partial - connect implementation)
    #include "async_nntp.h"
    #include <boost/asio/ip/tcp.hpp>
    
    namespace trn::nntp {
    
    AsyncNNTPClient::AsyncNNTPClient(boost::asio::io_context& ioc)
        : ioc_(ioc)
        , socket_(ioc)
    {}
    
    void AsyncNNTPClient::async_connect(
        std::string host,
        uint16_t port,
        ConnectHandler handler,
        std::chrono::seconds timeout
    ) {
        auto self = shared_from_this();
        auto resolver = std::make_shared<boost::asio::ip::tcp::resolver>(ioc_);
        
        resolver->async_resolve(
            host, 
            std::to_string(port),
            [this, self, handler = std::move(handler), timeout, resolver](
                const boost::system::error_code& ec,
                boost::asio::ip::tcp::resolver::results_type endpoints
            ) mutable {
                if (ec) {
                    handler(ec);
                    return;
                }
                
                auto timeout_timer = std::make_shared<boost::asio::steady_timer>(ioc_);
                timeout_timer->expires_after(timeout);
                
                start_connect(std::move(endpoints), std::move(handler), timeout_timer);
            }
        );
    }
    
    void AsyncNNTPClient::async_write_line(
        std::string line,
        std::function<void(boost::system::error_code)> handler
    ) {
        auto self = shared_from_this();
        
        if (line.size() < 2 || line.substr(line.size() - 2) != "\r\n") {
            line += "\r\n";
        }
        
        boost::asio::async_write(
            socket_,
            boost::asio::buffer(line),
            boost::asio::consign(
                std::move(handler),
                std::move(line)
            )
        );
    }
    
    void AsyncNNTPClient::close() {
        boost::system::error_code ec;
        socket_.close(ec);
    }
    
    } // namespace trn::nntp

---

## Phase 4: Async File I/O

### 4.1 File I/O Wrapper

**Files**: async_file_io.h, async_file_io.cpp

**Interface**:

    class AsyncFileIO {
    public:
        void async_read_file(const string& path, ReadFileHandler handler);
        void async_write_file(const string& path, string content, 
                             WriteFileHandler handler);
        void async_append_file(const string& path, string content,
                              WriteFileHandler handler);
    };

**Handler Types**:

    using ReadFileHandler = function<void(error_code, string)>;
    using WriteFileHandler = function<void(error_code)>;

### 4.2 Implementation Using boost::asio::stream_file

**Read Operation**:
1. Open file with `stream_file::read_only`
2. Get file size with `file.size()`
3. Allocate buffer (`shared_ptr<string>`)
4. `async_read_some()` into buffer
5. Resize buffer to actual bytes read
6. Return content via handler

**Write Operation**:
1. Open file with `write_only | create | truncate`
2. Use `async_write()` with consign to keep buffer alive
3. File auto-closes when `shared_ptr` goes out of scope

**Platform Support**:
- Windows: Uses IOCP (I/O Completion Ports) for true async
- Linux: Uses io_uring if kernel 5.10+, otherwise thread pool fallback
- POSIX: Thread pool fallback

### 4.3 Example Implementation

    // async_file_io.h
    #pragma once
    #include <boost/asio.hpp>
    #include <boost/asio/stream_file.hpp>
    #include <functional>
    #include <string>
    
    namespace trn::io {
    
    using ReadFileHandler = std::function<void(boost::system::error_code, std::string)>;
    using WriteFileHandler = std::function<void(boost::system::error_code)>;
    
    class AsyncFileIO {
    public:
        explicit AsyncFileIO(boost::asio::io_context& ioc) : ioc_(ioc) {}
        
        void async_read_file(
            const std::string& path,
            ReadFileHandler handler
        );
        
        void async_write_file(
            const std::string& path,
            std::string content,
            WriteFileHandler handler
        );
        
        void async_append_file(
            const std::string& path,
            std::string content,
            WriteFileHandler handler
        );
    
    private:
        boost::asio::io_context& ioc_;
    };
    
    } // namespace trn::io

    // async_file_io.cpp
    #include "async_file_io.h"
    #include <memory>
    
    namespace trn::io {
    
    void AsyncFileIO::async_read_file(
        const std::string& path,
        ReadFileHandler handler
    ) {
        auto file = std::make_shared<boost::asio::stream_file>(
            ioc_, 
            path, 
            boost::asio::stream_file::read_only
        );
        
        boost::system::error_code ec;
        auto size = file->size(ec);
        
        if (ec) {
            boost::asio::post(ioc_, [handler = std::move(handler), ec]() {
                handler(ec, "");
            });
            return;
        }
        
        auto buffer = std::make_shared<std::string>();
        buffer->resize(size);
        
        file->async_read_some(
            boost::asio::buffer(*buffer),
            [handler = std::move(handler), file, buffer](
                const boost::system::error_code& ec,
                std::size_t bytes_read
            ) mutable {
                if (ec && ec != boost::asio::error::eof) {
                    handler(ec, "");
                    return;
                }
                
                buffer->resize(bytes_read);
                handler(boost::system::error_code{}, std::move(*buffer));
            }
        );
    }
    
    void AsyncFileIO::async_write_file(
        const std::string& path,
        std::string content,
        WriteFileHandler handler
    ) {
        auto file = std::make_shared<boost::asio::stream_file>(
            ioc_,
            path,
            boost::asio::stream_file::write_only | 
            boost::asio::stream_file::create | 
            boost::asio::stream_file::truncate
        );
        
        boost::asio::async_write(
            *file,
            boost::asio::buffer(content),
            boost::asio::consign(
                std::move(handler),
                file,
                std::move(content)
            )
        );
    }
    
    } // namespace trn::io

### 4.4 Lifetime Management with consign

**Pattern**:

    async_write(
        file,
        buffer(content),
        consign(
            handler,
            file_ptr,        // Keep file alive
            move(content)    // Keep buffer alive
        )
    );

**Benefit**: Automatic resource lifetime extension without manual management

---

## Phase 5: Application-Level Async Flows

### 5.1 Article Reader State Machine

**Files**: article_reader.h, article_reader.cpp

**States**:
1. Connecting - `async_connect()` to NNTP server
2. Idle - waiting for user input
3. Fetching - `async_fetch_article()` in progress
4. Displaying - outputting article to console
5. Caching - `async_write_file()` to disk

**State Transitions**:

    Connecting -> Connected (on success) -> Idle
    Idle -> Fetching (on article command)
    Fetching -> Displaying (on article received)
    Displaying -> Caching (article output complete)
    Caching -> Idle (cache write complete)

### 5.2 Implementation Pattern

**Shared Pointer Lifecycle**:

    class ArticleReader : public enable_shared_from_this<ArticleReader> {
        void start() {
            auto self = shared_from_this();
            // All async operations capture self
        }
    };

**Callback Chaining**:

    connect_to_server() ->
      show_prompt() ->
        read_command() ->
          handle_command() ->
            fetch_article() ->
              display_article() ->
                cache_article() ->
                  show_prompt()  // Loop back

### 5.3 Example Implementation

    // article_reader.h
    #pragma once
    #include "async_nntp.h"
    #include "async_console.h"
    #include "async_file_io.h"
    #include <memory>
    
    namespace trn::app {
    
    class ArticleReader : public std::enable_shared_from_this<ArticleReader> {
    public:
        static std::shared_ptr<ArticleReader> create(
            std::shared_ptr<nntp::AsyncNNTPClient> nntp,
            std::shared_ptr<io::AsyncConsole> console,
            std::shared_ptr<io::AsyncFileIO> file_io
        ) {
            return std::shared_ptr<ArticleReader>(
                new ArticleReader(nntp, console, file_io)
            );
        }
        
        void start();
        void stop();
    
    private:
        ArticleReader(
            std::shared_ptr<nntp::AsyncNNTPClient> nntp,
            std::shared_ptr<io::AsyncConsole> console,
            std::shared_ptr<io::AsyncFileIO> file_io
        );
        
        void connect_to_server();
        void show_prompt();
        void read_command();
        void handle_command(std::string cmd);
        void fetch_and_display_article(long num);
    
        std::shared_ptr<nntp::AsyncNNTPClient> nntp_;
        std::shared_ptr<io::AsyncConsole> console_;
        std::shared_ptr<io::AsyncFileIO> file_io_;
        bool running_{false};
    };
    
    } // namespace trn::app

    // article_reader.cpp (partial)
    namespace trn::app {
    
    void ArticleReader::fetch_and_display_article(long num) {
        auto self = shared_from_this();
        
        console_->write_sync("Fetching article " + std::to_string(num) + "...\n");
        
        nntp_->async_fetch_article(
            num,
            [this, self, num](
                const boost::system::error_code& ec,
                std::vector<std::string> lines
            ) {
                if (ec) {
                    console_->write_sync("Failed: " + ec.message() + "\n");
                    this->show_prompt();
                    return;
                }
                
                for (const auto& line : lines) {
                    console_->write_sync(line + "\n");
                }
                
                std::string cache_path = "/tmp/article_" + std::to_string(num) + ".txt";
                std::string content;
                for (const auto& line : lines) {
                    content += line + "\n";
                }
                
                file_io_->async_write_file(
                    cache_path,
                    std::move(content),
                    [this, self](const boost::system::error_code& ec) {
                        if (!ec) {
                            console_->write_sync("[Cached]\n");
                        }
                        this->show_prompt();
                    }
                );
            }
        );
    }
    
    } // namespace trn::app

### 5.4 Main Event Loop

**File**: main.cpp

**Structure**:

    int main(int argc, char* argv[]) {
        // Setup signal handlers (SIGINT, SIGTERM)
        
        auto& ioc = IOContextManager::instance().io_context();
        
        // Create components
        auto console = make_shared<AsyncConsole>(ioc);
        auto nntp = AsyncNNTPClient::create(ioc);
        auto file_io = make_shared<AsyncFileIO>(ioc);
        
        // Create and start application
        auto app = ArticleReader::create(nntp, console, file_io);
        app->start();
        
        // Run event loop (blocks until stop())
        IOContextManager::instance().run(4);
        
        return 0;
    }

---

## Phase 6: Advanced Patterns

### 6.1 Retry Logic with Exponential Backoff

**Template Class**:

    template<typename AsyncOp, typename Handler>
    class RetryOperation {
        void attempt() {
            op_([this](error_code ec, auto... args) {
                if (!ec || current_attempt_ >= max_retries_) {
                    handler_(ec, forward(args)...);
                    return;
                }
                
                ++current_attempt_;
                timer_.expires_after(current_delay_);
                current_delay_ *= 2;
                
                timer_.async_wait([this](error_code) {
                    attempt();
                });
            });
        }
    };

**Usage**:

    async_retry(
        [&](auto handler) {
            nntp->async_connect("news.example.com", 119, handler);
        },
        [](error_code ec) {
            if (ec) {
                cerr << "Failed after retries\n";
            }
        },
        3,     // max retries
        500ms  // initial delay
    );

---

## Phase 7: Porting Existing Components

### 7.1 Port inews (Article Posting)

**Current**: Synchronous stdin read + NNTP send
**New**: Async stdin read + async NNTP send

**Flow**:
1. `async_read_headers()` - read until blank line
2. Validate headers (From, Newsgroups, Subject)
3. `async_read_body()` - read until EOF
4. `async_post_article()` to NNTP server
5. Wait for response (240 success or 44x error)

### 7.2 Port nntplist (Group Listing)

**Current**: Synchronous LIST command
**New**: Async LIST command with optional file output

**Flow**:
1. `async_connect()` to NNTP server
2. `async_send_command("LIST" or "LIST ACTIVE <wildcard>")`
3. `async_read_multiline()` until terminator
4. If output file specified, `async_write_file()`
5. Otherwise, `async_write()` to console

### 7.3 Port trn-artchk (Article Checker)

**Current**: Synchronous file read + NNTP validation
**New**: Async file read + async NNTP validation

**Flow**:
1. `async_read_file()` - read article from disk
2. Parse and validate headers
3. For each newsgroup:
   - `async_send_command("LIST ACTIVE <group>")`
   - Check if group exists
   - `async_send_command("XGTITLE <group>")` for description
4. Display validation results

---

## Phase 8: Error Handling

### 8.1 Error Categories

**Network Errors**:
- `connection_refused` - server down or wrong port
- `timed_out` - operation exceeded deadline
- `eof` - connection closed by server
- `operation_aborted` - cancelled by user or timeout

**File Errors**:
- `file_not_found` - file doesn't exist
- `permission_denied` - insufficient rights
- `no_space_on_device` - disk full

**Protocol Errors**:
- Custom NNTP error codes (400, 500 series)
- Parse errors (invalid response format)

### 8.2 Error Handling Pattern

    void some_async_operation(Handler handler) {
        async_op([handler](error_code ec, auto... args) {
            if (ec) {
                if (ec == asio::error::operation_aborted) {
                    return;
                }
                
                if (ec == asio::error::timed_out) {
                    return;
                }
                
                console->write_sync("Error: " + ec.message());
                return;
            }
            
            process_result(args...);
        });
    }

### 8.3 Graceful Shutdown

    void shutdown() {
        running_ = false;
        console_->cancel();
        nntp_->close();
        IOContextManager::instance().stop();
    }

---

## Phase 9: Testing Strategy

### 9.1 Unit Tests

**Framework**: Google Test

**Test Categories**:
1. `AsyncConsole` tests
2. `AsyncNNTPClient` tests
3. `AsyncFileIO` tests

**Mock Server**:
- Create mock NNTP server using boost::asio
- Respond to commands with canned responses
- Test timeout and error conditions

### 9.2 Integration Tests

**Scenarios**:
1. Full article fetch flow
2. Article posting flow
3. Newsgroup listing
4. Connection retry on failure
5. Graceful shutdown

---

## Phase 10: Performance Optimizations

### 10.1 Connection Pooling

**Purpose**: Reuse NNTP connections

    class NNTPConnectionPool {
        void get_connection(Handler handler);
        void return_connection(shared_ptr<AsyncNNTPClient> conn);
    };

### 10.2 Batch Article Fetcher

    class BatchArticleFetcher {
        void fetch_articles(vector<long> article_nums, Handler handler);
    };

---

## Implementation Timeline

### Week 1: Foundation
- `IOContextManager` singleton
- `AsyncConsole` for stdin/stdout
- Basic `AsyncNNTPClient`

### Week 2: Core Networking
- Complete NNTP protocol
- Timeout and retry logic
- `AsyncFileIO`

### Week 3: Application Logic
- `ArticleReader` state machine
- Main event loop
- Caching functionality

### Week 4: Component Ports
- Port inews
- Port nntplist
- Port trn-artchk

### Week 5: Testing & Optimization
- Unit tests
- Connection pooling
- Performance profiling

### Week 6: Polish
- Error handling
- Documentation
- Code review

---

## Key C++17 Features Used

| Feature | Usage | Example |
|---------|-------|---------|
| Structured bindings | Tuple unpacking | `auto [ec, data] = result;` |
| `std::optional` | Optional parameters | `optional<string> wildcard` |
| `std::string_view` | Non-owning strings | `void write(string_view text)` |
| `std::shared_ptr` | Async lifetime | `shared_from_this()` |
| Lambda captures | Async callbacks | `[self, handler](auto ec) {}` |

---

## Migration Checklist

### Preparation
- [ ] Set up C++17 compiler flags
- [ ] Link Boost.Asio 1.89 libraries
- [ ] Configure platform-specific settings

### Phase 1: Infrastructure
- [ ] Implement IOContextManager
- [ ] Create AsyncConsole wrapper
- [ ] Test basic console I/O

### Phase 2: Networking
- [ ] Port NNTP client to AsyncNNTPClient
- [ ] Add timeout and retry logic
- [ ] Test connection and commands

### Phase 3: File I/O
- [ ] Implement AsyncFileIO wrapper
- [ ] Test read/write/append operations

### Phase 4: Application
- [ ] Refactor main() to event loop
- [ ] Create ArticleReader state machine
- [ ] Test full article reading flow

### Phase 5: Components
- [ ] Port inews.cpp
- [ ] Port nntplist.cpp
- [ ] Port trn-artchk.cpp

### Phase 6: Advanced Features
- [ ] Add retry helper utility
- [ ] Implement timeout wrapper
- [ ] Create connection pool

### Phase 7: Testing
- [ ] Write unit tests
- [ ] Create integration tests
- [ ] Performance benchmarking

### Phase 8: Deployment
- [ ] Update documentation
- [ ] Code review
- [ ] Final cleanup

---

## Conclusion

This C+\+17-based async I/O migration plan provides a proven path to
modernizing TRN while maintaining code readability and debuggability.
The callback-based patterns are familiar to most C++ developers, avoiding
the complexity of C++20 coroutines while still gaining the benefits of
Boost.Asio 1.89's advanced features.

The incremental migration approach allows for testing at each phase,
reducing risk and ensuring stability throughout the transition.
