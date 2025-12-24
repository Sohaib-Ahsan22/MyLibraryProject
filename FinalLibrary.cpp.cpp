#define NOMINMAX
#define _SILENCE_ALL_CXX20_DEPRECATION_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <pqxx/pqxx>

using namespace std;

// DATABASE CONNECTION
string conn_string = "dbname=xyz user=postgres password=password hostaddr=127.0.0.1 port=5432";

// ---------------------------------------------------
// 1. ADD BOOK
// ---------------------------------------------------
void addBook(string title, string author) {
    try {
        pqxx::connection C(conn_string);
        pqxx::work W(C);

        // SQL Injection se bachne ke liye W.quote() use kia hai
        string sql = "INSERT INTO Books (Title, Author) VALUES ("
            + W.quote(title) + ", "
            + W.quote(author) + ");";

        W.exec(sql);
        W.commit();
        cout << ">> Book added successfully to Database.\n";
    }
    catch (const std::exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

// ---------------------------------------------------
// 2. BORROW BOOK (With Waitlist Logic)
// ---------------------------------------------------
void borrowBook(int id) {
    try {
        pqxx::connection C(conn_string);
        pqxx::work W(C);

        // Check if book exists and is borrowed
        pqxx::result R = W.exec("SELECT IsBorrowed, Title FROM Books WHERE BookID = " + to_string(id));

        if (R.size() == 0) {
            cout << ">> Book ID not found.\n";
            return;
        }

        bool isBorrowed = R[0]["IsBorrowed"].as<bool>();
        string title = R[0]["Title"].c_str();

        if (isBorrowed) {
            // Book already taken -> Add to Waitlist
            string name;
            cout << ">> Book is currently unavailable (Borrowed).\n";
            cout << ">> Enter Name to join Waitlist: ";
            cin.ignore();
            getline(cin, name);

            string sql = "INSERT INTO Waitlist (BookID, BorrowerName) VALUES ("
                + to_string(id) + ", " + W.quote(name) + ");";
            W.exec(sql);
            W.commit();
            cout << ">> Added " << name << " to the waitlist for '" << title << "'.\n";
        }
        else {
            // Book is free -> Borrow it
            string name, date;
            cout << "Enter Borrower Name: ";
            cin.ignore();
            getline(cin, name);
            cout << "Enter Date (YYYY-MM-DD): ";
            getline(cin, date);

            // Update Book Status AND Add to ActiveBorrows table
            W.exec("UPDATE Books SET IsBorrowed = TRUE WHERE BookID = " + to_string(id));

            string sql = "INSERT INTO ActiveBorrows (BookID, BorrowerName, BorrowDate) VALUES ("
                + to_string(id) + ", "
                + W.quote(name) + ", "
                + W.quote(date) + ");";
            W.exec(sql);

            W.commit();
            cout << ">> Book '" << title << "' issued to " << name << ".\n";
        }
    }
    catch (const std::exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

// ---------------------------------------------------
// 3. RETURN BOOK (Automatic Assignment Logic)
// ---------------------------------------------------
void returnBook(int id) {
    try {
        pqxx::connection C(conn_string);
        pqxx::work W(C);

        // Check if book is actually borrowed
        pqxx::result R = W.exec("SELECT IsBorrowed FROM Books WHERE BookID = " + to_string(id));
        if (R.size() == 0 || !R[0][0].as<bool>()) {
            cout << ">> This book is not currently borrowed (or wrong ID).\n";
            return;
        }

        // Remove the current borrower
        W.exec("DELETE FROM ActiveBorrows WHERE BookID = " + to_string(id));

        // CHECK WAITLIST: Is someone waiting?
        // FIFO: Get the person with the oldest RequestDate
        pqxx::result WaitRes = W.exec("SELECT WaitID, BorrowerName FROM Waitlist WHERE BookID = " + to_string(id) + " ORDER BY RequestDate ASC LIMIT 1");

        if (WaitRes.size() > 0) {
            // Yes, someone is waiting!
            int waitID = WaitRes[0]["WaitID"].as<int>();
            string nextPerson = WaitRes[0]["BorrowerName"].c_str();

            // 1. Remove from Waitlist
            W.exec("DELETE FROM Waitlist WHERE WaitID = " + to_string(waitID));

            // 2. Add to Active Borrows (Auto-Assign)
            string sql = "INSERT INTO ActiveBorrows (BookID, BorrowerName, BorrowDate) VALUES ("
                + to_string(id) + ", " + W.quote(nextPerson) + ", 'AUTO-ASSIGNED');";
            W.exec(sql);

            cout << ">> Book Returned. \n>> AUTOMATICALLY ASSIGNED to next in waitlist: " << nextPerson << endl;
        }
        else {
            // No one waiting. Mark book as available on shelf.
            W.exec("UPDATE Books SET IsBorrowed = FALSE WHERE BookID = " + to_string(id));
            cout << ">> Book returned successfully. Now available on shelf.\n";
        }

        W.commit();
    }
    catch (const std::exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}

// ---------------------------------------------------
// 4. DISPLAY & SEARCH
// ---------------------------------------------------
void displayBooks() {
    try {
        pqxx::connection C(conn_string);
        pqxx::work W(C);
        pqxx::result R = W.exec("SELECT * FROM Books ORDER BY BookID ASC");

        cout << "\n---------------- LIBRARY CATALOG ----------------\n";
        for (auto row : R) {
            cout << "ID: " << row["BookID"].c_str()
                << " | Title: " << row["Title"].c_str()
                << " | Author: " << row["Author"].c_str()
                << " | Status: " << (row["IsBorrowed"].as<bool>() ? "[BORROWED]" : "[AVAILABLE]")
                << endl;
        }
        cout << "-------------------------------------------------\n";
    }
    catch (const std::exception& e) { cerr << e.what() << endl; }
}

// ---------------------------------------------------
// 5. REMOVE BOOK (Safely)
// ---------------------------------------------------
void removeBook(int id) {
    try {
        pqxx::connection C(conn_string);
        pqxx::work W(C);

        // Step 1: Check if book exists
        pqxx::result R = W.exec("SELECT Title FROM Books WHERE BookID = " + to_string(id));
        if (R.size() == 0) {
            cout << ">> Error: Book ID not found.\n";
            return;
        }
        string title = R[0]["Title"].c_str();

        // Step 2: Delete dependencies (Foreign Keys) first
        W.exec("DELETE FROM ActiveBorrows WHERE BookID = " + to_string(id));
        W.exec("DELETE FROM Waitlist WHERE BookID = " + to_string(id));

        // Step 3: Now delete the actual book
        W.exec("DELETE FROM Books WHERE BookID = " + to_string(id));

        W.commit();
        cout << ">> Success: '" << title << "' (ID: " << id << ") and all its records have been deleted.\n";

    }
    catch (const std::exception& e) {
        cerr << "Error removing book: " << e.what() << endl;
    }
}

void searchBook(string title) {
    try {
        pqxx::connection C(conn_string);
        pqxx::work W(C);

        // Use ILIKE for case-insensitive search
        string sql = "SELECT * FROM Books WHERE Title ILIKE " + W.quote("%" + title + "%");
        pqxx::result R = W.exec(sql);

        if (R.size() > 0) {
            cout << ">> Found " << R.size() << " book(s):\n";
            for (auto row : R) {
                // ERROR WAS HERE: We must use .c_str() or .as<int>()
                cout << "   ID: " << row["BookID"].as<int>()
                    << " | " << row["Title"].c_str() << endl;
            }
        }
        else {
            cout << ">> No books found with that title.\n";
        }
    }
    catch (const std::exception& e) { cerr << e.what() << endl; }
}

// ---------------------------------------------------
// MAIN MENU
// ---------------------------------------------------
int main() {
    int option;
    cout << "\n\n**** Library Management System (Database Connected) ****\n" << endl;

    do {
        // Option 7 add kia hai
        cout << "\n1. Add Book\n2. Borrow Book\n3. Return Book\n4. Search Book\n5. Display All\n6. Remove Book\n7. Exit\nSelect: ";
        cin >> option;

        switch (option) {
        case 1: {
            string t, a;
            cout << "Title: "; cin.ignore(); getline(cin, t);
            cout << "Author: "; getline(cin, a);
            addBook(t, a);
            break;
        }
        case 2: {
            int id; cout << "Enter Book ID: "; cin >> id;
            borrowBook(id);
            break;
        }
        case 3: {
            int id; cout << "Enter Book ID: "; cin >> id;
            returnBook(id);
            break;
        }
        case 4: {
            string t; cout << "Enter Title Search: "; cin.ignore(); getline(cin, t);
            searchBook(t);
            break;
        }
        case 5: displayBooks(); break;

            // NEW CASE ADDED HERE
        case 6: {
            int id; cout << "Enter Book ID to Remove: "; cin >> id;
            // Confirmation (Safety Check)
            char confirm;
            cout << "Are you sure? This will delete borrowing history too (y/n): ";
            cin >> confirm;
            if (confirm == 'y' || confirm == 'Y') {
                removeBook(id);
            }
            else {
                cout << ">> Operation Cancelled.\n";
            }
            break;
        }

        case 7: cout << "Exiting...\n"; break;
        default: cout << "Invalid Option.\n";
        }
    } while (option != 7);

    return 0;
}
