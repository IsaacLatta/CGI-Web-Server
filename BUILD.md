# Build

> **Not** cross-platform, **Linux only** (required for syscalls).

## Dependencies

This project uses a local [vcpkg](https://github.com/microsoft/vcpkg) checkout for package management.
You will need [CMake.](https://cmake.org/download/), a C++20 compiler and
Perl (for asio's openssl vcpkg port) installed. 

## Clone and Build

```bash
git clone https://github.com/IsaacLatta/CGI-Web-Server.git
cd CGI-Web-Server
```

Init and bootstrap vcpkg:
```bash
git submodule update --init --recursive && ./vcpkg/bootstrap-vcpkg.sh
```

Configure and build:
```bash
cmake -S . -B build && cmake --build build
```