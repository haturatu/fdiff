A simple file difference tracker. It helps you track changes in your files by maintaining an index of file hashes.  
Extracts only Git’s diff detection functionality, and can be faster than Git in some cases.  
Since it processes asynchronously, daemonization is not required, making it a good choice if you prefer not to use tools like `inotifywait`.  

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
