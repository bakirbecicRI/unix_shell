# Mini Unix Shell (C++)

A minimal Unix-like shell implemented in C++ on Linux, built to understand
core operating system concepts such as process creation, command execution,
and I/O redirection.

This project is intentionally lightweight but focuses on correctness and
clarity rather than feature completeness.

---

## Features

- Interactive shell prompt with current working directory
- Built-in commands:
  - `cd` – change directory
  - `pwd` – print working directory
  - `exit` – exit the shell
  - `help` – display supported features
  - `history` – show command history (in-memory)
- Execution of external commands using `fork()` and `execvp()`
- Input and output redirection:
  - `<` input redirection
  - `>` output overwrite
  - `>>` output append
- Argument parsing with support for:
  - single quotes `'...'`
  - double quotes `"..."` (with basic escaping)
- Environment variable expansion (e.g. `$HOME`, `$USER`)

---

## Example Usage

```bash
myshell:/home/user$ echo "hello world"
hello world

myshell:/home/user$ echo test > out.txt
myshell:/home/user$ cat < out.txt
test

myshell:/home/user$ cd $HOME
myshell:/home/user$ pwd
/home/user
```

---

## Build & Run

```bash
g++ -std=c++17 -Wall -Wextra -O2 main.cpp -o myshell
./myshell
```

Tested on Linux (Ubuntu / WSL).

---

## Implementation Overview

The shell follows the classic Unix execution model:

1. Read user input
2. Tokenize input with quote handling
3. Expand environment variables
4. Parse arguments and I/O redirections
5. Execute built-in commands in the parent process
6. Execute external commands via:
   - `fork()`
   - `execvp()`
   - `waitpid()`
7. Apply redirections using `open()` and `dup2()`

---

## Why This Project?

This project was built to gain hands-on experience with:

- Unix process model (parent/child processes)
- System calls: `fork`, `exec`, `wait`, `dup2`
- File descriptors and I/O redirection
- Shell parsing fundamentals
- Linux-based development in C++

---

## Limitations

- No pipeline (`|`) support
- No background execution (`&`)
- No persistent history

These were intentionally omitted to keep the project focused and readable.

---

## License

This project is provided for educational purposes.
