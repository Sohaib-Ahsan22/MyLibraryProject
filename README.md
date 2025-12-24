# Library Management System (C++ & PostgreSQL)

This is a console-based Library Management System connected to a PostgreSQL database. It handles book borrowing, returning, and automatic waitlist management using FIFO logic.

## Features
- **Persistent Storage:** Data is stored in PostgreSQL, not RAM.
- **Waitlist System:** If a book is borrowed, users are added to a queue.
- **Auto-Assignment:** When a book is returned, it is automatically assigned to the next person in the waitlist.
- **Search:** Case-insensitive search functionality.

## How to Run
1. Install **Visual Studio** with "Desktop development with C++".
2. Install **PostgreSQL** and **pgAdmin**.
3. Install `libpqxx` library using vcpkg:
   `vcpkg install libpqxx:x64-windows`
4. Create a database in pgAdmin and run the commands from `database_setup.sql`.
5. Update the connection string in the C++ code with your database password.
6. Build and Run!

