# Justin
🔥 A minimal AUR helper written in **C** with support for
installation of legacy versions. 🔥\
Its name is Justin.

## Usage
```text
Usage: justin <target> [flags]
target :: Package to install
-l     :: Always install the latest version
-y     :: Accept prompts by default
```

## Dependencies
The goal is to use as little dependencies as possible, to depend on commonly used libraries, and to use libraries with 
exposed C APIs. The only dependencies invoked through command-line are ``sudo`` and ``makepkg``.
- [sudo](https://archlinux.org/packages/core/x86_64/sudo/) (this is required by [base-devel](https://archlinux.org/packages/core/any/base-devel/) and you should probably install that anyways)
- [libgit2](https://archlinux.org/packages/extra/x86_64/libgit2/)
- libcurl ([curl](https://archlinux.org/packages/core/x86_64/curl/))
- libalpm (part of [pacman](https://archlinux.org/packages/core/x86_64/pacman/))
- makepkg (part of [pacman](https://archlinux.org/packages/core/x86_64/pacman/))
- libjson-c ([json-c](https://archlinux.org/packages/core/x86_64/json-c/))
