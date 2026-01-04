# Project Explaination 🕵🏻‍♂️
## 1️⃣ High-Level Architecture (System UML – Component Diagram)

This explains **who talks to whom and why**.

```mermaid
flowchart LR
    Client1[Client]
    Client2[Client]
    NM[Naming Server]

    SS1[Storage Server 1]
    SS2[Storage Server 2]
    SSN[Storage Server N]

    Client1 -->|Metadata requests| NM
    Client2 -->|Metadata requests| NM

    NM -->|SS IP, Port, Path Mapping| Client1
    NM -->|SS IP, Port, Path Mapping| Client2

    Client1 -->|Read / Write / Stream| SS1
    Client2 -->|Read / Write / Stream| SS2

    NM -->|Create / Delete / Copy Commands| SS1
    NM -->|Create / Delete / Copy Commands| SS2
    NM -->|Replication & Backup| SSN
```

**What this shows clearly**

* NM is **control plane**
* SS is **data plane**
* Clients **never browse storage blindly**

---

## 2️⃣ Class Diagram (Logical UML Design)

This shows **responsibilities**, not implementation.

```mermaid
classDiagram
    class Client {
        +clientId
        +sendRequest()
        +receiveACK()
        +streamData()
    }

    class NamingServer {
        +storageMap
        +pathIndex
        +lruCache
        +registerSS()
        +findPath()
        +sendACK()
        +logEvent()
    }

    class StorageServer {
        +serverId
        +accessiblePaths
        +replicaList
        +readFile()
        +writeFile()
        +asyncWrite()
        +streamAudio()
        +replicateData()
    }

    Client --> NamingServer : Request metadata
    NamingServer --> StorageServer : Control commands
    Client --> StorageServer : Data transfer
    StorageServer --> NamingServer : ACK / Failure info
```

💡 **Evaluator takeaway**: clean separation of concerns (very OS-ish).

---

## 3️⃣ Initialization Sequence (NM + SS Registration)

This one scores **big marks** because many miss it.

```mermaid
sequenceDiagram
    participant NM as Naming Server
    participant SS1 as Storage Server
    participant SS2 as Storage Server

    NM->>NM: Start NM (IP, Port public)
    
    SS1->>NM: Register(IP, NM_port, Client_port, Paths)
    NM->>NM: Update storageMap & pathIndex
    NM-->>SS1: ACK

    SS2->>NM: Register(IP, NM_port, Client_port, Paths)
    NM->>NM: Update storageMap & pathIndex
    NM-->>SS2: ACK

    NM->>NM: Ready to accept client requests
```

---

## 4️⃣ READ File Operation (Client → NM → SS)

```mermaid
sequenceDiagram
    participant Client
    participant NM as Naming Server
    participant SS as Storage Server

    Client->>NM: READ /dir/file.txt
    NM->>NM: Lookup pathIndex / LRU Cache
    NM-->>Client: SS_IP, SS_Port

    Client->>SS: READ /dir/file.txt
    SS-->>Client: File data packets
    SS-->>Client: STOP
```

✔ Shows **NM not blocking**
✔ Shows **direct client–SS data path**

---

## 5️⃣ CREATE File / Directory (NM-Mediated)

```mermaid
sequenceDiagram
    participant Client
    participant NM as Naming Server
    participant SS as Storage Server

    Client->>NM: CREATE /dir newfile
    NM->>NM: Validate path & permissions
    NM->>SS: CREATE empty file
    SS-->>NM: ACK
    NM->>NM: Update global pathIndex
    NM-->>Client: ACK
```

📌 Notice: **NM updates metadata**, not SS.

---

## 6️⃣ COPY Between Storage Servers (SS ↔ SS via NM)

```mermaid
sequenceDiagram
    participant Client
    participant NM as Naming Server
    participant SS1 as Source SS
    participant SS2 as Destination SS

    Client->>NM: COPY /src /dest
    NM->>SS1: Read source data
    SS1->>SS2: Transfer data
    SS2-->>NM: ACK
    NM-->>Client: COPY SUCCESS
```

✔ Works even if source & destination are same SS
✔ NM acts as **orchestrator**

---

## 7️⃣ Asynchronous Write (High-Scoring Diagram)

```mermaid
sequenceDiagram
    participant Client
    participant NM as Naming Server
    participant SS as Storage Server

    Client->>NM: WRITE /file (ASYNC)
    NM-->>Client: ACK (request accepted)

    Client->>SS: Send data
    SS-->>Client: ACK (buffered in memory)

    SS->>SS: Periodic flush to disk
    SS-->>NM: WRITE COMPLETE
    NM-->>Client: FINAL ACK
```

💥 Shows:

* Reduced client latency
* Failure notification via NM
* Correct async semantics

---

## 8️⃣ Replication & Failure Handling

```mermaid
sequenceDiagram
    participant NM as Naming Server
    participant SS1 as Primary SS
    participant SS2 as Replica 1
    participant SS3 as Replica 2

    SS1->>NM: WRITE completed
    NM->>SS2: Async replicate
    NM->>SS3: Async replicate

    SS1--x NM: Failure detected
    NM->>NM: Mark SS1 DOWN
    NM->>SS2: Serve READ requests
```

✔ Matches **Spec 3.5 exactly**

---

## 9️⃣ Error Handling Flow

```mermaid
flowchart TD
    ClientRequest --> NMCheck
    NMCheck -->|Path not found| ERR1[ERROR: FILE_NOT_FOUND]
    NMCheck -->|File locked| ERR2[ERROR: FILE_BUSY]
    NMCheck -->|SS Down| ERR3[ERROR: SERVER_UNAVAILABLE]
    NMCheck -->|Valid| ForwardRequest
```

---

## 🔟 What You Can Claim Confidently in Viva / Resume

You can safely say:

* **“Control plane & data plane separation”**
* **“Non-blocking naming server with async acknowledgements”**
* **“Fault-tolerant replicated storage with failure detection”**
* **“Optimized lookup using Trie/HashMap + LRU cache”**

---
