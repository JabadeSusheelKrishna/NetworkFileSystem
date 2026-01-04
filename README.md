# Network File System (NFS) - Complete Implementation in C

A fully-featured, distributed Network File System implementation in C with support for multiple storage servers, efficient path searching, LRU caching, asynchronous writes, concurrent client access, and data replication. for clear explaination, please see [explaination doc](./explaination.md)

## Table of Contents

- [Features](#features)
- [Architecture](#architecture)
- [Components](#components)
- [Building the Project](#building-the-project)
- [Running the System](#running-the-system)
- [Usage Examples](#usage-examples)
- [Technical Details](#technical-details)
- [Error Codes](#error-codes)
- [Project Structure](#project-structure)

---

## Features

### Core Functionality
- ✅ **Naming Server (NM)**: Central coordinator for file location and metadata
- ✅ **Storage Servers (SS)**: Distributed storage with dynamic registration
- ✅ **Client Operations**: Complete file system operations
  - READ, WRITE (sync/async)
  - CREATE, DELETE (files and directories)
  - COPY between storage servers
  - LIST all accessible paths
  - GET_INFO for file metadata

### Advanced Features
- ✅ **Efficient Path Searching**: Trie-based data structure for O(m) path lookup
- ✅ **LRU Caching**: Recently accessed paths cached for faster retrieval
- ✅ **Asynchronous Writes**: Large files written asynchronously with immediate client response
- ✅ **Concurrent Access Control**: Multiple clients with read/write locking
- ✅ **Data Replication**: Automatic backup across multiple storage servers
- ✅ **Error Handling**: Comprehensive error codes and messaging
- ✅ **Logging**: Detailed operation logs with timestamps

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        Clients                               │
│  (Multiple concurrent clients with various operations)       │
└────────────────┬────────────────────────────────────────────┘
                 │
                 │ Requests
                 ▼
┌─────────────────────────────────────────────────────────────┐
│                   Naming Server (NM)                         │
│  • Path resolution (Trie + LRU Cache)                       │
│  • Storage server registry                                   │
│  • Concurrent access control                                 │
│  • Backup management                                         │
└────────┬────────────────────────────────────────────────────┘
         │
         │ Coordinates
         ▼
┌─────────────────────────────────────────────────────────────┐
│              Storage Servers (SS1, SS2, SS3, ...)           │
│  • File storage and retrieval                               │
│  • Async write handling                                      │
│  • Direct client communication                               │
│  • Inter-server copying                                      │
└─────────────────────────────────────────────────────────────┘
```

---

## Components

### 1. Naming Server (NM)
- **Role**: Central registry and coordinator
- **Ports**: 
  - One port for all incoming connections (SS and clients)
- **Key Features**:
  - Trie-based path lookup (O(m) where m = path length)
  - LRU cache (100 entries) for recent searches
  - Concurrent request handling with thread pool
  - Read/write lock management
  - Backup server tracking (2 replicas per SS)

### 2. Storage Server (SS)
- **Role**: Physical file storage
- **Ports**:
  - NM port: For naming server commands
  - Client port: For direct client data transfer
- **Key Features**:
  - Dynamic registration with NM
  - Recursive path discovery
  - Async write with chunked flushing
  - Inter-server file copying
  - Support for file and directory operations

### 3. Client
- **Role**: User interface to NFS
- **Modes**:
  - Interactive mode (REPL-style)
  - Command-line mode (single operations)
- **Operations**: All standard file operations plus streaming

---

## Building the Project

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential gcc make

### Compilation
```bash
# Build all components
make

# Build individual components
make naming_server
make storage_server
make client

# Clean build artifacts
make clean

# Rebuild everything
make rebuild
```

### Create Test Environment
```bash
# Setup test directories and files
make test-setup
```

---

## Running the System

### Step 1: Start the Naming Server

```bash
./naming_server <port>

# Example
./naming_server 8080
```

The naming server will start and wait for storage servers to register.

### Step 2: Start Storage Servers

Each storage server needs:
- NM IP and port
- Its own NM communication port
- Its own client communication port
- Base paths to make accessible

```bash
./storage_server <nm_ip> <nm_port> <client_port> <nm_service_port> [path1] [path2] ...

# Example: Start first storage server
./storage_server 127.0.0.1 9001 9002 8080 /home/user/test_storage/ss1

# Example: Start second storage server (in another terminal)
./storage_server 127.0.0.1 9003 9004 8080 /home/user/test_storage/ss2

# Example: Start third storage server (for replication)
./storage_server 127.0.0.1 9005 9006 8080 /home/user/test_storage/ss3
```

**Notes**:
- Each SS must have unique ports
- Paths can be files or directories (directories are recursively indexed)
- Storage servers can be started dynamically even after the system is running

### Step 3: Run Client

#### Interactive Mode
```bash
./client <nm_ip> <nm_port>

# Example
./client 127.0.0.1 8080
```

This opens an interactive shell where you can type commands:
```
nfs> LIST
nfs> READ /home/user/test_storage/ss1/file1.txt
nfs> WRITE /home/user/test_storage/ss1/newfile.txt Hello World!
nfs> EXIT
```

#### Command-Line Mode
```bash
./client <nm_ip> <nm_port> <command> [args...]

# Examples
./client 127.0.0.1 8080 LIST
./client 127.0.0.1 8080 READ /path/to/file.txt
./client 127.0.0.1 8080 WRITE /path/to/file.txt "Content here" --SYNC
./client 127.0.0.1 8080 CREATE /path/to/newfile.txt
./client 127.0.0.1 8080 CREATE /path/to/newdir --DIR
./client 127.0.0.1 8080 DELETE /path/to/file.txt
./client 127.0.0.1 8080 COPY /source/file.txt /dest/file.txt
./client 127.0.0.1 8080 INFO /path/to/file.txt
```

---

## Usage Examples

### Example 1: Basic File Operations

```bash
# Terminal 1: Start Naming Server
./naming_server 8080

# Terminal 2: Start Storage Server
mkdir -p ~/nfs_storage
./storage_server 127.0.0.1 9001 9002 8080 ~/nfs_storage

# Terminal 3: Client Operations
./client 127.0.0.1 8080

# In client interactive mode:
nfs> LIST
nfs> CREATE ~/nfs_storage/test.txt
nfs> WRITE ~/nfs_storage/test.txt "Hello, NFS!"
nfs> READ ~/nfs_storage/test.txt
nfs> INFO ~/nfs_storage/test.txt
```

### Example 2: Multi-Server Copy

```bash
# Start two storage servers on different directories
./storage_server 127.0.0.1 9001 9002 8080 ~/ss1_storage
./storage_server 127.0.0.1 9003 9004 8080 ~/ss2_storage

# Copy file from one server to another
./client 127.0.0.1 8080 COPY ~/ss1_storage/file.txt ~/ss2_storage/file.txt
```

### Example 3: Asynchronous Write

```bash
# Large files are automatically written asynchronously
# Or explicitly request async write
nfs> WRITE_ASYNC /path/to/large_file.txt "Very large content..."
```

---

## Technical Details

### Path Resolution Algorithm

1. **Check LRU Cache** (O(1)): Most recently used paths
2. **Search Trie** (O(m)): Where m is the path length
3. **Update Cache**: Add to cache for future lookups
4. **Return SS Info**: IP and port of storage server

### Write Operations

#### Synchronous Write (Default for small files)
```
Client → NM (get SS info) → SS (write immediately) → Client (ACK)
```

#### Asynchronous Write (Large files > 1MB)
```
Client → NM → SS (accept data) → Client (immediate ACK)
                ↓
            Background write in chunks
                ↓
            NM ← SS (completion notification)
```

### Concurrent Access Control

- **Multiple Readers**: Allowed simultaneously
- **Single Writer**: Exclusive access, blocks readers
- **Read-Write Lock**: Per-file locking mechanism

### Data Replication

- When 3+ storage servers exist:
  - Each SS has 2 backup replicas
  - CREATE/WRITE operations replicated asynchronously
  - On SS failure, NM redirects to backup
  - On SS recovery, data synchronization occurs

### Communication Protocol

All components use a structured `Message` format:
```c
typedef struct {
    OperationType operation;
    char path[1024];
    char path2[1024];      // For copy operations
    char data[8192];
    int data_size;
    int error_code;
    WriteMode write_mode;
    FileType file_type;
    // ... additional fields
} Message;
```

---

## Error Codes

| Code | Error                  | Description                                    |
|------|------------------------|------------------------------------------------|
| 0    | SUCCESS                | Operation completed successfully               |
| 1    | FILE_NOT_FOUND         | Requested file/directory does not exist        |
| 2    | FILE_IN_USE            | File is being written by another client        |
| 3    | PERMISSION_DENIED      | Insufficient permissions for operation         |
| 4    | INVALID_PATH           | Path format is invalid                         |
| 5    | SS_UNAVAILABLE         | Storage server is not available                |
| 6    | NETWORK_ERROR          | Network communication error                    |
| 7    | FILE_EXISTS            | File already exists (CREATE operation)         |
| 8    | INVALID_OPERATION      | Operation not supported                        |
| 9    | ASYNC_WRITE_FAILED     | Asynchronous write operation failed            |
| 10   | TIMEOUT                | Operation timeout                              |

---

## Project Structure

```
NetworkFileSystem/
├── common.h              # Shared definitions, structures, constants
├── utils.c               # Common utility functions (logging, socket ops)
├── trie.h                # Trie data structure header
├── trie.c                # Trie implementation for path searching
├── lru_cache.c           # LRU cache implementation
├── naming_server.c       # Naming server implementation
├── storage_server.c      # Storage server implementation
├── client.c              # Client implementation
├── Makefile              # Build system
├── README.md             # This file
└── instructions.md       # Original project requirements
```

---

## Key Implementation Highlights

### 1. Efficient Data Structures
- **Trie**: O(m) path lookup vs O(n*m) linear search
- **LRU Cache**: O(1) cache hit, O(n) cache miss
- **Hash-based file locks**: O(1) lock lookup

### 2. Threading & Concurrency
- Thread-per-connection model
- Mutex protection for shared data structures
- Condition variables for file locking

### 3. Network Design
- Non-blocking socket I/O where appropriate
- Structured message protocol
- Graceful connection handling

### 4. Scalability
- Dynamic storage server addition
- Horizontal scaling with multiple SS
- Load distribution through multiple clients

---

## Testing Recommendations

1. **Single Server Test**: Verify all operations with one SS
2. **Multi-Server Test**: Test file copying between servers
3. **Concurrent Client Test**: Run multiple clients simultaneously
4. **Failure Test**: Kill an SS and verify failover to backup
5. **Large File Test**: Test async write with files > 1MB
6. **Stress Test**: Many concurrent operations

---

## Known Limitations & Future Enhancements

### Current Limitations
- Maximum 100 storage servers (configurable in common.h)
- Maximum 10,000 paths per server
- Fixed message buffer size (8KB)

### Potential Enhancements
- File content caching on NM
- Compression for large file transfers
- Authentication and authorization
- SSL/TLS encryption
- Heartbeat mechanism for SS health checking
- Dynamic load balancing
- Directory-level operations (recursive copy/delete)

---

## Troubleshooting

### Port Already in Use
```bash
# Find process using port
sudo lsof -i :8080
# Kill process
sudo kill -9 <PID>
```

### Permission Denied
```bash
# Ensure paths are accessible
chmod 755 /path/to/storage
```

### Connection Refused
- Verify NM is running
- Check firewall settings
- Confirm correct IP/port

### mpv Not Found (for streaming)
```bash
sudo apt-get install mpv
```

---

## Credits

**Project**: Network File System Implementation  
**Language**: C  
**Platform**: Linux (Ubuntu/Debian tested)  
**Features**: Distributed file system with advanced caching, replication, and concurrent access

---

## License

This is an academic project. Use for educational purposes.

---

For questions or issues, refer to the implementation code comments or the original `instructions.md` file.
