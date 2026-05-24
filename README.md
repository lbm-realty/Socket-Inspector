# Network Interceptor

## Overview
A high performance, multithreaded reverse proxy written in C. It intercepts outgoing AI API requests at the network layer to scan payloads and block sensitive data leaks before they reach external servers.

## Architecture
This proxy avoids heavy web frameworks to maximize network throughput. It relies on standard POSIX libraries to manage concurrent traffic.

* **Threads:** Utilizes a producer-consumer model via pthreads.
* **Socket Management:** The main thread accepts raw TCP connections and pushes file descriptors into a mutex locked queue.
* **Worker Threads:** Consumer threads pop descriptors from the queue, read the HTTP payload, and execute a high-speed scan for blacklisted signatures.
* **Concurrency:** Designed to clear the socket queue with single digit millisecond latency under heavy load.

## Benchmarks
Load testing was conducted using wrk on a Linux HPC cluster. 

**Test Parameters:**
* 400 concurrent TCP connections
* 12 OS threads
* 15 second sustained duration

**Results:**
* **Throughput:** ~3,500 Requests/sec
* **Latency:** ~7.6ms average overhead
* **Accuracy:** 100% block rate on 53,785 injected vulnerability payloads without a single dropped connection.

## Quick Start

**Dependencies:**
* GCC
* POSIX Threads

**Build:**
```bash
gcc main.c -o proxy -lpthread
```

**Run:**
```bash
./proxy
```

**Verify Interception:**
```bash
curl -X POST http://localhost:8080 -H "Content-Type: application/json" -d '{"prompt": "Test data sk-proj-12345ABC"}
```
