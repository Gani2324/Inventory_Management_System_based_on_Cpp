# Inventory Management System (C++ + SQLite)

A minimal, **menu-driven** inventory app written in **C++17** with **SQLite**.
It covers:
- Add suppliers & products
- Receive stock (purchase)
- Make a sale & generate bill
- Update product price
- View inventory & low-stock items
- Sales summary with aggregates
- **Triggers** to keep stock consistent and prevent negative inventory

## Tech
- C++17
- SQLite3 (single embedded DB file `inventory.db` — no server required)

## Build

### Linux/macOS (with SQLite installed)
```bash
g++ -std=gnu++17 -O2 -Wall -I./src -o inventory src/main.cpp -lsqlite3
```

If you don’t have SQLite dev libs:
- Ubuntu/Debian: `sudo apt-get install libsqlite3-dev`
- macOS (Homebrew): `brew install sqlite`

### Windows (MSYS2/MinGW)
1. Install MSYS2 and run:
   ```bash
   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-sqlite
   ```
2. Build:
   ```bash
   g++ -std=gnu++17 -O2 -Wall -I./src -o inventory.exe src/main.cpp -lsqlite3
   ```

> Optionally use CMake:
```bash
cmake -S . -B build
cmake --build build --config Release
```

## Run
```bash
./inventory   # or inventory.exe on Windows
```
The program creates `inventory.db` in the working directory on first run.

## SQL Schema Highlights
- Tables: `suppliers`, `products`, `purchases`, `sales`, `sale_items`
- Triggers:
  - Increase stock on purchase insert
  - Prevent negative stock before sale items insert
  - Decrease stock on sale items insert
  - Restore stock on sale items delete
  - Maintain `sales.total` via aggregate of sale items

See `sql/schema.sql` for full details.

## Notes
- This is a teaching project. Error handling is friendly and verbose.
- You can safely delete `inventory.db` to start fresh.
