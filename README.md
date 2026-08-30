# Banking System 
**Course:** Advanced Programming  
**Group:** [Group Name]  
**Members:** [Member 1] — [Member 2]

---

## Project Structure
```
├── main.cpp        # Entry point
├── src/            # Source files (.cpp)
├── include/        # Header files (.h)
├── data/           # JSON data files (git-ignored at runtime)
├── Makefile        # Build automation
└── README.md
```

---

## How to Compile & Run

This project uses a **Makefile** to make compiling easier.
Instead of typing a long g++ command every time, you just use `make`.

### Step 1 — Make sure g++ is installed
```bash
g++ --version
```
If you get a version number you're good. If not, install it:
- **Windows:** install [MinGW](https://www.mingw-w64.org/) and add it to your PATH
- **Linux:** `sudo apt install g++`
- **Mac:** `xcode-select --install`

### Step 2 — Open a terminal in the project folder
- In VS Code: **Terminal → New Terminal**
- Make sure you're in the root of the project (where the Makefile is)

### Step 3 — Compile
```bash
make
```
This reads the Makefile and compiles all `.cpp` files into an executable called `bank_system`.
You will see output like:
```
g++ -std=c++17 -Wall -c main.cpp -o main.o
g++ -std=c++17 -Wall -c src/bank.cpp -o src/bank.o
g++ -std=c++17 -Wall -o bank_system main.o src/bank.o
```

### Step 4 — Run
```bash
./bank_system
```
On Windows:
```cmd
bank_system.exe
```

### Step 5 — Clean compiled files
If you want to recompile everything from scratch:
```bash
make clean
```
This deletes all `.o` files and the executable. Then just run `make` again.

### Compile and run in one command
```bash
make run
```

---

## Quick Reference

| Command | What it does |
|---|---|
| `make` | Compiles the project |
| `make run` | Compiles and runs immediately |
| `make clean` | Deletes compiled files |

---

## Features (Phase 1)
- Branch management: create and list branches
- Account management: create, close, delete, list accounts
- Transactions: deposit, withdraw, transfer
- History: get balance, transaction history, transaction details
- Data management: clear history, reset all
- Passwords hashed with SHA-256
- All data persisted in JSON files under data/

---

## Notes
- Data files are created automatically on first run
- If no data files exist, system starts with empty state
- Never commit your data/ JSON files (already in .gitignore)
- Never push your passwords or any sensitive data