# fdiff
A simple file difference tracker. It helps you track changes in your files by maintaining an index of file hashes.  
Extracts only Git’s diff detection functionality, and can be faster than Git in some cases.  
Since it processes asynchronously, daemonization is not required, making it a good choice if you prefer not to use tools like `inotifywait`.  

## Features
- **Fast**: Uses file hashes to quickly determine changes.
- **Simple**: Easy to use with a straightforward command-line interface.
- **No Daemon**: Operates without needing a background service, making it lightweight.
- **Status Codes**: Provides clear exit codes for script integration.
- **Use strlcpy/strlcat**: Ensures safe string operations to prevent buffer overflows.
  
Thanks to OpenBSD for the `strlcpy` and `strlcat` functions, which are used to safely copy and concatenate strings, preventing buffer overflows.  

As shown below, each execution is assigned a status code, allowing flexible branching in shell scripts.

```
#define EXIT_OK 0
#define EXIT_FAIL 1
#define EXIT_NOFILE 3
#define EXIT_INTERNAL 4
#define EXIT_ALREADY_INITIALIZED 5
#define EXIT_ALREADY_ADDED 6
#define EXIT_DIFF_FOUND 7
```

example usage:
```bash
$ echo $?
0
$ fdiff init
Already initialized.
$ echo $?
5
$ fdiff add .
$ echo $?
0
$ fdiff add .
$ echo $?
6
```

## Installation

To build the project, run:
```bash
make
```

To install the binary to `/usr/local/bin`, run:
```bash
sudo make install
```

## Uninstallation
To uninstall the binary, run:
```bash
sudo make uninstall
```

## Usage

`fdiff` works by maintaining an index of files in a `.fdiff` directory. You first need to initialize it, then you can add files and check their status.

### Initialize

Initializes a new fdiff repository in the current directory. This creates a `.fdiff` directory.
```bash
fdiff init
```

### Add files

Adds one or more files or directories to be tracked.
```bash
fdiff add <path>...
```
For example, to add all files in the `src` directory:
```bash
fdiff add src
```
or to add current directory:
```bash
fdiff add .
```

### Check status

Shows the status of tracked files, indicating new, modified, or deleted files.
```bash
fdiff status
```
