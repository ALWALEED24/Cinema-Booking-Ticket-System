#include "Database.h"
#include <iostream>
#include <iomanip> 
#include <conio.h> 
#include <cctype>  
#include <ctime>
#include <vector> 
#include <sstream> 
#include <set>
#include <regex>
#include <algorithm>
#include <locale>

sql::Connection* globalCon = nullptr;
void connectToDatabase() {
    try {
        sql::Driver* driver = get_driver_instance();
        globalCon = driver->connect("tcp://127.0.0.1:3306", "root", ""); // Update with your MySQL credentials
        globalCon->setSchema("cinema booking ticket system");           // Update with your database name
    }
    catch (sql::SQLException& e) {
        cout << "SQL Error: " << e.what() << " (MySQL error code: " << e.getErrorCode()
            << ", SQLState: " << e.getSQLState() << " )" << endl;
    }
}


void closeDatabaseConnection() {
    if (globalCon) {
        globalCon->close();
        delete globalCon;
        globalCon = nullptr;
    }
}

void registerUser(const string& firstName, const string& lastName, const string& phoneNumber,
    const string& email, const string& userName, const string& password,
    const string& date, const string& matricNumber, bool isStudent) {
    try {
        sql::PreparedStatement* pstmt = globalCon->prepareStatement(
            "INSERT INTO users (FirstName, LastName, PhoneNumber, Email, UserName, Password, Date, MatricNumber, IsStudent) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");

        pstmt->setString(1, firstName);
        pstmt->setString(2, lastName);
        pstmt->setString(3, phoneNumber);
        pstmt->setString(4, email);
        pstmt->setString(5, userName);
        pstmt->setString(6, password);
        pstmt->setString(7, date);
        pstmt->setString(8, matricNumber.empty() ? "N/A" : matricNumber);
        pstmt->setBoolean(9, isStudent);

        pstmt->executeUpdate();
        cout << "User registered successfully!" << endl;

        delete pstmt;
    }
    catch (sql::SQLException& e) {
        cerr << "SQL Error: " << e.what() << " (MySQL error code: " << e.getErrorCode()
            << ", SQLState: " << e.getSQLState() << ")" << endl;
    }
}


int insertBooking(int userID, int movieID, const string& screenType,
    const string& bookingDate,
    const string& timeSlot, const string& seatsBooked, const string& ticketType, double totalPrice) {
    try {
        // Check if a booking already exists
        sql::PreparedStatement* checkStmt = globalCon->prepareStatement(
            "SELECT BookingID, SeatsBooked FROM booking WHERE User_id = ? AND Movie_id = ? AND BookingDate = ? AND TimeSlot = ? AND ScreenType = ?");
        checkStmt->setInt(1, userID);
        checkStmt->setInt(2, movieID);
        checkStmt->setString(3, bookingDate);
        checkStmt->setString(4, timeSlot);
        checkStmt->setString(5, screenType);

        sql::ResultSet* res = checkStmt->executeQuery();

        if (res->next()) {
            // Booking exists, update the SeatsBooked and TotalPrice
            int bookingID = res->getInt("BookingID");
            string existingSeats = res->getString("SeatsBooked");

            // Merge seats and remove duplicates
            set<string> uniqueSeats;
            istringstream stream(existingSeats + "," + seatsBooked);
            string seat;
            while (getline(stream, seat, ',')) {
                // Manually handle trimming by removing whitespace from both ends
                seat.erase(seat.begin(), find_if(seat.begin(), seat.end(), [](unsigned char ch) { return !isspace(ch); }));
                seat.erase(find_if(seat.rbegin(), seat.rend(), [](unsigned char ch) { return !isspace(ch); }).base(), seat.end());
                uniqueSeats.insert(seat);
            }

            string updatedSeats;
            for (const auto& s : uniqueSeats) {
                if (!updatedSeats.empty()) updatedSeats += ",";
                updatedSeats += s;
            }

            sql::PreparedStatement* updateStmt = globalCon->prepareStatement(
                "UPDATE booking SET SeatsBooked = ?, TicketTypes = ?, TotalPrice = TotalPrice + ? WHERE BookingID = ?");
            updateStmt->setString(1, updatedSeats);
            updateStmt->setString(2, ticketType);
            updateStmt->setDouble(3, totalPrice);
            updateStmt->setInt(4, bookingID);
            updateStmt->executeUpdate();
            delete updateStmt;

            delete res;
            delete checkStmt;

            return bookingID; // Return existing booking ID
        }
        delete res;
        delete checkStmt;

        // No existing booking; insert a new one
        sql::PreparedStatement* insertStmt = globalCon->prepareStatement(
            "INSERT INTO booking (User_id, Movie_id, ScreenType, BookingDate, TimeSlot, SeatsBooked, TicketTypes, TotalPrice) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
        insertStmt->setInt(1, userID);
        insertStmt->setInt(2, movieID);
        insertStmt->setString(3, screenType);
        insertStmt->setString(4, bookingDate);
        insertStmt->setString(5, timeSlot);
        insertStmt->setString(6, seatsBooked);
        insertStmt->setString(7, ticketType);
        insertStmt->setDouble(8, totalPrice);

        insertStmt->executeUpdate();

        // Retrieve the newly inserted BookingID
        sql::PreparedStatement* selectStmt = globalCon->prepareStatement("SELECT LAST_INSERT_ID() AS BookingID");
        sql::ResultSet* newRes = selectStmt->executeQuery();
        int bookingID = 0;
        if (newRes->next()) {
            bookingID = newRes->getInt("BookingID");
        }

        delete newRes;
        delete selectStmt;
        delete insertStmt;

        return bookingID;
    }
    catch (sql::SQLException& e) {
        cerr << "SQL Error (insertBooking): " << e.what() << endl;
        return -1;
    }
}



int insertPayment(int userID, int bookingID, const string& paymentMethod, double paymentAmount) {
    try {
        // Validate UserID
        sql::PreparedStatement* checkUser = globalCon->prepareStatement(
            "SELECT COUNT(*) FROM users WHERE User_id = ?");
        checkUser->setInt(1, userID);
        sql::ResultSet* resUser = checkUser->executeQuery();
        resUser->next();

        if (resUser->getInt(1) == 0) {
            cerr << "Error: UserID " << userID << " does not exist.\n";
            delete resUser;
            delete checkUser;
            return -1;
        }
        delete resUser;
        delete checkUser;

        // Validate BookingID
        sql::PreparedStatement* checkBooking = globalCon->prepareStatement(
            "SELECT COUNT(*) FROM booking WHERE BookingID = ?");
        checkBooking->setInt(1, bookingID);
        sql::ResultSet* resBooking = checkBooking->executeQuery();
        resBooking->next();

        if (resBooking->getInt(1) == 0) {
            cerr << "Error: BookingID " << bookingID << " does not exist.\n";
            delete resBooking;
            delete checkBooking;
            return -1;
        }
        delete resBooking;
        delete checkBooking;

        // Insert Payment
        sql::PreparedStatement* pstmt = globalCon->prepareStatement(
            "INSERT INTO payment (User_id, Booking_id, PaymentMethod, PaymentAmount, PaymentDate, PaymentConfirmed) "
            "VALUES (?, ?, ?, ?, NOW(), ?)");

        pstmt->setInt(1, userID);
        pstmt->setInt(2, bookingID);
        pstmt->setString(3, paymentMethod);
        pstmt->setDouble(4, paymentAmount);
        pstmt->setInt(5, 1); // PaymentConfirmed set to 1 (success)

        pstmt->executeUpdate();

        // Retrieve the generated PaymentID
        sql::PreparedStatement* pstmtSelect = globalCon->prepareStatement("SELECT LAST_INSERT_ID() AS PaymentID");
        sql::ResultSet* res = pstmtSelect->executeQuery();
        int paymentID = 0;
        if (res->next()) {
            paymentID = res->getInt("PaymentID");
        }

        delete res;
        delete pstmtSelect;
        delete pstmt;

        cout << "Payment successfully stored in the database! Payment ID: " << paymentID << "\n";
        return paymentID;
    }
    catch (sql::SQLException& e) {
        cerr << "SQL Error (insertPayment): " << e.what() << endl;
        return -1;
    }
}


void displayMoviePage(int userID, bool isStudent) {
    while (true) {
        system("cls");

        // Centered header
        cout << "\n\n\n\n\n\n\n";
        cout << "                                                                     *********************************************************************\n";
        cout << "                                                                     *                           MOVIE PAGE                              *\n";
        cout << "                                                                     *********************************************************************\n\n";

        try {
            // SQL query to fetch the top 9 recommended movies
            string topMoviesQuery = "SELECT Movie_id, Title, Genre, Rating, Duration, ReleaseDate "
                "FROM movies ORDER BY Rating DESC LIMIT 9";

            sql::PreparedStatement* pstmt = globalCon->prepareStatement(topMoviesQuery);
            sql::ResultSet* res = pstmt->executeQuery();

            // Display header for the movies table
            cout << "                                                 Top 9 Recommended Movies:\n";
            cout << "                                                 +-------+------------------------------------------+------------------+--------+----------+--------------+\n";
            cout << "                                                 | ID    | Title                                    | Genre            | Rating | Duration | Release Date |\n";
            cout << "                                                 +-------+------------------------------------------+------------------+--------+----------+--------------+\n";

            // Display movie details row by row
            bool hasMovies = false;
            while (res->next()) {
                hasMovies = true;

                string duration = res->getString("Duration");
                if (duration.empty()) duration = "N/A";

                cout << "                                                 | " << setw(5) << res->getInt("Movie_id") << " | "
                    << left << setw(40) << res->getString("Title") << " | "
                    << setw(16) << res->getString("Genre") << " | "
                    << setw(6) << fixed << setprecision(1) << res->getDouble("Rating") << " | "
                    << setw(8) << duration << " | "
                    << setw(12) << res->getString("ReleaseDate") << " |\n";
            }

            cout << "                                                 +-------+------------------------------------------+------------------+--------+----------+--------------+\n";

            // If no movies are found
            if (!hasMovies) {
                cout << "\033[31m\n                                                 No recommended movies are available at the moment.\033[0m\n";
                system("pause");
                continue;
            }

            delete res;
            delete pstmt;

            // Provide user options for interaction
            cout << "\n                                                 Options:\n";
            cout << "                                                 1. Select a Movie by ID\n";
            cout << "                                                 2. View and Search Movies\n";
            cout << "                                                 3. Update My Profile\n";
            cout << "                                                 4. View My Bookings\n";
            cout << "                                                 5. Go Back to Main Menu\n";
            cout << "                                                 Enter your choice: ";

            string input;
            cin >> input;

            // Validate input
            if (input.find_first_not_of("0123456789") != string::npos || input.empty()) {
                cout << "\033[31m\n                                                 Invalid choice. Please enter a valid number.\033[0m\n";
                system("pause");
                continue;
            }

            int choice = stoi(input);

            switch (choice) {
            case 1: {
                cout << "\n                                                 Enter the Movie ID: ";
                int selectedMovieID;
                cin >> selectedMovieID;

                // Validate Movie ID
                string selectedMovieTitle;
                try {
                    sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                        "SELECT Title FROM movies WHERE Movie_id = ?");
                    pstmt->setInt(1, selectedMovieID);
                    sql::ResultSet* res = pstmt->executeQuery();

                    if (res->next()) {
                        selectedMovieTitle = res->getString("Title");
                        delete res;
                        delete pstmt;

                        cout << "\n                                                 You have selected the movie: " << selectedMovieTitle << "\n";
                        system("pause");

                        // Proceed to the next step
                        displayDaysPage(selectedMovieTitle, userID, selectedMovieID, isStudent);
                        return; // Exit the movie page loop
                    }
                    else {
                        delete res;
                        delete pstmt;
                        cout << "\033[31m\n                                                 Invalid Movie ID. Please try again.\033[0m\n";
                        system("pause");
                    }
                }
                catch (sql::SQLException& e) {
                    cerr << "\033[31mSQL Error (Validate Movie ID):\033[0m " << e.what() << endl;
                    system("pause");
                }
                break;
            }
            case 2:
                browseAndSearchMovies(userID, isStudent); // Navigate to the search page
                return; // Exit the movie page loop
            case 3:
                updateUserProfile(userID); // Navigate to the user profile update page
                break;
            case 4:
                viewUserBookings(userID); // Navigate to the view bookings page
                break;
            case 5:
                cout << "\n                                                 Returning to the main menu...\n";
                system("pause");
                return;

            default:
                cout << "\033[31m\n                                                 Invalid choice. Please try again.\033[0m\n";
                system("pause");
            }
        }
        catch (sql::SQLException& e) {
            cout << "SQL Error: " << e.what() << " (MySQL error code: " << e.getErrorCode()
                << ", SQLState: " << e.getSQLState() << " )" << endl;
            system("pause");
        }
    }
}


void viewUserBookings(int userID) {
    system("cls");

    cout << "\n\n\n\n\n\n\n";
    cout << "                                                                     *********************************************************************\n";
    cout << "                                                                     *                        MY BOOKINGS                                *\n";
    cout << "                                                                     *********************************************************************\n\n";

    try {
        sql::PreparedStatement* pstmt = globalCon->prepareStatement(
            "SELECT b.BookingID, m.Title, b.BookingDate, b.TimeSlot, b.SeatsBooked, b.TotalPrice "
            "FROM booking b JOIN movies m ON b.Movie_id = m.Movie_id WHERE b.User_id = ?");
        pstmt->setInt(1, userID);

        sql::ResultSet* res = pstmt->executeQuery();

        cout << "                                                 +---------+------------------------------------------+--------------+----------+----------------------+--------------+\n";
        cout << "                                                 | Booking | Movie Title                              | Date         | Time     | Seats               | Total Price  |\n";
        cout << "                                                 +---------+------------------------------------------+--------------+----------+----------------------+--------------+\n";

        while (res->next()) {
            cout << "                                                 | " << setw(7) << res->getInt("BookingID") << " | "
                << setw(40) << res->getString("Title") << " | "
                << setw(12) << res->getString("BookingDate") << " | "
                << setw(8) << res->getString("TimeSlot") << " | "
                << setw(20) << res->getString("SeatsBooked") << " | "
                << setw(12) << fixed << setprecision(2) << res->getDouble("TotalPrice") << " |\n";
        }

        cout << "                                                 +---------+------------------------------------------+--------------+----------+----------------------+--------------+\n";

        delete res;
        delete pstmt;
    }
    catch (sql::SQLException& e) {
        cerr << "SQL Error (View Bookings): " << e.what() << endl;
    }

    system("pause");
}


void updateUserProfile(int userID) {
    while (true) {
        system("cls");

        cout << "\n\n\n\n\n";
        cout << "                                                                     *********************************************************************\n";
        cout << "                                                                     *                         UPDATE PROFILE                            *\n";
        cout << "                                                                     *********************************************************************\n";
        cout << "                                                                     Options:\n";
        cout << "                                                                     1. Update First Name\n";
        cout << "                                                                     2. Update Last Name\n";
        cout << "                                                                     3. Update Phone Number\n";
        cout << "                                                                     4. Update Email\n";
        cout << "                                                                     5. Update Username\n";
        cout << "                                                                     6. Update Password\n";
        cout << "                                                                     7. Back to Profile Page\n";
        cout << "                                                                     Enter your choice: ";

        string input;
        cin >> input;

        if (input.find_first_not_of("0123456789") != string::npos || input.empty()) {
            cout << "\033[31m\n                                                                     Invalid choice. Please enter a valid number.\033[0m\n";
            system("pause");
            continue;
        }

        int choice = stoi(input);

        if (choice == 7) return; // Back to profile page

        string fieldName, newValue, confirmPassword, currentValue;
        switch (choice) {
        case 1:
            fieldName = "FirstName";
            break;
        case 2:
            fieldName = "LastName";
            break;
        case 3:
            fieldName = "PhoneNumber";
            break;
        case 4:
            fieldName = "Email";
            break;
        case 5:
            fieldName = "UserName";
            break;
        case 6:
            fieldName = "Password";
            break;
        default:
            cout << "\033[31m\n                                                                     Invalid choice. Please try again.\033[0m\n";
            system("pause");
            continue;
        }

        try {
            // Fetch the current value from the database
            sql::PreparedStatement* fetchStmt = globalCon->prepareStatement(
                "SELECT " + fieldName + " FROM users WHERE User_id = ?");
            fetchStmt->setInt(1, userID);
            sql::ResultSet* res = fetchStmt->executeQuery();

            if (res->next()) {
                currentValue = res->getString(fieldName);
            }
            delete res;
            delete fetchStmt;
        }
        catch (sql::SQLException& e) {
            cerr << "SQL Error (fetch current value): " << e.what() << endl;
            system("pause");
            continue;
        }

        cout << "\n                                                                     Enter new value for " << fieldName << ": ";
        cin.ignore();
        getline(cin, newValue);

        // Validate input
        if (newValue == currentValue) {
            cout << "\033[31m\n                                                                     New value cannot be the same as the current value. Please try again.\033[0m\n";
            system("pause");
            continue;
        }

        // Duplicate checks and validations
        if (fieldName == "PhoneNumber" || fieldName == "UserName" || fieldName == "Email") {
            try {
                string query = "SELECT COUNT(*) AS Count FROM users WHERE " + fieldName + " = ? AND User_id != ?";
                sql::PreparedStatement* checkStmt = globalCon->prepareStatement(query);
                checkStmt->setString(1, newValue);
                checkStmt->setInt(2, userID);
                sql::ResultSet* checkRes = checkStmt->executeQuery();

                if (checkRes->next() && checkRes->getInt("Count") > 0) {
                    cout << "\033[31m\n                                                                     This " << fieldName << " is already in use. Please choose a different one.\033[0m\n";
                    delete checkRes;
                    delete checkStmt;
                    system("pause");
                    continue; // Restart the loop
                }
                delete checkRes;
                delete checkStmt;
            }
            catch (sql::SQLException& e) {
                cerr << "SQL Error (duplicate check for " << fieldName << "): " << e.what() << endl;
                system("pause");
                continue;
            }
        }

        if (fieldName == "PhoneNumber" && (newValue.length() != 11 || !all_of(newValue.begin(), newValue.end(), ::isdigit))) {
            cout << "\033[31m\n                                                                     Phone number must be 11 digits. Please try again.\033[0m\n";
            system("pause");
            continue;
        }

        if (fieldName == "Email" && (newValue.find('@') == string::npos || newValue.length() < 5)) {
            cout << "\033[31m\n                                                                     Invalid email format. the email should be include @.\033[0m\n";
            system("pause");
            continue;
        }

        if (fieldName == "Password") {
            cout << "                                                                     Confirm Password: ";
            getline(cin, confirmPassword);
            if (newValue != confirmPassword) {
                cout << "\033[31m\n                                                                     Passwords do not match. please enter same password.\033[0m\n";
                system("pause");
                continue;
            }
        }

        // Update the database
        try {
            sql::PreparedStatement* updateStmt = globalCon->prepareStatement(
                "UPDATE users SET " + fieldName + " = ? WHERE User_id = ?");
            updateStmt->setString(1, newValue);
            updateStmt->setInt(2, userID);
            updateStmt->executeUpdate();
            delete updateStmt;

            cout << "\033[32m\n                                                                     " << fieldName << " updated successfully!\033[0m\n";
        }
        catch (sql::SQLException& e) {
            cerr << "SQL Error (updateUserProfile): " << e.what() << endl;
        }

        system("pause");
    }
}


void browseAndSearchMovies(int userID, bool isStudent) {
    int menuChoice;

    do {
        system("cls");

        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     *********************************************************************\n";
        cout << "                                                                     *                            SEARCH PAGE                            *\n";
        cout << "                                                                     *********************************************************************\n\n";

        try {
            string query = "SELECT Movie_id, Title, Genre, Rating, Duration, ReleaseDate FROM movies ORDER BY Rating DESC";

            sql::PreparedStatement* pstmt = globalCon->prepareStatement(query);
            sql::ResultSet* res = pstmt->executeQuery();

            cout << "\n                                                 Available Movies:\n";
            cout << "                                                 +-------+------------------------------------------+------------------+--------+----------+--------------+\n";
            cout << "                                                 | ID    | Title                                    | Genre            | Rating | Duration | Release Date |\n";
            cout << "                                                 +-------+------------------------------------------+------------------+--------+----------+--------------+\n";

            while (res->next()) {
                string duration = res->getString("Duration");
                if (duration.empty()) duration = "N/A";

                cout << "                                                 | " << setw(5) << res->getInt("Movie_id") << " | "
                    << left << setw(40) << res->getString("Title") << " | "
                    << setw(16) << res->getString("Genre") << " | "
                    << setw(6) << fixed << setprecision(1) << res->getDouble("Rating") << " | "
                    << setw(8) << duration << " | "
                    << setw(12) << res->getString("ReleaseDate") << " |\n";
            }

            cout << "                                                 +-------+------------------------------------------+------------------+--------+----------+--------------+\n";

            delete res;
            delete pstmt;
        }
        catch (sql::SQLException& e) {
            cout << "SQL Error: " << e.what() << " (MySQL error code: " << e.getErrorCode()
                << ", SQLState: " << e.getSQLState() << " )" << endl;
        }

        cout << "\n                                                 Options:\n";
        cout << "                                                 1. Enter the Movie ID to select a movie\n";
        cout << "                                                 2. Search for a movie by keyword\n";
        cout << "                                                 3. Go back to the main movie page\n";
        cout << "                                                 Enter your choice: ";

        string input;
        cin >> input;

        if (input.find_first_not_of("0123456789") != string::npos || input.empty()) {
            cout << "\033[31m\n                                                 Invalid choice. Please enter a number between 1 and 3.\033[0m\n";
            system("pause");
            continue;
        }

        menuChoice = stoi(input);

        switch (menuChoice) {
        case 1: {
            int movieID;
            while (true) {
                cout << "\n                                                 Enter the Movie ID to select a movie: ";
                cin >> input;

                if (input.find_first_not_of("0123456789") != string::npos || input.empty()) {
                    cout << "\033[31m\n                                                 Invalid Movie ID. Please enter a valid number.\033[0m\n";
                    system("pause");
                    continue;
                }

                movieID = stoi(input);

                try {
                    string movieQuery = "SELECT Title FROM movies WHERE Movie_id = ?";
                    sql::PreparedStatement* pstmt = globalCon->prepareStatement(movieQuery);
                    pstmt->setInt(1, movieID);
                    sql::ResultSet* res = pstmt->executeQuery();

                    if (res->next()) {
                        string chosenMovie = res->getString("Title");
                        cout << "\n                                                 You chose: \"" << chosenMovie << "\".\n";

                        displayDaysPage(chosenMovie, userID, movieID, isStudent);
                        delete res;
                        delete pstmt;
                        return;
                    }
                    else {
                        cout << "\033[31m\n                                                 Invalid Movie ID. Please try again.\033[0m\n";
                        system("pause");
                    }

                    delete res;
                    delete pstmt;
                }
                catch (sql::SQLException& e) {
                    cout << "SQL Error: " << e.what() << endl;
                }
            }
            break;
        }

        case 2: {
            while (true) {
                system("cls");
                cout << "\n\n\n\n\n\n\n\n\n";
                cout << "                                                                     *********************************************************************\n";
                cout << "                                                                     *                         SEARCH BY KEYWORD                        *\n";
                cout << "                                                                     *********************************************************************\n\n";

                cout << "\n                                                 Enter a keyword to search for a movie: ";
                string searchKeyword;
                cin.ignore();
                getline(cin, searchKeyword);

                if (searchKeyword.empty()) {
                    cout << "\033[31m\n                                                 Please enter a valid keyword.\033[0m\n";
                    system("pause");
                    continue;
                }

                try {
                    string searchQuery = "SELECT Movie_id, Title, Genre, Rating, Duration, ReleaseDate "
                        "FROM movies WHERE Title LIKE ? ORDER BY Rating DESC";

                    sql::PreparedStatement* pstmt = globalCon->prepareStatement(searchQuery);
                    pstmt->setString(1, "%" + searchKeyword + "%");

                    sql::ResultSet* res = pstmt->executeQuery();

                    cout << "\n                                                 Movies Matching Your Search:\n";
                    cout << "                                                 +-------+------------------------------------------+------------------+--------+----------+--------------+\n";
                    cout << "                                                 | ID    | Title                                    | Genre            | Rating | Duration | Release Date |\n";
                    cout << "                                                 +-------+------------------------------------------+------------------+--------+----------+--------------+\n";

                    bool hasResults = false;

                    while (res->next()) {
                        hasResults = true;
                        string duration = res->getString("Duration");
                        if (duration.empty()) duration = "N/A";

                        cout << "                                                 | " << setw(5) << res->getInt("Movie_id") << " | "
                            << left << setw(40) << res->getString("Title") << " | "
                            << setw(16) << res->getString("Genre") << " | "
                            << setw(6) << res->getDouble("Rating") << " | "
                            << setw(8) << duration << " | "
                            << setw(12) << res->getString("ReleaseDate") << " |\n";
                    }

                    cout << "                                                 +-------+------------------------------------------+------------------+--------+----------+--------------+\n";

                    if (!hasResults) {
                        cout << "\033[31m\n                                                 No movies found matching your search. Please try again.\033[0m\n";
                        system("pause");
                        continue;
                    }

                    delete res;
                    delete pstmt;

                    while (true) {
                        cout << "\n                                                 Enter the Movie ID to select a movie or 0 to go back: ";
                        cin >> input;

                        if (input.find_first_not_of("0123456789") != string::npos || input.empty()) {
                            cout << "\033[31m\n                                                 Invalid input. Please enter a valid number.\033[0m\n";
                            system("pause");
                            continue;
                        }

                        int movieID = stoi(input);

                        if (movieID == 0) {
                            break;
                        }

                        try {
                            string movieQuery = "SELECT Title FROM movies WHERE Movie_id = ?";
                            sql::PreparedStatement* detailsPstmt = globalCon->prepareStatement(movieQuery);
                            detailsPstmt->setInt(1, movieID);

                            sql::ResultSet* detailsRes = detailsPstmt->executeQuery();

                            if (detailsRes->next()) {
                                string chosenMovie = detailsRes->getString("Title");
                                cout << "\n                                                 You chose: \"" << chosenMovie << "\".\n";

                                displayDaysPage(chosenMovie, userID, movieID, isStudent);
                                delete detailsRes;
                                delete detailsPstmt;
                                return;
                            }
                            else {
                                cout << "\033[31m\n                                                 Invalid Movie ID. Please try again.\033[0m\n";
                                system("pause");
                            }

                            delete detailsRes;
                            delete detailsPstmt;
                        }
                        catch (sql::SQLException& e) {
                            cout << "SQL Error: " << e.what() << endl;
                        }
                    }
                }
                catch (sql::SQLException& e) {
                    cout << "SQL Error: " << e.what() << endl;
                }
            }
            break;
        }

        case 3:
            displayMoviePage(userID, isStudent);
            return;

        default:
            cout << "\033[31m\n                                                 Invalid choice. Please try again.\033[0m\n";
            system("pause");
        }
    } while (true);
}


void displayDaysPage(const string& chosenMovie, int userID, int movieID, bool isStudent) {
    while (true) {
        system("cls");

        // Get current date
        time_t now = time(0);
        tm localTime;
        tm* timePtr = &localTime;

        localtime_s(timePtr, &now);

        // the next 11 days in YYYY-MM-DD format
        string days[11];
        for (int i = 0; i < 11; i++) {
            char buffer[20];
            strftime(buffer, sizeof(buffer), "%Y-%m-%d", timePtr);
            days[i] = buffer;

            timePtr->tm_mday += 1;
            mktime(timePtr); // Normalize the date
        }

        // Display the Days Selection Page
        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     *********************************************************\n";
        cout << "                                                                     *                       SELECT A DAY                    *\n";
        cout << "                                                                     *********************************************************\n\n";

        cout << "                                                                               +------------+----------------+\n";
        cout << "                                                                               |   Option   |      Day       |\n";
        cout << "                                                                               +------------+----------------+\n";

        for (int i = 0; i < 11; i++) {
            cout << "                                                                               | " << setw(10) << (i + 1) << " | " << setw(14) << days[i] << " |\n";
        }
        cout << "                                                                               +------------+----------------+\n";

        cout << "\n                                                    Choose the day you want (or 0 to go back): ";
        string input;
        cin >> input;

        // Trim whitespace and validate input
        input.erase(remove_if(input.begin(), input.end(), ::isspace), input.end());

        // Validate input as an integer
        if (input.empty() || input.find_first_not_of("0123456789") != string::npos) {
            cout << "\033[31m\n                                                 *** INVALID INPUT! ***\n";
            cout << "                                                 Please enter a valid number between 0 and 11.\033[0m\n";
            system("pause");
            continue;
        }

        int dayChoice = stoi(input);

        // Navigate based on user's choice
        if (dayChoice == 0) {
            cout << "\n                                                 Returning to the search page...\n";
            browseAndSearchMovies(userID, isStudent); // Pass userID and isStudent for context
            return;
        }

        if (dayChoice >= 1 && dayChoice <= 11) {
            string chosenDay = days[dayChoice - 1];
            cout << "\n                                                 You chose \"" << chosenDay << "\".\n";

            // Proceed to the time and screen selection page
            displayScreenAndTimePage(userID, movieID, chosenDay, isStudent);
            return;
        }
        else {
            // Display error in red using ANSI escape codes
            cout << "\033[31m\n                                                 INVALID CHOICE! \n";
            cout << "                                                 Please enter a number between 0 and 11.\033[0m\n";
            system("pause");
        }
    }
}


void displayScreenAndTimePage(int userID, int movieID, const string& chosenDay, bool isStudent) {
    while (true) {
        system("cls");

        // Define available time slots
        vector<string> timeSlots = { "10:00 AM", "01:00 PM", "04:00 PM", "07:00 PM", "10:00 PM" };

        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     *********************************************************\n";
        cout << "                                                                     *                     SELECT A TIME SLOT                *\n";
        cout << "                                                                     *********************************************************\n\n";

        cout << "                                                                               +------------+-------------------+\n";
        cout << "                                                                               |   Option   |     Time Slot      |\n";
        cout << "                                                                               +------------+-------------------+\n";

        for (size_t i = 0; i < timeSlots.size(); ++i) {
            cout << "                                                                               | " << setw(10) << (i + 1) << " | " << setw(17) << timeSlots[i] << " |\n";
        }
        cout << "                                                                               +------------+-------------------+\n";

        cout << "\n                                                     Choose a time slot (or 0 to go back): ";
        string input;
        cin >> input;

        // Validate input as numeric
        if (input.empty() || input.find_first_not_of("0123456789") != string::npos) {
            cout << "\033[31m\nINVALID INPUT! Please enter a valid number.\033[0m\n";
            system("pause");
            continue;
        }

        int timeChoice = stoi(input);

        if (timeChoice == 0) {
            cout << "\n                                                 Returning to the day selection page...\n";
            displayDaysPage("<CHOSEN MOVIE>", userID, movieID, isStudent); // Pass valid chosenMovie argument for consistency
            return;
        }

        if (timeChoice >= 1 && timeChoice <= static_cast<int>(timeSlots.size())) {
            string chosenTimeSlot = timeSlots[timeChoice - 1];

            while (true) {
                system("cls");

                // Define screen types
                vector<string> screenTypes = { "Standard", "IMAX", "3D", "VIP" };

                cout << "\n\n\n\n\n\n\n\n\n";
                cout << "                                                                     *********************************************************\n";
                cout << "                                                                     *                     SELECT SCREEN TYPE                *\n";
                cout << "                                                                     *********************************************************\n\n";

                cout << "                                                                               +------------+-------------------+\n";
                cout << "                                                                               |   Option   |    Screen Type    |\n";
                cout << "                                                                               +------------+-------------------+\n";

                for (size_t i = 0; i < screenTypes.size(); ++i) {
                    cout << "                                                                               | " << setw(10) << (i + 1) << " | " << setw(17) << screenTypes[i] << " |\n";
                }
                cout << "                                                                               +------------+-------------------+\n";

                cout << "\n                                                     Choose the screen type (or 0 to go back): ";
                cin >> input;

                // Validate input as numeric
                if (input.empty() || input.find_first_not_of("0123456789") != string::npos) {
                    cout << "\033[31m\nINVALID INPUT! Please enter a valid number.\033[0m\n";
                    system("pause");
                    continue;
                }

                int screenChoice = stoi(input);

                if (screenChoice == 0) {
                    cout << "\n                                                 Returning to the time slot selection page...\n";
                    break;
                }

                if (screenChoice >= 1 && screenChoice <= static_cast<int>(screenTypes.size())) {
                    string screenType = screenTypes[screenChoice - 1];

                    // Proceed to the seat selection page with the student flag
                    displaySeatsPage(chosenDay, chosenTimeSlot, screenType, userID, movieID, isStudent);
                    return;
                }
                else {
                    cout << "\033[31m\nINVALID CHOICE!\n";
                    cout << "Please enter a number between 0 and " << screenTypes.size() << ".\033[0m\n"; // Reset color
                    system("pause");
                }
            }
        }
        else {
            cout << "\033[31m\nINVALID CHOICE!\n";
            cout << "Please enter a number between 0 and " << timeSlots.size() << ".\033[0m\n"; // Reset color
            system("pause");
        }
    }
}

string selectedSeatsToString(const vector<string>& seats) {
    string result;
    for (const auto& seat : seats) {
        if (!result.empty()) {
            result += ", ";
        }
        result += seat;
    }
    return result;
}


void displaySeatsPage(const string& chosenDay, const string& chosenTimeSlot, const string& screenType, int userID, int movieID, bool isStudent) {
    vector<string> selectedSeats;
    string seatChoice;
    set<string> bookedSeats;

    // Fetch movie details
    string movieTitle, genre, duration;
    double rating = 0.0;

    try {
        sql::PreparedStatement* pstmt = globalCon->prepareStatement(
            "SELECT Title, Genre, Duration, Rating FROM movies WHERE Movie_id = ?");
        pstmt->setInt(1, movieID);
        sql::ResultSet* res = pstmt->executeQuery();

        if (res->next()) {
            movieTitle = res->getString("Title");
            genre = res->getString("Genre");
            duration = res->getString("Duration");
            rating = res->getDouble("Rating");
        }
        else {
            cout << "\033[31mError: Movie details not found.\033[0m\n";
            delete res;
            delete pstmt;
            return;
        }

        delete res;
        delete pstmt;
    }
    catch (sql::SQLException& e) {
        cout << "SQL Error (fetch movie details): " << e.what() << endl;
        return;
    }

    // Fetch already booked seats
    try {
        sql::PreparedStatement* pstmt = globalCon->prepareStatement(
            "SELECT SeatsBooked FROM booking WHERE Movie_id = ? AND BookingDate = ? AND TimeSlot = ? AND ScreenType = ?");
        pstmt->setInt(1, movieID);
        pstmt->setString(2, chosenDay);
        pstmt->setString(3, chosenTimeSlot);
        pstmt->setString(4, screenType);

        sql::ResultSet* res = pstmt->executeQuery();
        while (res->next()) {
            string seats = res->getString("SeatsBooked");
            istringstream seatStream(seats);
            string seat;
            while (getline(seatStream, seat, ',')) {
                seat.erase(seat.begin(), find_if(seat.begin(), seat.end(), [](unsigned char ch) { return !isspace(ch); }));
                seat.erase(find_if(seat.rbegin(), seat.rend(), [](unsigned char ch) { return !isspace(ch); }).base(), seat.end());
                bookedSeats.insert(seat);
            }
        }

        delete res;
        delete pstmt;
    }
    catch (sql::SQLException& e) {
        cout << "SQL Error (fetch booked seats): " << e.what() << endl;
        return;
    }

    // Seat selection loop
    while (true) {
        system("cls");

        cout << "\n\n\n\n\n";
        cout << "                                                                     *********************************************************************\n";
        cout << "                                                                     *                          SEAT SELECTION                          *\n";
        cout << "                                                                     *********************************************************************\n\n";

        cout << "                                                                              Movie: " << movieTitle
            << " | Genre: " << genre << " | Rating: " << fixed << setprecision(1) << rating
            << " | Duration: " << duration << "\n"
            << "                                                                              Date: " << chosenDay
            << " | Time: " << chosenTimeSlot << " | Screen Type: " << screenType << "\n\n";

        // Medium-sized layout for seats
        int rows = 8, cols = 8;
        vector<vector<string>> seats(rows, vector<string>(cols));
        char rowLetter = 'A';

        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                seats[i][j] = rowLetter + to_string(j + 1);
            }
            rowLetter++;
        }

        // Display seats
        cout << "                                                                              Seats:\n";
        for (int i = 0; i < rows; ++i) {
            cout << "                                                                              ";
            for (int j = 0; j < cols; ++j) {
                if (bookedSeats.find(seats[i][j]) != bookedSeats.end()) {
                    cout << " \033[41;97m" << setw(4) << seats[i][j] << " \033[0m";
                }
                else if (find(selectedSeats.begin(), selectedSeats.end(), seats[i][j]) != selectedSeats.end()) {
                    cout << " \033[33;97m" << setw(4) << seats[i][j] << " \033[0m";
                }
                else {
                    cout << " \033[32m" << setw(4) << seats[i][j] << " \033[0m";
                }
            }
            cout << "\n";
        }

        // Instructions
        cout << "\nInstructions:\n";
        cout << "1. Enter seat names to select (e.g., A1).\n";
        cout << "2. Type 'done' to finalize your selection.\n";
        cout << "3. Type 'remove' to remove a selected seat.\n";
        cout << "4. Type '0' to return to the previous page.\n";
        cout << "                                                                     Your choice: ";
        cin >> seatChoice;

        if (seatChoice == "done") {
            if (selectedSeats.empty()) {
                cout << "\033[41;97mPlease select at least one seat.\033[0m\n";
                system("pause");
                continue;
            }
            break;
        }
        else if (seatChoice == "remove") {
            if (!selectedSeats.empty()) {
                cout << "Enter the seat to remove: ";
                string removeSeat;
                cin >> removeSeat;

                auto it = find(selectedSeats.begin(), selectedSeats.end(), removeSeat);
                if (it != selectedSeats.end()) {
                    selectedSeats.erase(it);
                    bookedSeats.erase(removeSeat); // Remove from bookedSeats to make it selectable again
                    cout << "\033[33mRemoved seat " << removeSeat << " from your selection.\033[0m\n";
                }
                else {
                    cout << "\033[31mSeat " << removeSeat << " is not in your selection.\033[0m\n";
                }
            }
            else {
                cout << "\033[31mNo seats to remove.\033[0m\n";
            }
            system("pause");
        }
        else if (seatChoice == "0") {
            cout << "\033[32m\nReturning to the screen and time selection page...\033[0m\n";
            system("pause");
            displayScreenAndTimePage(userID, movieID, chosenDay, isStudent);
            return; // Ensure that the seat selection loop exits properly
        }
        else {
            // Validate the seat choice
            bool validSeat = false;
            for (const auto& row : seats) {
                if (find(row.begin(), row.end(), seatChoice) != row.end()) {
                    validSeat = true;
                    break;
                }
            }

            if (!validSeat) {
                cout << "\033[31m\nInvalid seat name! Please enter a valid seat (e.g., A1).\033[0m\n";
                system("pause");
                continue;
            }

            if (bookedSeats.find(seatChoice) != bookedSeats.end()) {
                cout << "\033[31m\nSeat " << seatChoice << " is already booked. Please choose another seat.\033[0m\n";
                system("pause");
            }
            else {
                selectedSeats.push_back(seatChoice);
                bookedSeats.insert(seatChoice); // Temporarily mark seat as booked
            }
        }
    }

    // Save selected seats to the database
    try {
        string selectedSeatsString = selectedSeatsToString(selectedSeats);

        sql::PreparedStatement* stmt = globalCon->prepareStatement(
            "INSERT INTO booking (Movie_id, BookingDate, TimeSlot, ScreenType, SeatsBooked, User_id) "
            "VALUES (?, ?, ?, ?, ?, ?)");
        stmt->setInt(1, movieID);
        stmt->setString(2, chosenDay);
        stmt->setString(3, chosenTimeSlot);
        stmt->setString(4, screenType);
        stmt->setString(5, selectedSeatsString);
        stmt->setInt(6, userID);
        stmt->executeUpdate();
        delete stmt;
    }
    catch (sql::SQLException& e) {
        cout << "SQL Error (save selected seats): " << e.what() << endl;
        return;
    }

    cout << "\nSeats successfully booked! Proceeding...\n";
    system("pause");

    // Redirect to appropriate ticket selection page
    if (isStudent) {
        displayStudentTicketPage(userID, movieID, chosenDay, chosenTimeSlot, screenType, selectedSeats);
    }
    else {
        displayTicketTypeSelectionPage(userID, movieID, chosenDay, chosenTimeSlot, screenType, selectedSeats, isStudent);
    }
}



void displayStudentTicketPage(int userID, int movieID, const string& chosenDay,
    const string& chosenTimeSlot, const string& screenType,
    const vector<string>& selectedSeats) {
    double studentTicketPrice = 15.0; // price for student tickets
    double totalTicketPrice = studentTicketPrice * selectedSeats.size();

    while (true) {
        system("cls");

        cout << "\n\n\n\n\n\n";
        cout << "                                                                     *********************************************************************\n";
        cout << "                                                                     *                           STUDENT TICKET SELECTION                *\n";
        cout << "                                                                     *********************************************************************\n";
        cout << "\n";
        cout << "                                                                     You have selected the following seats:\n";
        cout << "                                                                     +---------------------+---------------------+\n";
        cout << "                                                                     |        Seat         |      Price (RM)     |\n";
        cout << "                                                                     +---------------------+---------------------+\n";
        for (const auto& seat : selectedSeats) {
            cout << "                                                                     | " << setw(20) << left << seat
                << "| " << setw(20) << fixed << setprecision(2) << studentTicketPrice << "|\n";
        }
        cout << "                                                                     +---------------------+---------------------+\n";
        cout << "                                                                     Total Price: RM " << fixed << setprecision(2) << totalTicketPrice << "\n";
        cout << "\n";
        cout << "                                                                     1. Proceed to Meal Selection\n";
        cout << "                                                                     2. Go Back to Seat Selection\n";
        cout << "                                                                     Enter your choice: ";

        string choice;
        cin >> choice;

        if (choice == "1") {
            // Save the ticket information to the database
            try {
                string selectedSeatsString = selectedSeatsToString(selectedSeats);
                sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                    "UPDATE booking SET TicketTypes = 'Student', TotalPrice = ?, SeatsBooked = ? WHERE User_id = ? AND Movie_id = ? "
                    "AND BookingDate = ? AND TimeSlot = ? AND ScreenType = ?");
                pstmt->setDouble(1, totalTicketPrice);
                pstmt->setString(2, selectedSeatsString);
                pstmt->setInt(3, userID);
                pstmt->setInt(4, movieID);
                pstmt->setString(5, chosenDay);
                pstmt->setString(6, chosenTimeSlot);
                pstmt->setString(7, screenType);
                pstmt->executeUpdate();
                delete pstmt;
            }
            catch (sql::SQLException& e) {
                cout << "\033[31mSQL Error (Save Student Ticket): " << e.what() << "\033[0m\n";
                system("pause");
                return;
            }

            cout << "\033[32m\n                                                                     Tickets successfully selected! Proceeding to meal selection...\033[0m\n";
            system("pause");

            // Redirect to the meal selection page
            displayMealPage(movieID, userID, chosenDay, chosenTimeSlot, screenType, selectedSeats, { "Student" }, true, true);
            return;
        }
        else if (choice == "2") {
            cout << "\n                                                                     Returning to Seat Selection...\n";
            system("pause");
            displaySeatsPage(chosenDay, chosenTimeSlot, screenType, userID, movieID, true);
            return;
        }
        else {
            cout << "\033[31m                                                                     Invalid choice. Please try again.\033[0m\n";
            system("pause");
        }
    }
}


void displayTicketTypeSelectionPage(int userID, int movieID, const string& chosenDay,
    const string& chosenTimeSlot, const string& screenType,
    const vector<string>& selectedSeats, bool isStudent) {

    vector<string> ticketTypes = { "Individual", "Kids", "Senior" };
    vector<double> ticketPrices = { 22.0, 15.0, 18.0 }; // Prices for each ticket type
    vector<string> selectedTicketTypes;

    while (true) {
        system("cls");

        cout << "\n\n\n\n\n";
        cout << "                                                                     *********************************************************************\n";
        cout << "                                                                     *                      TICKET SELECTION PAGE                      *\n";
        cout << "                                                                     *********************************************************************\n\n";

        // Display selected seats
        cout << "                                                                     Selected Seats:\n";
        for (const auto& seat : selectedSeats) {
            cout << "                                                                     - " << seat << "\n";
        }
        cout << "\n";

        if (isStudent) {
            cout << "\033[32m                                                                     You are eligible for a student ticket. Proceeding to the student ticket page.\033[0m\n";
            system("pause");
            displayStudentTicketPage(userID, movieID, chosenDay, chosenTimeSlot, screenType, selectedSeats);
            return;
        }
        else {
            // Display ticket price table
            cout << "                                                                     +-------------------+---------------------+\n";
            cout << "                                                                     | Ticket Type       | Price per Ticket    |\n";
            cout << "                                                                     +-------------------+---------------------+\n";
            for (size_t i = 0; i < ticketTypes.size(); ++i) {
                cout << "                                                                     | " << setw(17) << left << ticketTypes[i]
                    << "| RM " << setw(20) << fixed << setprecision(2) << ticketPrices[i] << "|\n";
            }
            cout << "                                                                     +-------------------+---------------------+\n\n";

            // Allow users to select ticket types
            for (const auto& seat : selectedSeats) {
                while (true) {
                    cout << "                                                                     For Seat " << seat << ", select a ticket type (1/2/3): ";
                    string choice;
                    cin >> choice;

                    if (choice == "1" || choice == "2" || choice == "3") {
                        selectedTicketTypes.push_back(ticketTypes[stoi(choice) - 1]);
                        break;
                    }
                    else {
                        cout << "\033[31m                                                                     Invalid choice. Please select 1, 2, or 3.\033[0m\n";
                    }
                }
            }
        }

        // Calculate total price
        double totalPrice = 0.0;
        for (const auto& ticketType : selectedTicketTypes) {
            size_t index = find(ticketTypes.begin(), ticketTypes.end(), ticketType) - ticketTypes.begin();
            totalPrice += ticketPrices[index];
        }

        cout << "\n                                                                     Total Price: RM " << fixed << setprecision(2) << totalPrice << "\n";
        cout << "                                                                     Do you want to proceed with the booking?\n";
        cout << "                                                                     1. Confirm Tickets\n";
        cout << "                                                                     2. Go Back to Seat Selection\n";
        cout << "                                                                     Enter your choice: ";
        string proceedChoice;
        cin >> proceedChoice;

        if (proceedChoice == "1") {
            try {
                sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                    "UPDATE booking SET TicketTypes = ?, TotalPrice = ? WHERE User_id = ? AND Movie_id = ? "
                    "AND BookingDate = ? AND TimeSlot = ? AND ScreenType = ?");
                string ticketTypesString = vectorToCommaSeparatedString(selectedTicketTypes);
                pstmt->setString(1, ticketTypesString);
                pstmt->setDouble(2, totalPrice);
                pstmt->setInt(3, userID);
                pstmt->setInt(4, movieID);
                pstmt->setString(5, chosenDay);
                pstmt->setString(6, chosenTimeSlot);
                pstmt->setString(7, screenType);
                pstmt->executeUpdate();
                delete pstmt;
            }
            catch (sql::SQLException& e) {
                cout << "\033[31mSQL Error (save ticket types): " << e.what() << "\033[0m\n";
                return;
            }

            cout << "\n                                                                     Tickets successfully selected! Proceeding to meal selection...\n";
            system("pause");

            displayMealPage(movieID, userID, chosenDay, chosenTimeSlot, screenType, selectedSeats, selectedTicketTypes, isStudent);
            return;
        }
        else if (proceedChoice == "2") {
            cout << "\n                                                                     Returning to the seat selection page...\n";
            system("pause");
            displaySeatsPage(chosenDay, chosenTimeSlot, screenType, userID, movieID, isStudent);
            return;
        }
        else {
            cout << "\033[31m                                                                     Invalid choice! Please select either 1 or 2.\033[0m\n";
            system("pause");
        }
    }
}



void displayMealPage(int movieID, int userID, const string& chosenDay, const string& chosenTimeSlot,
    const string& screenType, const vector<string>& selectedSeats,
    const vector<string>& selectedTicketTypes, bool isStudent, bool allowMealRemoval) {
    vector<pair<int, pair<string, double>>> availableMeals;
    vector<pair<string, double>> selectedMeals;
    double totalMealPrice = 0.0;

    // Fetch meals from the database
    try {
        sql::PreparedStatement* pstmt = globalCon->prepareStatement("SELECT MealID, MealName, MealPrice FROM meals");
        sql::ResultSet* res = pstmt->executeQuery();

        while (res->next()) {
            int mealID = res->getInt("MealID");
            string mealName = res->getString("MealName");
            double mealPrice = res->getDouble("MealPrice");
            availableMeals.emplace_back(mealID, make_pair(mealName, mealPrice));
        }

        if (availableMeals.empty()) {
            cout << "\033[31mNo meals available in the database.\033[0m\n";
            system("pause");
            delete res;
            delete pstmt;
            return;
        }

        delete res;
        delete pstmt;
    }
    catch (sql::SQLException& e) {
        cout << "\033[31mSQL Error (fetch meals):\033[0m " << e.what() << " (MySQL error code: " << e.getErrorCode()
            << ", SQLState: " << e.getSQLState() << ")" << endl;
        system("pause");
        return;
    }

    while (true) {
        system("cls");

        // Display available meals
        cout << "\n\n\n\n\n";
        cout << "                                                                     *********************************************************\n";
        cout << "                                                                     *                        MEAL SELECTION                 *\n";
        cout << "                                                                     *********************************************************\n\n";

        cout << "                                                                               Available Meals:\n";
        cout << "                                                                               +------------+------------------+-------------+\n";
        cout << "                                                                               |   Option   |       Meal       |   Price     |\n";
        cout << "                                                                               +------------+------------------+-------------+\n";
        for (size_t i = 0; i < availableMeals.size(); ++i) {
            cout << "                                                                               | " << setw(10) << (i + 1) << " | "
                << setw(16) << availableMeals[i].second.first << " | "
                << setw(11) << fixed << setprecision(2) << availableMeals[i].second.second << " |\n";
        }
        cout << "                                                                               +------------+------------------+-------------+\n";

        // Display selected meals
        if (!selectedMeals.empty()) {
            cout << "\n                                                                               Selected Meals:\n";
            cout << "                                                                               +------------------+-------------+\n";
            cout << "                                                                               |       Meal       |   Price     |\n";
            cout << "                                                                               +------------------+-------------+\n";
            for (const auto& meal : selectedMeals) {
                cout << "                                                                               | " << setw(16) << meal.first << " | "
                    << setw(11) << fixed << setprecision(2) << meal.second << " |\n";
            }
            cout << "                                                                               +------------------+-------------+\n";
            cout << "                                                                               Total Price: RM " << fixed << setprecision(2) << totalMealPrice << "\n";
        }

        // Display instructions
        cout << "\n                                                                               Instructions:\n";
        cout << "                                                                               1. Enter the meal number to select a meal.\n";
        if (allowMealRemoval) {
            cout << "                                                                               2. Type 'remove' to remove a selected meal by its number.\n";
        }
        cout << "                                                                               3. Type 'done' to proceed to payment.\n";
        cout << "                                                                               4. Type '0' to return to the previous page.\n";
        cout << "                                                                               Your choice: ";
        string choice;
        cin >> choice;

        if (choice == "done") {
            break; // Proceed to payment page
        }
        else if (allowMealRemoval && choice == "remove") {
            if (selectedMeals.empty()) {
                cout << "\033[31m                                                                               No meals to remove.\033[0m\n";
                system("pause");
                continue;
            }

            cout << "\n                                                                               Enter the number of the meal to remove: ";
            int removeIndex;
            cin >> removeIndex;

            if (removeIndex > 0 && removeIndex <= static_cast<int>(selectedMeals.size())) {
                totalMealPrice -= selectedMeals[removeIndex - 1].second;
                selectedMeals.erase(selectedMeals.begin() + (removeIndex - 1));
                cout << "\033[33m                                                                               Removed meal " << removeIndex << " from your selection.\033[0m\n";
            }
            else {
                cout << "\033[31m                                                                               Invalid meal number. Please try again.\033[0m\n";
            }

            system("pause");
        }
        else if (choice == "0") {
            cout << "\n                                                                               Returning to the ticket selection page...\n";
            system("pause");
            displayTicketTypeSelectionPage(userID, movieID, chosenDay, chosenTimeSlot, screenType, selectedSeats, isStudent);
            return;
        }
        else if (isdigit(choice[0])) {
            int mealIndex = stoi(choice) - 1;
            if (mealIndex >= 0 && mealIndex < static_cast<int>(availableMeals.size())) {
                selectedMeals.emplace_back(availableMeals[mealIndex].second.first, availableMeals[mealIndex].second.second);
                totalMealPrice += availableMeals[mealIndex].second.second;
                cout << "\033[32m                                                                               Added " << availableMeals[mealIndex].second.first << " to your selection.\033[0m\n";
            }
            else {
                cout << "\033[31m                                                                               Invalid meal option. Please try again.\033[0m\n";
            }
            system("pause");
        }
        else {
            cout << "\033[31m                                                                               Invalid input. Please try again.\033[0m\n";
            system("pause");
        }
    }

    // Save the booking and meal information
    string seatsBooked = vectorToCommaSeparatedString(selectedSeats);
    string ticketType = vectorToCommaSeparatedString(selectedTicketTypes);

    int bookingID = insertBooking(userID, movieID, screenType, chosenDay, chosenTimeSlot, seatsBooked, ticketType, totalMealPrice);

    if (bookingID <= 0) {
        cerr << "Error: Unable to create booking. Please try again.\n";
        system("pause");
        return; // Exit to prevent further issues
    }

    // Save selected meals to the database
    for (const auto& meal : selectedMeals) {
        try {
            sql::PreparedStatement* mealStmt = globalCon->prepareStatement(
                "INSERT INTO booking_meals (BookingID, MealName, MealPrice) VALUES (?, ?, ?)");
            mealStmt->setInt(1, bookingID);
            mealStmt->setString(2, meal.first);
            mealStmt->setDouble(3, meal.second);
            mealStmt->executeUpdate();
            delete mealStmt;
        }
        catch (sql::SQLException& e) {
            cout << "SQL Error (save meals): " << e.what() << endl;
        }
    }

    // Proceed to the payment page
    displayPaymentPage(userID, bookingID, movieID, chosenDay, chosenTimeSlot, screenType, selectedSeats, selectedTicketTypes, totalMealPrice, isStudent, selectedMeals);
}


std::string vectorToCommaSeparatedString(const std::vector<std::string>& inputVector) {
    std::ostringstream oss; // String stream to build the result
    for (size_t i = 0; i < inputVector.size(); ++i) {
        oss << inputVector[i];
        if (i < inputVector.size() - 1) {
            oss << ","; // Add comma only if not the last element
        }
    }
    return oss.str(); // Convert the stream to a string and return it
}

void displayPaymentPage(int userID, int bookingID, int movieID, const string& chosenDay,
    const string& chosenTimeSlot, const string& screenType, const vector<string>& selectedSeats,
    const vector<string>& selectedTicketTypes, double totalMealPrice, bool isStudent,
    const vector<pair<string, double>>& selectedMeals) {

    double grandTotal = totalMealPrice;
    map<string, pair<int, double>> ticketSummary;

    // Define ticket prices
    map<string, double> ticketPrices = {
        {"Individual", 22.0},
        {"Kids", 15.0},
        {"Senior", 18.0},
        {"Student", 15.0}
    };

    // Calculate ticket totals
    for (const auto& ticketType : selectedTicketTypes) {
        double ticketPrice = ticketPrices[ticketType];
        grandTotal += ticketPrice;

        if (ticketSummary.find(ticketType) == ticketSummary.end()) {
            ticketSummary[ticketType] = { 1, ticketPrice };
        }
        else {
            ticketSummary[ticketType].first += 1;
            ticketSummary[ticketType].second += ticketPrice;
        }
    }

    // Apply student discount if applicable
    double discountedTotal = isStudent ? grandTotal * 0.6 : grandTotal;
    double paidAmount = 0.0;

    string userName = "Unknown User", movieTitle = "Unknown Movie";

    // Fetch user name
    try {
        sql::PreparedStatement* pstmt = globalCon->prepareStatement("SELECT CONCAT(FirstName, ' ', LastName) AS FullName FROM users WHERE User_id = ?");
        pstmt->setInt(1, userID);
        sql::ResultSet* res = pstmt->executeQuery();
        if (res->next()) userName = res->getString("FullName");
        delete res;
        delete pstmt;
    }
    catch (sql::SQLException& e) {
        cerr << "SQL Error (Fetch User Name): " << e.what() << endl;
    }

    // Fetch movie title
    try {
        sql::PreparedStatement* pstmt = globalCon->prepareStatement("SELECT Title FROM movies WHERE Movie_id = ?");
        pstmt->setInt(1, movieID);
        sql::ResultSet* res = pstmt->executeQuery();
        if (res->next()) movieTitle = res->getString("Title");
        delete res;
        delete pstmt;
    }
    catch (sql::SQLException& e) {
        cerr << "SQL Error (Fetch Movie Title): " << e.what() << endl;
    }

    while (true) {
        system("cls");

        cout << "\n\n\n";
        cout << "                                       *********************************************************\n";
        cout << "                                       *                      PAYMENT SUMMARY                  *\n";
        cout << "                                       *********************************************************\n";
        cout << "                                       +-------------------+-------------------+-------------------+-------------------+\n";
        cout << "                                       | Ticket Type       | Number of Tickets | Price per Ticket  | Total Price (RM)  |\n";
        cout << "                                       +-------------------+-------------------+-------------------+-------------------+\n";

        for (const auto& ticket : ticketSummary) {
            cout << "                                       | " << setw(17) << left << ticket.first
                << " | " << setw(17) << ticket.second.first
                << " | " << setw(17) << fixed << setprecision(2) << (ticket.second.second / ticket.second.first)
                << " | " << setw(17) << ticket.second.second << " |\n";
        }

        cout << "                                       +-------------------+-------------------+-------------------+-------------------+\n";

        // Display meal details
        cout << "\n                                       *********************************************************\n";
        cout << "                                       *                          MEALS                        *\n";
        cout << "                                       *********************************************************\n";
        cout << "                                       +--------------------------+-------------------+\n";
        cout << "                                       | Meal Name                | Price (RM)        |\n";
        cout << "                                       +--------------------------+-------------------+\n";

        if (!selectedMeals.empty()) {
            for (const auto& meal : selectedMeals) {
                cout << "                                       | " << setw(24) << left << meal.first
                    << " | " << setw(17) << right << fixed << setprecision(2) << meal.second << " |\n";
            }
        }
        else {
            cout << "                                       | No meals selected        |                   |\n";
        }
        cout << "                                       +--------------------------+-------------------+\n";

        if (isStudent) {
            cout << "\033[32m                                       Student Discount Applied (40% Off): RM "
                << fixed << setprecision(2) << grandTotal - discountedTotal << "\033[0m\n";
        }
        cout << "                                       Final Amount to Pay: RM " << fixed << setprecision(2) << discountedTotal << "\n\n";

        // Payment options
        cout << "                                       Choose Payment Method:\n";
        cout << "                                       1. Visa\n";
        cout << "                                       2. MasterCard\n";
        cout << "                                       3. Cash\n";
        cout << "                                       4. Cancel Payment\n";
        cout << "                                       Enter your choice: ";
        string paymentMethod;
        cin >> paymentMethod;

        if (paymentMethod == "1" || paymentMethod == "2") {
            displayCardInformationPage(userID, bookingID, discountedTotal, selectedTicketTypes, selectedMeals, totalMealPrice, isStudent, chosenDay, movieTitle);
            return;
        }
        else if (paymentMethod == "3") {
            double remainingBalance = discountedTotal;

            cout << "\n                                       Total to pay: RM " << fixed << setprecision(2) << discountedTotal << endl;

            while (remainingBalance > 0) {
                cout << "                                       Enter payment amount: RM ";
                double payment;
                cin >> payment;

                if (payment <= 0) {
                    cout << "\033[31m                                       Invalid amount. Please enter a valid amount.\033[0m\n";
                    continue;
                }

                paidAmount += payment;
                remainingBalance = discountedTotal - paidAmount;

                if (remainingBalance > 0) {
                    cout << "                                       You still need to pay RM " << fixed << setprecision(2) << remainingBalance << ": ";
                }
                else {
                    double refundAmount = -remainingBalance;
                    cout << "\033[32m                                       Payment successful!\033[0m\n";

                    if (refundAmount > 0) {
                        cout << "\033[32m                                       Refunding RM " << fixed << setprecision(2) << refundAmount << "\033[0m\n";
                        system("pause");
                    }

                    int paymentID = insertPayment(userID, bookingID, "Cash", discountedTotal);
                    if (paymentID != -1) {
                        displayReceipt(userName, movieTitle, selectedTicketTypes, selectedMeals, grandTotal, discountedTotal, chosenDay);
                    }
                    else {
                        cerr << "\n                                       Payment failed. Please try again.\n";
                    }
                    return;
                }
            }
        }
        else if (paymentMethod == "4") {
            if (paidAmount > 0) {
                cout << "\033[32m                                       Payment canceled! Refunding RM " << fixed << setprecision(2) << paidAmount << "\033[0m\n";
                system("pause");
            }
            else {
                cout << "\033[32m                                       Your payment have canceled .\033[0m\n";
                system("pause");
                cancelBooking(bookingID);
            }
            return;
        }
        else {
            cout << "\033[31m                                       Invalid choice. Please try again.\033[0m\n";
            system("pause");
        }
    }
}



void displayCardInformationPage(int userID, int bookingID, double grandTotal,
    const vector<string>& selectedTicketTypes,
    const vector<pair<string, double>>& selectedMeals,
    double totalMealPrice, bool isStudent,
    const string& bookingDate, const string& movieTitle) {

    vector<pair<string, string>> cardDetails = {
        {"Card Number (16 digits):", ""}, // Index 0
        {"Expiry Date (MM/YY):", ""},     // Index 1
        {"CVV (3 digits):", ""}          // Index 2
    };
    vector<bool> validationStatus = { false, false, false };

    while (true) {
        system("cls");
        cout << "\n\n\n\n\n";
        cout << "                                                                     *********************************************************\n";
        cout << "                                                                     *                      CARD INFORMATION                *\n";
        cout << "                                                                     *********************************************************\n\n";

        // Loop through and validate each field
        for (size_t i = 0; i < cardDetails.size(); ++i) {
            while (!validationStatus[i]) {
                cout << "                                                                     " << cardDetails[i].first << " ";
                cin >> cardDetails[i].second;

                // Validation Logic
                if (i == 0) { // Validate Card Number
                    if (cardDetails[i].second.length() != 16 || !all_of(cardDetails[i].second.begin(), cardDetails[i].second.end(), ::isdigit)) {
                        cout << "\033[31m                                                                     Invalid card number. Please enter exactly 16 digits.\033[0m\n";
                    }
                    else {
                        validationStatus[i] = true;
                    }
                }
                else if (i == 1) { // Validate Expiry Date
                    if (cardDetails[i].second.size() != 5 || cardDetails[i].second[2] != '/' ||
                        !isdigit(cardDetails[i].second[0]) || !isdigit(cardDetails[i].second[1]) ||
                        !isdigit(cardDetails[i].second[3]) || !isdigit(cardDetails[i].second[4])) {
                        cout << "\033[31m                                                                     Invalid expiry date. Please use MM/YY format.\033[0m\n";
                    }
                    else {
                        int month = stoi(cardDetails[i].second.substr(0, 2));
                        int year = stoi("20" + cardDetails[i].second.substr(3, 2));
                        if (month < 1 || month > 12) {
                            cout << "\033[31m                                                                     Invalid expiry date. Month must be between 01 and 12.\033[0m\n";
                        }
                        else {
                            time_t now = time(0);
                            tm localTime;
                            localtime_s(&localTime, &now);
                            int currentYear = localTime.tm_year + 1900;
                            int currentMonth = localTime.tm_mon + 1;

                            if (year < currentYear || (year == currentYear && month < currentMonth)) {
                                cout << "\033[31m                                                                     Card has expired. Please use a valid card.\033[0m\n";
                            }
                            else {
                                validationStatus[i] = true;
                            }
                        }
                    }
                }
                else if (i == 2) { // Validate CVV
                    if (cardDetails[i].second.length() != 3 || !all_of(cardDetails[i].second.begin(), cardDetails[i].second.end(), ::isdigit)) {
                        cout << "\033[31m                                                                     Invalid CVV. Please enter a valid 3-digit CVV.\033[0m\n";
                    }
                    else {
                        validationStatus[i] = true;
                    }
                }
            }
        }

        // Confirm payment or allow the user to go back
        cout << "\n                                                                     1. Confirm Payment\n";
        cout << "                                                                     2. Go Back to Payment Page\n";
        cout << "                                                                     Enter your choice: ";
        string choice;
        cin >> choice;

        if (choice == "1") {
            try {
                sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                    "INSERT INTO payment (User_id, Booking_id, PaymentMethod, PaymentAmount, PaymentConfirmed) VALUES (?, ?, ?, ?, ?)");
                pstmt->setInt(1, userID);
                pstmt->setInt(2, bookingID);
                pstmt->setString(3, "Visa/MasterCard");
                pstmt->setDouble(4, grandTotal);
                pstmt->setInt(5, 1); // Payment confirmed
                pstmt->executeUpdate();
                delete pstmt;

                // Payment success message
                cout << "\n                                                                     \033[32mPayment successful! Redirecting to receipt...\033[0m\n";
                system("pause");

                // Display receipt
                double discountedAmount = isStudent ? grandTotal * 0.6 : grandTotal;
                displayReceipt(getUserName(userID), movieTitle, selectedTicketTypes, selectedMeals, grandTotal, discountedAmount, bookingDate);
                return; // Exit after displaying the receipt
            }
            catch (sql::SQLException& e) {
                cerr << "\033[31mSQL Error (Payment): " << e.what() << "\033[0m\n";
                system("pause");
                return;
            }
        }
        else if (choice == "2") {
            // Go back to payment page
            return;
        }
        else {
            cout << "\033[31m                                                                     Invalid choice. Please select 1 or 2.\033[0m\n";
            system("pause");
        }
    }
}


void displayReceipt(const string& userName, const string& movieTitle,
    const vector<string>& ticketTypes, const vector<pair<string, double>>& selectedMeals,
    double totalAmount, double discountedAmount, const string& bookingTime) {
    system("cls");

    // Get the current date and time for the receipt
    time_t now = time(0);
    tm localTime;
    localtime_s(&localTime, &now);

    char receiptDate[20];
    char receiptTime[10];
    strftime(receiptDate, sizeof(receiptDate), "%Y-%m-%d", &localTime);
    strftime(receiptTime, sizeof(receiptTime), "%H:%M:%S", &localTime);

    cout << "\n\n\n\n";
    cout << "                                                                     *********************************************************************\n";
    cout << "                                                                     *                               RECEIPT                              *\n";
    cout << "                                                                     *********************************************************************\n";
    cout << "                                                                     | User Name: " << setw(46) << left << userName << "|\n";
    cout << "                                                                     | Movie:     " << setw(46) << left << movieTitle << "|\n";
    cout << "                                                                     | Showtime:  " << setw(46) << left << bookingTime << "|\n";
    cout << "                                                                     | Receipt Date: " << setw(43) << left << receiptDate << "|\n";
    cout << "                                                                     | Receipt Time: " << setw(43) << left << receiptTime << "|\n";
    cout << "                                                                     *********************************************************************\n";

    // Ticket details
    cout << "                                                                     *                            TICKET DETAILS                          *\n";
    cout << "                                                                     *********************************************************************\n";
    cout << "                                                                     +-------------------+-------------------+\n";
    cout << "                                                                     | Ticket Type       | Quantity          |\n";
    cout << "                                                                     +-------------------+-------------------+\n";

    map<string, int> ticketCount;
    for (const string& ticket : ticketTypes) {
        ticketCount[ticket]++;
    }
    for (const auto& ticket : ticketCount) {
        cout << "                                                                     | " << setw(17) << ticket.first
            << " | " << setw(17) << ticket.second << " |\n";
    }
    cout << "                                                                     +-------------------+-------------------+\n";

    // Meal details
    cout << "                                                                     *********************************************************************\n";
    cout << "                                                                     *                            MEAL DETAILS                           *\n";
    cout << "                                                                     *********************************************************************\n";
    cout << "                                                                     +--------------------------+-------------------+\n";
    cout << "                                                                     | Meal Name                | Meal Price (RM)   |\n";
    cout << "                                                                     +--------------------------+-------------------+\n";

    if (!selectedMeals.empty()) {
        for (const auto& meal : selectedMeals) {
            cout << "                                                                     | " << setw(24) << left << meal.first
                << " | " << setw(17) << right << fixed << setprecision(2) << meal.second << " |\n";
        }
    }
    else {
        cout << "                                                                     | No Meals Selected        |                   |\n";
    }
    cout << "                                                                     +--------------------------+-------------------+\n";

    // Price summary
    cout << "                                                                     *********************************************************************\n";
    cout << "                                                                     *                            PRICE SUMMARY                          *\n";
    cout << "                                                                     *********************************************************************\n";

    if (totalAmount != discountedAmount) {
        cout << "                                                                     | Total Price Before Discount: RM " << setw(34) << left << fixed << setprecision(2) << totalAmount << " |\n";
        cout << "                                                                     | Discounted Price:           RM " << setw(34) << left << fixed << setprecision(2) << discountedAmount << " |\n";
    }
    else {
        cout << "                                                                     | Final Price:               RM " << setw(34) << left << fixed << setprecision(2) << totalAmount << " |\n";
    }
    cout << "                                                                     *********************************************************************\n";

    // Thank you note
    cout << "                                                                     *                  \033[32mTHANK YOU, " << userName << "! ENJOY YOUR MOVIE!\033[0m                  *\n";
    cout << "                                                                     *********************************************************************\n";

    system("pause");
}


bool loginUser(const string& emailOrUsername, const string& password, bool& isStudent, int& userID) {
    try {
        // Fetch user details based on email or username
        sql::PreparedStatement* pstmt = globalCon->prepareStatement(
            "SELECT User_id, IsStudent, Password FROM users WHERE Email = ? OR UserName = ?");
        pstmt->setString(1, emailOrUsername);
        pstmt->setString(2, emailOrUsername);

        sql::ResultSet* res = pstmt->executeQuery();

        if (res->next()) {
            // Fetch stored hashed password
            string storedHashedPassword = res->getString("Password");

            // Verify the password (replace this with actual hashing library comparison)
            if (storedHashedPassword == password) {
                userID = res->getInt("User_id");
                isStudent = res->getBoolean("IsStudent");
                delete res;
                delete pstmt;
                return true;
            }
            else {
                cout << "\033[31mIncorrect password. Please try again.\033[0m\n";
            }
        }
        else {
            cout << "\033[31mUser not found with the provided email or username.\033[0m\n";
        }

        delete res;
        delete pstmt;
        return false;
    }
    catch (sql::SQLException& e) {
        cerr << "\033[31mSQL Error (loginUser): " << e.what() << "\033[0m\n";
        return false;
    }
}



string getUserName(int userID) {
    try {
        sql::PreparedStatement* pstmt = globalCon->prepareStatement(
            "SELECT CONCAT(FirstName, ' ', LastName) AS FullName FROM users WHERE User_id = ?");
        pstmt->setInt(1, userID);
        sql::ResultSet* res = pstmt->executeQuery();

        string userName = "Unknown User";
        if (res->next()) {
            userName = res->getString("FullName");
        }

        delete res;
        delete pstmt;
        return userName;
    }
    catch (sql::SQLException& e) {
        cerr << "SQL Error (getUserName): " << e.what() << endl;
        return "Error Fetching Name";
    }
}

void cancelBooking(int bookingID) {
    try {
        sql::PreparedStatement* pstmt = globalCon->prepareStatement(
            "DELETE FROM booking WHERE BookingID = ?");
        pstmt->setInt(1, bookingID);
        pstmt->executeUpdate();
        delete pstmt;

        cout << "\033[32mBooking successfully canceled.\033[0m\n";
    }
    catch (sql::SQLException& e) {
        cerr << "\033[31mSQL Error (cancelBooking): " << e.what() << "\033[0m\n";
    }
}

bool checkUserIDExists(int userID) {
    try {
        sql::PreparedStatement* pstmt = globalCon->prepareStatement(
            "SELECT 1 FROM users WHERE User_id = ? LIMIT 1");
        pstmt->setInt(1, userID);

        sql::ResultSet* res = pstmt->executeQuery();
        bool exists = res->next(); // If any result, user exists.

        delete res;
        delete pstmt;

        return exists;
    }
    catch (sql::SQLException& e) {
        cerr << "\033[31mSQL Error (checkUserIDExists): " << e.what() << "\033[0m\n";
        return false; // Return false for error states.
    }
}

string getMovieTitle(int movieID) {
    try {
        sql::PreparedStatement* pstmt = globalCon->prepareStatement(
            "SELECT Title FROM movies WHERE Movie_id = ?");
        pstmt->setInt(1, movieID);

        sql::ResultSet* res = pstmt->executeQuery();
        string title = "Unknown Movie"; // Default if no movie found.

        if (res->next()) {
            title = res->getString("Title");
        }
        else {
            cerr << "\033[33mWarning: Movie ID " << movieID << " not found in the database.\033[0m\n";
        }

        delete res;
        delete pstmt;

        return title;
    }
    catch (sql::SQLException& e) {
        cerr << "\033[31mSQL Error (getMovieTitle): " << e.what() << "\033[0m\n";
        return "Error Fetching Title"; // Error indicator.
    }
}



//admin page
void adminPage() {
    int choice;

    while (true) {
        system("cls");
        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                           ADMIN PAGE                         *\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "\n";
        cout << "                                                                     1. Manage Users\n";
        cout << "                                                                     2. Manage Payments\n";
        cout << "                                                                     3. Manage Meals\n";
        cout << "                                                                     4. Manage Movies\n";
        cout << "                                                                     5. Manage Bookings\n";
        cout << "                                                                     6. View Reports\n";
        cout << "                                                                     7. Exit Admin Page\n";

        cout << "\n                                                                     Enter your choice: ";

        // Validate input
        string input;
        cin >> input;

        // Input validation
        if (input.length() != 1 || !isdigit(input[0])) {
            cout << "\n\033[31mInvalid input. Please enter a number between 1 and 7.\033[0m\n";
            system("pause");
            continue;
        }

        choice = stoi(input);

        // Switch case for handling menu options
        switch (choice) {
        case 1:
            cout << "\033[34mLoading Manage Users...\033[0m\n";
            system("pause");
            manageUsers();
            break;
        case 2:
            cout << "\033[34mLoading Manage Payments...\033[0m\n";
            system("pause");
            managePayments();
            break;
        case 3:
            cout << "\033[34mLoading Manage Meals...\033[0m\n";
            system("pause");
            manageMeals();
            break;
        case 4:
            cout << "\033[34mLoading Manage Movies...\033[0m\n";
            system("pause");
            manageMovies();
            break;
        case 5:
            cout << "\033[34mLoading Manage Bookings...\033[0m\n";
            system("pause");
            manageBookings();
            break;
        case 6:
            cout << "\033[34mLoading Reports Page...\033[0m\n";
            system("pause");
            displayReportPage();
            break;
        case 7:
            cout << "\n\033[32mExiting Admin Page. Thank you!\033[0m\n";
            system("pause");
            return;
        default:
            cout << "\n\033[31mInvalid choice. Please select a valid option.\033[0m\n";
            system("pause");
            break;
        }
    }
}

//user manae
void manageUsers() {
    int choice;

    while (true) {
        system("cls");
        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                          MANAGE USERS                        *\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "\n";
        cout << "                                                                     1. View All Users\n";
        cout << "                                                                     2. Add New User\n";
        cout << "                                                                     3. Update User Information\n";
        cout << "                                                                     4. Delete User\n";
        cout << "                                                                     5. Return to Admin Page\n";
        cout << "\n                                                                     Enter your choice: ";

        string input;
        cin >> input;

        if (input.length() != 1 || !isdigit(input[0])) {
            cout << "\nInvalid input. Please enter a number between 1 and 5.\n";
            system("pause");
            continue;
        }

        choice = stoi(input);

        switch (choice) {
        case 1:
            viewAllUsers();
            break;
        case 2:
            addNewUser();
            break;
        case 3:
            updateUser();
            break;
        case 4:
            deleteUser();
            break;
        case 5:
            return;
        default:
            cout << "\nInvalid choice. Please select a valid option.\n";
            system("pause");
            break;
        }
    }
}

void viewAllUsers() {
    while (true) {
        system("cls");

        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                        VIEW ALL USERS                        *\n";
        cout << "                                                                     ***************************************************************\n";

        cout << "\n                                                                     Options:\n";
        cout << "                                                                     1. View All Users\n";
        cout << "                                                                     2. Search User\n";
        cout << "                                                                     3. Return to Manage Users page\n";
        cout << "                                                                     Enter your choice: ";

        string input;
        cin >> input;

        if (input.find_first_not_of("0123456789") != string::npos || input.empty()) {
            cout << "\033[31m\n                                                                     Invalid input. Please enter a number (1, 2, or 3).\033[0m\n";
            system("pause");
            continue;
        }

        int choice = stoi(input);

        if (choice == 3) {
            return; // Return to Manage Users page
        }

        if (choice == 2) {
            string searchName;
            cout << "\n                                                                     Enter Name to search: ";
            cin.ignore();
            getline(cin, searchName);

            try {
                sql::PreparedStatement* searchPstmt = globalCon->prepareStatement(
                    "SELECT User_id, FirstName, LastName, Email, UserName, PhoneNumber, Admin, MatricNumber, IsStudent "
                    "FROM users WHERE FirstName LIKE ? OR LastName LIKE ?");
                searchPstmt->setString(1, "%" + searchName + "%");
                searchPstmt->setString(2, "%" + searchName + "%");
                sql::ResultSet* searchRes = searchPstmt->executeQuery();

                cout << "\n                                                                     +-------+----------------+----------------+-------------------+-------------------+-----------------+-------+---------------+------------+\n";
                cout << "                                                                     | UserID| First Name     | Last Name      | Email             | Username          | Phone Number    | Admin | MatricNumber  | IsStudent  |\n";
                cout << "                                                                     +-------+----------------+----------------+-------------------+-------------------+-----------------+-------+---------------+------------+\n";

                bool userFound = false;
                while (searchRes->next()) {
                    userFound = true;
                    cout << "                                                                     | " << setw(6) << searchRes->getInt("User_id")
                        << " | " << setw(14) << searchRes->getString("FirstName")
                        << " | " << setw(14) << searchRes->getString("LastName")
                        << " | " << setw(17) << searchRes->getString("Email")
                        << " | " << setw(17) << searchRes->getString("UserName")
                        << " | " << setw(14) << searchRes->getString("PhoneNumber")
                        << " | " << setw(5) << (searchRes->getInt("Admin") == 1 ? "Yes" : "No")
                        << " | " << setw(13) << (searchRes->isNull("MatricNumber") ? "N/A" : searchRes->getString("MatricNumber"))
                        << " | " << setw(10) << (searchRes->getInt("IsStudent") == 1 ? "Yes" : "No") << " |\n";
                }

                cout << "                                                                     +-------+----------------+----------------+-------------------+-------------------+-----------------+-------+---------------+------------+\n";

                if (!userFound) {
                    cout << "\033[31m\n                                                                     No users found with the specified name.\033[0m\n";
                }

                delete searchRes;
                delete searchPstmt;

                system("pause");
                continue; // Restart the loop
            }
            catch (sql::SQLException& e) {
                cerr << "\033[31mSQL Error (Search User):\033[0m " << e.what() << endl;
                system("pause");
                continue;
            }
        }

        if (choice == 1) {
            int minID = 0; // Start from the smallest User_id
            int pageSize = 10; // Number of users to display per page

            try {
                while (true) {
                    system("cls");

                    cout << "\n                                                                   ***************************************************************\n";
                    cout << "                                                                     *                        VIEW ALL USERS                        *\n";
                    cout << "                                                                     ***************************************************************\n";

                    sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                        "SELECT * FROM users WHERE User_id >= ? ORDER BY User_id ASC LIMIT ?");
                    pstmt->setInt(1, minID);
                    pstmt->setInt(2, pageSize);
                    sql::ResultSet* res = pstmt->executeQuery();

                    cout << "\n                                                                     +-------+----------------+----------------+-------------------+-------------------+-----------------+-------+---------------+------------+\n";
                    cout << "                                                                     | UserID| First Name     | Last Name      | Email             | Username          | Phone Number    | Admin | MatricNumber  | IsStudent  |\n";
                    cout << "                                                                     +-------+----------------+----------------+-------------------+-------------------+-----------------+-------+---------------+------------+\n";

                    bool hasMore = false;
                    int lastID = minID;
                    while (res->next()) {
                        lastID = res->getInt("User_id");
                        cout << "                                                                     | " << setw(6) << res->getInt("User_id")
                            << " | " << setw(14) << res->getString("FirstName")
                            << " | " << setw(14) << res->getString("LastName")
                            << " | " << setw(17) << res->getString("Email")
                            << " | " << setw(17) << res->getString("UserName")
                            << " | " << setw(14) << res->getString("PhoneNumber")
                            << " | " << setw(5) << (res->getInt("Admin") == 1 ? "Yes" : "No")
                            << " | " << setw(13) << (res->isNull("MatricNumber") ? "N/A" : res->getString("MatricNumber"))
                            << " | " << setw(10) << (res->getInt("IsStudent") == 1 ? "Yes" : "No") << " |\n";

                        hasMore = true;
                    }

                    cout << "                                                                     +-------+----------------+----------------+-------------------+-------------------+-----------------+-------+---------------+------------+\n";

                    delete res;
                    delete pstmt;

                    // **Navigation options**
                    cout << "\n                                                                     Navigation:\n";
                    if (minID > 0) {
                        cout << "                                                                     1. Previous Page\n";
                    }
                    if (hasMore) {
                        cout << "                                                                     2. Next Page\n";
                    }
                    cout << "                                                                     3. Return to Manage Users page\n";
                    cout << "                                                                     Enter your choice: ";

                    string navInput;
                    cin >> navInput;

                    if (navInput.find_first_not_of("0123456789") != string::npos || navInput.empty()) {
                        cout << "\033[31m\n                                                                     Invalid input. Please enter a valid number.\033[0m\n";
                        system("pause");
                        continue;
                    }

                    int navChoice = stoi(navInput);

                    if (navChoice == 3) {
                        break; // Exit the loop and return to Manage Users page
                    }
                    if (navChoice == 1 && minID > 0) {
                        minID -= pageSize;
                        if (minID < 0) minID = 0; // Prevent negative ID
                    }
                    else if (navChoice == 2 && hasMore) {
                        minID = lastID; // Move to the next page
                    }
                    else {
                        cout << "\033[31m\n                                                                     Invalid choice. Please try again.\033[0m\n";
                        system("pause");
                    }
                }
            }
            catch (sql::SQLException& e) {
                cerr << "\033[31mSQL Error (View All Users):\033[0m " << e.what() << endl;
                system("pause");
                continue;
            }
        }
    }
}

void updateUser() {
    int minID = 0;
    int pageSize = 10;

    while (true) {
        system("cls");

        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                     UPDATE USER INFORMATION                  *\n";
        cout << "                                                                     ***************************************************************\n";

        try {
            sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                "SELECT User_id, FirstName, LastName, Email, UserName, PhoneNumber, Admin, IsStudent, MatricNumber "
                "FROM users WHERE User_id >= ? ORDER BY User_id ASC LIMIT ?");
            pstmt->setInt(1, minID);
            pstmt->setInt(2, pageSize);
            sql::ResultSet* res = pstmt->executeQuery();

            cout << "\n                                                                   +-------+----------------+----------------+-------------------+-------------------+-----------------+-------+-----------+-------------------+\n";
            cout << "                                                                     | UserID | First Name     | Last Name      | Email             | Username          | Phone Number    | Admin | IsStudent | Matric Number     |\n";
            cout << "                                                                     +-------+----------------+----------------+-------------------+-------------------+-----------------+-------+-----------+-------------------+\n";

            bool hasMore = false;
            int lastID = minID;
            while (res->next()) {
                lastID = res->getInt("User_id");
                cout << "                                                                     | " << setw(6) << res->getInt("User_id")
                    << " | " << setw(14) << res->getString("FirstName")
                    << " | " << setw(14) << res->getString("LastName")
                    << " | " << setw(17) << res->getString("Email")
                    << " | " << setw(17) << res->getString("UserName")
                    << " | " << setw(14) << res->getString("PhoneNumber")
                    << " | " << setw(5) << (res->getInt("Admin") == 1 ? "Yes" : "No")
                    << " | " << setw(9) << (res->getInt("IsStudent") == 1 ? "Yes" : "No")
                    << " | " << setw(17) << (res->isNull("MatricNumber") ? "N/A" : res->getString("MatricNumber")) << " |\n";

                hasMore = true;
            }

            cout << "                                                                     +-------+----------------+----------------+-------------------+-------------------+-----------------+-------+-----------+-------------------+\n";

            delete res;
            delete pstmt;

            cout << "\n                                                                     Options:\n";
            if (minID > 0) {
                cout << "                                                                     1. Previous Page\n";
            }
            if (hasMore) {
                cout << "                                                                     2. Next Page\n";
            }
            cout << "                                                                     3. Enter User ID to Update\n";
            cout << "                                                                     4. Return to User Management\n";
            cout << "                                                                     Enter your choice: ";

            string input;
            cin >> input;

            if (input.find_first_not_of("0123456789") != string::npos || input.empty()) {
                cout << "\033[31m\n                                                                     Invalid input. Please enter a valid number.\033[0m\n";
                system("pause");
                continue;
            }

            int choice = stoi(input);

            if (choice == 4) {
                return; // Go back to user management page
            }
            if (choice == 1 && minID > 0) {
                minID -= pageSize;
                if (minID < 0) minID = 0; // Prevent negative page index
            }
            else if (choice == 2 && hasMore) {
                minID = lastID; // Move to next page
            }
            else if (choice == 3) {
                int userID;
                cout << "\n                                                                     Enter the User ID to update: ";
                cin >> userID;

                if (cin.fail()) {
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    cout << "\033[31m\n                                                                     Invalid input. Please enter a valid User ID.\033[0m\n";
                    system("pause");
                    continue;
                }

                bool userExists = false;
                try {
                    sql::PreparedStatement* pstmt = globalCon->prepareStatement("SELECT COUNT(*) AS UserCount FROM users WHERE User_id = ?");
                    pstmt->setInt(1, userID);
                    sql::ResultSet* res = pstmt->executeQuery();

                    if (res->next() && res->getInt("UserCount") > 0) {
                        userExists = true;
                    }

                    delete res;
                    delete pstmt;
                }
                catch (sql::SQLException& e) {
                    cerr << "\033[31mSQL Error (Check User ID): " << e.what() << "\033[0m\n";
                    system("pause");
                    continue;
                }

                if (!userExists) {
                    cout << "\033[31m\n                                                                     User ID not found. Please try again.\033[0m\n";
                    system("pause");
                    continue;
                }

                cout << "\n                                                                     Options:\n";
                cout << "                                                                     1. First Name\n";
                cout << "                                                                     2. Last Name\n";
                cout << "                                                                     3. Phone Number\n";
                cout << "                                                                     4. Email\n";
                cout << "                                                                     5. Username\n";
                cout << "                                                                     6. Password\n";
                cout << "                                                                     7. Admin (1 for Yes, 0 for No)\n";
                cout << "                                                                     8. Is Student (1 for Yes, 0 for No)\n";
                cout << "                                                                     9. Matric Number\n";
                cout << "                                                                     10. Back to User List\n";
                cout << "\n                                                                     Enter your choice: ";
                string updateInput;
                cin >> updateInput;

                if (updateInput.find_first_not_of("0123456789") != string::npos || updateInput.empty()) {
                    cout << "\033[31m\n                                                                     Invalid input. Please enter a number between 1 and 10.\033[0m\n";
                    system("pause");
                    continue;
                }

                int updateChoice = stoi(updateInput);
                if (updateChoice == 10) {
                    break; // Return to user list
                }

                string fieldName, newValue;
                switch (updateChoice) {
                case 1: fieldName = "FirstName"; break;
                case 2: fieldName = "LastName"; break;
                case 3: fieldName = "PhoneNumber"; break;
                case 4: fieldName = "Email"; break;
                case 5: fieldName = "UserName"; break;
                case 6: fieldName = "Password"; break;
                case 7: fieldName = "Admin"; break;
                case 8: fieldName = "IsStudent"; break;
                case 9: fieldName = "MatricNumber"; break;
                default:
                    cout << "\033[31m\n                                                                     Invalid choice. Please try again.\033[0m\n";
                    system("pause");
                    continue;
                }

                cout << "\n                                                                     Enter new value for " << fieldName << ": ";
                cin.ignore();
                getline(cin, newValue);

                try {
                    sql::PreparedStatement* pstmt = globalCon->prepareStatement("UPDATE users SET " + fieldName + " = ? WHERE User_id = ?");
                    pstmt->setString(1, newValue);
                    pstmt->setInt(2, userID);
                    pstmt->executeUpdate();
                    delete pstmt;

                    cout << "\033[32m\n                                                                     " << fieldName << " updated successfully!\033[0m\n";
                }
                catch (sql::SQLException& e) {
                    cerr << "\033[31m Error (Update User): " << e.what() << "\033[0m\n";
                }

                system("pause");
            }
        }
        catch (sql::SQLException& e) {
            cerr << "SQL Error (View Users for Update): " << e.what() << endl;
            system("pause");
            return;
        }
    }
}

void deleteUser() {
    while (true) {
        system("cls");

        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                          DELETE USER                        *\n";
        cout << "                                                                     ***************************************************************\n";

        // Display all users for reference
        try {
            sql::PreparedStatement* pstmt = globalCon->prepareStatement("SELECT User_id, FirstName, LastName, Email, UserName, PhoneNumber, Admin FROM users");
            sql::ResultSet* res = pstmt->executeQuery();

            cout << "\n                                                                     +-------+----------------+----------------+-------------------+-------------------+-----------------+-------+\n";
            cout << "                                                                     | UserID| First Name     | Last Name      | Email             | Username          | Phone Number    | Admin |\n";
            cout << "                                                                     +-------+----------------+----------------+-------------------+-------------------+-----------------+-------+\n";

            bool hasUsers = false;
            while (res->next()) {
                hasUsers = true;
                cout << "                                                                     | " << setw(6) << res->getInt("User_id")
                    << " | " << setw(14) << res->getString("FirstName")
                    << " | " << setw(14) << res->getString("LastName")
                    << " | " << setw(17) << res->getString("Email")
                    << " | " << setw(17) << res->getString("UserName")
                    << " | " << setw(14) << res->getString("PhoneNumber")
                    << " | " << setw(5) << (res->getInt("Admin") == 1 ? "Yes" : "No") << " |\n";
            }

            cout << "                                                                     +-------+----------------+----------------+-------------------+-------------------+-----------------+-------+\n";

            if (!hasUsers) {
                cout << "\033[31m\n                                                                     No users found in the system.\033[0m\n";
                system("pause");
                return;
            }

            delete res;
            delete pstmt;
        }
        catch (sql::SQLException& e) {
            cerr << "\033[31mSQL Error (View All Users): " << e.what() << "\033[0m\n";
            system("pause");
            return;
        }

        // Ask admin for user ID to delete
        cout << "\n                                                                     Enter the User ID to delete or 0 to return: ";
        int userID;
        cin >> userID;

        if (userID == 0) {
            return; // Return to Manage Users page
        }

        // Check if user exists
        bool userExists = false;
        try {
            sql::PreparedStatement* pstmt = globalCon->prepareStatement("SELECT COUNT(*) AS UserCount FROM users WHERE User_id = ?");
            pstmt->setInt(1, userID);
            sql::ResultSet* res = pstmt->executeQuery();

            if (res->next() && res->getInt("UserCount") > 0) {
                userExists = true;
            }

            delete res;
            delete pstmt;
        }
        catch (sql::SQLException& e) {
            cerr << "\033[31mSQL Error (Check User Existence): " << e.what() << "\033[0m\n";
            system("pause");
            return;
        }

        if (!userExists) {
            cout << "\033[31m\n                                                                     User ID not found. Please try again.\033[0m\n";
            system("pause");
            continue;
        }

        // Confirm deletion
        cout << "\n                                                                     Are you sure you want to delete User ID " << userID << "? (y/n): ";
        char confirm;
        cin >> confirm;

        if (tolower(confirm) != 'y') {
            cout << "\033[33m\n                                                                     Deletion canceled. Returning to Manage Users page.\033[0m\n";
            system("pause");
            return;
        }

        // Delete the user
        try {
            sql::PreparedStatement* pstmt = globalCon->prepareStatement("DELETE FROM users WHERE User_id = ?");
            pstmt->setInt(1, userID);
            pstmt->executeUpdate();
            delete pstmt;

            cout << "\033[32m\n                                                                     User ID " << userID << " deleted successfully!\033[0m\n";
        }
        catch (sql::SQLException& e) {
            cerr << "\033[31mSQL Error (Delete User): " << e.what() << "\033[0m\n";
        }

        system("pause");
    }
}

void addNewUser() {
    while (true) {
        system("cls");
        string firstName, lastName, phoneNumber, email, username, password, confirmPassword, date;
        int isAdmin;

        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                          ADD NEW USER                        *\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     Note: Enter '0' at any input prompt to return to the previous page.\n\n";

        // First Name
        cout << "                                                                     Enter First Name: ";
        cin.ignore();
        getline(cin, firstName);
        if (firstName == "0") return;

        // Last Name
        cout << "                                                                     Enter Last Name: ";
        getline(cin, lastName);
        if (lastName == "0") return;

        // Validate email and check if it exists
        while (true) {
            cout << "                                                                     Enter Email: ";
            getline(cin, email);
            if (email == "0") return;

            if (email.find('@') == string::npos || email.find('.') == string::npos) {
                cout << "\033[31m                                                                     Invalid email format. Please enter a valid email.\033[0m\n";
                continue;
            }

            try {
                sql::PreparedStatement* pstmt = globalCon->prepareStatement("SELECT COUNT(*) FROM users WHERE Email = ?");
                pstmt->setString(1, email);
                sql::ResultSet* res = pstmt->executeQuery();
                res->next();
                if (res->getInt(1) > 0) {
                    cout << "\033[31m                                                                     This email is already in use. Please enter a different email.\033[0m\n";
                    delete res;
                    delete pstmt;
                    continue;
                }
                delete res;
                delete pstmt;
            }
            catch (sql::SQLException& e) {
                cerr << "\033[31mSQL Error (Check Email): " << e.what() << "\033[0m\n";
                system("pause");
                return;
            }

            break;
        }

        // Validate username and check if it exists
        while (true) {
            cout << "                                                                     Enter Username: ";
            getline(cin, username);
            if (username == "0") return;

            try {
                sql::PreparedStatement* pstmt = globalCon->prepareStatement("SELECT COUNT(*) FROM users WHERE UserName = ?");
                pstmt->setString(1, username);
                sql::ResultSet* res = pstmt->executeQuery();
                res->next();
                if (res->getInt(1) > 0) {
                    cout << "\033[31m                                                                     This username is already taken. Please enter a different username.\033[0m\n";
                    delete res;
                    delete pstmt;
                    continue;
                }
                delete res;
                delete pstmt;
            }
            catch (sql::SQLException& e) {
                cerr << "\033[31mSQL Error (Check Username): " << e.what() << "\033[0m\n";
                system("pause");
                return;
            }

            break;
        }

        // Validate phone number and check if it exists
        while (true) {
            cout << "                                                                     Enter Phone Number: ";
            getline(cin, phoneNumber);
            if (phoneNumber == "0") return;

            if (phoneNumber.length() != 11 || !all_of(phoneNumber.begin(), phoneNumber.end(), ::isdigit)) {
                cout << "\033[31m                                                                     Invalid phone number. Please enter an 11-digit number.\033[0m\n";
                continue;
            }

            try {
                sql::PreparedStatement* pstmt = globalCon->prepareStatement("SELECT COUNT(*) FROM users WHERE PhoneNumber = ?");
                pstmt->setString(1, phoneNumber);
                sql::ResultSet* res = pstmt->executeQuery();
                res->next();
                if (res->getInt(1) > 0) {
                    cout << "\033[31m                                                                     This phone number is already registered. Please enter a different number.\033[0m\n";
                    delete res;
                    delete pstmt;
                    continue;
                }
                delete res;
                delete pstmt;
            }
            catch (sql::SQLException& e) {
                cerr << "\033[31mSQL Error (Check Phone Number): " << e.what() << "\033[0m\n";
                system("pause");
                return;
            }

            break;
        }

        // Validate password and confirmation
        while (true) {
            cout << "                                                                     Enter Password: ";
            getline(cin, password);
            if (password == "0") return;

            cout << "                                                                     Confirm Password: ";
            getline(cin, confirmPassword);
            if (confirmPassword == "0") return;

            if (password != confirmPassword) {
                cout << "\033[31m                                                                     Passwords do not match. Please try again.\033[0m\n";
            }
            else {
                break;
            }
        }

        // Date
        cout << "                                                                     Enter Date (YYYY-MM-DD): ";
        getline(cin, date);
        if (date == "0") return;

        // Admin status
        cout << "                                                                     Is Admin (1 for Yes, 0 for No): ";
        cin >> isAdmin;

        // Insert new user into the database
        try {
            sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                "INSERT INTO users (FirstName, LastName, PhoneNumber, Email, UserName, Password, Date, Admin) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
            pstmt->setString(1, firstName);
            pstmt->setString(2, lastName);
            pstmt->setString(3, phoneNumber);
            pstmt->setString(4, email);
            pstmt->setString(5, username);
            pstmt->setString(6, password);
            pstmt->setString(7, date);
            pstmt->setInt(8, isAdmin);

            pstmt->executeUpdate();
            delete pstmt;

            cout << "\033[32m\n                                                                     User added successfully!\033[0m\n";
        }
        catch (sql::SQLException& e) {
            cerr << "\033[31mSQL Error (Add New User): " << e.what() << "\033[0m\n";
        }

        // Ask admin if they want to add another user or return
        cout << "\n                                                                     Would you like to add another user? (y/n): ";
        char choice;
        cin >> choice;

        if (tolower(choice) != 'y') {
            return; // Return to Manage Users page
        }
    }
}

//payment manage
void managePayments() {
    int choice;

    while (true) {
        system("cls");
        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                       PAYMENT MANAGEMENT                     *\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "\n";
        cout << "                                                                     1. View All Payments\n";
        cout << "                                                                     2. Add New Payment\n";
        cout << "                                                                     3. Update Payment Information\n";
        cout << "                                                                     4. Delete Payment\n";
        cout << "                                                                     5. Return to Admin Page\n";
        cout << "\n                                                                     Enter your choice: ";

        string input;
        cin >> input;

        if (input.length() != 1 || !isdigit(input[0])) {
            cout << "\n\033[31mInvalid input. Please enter a number between 1 and 5.\033[0m\n";
            system("pause");
            continue;
        }

        choice = stoi(input);

        switch (choice) {
        case 1:
            viewAllPayments();
            break;
        case 2:
            addNewPayment();
            break;
        case 3:
            updatePayment();
            break;
        case 4:
            deletePayment();
            break;
        case 5:
            return; // Exit to Admin Page
        default:
            cout << "\n\033[31mInvalid choice. Please select a valid option.\033[0m\n";
            system("pause");
            break;
        }
    }
}

void viewAllPayments() {
    while (true) {
        system("cls");
        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                       VIEW ALL PAYMENTS                      *\n";
        cout << "                                                                     ***************************************************************\n";

        cout << "\n                                                                     Options:\n";
        cout << "                                                                     1. View All Payments\n";
        cout << "                                                                     2. Search Payment by Payment ID\n";
        cout << "                                                                     3. Return to Main Menu\n";
        cout << "                                                                     Enter your choice: ";

        string input;
        cin >> input;

        if (input.find_first_not_of("0123456789") != string::npos || input.empty()) {
            cout << "\033[31m\n                                                                     Invalid choice. Please enter a valid number.\033[0m\n";
            system("pause");
            continue;
        }

        int choice = stoi(input);

        if (choice == 3) {
            return; // Return to main menu
        }

        if (choice == 2) {
            int searchPaymentID;
            cout << "\n                                                                     Enter Payment ID to search: ";
            cin >> searchPaymentID;

            if (cin.fail()) {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cout << "\033[31m\n                                                                     Invalid input. Please enter a valid Payment ID.\033[0m\n";
                system("pause");
                continue;
            }

            try {
                sql::PreparedStatement* searchPstmt = globalCon->prepareStatement("SELECT * FROM payment WHERE Payment_id = ?");
                searchPstmt->setInt(1, searchPaymentID);
                sql::ResultSet* searchRes = searchPstmt->executeQuery();

                cout << "\n                                                                     +-------------+-------------+-------------+------------------+------------------+---------------------+-----------------+\n";
                cout << "                                                                     | Payment ID  | User ID     | Booking ID  | Payment Method   | Payment Amount   | Payment Date        | Payment Confirmed |\n";
                cout << "                                                                     +-------------+-------------+-------------+------------------+------------------+---------------------+-----------------+\n";

                bool paymentFound = false;
                while (searchRes->next()) {
                    paymentFound = true;
                    cout << "                                                                     | " << setw(11) << searchRes->getInt("Payment_id")
                        << " | " << setw(11) << searchRes->getInt("User_id")
                        << " | " << setw(11) << searchRes->getInt("Booking_id")
                        << " | " << setw(16) << searchRes->getString("PaymentMethod")
                        << " | " << setw(16) << fixed << setprecision(2) << searchRes->getDouble("PaymentAmount")
                        << " | " << setw(19) << searchRes->getString("PaymentDate")
                        << " | " << setw(15) << (searchRes->getInt("PaymentConfirmed") == 1 ? "Yes" : "No") << " |\n";
                }

                cout << "                                                                     +-------------+-------------+-------------+------------------+------------------+---------------------+-----------------+\n";

                if (!paymentFound) {
                    cout << "\033[31m\n                                                                     No payment found with the specified Payment ID.\033[0m\n";
                }

                delete searchRes;
                delete searchPstmt;

                system("pause");
                continue; // Restart loop
            }
            catch (sql::SQLException& e) {
                cerr << "\033[31mSQL Error (Search Payment):\033[0m " << e.what() << endl;
                system("pause");
                continue;
            }
        }

        if (choice == 1) {
            int minID = 0; // Start from the smallest Payment ID
            int pageSize = 10; // Number of payments per page

            while (true) {
                system("cls");
                cout << "\n                                                                     ***************************************************************\n";
                cout << "                                                                     *                     VIEW ALL PAYMENTS - PAGE                     *\n";
                cout << "                                                                     ***************************************************************\n";

                try {
                    sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                        "SELECT * FROM payment WHERE Payment_id >= ? ORDER BY Payment_id ASC LIMIT ?");
                    pstmt->setInt(1, minID);
                    pstmt->setInt(2, pageSize);
                    sql::ResultSet* res = pstmt->executeQuery();

                    cout << "\n                                                                     +-------------+-------------+-------------+------------------+------------------+---------------------+-----------------+\n";
                    cout << "                                                                     | Payment ID  | User ID     | Booking ID  | Payment Method   | Payment Amount   | Payment Date        | Payment Confirmed |\n";
                    cout << "                                                                     +-------------+-------------+-------------+------------------+------------------+---------------------+-----------------+\n";

                    bool hasNextPage = false;
                    int lastID = minID;
                    while (res->next()) {
                        lastID = res->getInt("Payment_id");
                        cout << "                                                                     | " << setw(11) << res->getInt("Payment_id")
                            << " | " << setw(11) << res->getInt("User_id")
                            << " | " << setw(11) << res->getInt("Booking_id")
                            << " | " << setw(16) << res->getString("PaymentMethod")
                            << " | " << setw(16) << fixed << setprecision(2) << res->getDouble("PaymentAmount")
                            << " | " << setw(19) << res->getString("PaymentDate")
                            << " | " << setw(15) << (res->getInt("PaymentConfirmed") == 1 ? "Yes" : "No") << " |\n";

                        hasNextPage = true; // Indicate if there is more data
                    }

                    cout << "                                                                     +-------------+-------------+-------------+------------------+------------------+---------------------+-----------------+\n";

                    delete res;
                    delete pstmt;

                    cout << "\n                                                                     Navigation:\n";
                    if (minID > 0) {
                        cout << "                                                                     1. Previous Page\n";
                    }
                    if (hasNextPage) {
                        cout << "                                                                     2. Next Page\n";
                    }
                    cout << "                                                                     3. Return to Main Menu\n";
                    cout << "                                                                     Enter your choice: ";

                    string navInput;
                    cin >> navInput;

                    if (navInput.find_first_not_of("0123456789") != string::npos || navInput.empty()) {
                        cout << "\033[31m\n                                                                     Invalid input. Please enter a valid number.\033[0m\n";
                        system("pause");
                        continue;
                    }

                    int navChoice = stoi(navInput);

                    if (navChoice == 3) {
                        break; // Exit the loop and return to main menu
                    }
                    if (navChoice == 1 && minID > 0) {
                        minID -= pageSize;
                        if (minID < 0) minID = 0; // Prevent negative minID
                    }
                    else if (navChoice == 2 && hasNextPage) {
                        minID = lastID; // Move to the next page
                    }
                    else {
                        cout << "\033[31m\n                                                                     Invalid choice. Please try again.\033[0m\n";
                        system("pause");
                    }
                }
                catch (sql::SQLException& e) {
                    cerr << "\033[31mSQL Error (View All Payments):\033[0m " << e.what() << endl;
                    system("pause");
                    continue;
                }
            }
        }
        else {
            cout << "\033[31m\n                                                                     Invalid choice. Please try again.\033[0m\n";
            system("pause");
        }
    }
}

void addNewPayment() {
    while (true) {
        system("cls");

        int userID, bookingID;
        string paymentMethod, paymentDate;
        double paymentAmount;

        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                         ADD NEW PAYMENT                      *\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     Note: Enter '0' at any input prompt to return to the previous page.\n\n";

        // Input payment details
        cout << "                                                                     Enter User ID: ";
        cin >> userID;

        if (userID == 0) {
            return; // Go back to the previous menu
        }

        cout << "                                                                     Enter Booking ID: ";
        cin >> bookingID;

        if (bookingID == 0) {
            return; // Go back to the previous menu
        }

        cout << "                                                                     Enter Payment Method (Visa/MasterCard/Cash): ";
        cin.ignore();
        getline(cin, paymentMethod);

        // Validate Payment Method
        while (paymentMethod != "Visa" && paymentMethod != "MasterCard" && paymentMethod != "Cash") {
            cout << "\033[31m                                                                     Invalid Payment Method. Please enter 'Visa', 'MasterCard', or 'Cash':\033[0m ";
            getline(cin, paymentMethod);
        }

        cout << "                                                                     Enter Payment Amount: ";
        cin >> paymentAmount;

        if (paymentAmount == 0) {
            return; // Go back to the previous menu
        }

        cout << "                                                                     Enter Payment Date (YYYY-MM-DD): ";
        cin.ignore();
        getline(cin, paymentDate);

        if (paymentDate == "0") {
            return; // Go back to the previous menu
        }

        // Insert payment into the database
        try {
            sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                "INSERT INTO payment (User_id, Booking_id, PaymentMethod, PaymentAmount, PaymentDate) VALUES (?, ?, ?, ?, ?)");
            pstmt->setInt(1, userID);
            pstmt->setInt(2, bookingID);
            pstmt->setString(3, paymentMethod);
            pstmt->setDouble(4, paymentAmount);
            pstmt->setString(5, paymentDate);

            pstmt->executeUpdate();
            delete pstmt;

            cout << "\033[32m\n                                                                     Payment added successfully!\033[0m\n";
        }
        catch (sql::SQLException& e) {
            cerr << "SQL Error (Add New Payment): " << e.what() << endl;
        }

        // Prompt admin to add another payment or go back
        cout << "\n                                                                     Would you like to add another payment? (y/n): ";
        char choice;
        cin >> choice;

        if (tolower(choice) != 'y') {
            return; // Go back to the previous menu
        }
    }
}

void deletePayment() {
    system("cls");

    int paymentID;

    cout << "\n\n\n\n\n\n\n\n\n";
    cout << "                                                                     ***************************************************************\n";
    cout << "                                                                     *                        DELETE PAYMENT                       *\n";
    cout << "                                                                     ***************************************************************\n";

    // Display all payments
    try {
        sql::PreparedStatement* pstmt = globalCon->prepareStatement("SELECT * FROM payment");
        sql::ResultSet* res = pstmt->executeQuery();

        cout << "\n                                                                     +------------+----------+------------+----------------+------------------+--------------+\n";
        cout << "                                                                     | Payment ID | User ID  | Booking ID | Payment Method | Payment Amount   | Payment Date |\n";
        cout << "                                                                     +------------+----------+------------+----------------+------------------+--------------+\n";

        while (res->next()) {
            cout << "                                                                     | " << setw(10) << res->getInt("Payment_id")
                << " | " << setw(8) << res->getInt("User_id")
                << " | " << setw(10) << res->getInt("Booking_id")
                << " | " << setw(14) << res->getString("PaymentMethod")
                << " | " << setw(16) << fixed << setprecision(2) << res->getDouble("PaymentAmount")
                << " | " << setw(12) << res->getString("PaymentDate") << " |\n";
        }

        cout << "                                                                     +------------+----------+------------+----------------+------------------+--------------+\n";

        delete res;
        delete pstmt;
    }
    catch (sql::SQLException& e) {
        cerr << "SQL Error (View Payments): " << e.what() << endl;
        system("pause");
        return;
    }

    // Input the payment ID to delete
    cout << "\n                                                                     Enter Payment ID to delete: ";
    cin >> paymentID;

    // Delete payment from the database
    try {
        sql::PreparedStatement* pstmt = globalCon->prepareStatement("DELETE FROM payment WHERE Payment_id = ?");
        pstmt->setInt(1, paymentID);

        int rowsAffected = pstmt->executeUpdate();
        delete pstmt;

        if (rowsAffected > 0) {
            cout << "\033[32m\n                                                                     Payment deleted successfully!\033[0m\n";
        }
        else {
            cout << "\033[31m\n                                                                     Payment ID not found. No records deleted.\033[0m\n";
        }
    }
    catch (sql::SQLException& e) {
        cerr << "SQL Error (Delete Payment): " << e.what() << endl;
    }

    system("pause");
}

void updatePayment() {
    while (true) {
        system("cls");

        int paymentID;
        string fieldToUpdate, newValue;

        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                        UPDATE PAYMENT                       *\n";
        cout << "                                                                     ***************************************************************\n";

        // Display all payments
        try {
            sql::PreparedStatement* pstmt = globalCon->prepareStatement("SELECT * FROM payment");
            sql::ResultSet* res = pstmt->executeQuery();

            cout << "\n                                                                     +------------+----------+------------+----------------+------------------+--------------+\n";
            cout << "                                                                     | Payment ID | User ID  | Booking ID | Payment Method | Payment Amount   | Payment Date |\n";
            cout << "                                                                     +------------+----------+------------+----------------+------------------+--------------+\n";

            while (res->next()) {
                cout << "                                                                     | " << setw(10) << res->getInt("Payment_id")
                    << " | " << setw(8) << res->getInt("User_id")
                    << " | " << setw(10) << res->getInt("Booking_id")
                    << " | " << setw(14) << res->getString("PaymentMethod")
                    << " | " << setw(16) << fixed << setprecision(2) << res->getDouble("PaymentAmount")
                    << " | " << setw(12) << res->getString("PaymentDate") << " |\n";
            }

            cout << "                                                                     +------------+----------+------------+----------------+------------------+--------------+\n";

            delete res;
            delete pstmt;
        }
        catch (sql::SQLException& e) {
            cerr << "SQL Error (View Payments): " << e.what() << endl;
            system("pause");
            return;
        }

        // Input the payment ID to update
        cout << "\n                                                                     Enter Payment ID to update (or 0 to go back): ";
        cin >> paymentID;

        if (paymentID == 0) {
            return; // Exit to the previous menu
        }

        // Display update options
        cout << "\n                                                                     Options:\n";
        cout << "                                                                     1. Update User ID\n";
        cout << "                                                                     2. Update Booking ID\n";
        cout << "                                                                     3. Update Payment Method\n";
        cout << "                                                                     4. Update Payment Amount\n";
        cout << "                                                                     5. Update Payment Date\n";
        cout << "                                                                     6. Return to Payment Management Page\n";
        cout << "                                                                     Enter your choice: ";

        int choice;
        cin >> choice;
        cin.ignore();

        if (choice == 6) {
            return; // Exit to the previous menu
        }

        switch (choice) {
        case 1:
            fieldToUpdate = "User_id";
            break;
        case 2:
            fieldToUpdate = "Booking_id";
            break;
        case 3:
            fieldToUpdate = "PaymentMethod";
            break;
        case 4:
            fieldToUpdate = "PaymentAmount";
            break;
        case 5:
            fieldToUpdate = "PaymentDate";
            break;
        default:
            cout << "\033[31m\n                                                                     Invalid choice. Returning to Payment Management Page.\033[0m\n";
            system("pause");
            return;
        }

        cout << "                                                                     Enter new value for " << fieldToUpdate << ": ";
        getline(cin, newValue);

        // Update the payment record
        try {
            string query = "UPDATE payment SET " + fieldToUpdate + " = ? WHERE Payment_id = ?";
            sql::PreparedStatement* pstmt = globalCon->prepareStatement(query);
            pstmt->setString(1, newValue);
            pstmt->setInt(2, paymentID);

            int rowsAffected = pstmt->executeUpdate();
            delete pstmt;

            if (rowsAffected > 0) {
                cout << "\033[32m\n                                                                     Payment updated successfully!\033[0m\n";
            }
            else {
                cout << "\033[31m\n                                                                     Payment ID not found. No records updated.\033[0m\n";
            }
        }
        catch (sql::SQLException& e) {
            cerr << "SQL Error (Update Payment): " << e.what() << endl;
        }

        system("pause");
    }
}

//movie manage
void manageMovies() {
    int choice;

    while (true) {
        system("cls");
        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                       MOVIE MANAGEMENT                       *\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     1. Add Movie\n";
        cout << "                                                                     2. Delete Movie\n";
        cout << "                                                                     3. View Movies\n";
        cout << "                                                                     4. Update Movie\n";
        cout << "                                                                     5. Return to Admin Page\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     Enter your choice: ";

        string input;
        cin >> input;

        if (input.length() != 1 || !isdigit(input[0])) {
            cout << "\033[31m\n                                                                     Invalid input. Please enter a number between 1 and 5.\033[0m\n";
            system("pause");
            continue;
        }

        choice = stoi(input);

        switch (choice) {
        case 1:
            addMovie(); // To be implemented
            break;
        case 2:
            deleteMovie(); // To be implemented
            break;
        case 3:
            viewAllMovies(); // Reuse the existing function
            break;
        case 4:
            updateMovie(); // To be implemented
            break;
        case 5:
            return; // Exit movie management
        default:
            cout << "\033[31m\n                                                                     Invalid choice. Please try again.\033[0m\n";
            system("pause");
        }
    }
}

void viewAllMovies() {
    while (true) {
        system("cls");
        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                          VIEW ALL MOVIES                     *\n";
        cout << "                                                                     ***************************************************************\n";

        cout << "\n                                                                     Options:\n";
        cout << "                                                                     1. View All Movies\n";
        cout << "                                                                     2. Search Movie\n";
        cout << "                                                                     3. Return to Main Menu\n";
        cout << "                                                                     Enter your choice: ";

        string input;
        cin >> input;

        if (input.find_first_not_of("0123456789") != string::npos || input.empty()) {
            cout << "\033[31m\n                                                                     Invalid input. Please enter a number (1, 2, or 3).\033[0m\n";
            system("pause");
            continue;
        }

        int choice = stoi(input);

        if (choice == 3) {
            return; // Go back to the main menu
        }

        if (choice == 2) {
            string searchTitle;
            cout << "\n                                                                     Enter Movie Title to search: ";
            cin.ignore();
            getline(cin, searchTitle);

            try {
                sql::PreparedStatement* searchPstmt = globalCon->prepareStatement(
                    "SELECT * FROM movies WHERE Title LIKE ?");
                searchPstmt->setString(1, "%" + searchTitle + "%");
                sql::ResultSet* searchRes = searchPstmt->executeQuery();

                cout << "\n                                                                     +-------+----------------------------------+---------------+------------+--------+----------+\n";
                cout << "                                                                     | ID    | Title                            | Genre         | ReleaseDate| Rating | Duration |\n";
                cout << "                                                                     +-------+----------------------------------+---------------+------------+--------+----------+\n";

                bool movieFound = false;
                while (searchRes->next()) {
                    movieFound = true;
                    cout << "                                                                     | " << setw(5) << searchRes->getInt("Movie_id")
                        << " | " << setw(32) << searchRes->getString("Title")
                        << " | " << setw(13) << searchRes->getString("Genre")
                        << " | " << setw(10) << searchRes->getString("ReleaseDate")
                        << " | " << setw(6) << fixed << setprecision(1) << searchRes->getDouble("Rating")
                        << " | " << setw(8) << searchRes->getString("Duration") << " |\n";
                }

                cout << "                                                                     +-------+----------------------------------+---------------+------------+--------+----------+\n";

                if (!movieFound) {
                    cout << "\033[31m\n                                                                     No movies found with the specified title.\033[0m\n";
                }

                delete searchRes;
                delete searchPstmt;

                system("pause");
                continue; // Restart the loop
            }
            catch (sql::SQLException& e) {
                cerr << "\033[31mSQL Error (Search Movie):\033[0m " << e.what() << endl;
                system("pause");
                return;
            }
        }

        if (choice == 1) {
            int minID = 0; // Start from the smallest Movie_id
            int pageSize = 10; // Number of movies to display per page

            try {
                while (true) {
                    system("cls");

                    cout << "\n                                                                   ***************************************************************\n";
                    cout << "                                                                     *                          VIEW ALL MOVIES                     *\n";
                    cout << "                                                                     ***************************************************************\n";

                    sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                        "SELECT * FROM movies WHERE Movie_id >= ? ORDER BY Movie_id ASC LIMIT ?");
                    pstmt->setInt(1, minID);
                    pstmt->setInt(2, pageSize);
                    sql::ResultSet* res = pstmt->executeQuery();

                    cout << "\n                                                                     +-------+----------------------------------+---------------+------------+--------+----------+\n";
                    cout << "                                                                     | ID    | Title                            | Genre         | ReleaseDate| Rating | Duration |\n";
                    cout << "                                                                     +-------+----------------------------------+---------------+------------+--------+----------+\n";

                    bool hasMore = false;
                    int lastID = minID;
                    while (res->next()) {
                        lastID = res->getInt("Movie_id");
                        cout << "                                                                     | " << setw(5) << res->getInt("Movie_id")
                            << " | " << setw(32) << res->getString("Title")
                            << " | " << setw(13) << res->getString("Genre")
                            << " | " << setw(10) << res->getString("ReleaseDate")
                            << " | " << setw(6) << fixed << setprecision(1) << res->getDouble("Rating")
                            << " | " << setw(8) << res->getString("Duration") << " |\n";

                        hasMore = true;
                    }

                    cout << "                                                                     +-------+----------------------------------+---------------+------------+--------+----------+\n";

                    delete res;
                    delete pstmt;

                    cout << "\n                                                                     Navigation:\n";
                    if (minID > 0) {
                        cout << "                                                                     1. Previous Page\n";
                    }
                    if (hasMore) {
                        cout << "                                                                     2. Next Page\n";
                    }
                    cout << "                                                                     3. Return to Main Menu\n";
                    cout << "                                                                     Enter your choice: ";

                    string navInput;
                    cin >> navInput;

                    if (navInput.find_first_not_of("0123456789") != string::npos || navInput.empty()) {
                        cout << "\033[31m\n                                                                     Invalid input. Please enter a valid number.\033[0m\n";
                        system("pause");
                        continue;
                    }

                    int navChoice = stoi(navInput);

                    if (navChoice == 3) {
                        break; // Exit the loop and return to Main Menu
                    }
                    if (navChoice == 1 && minID > 0) {
                        minID -= pageSize;
                        if (minID < 0) minID = 0; // Prevent negative ID
                    }
                    else if (navChoice == 2 && hasMore) {
                        minID = lastID; // Move to the next page
                    }
                    else {
                        cout << "\033[31m\n                                                                     Invalid choice. Please try again.\033[0m\n";
                        system("pause");
                    }
                }
            }
            catch (sql::SQLException& e) {
                cerr << "\033[31mSQL Error (View All Movies):\033[0m " << e.what() << endl;
                system("pause");
                return;
            }
        }

        cout << "\033[31m\n                                                                     Invalid choice. Please try again.\033[0m\n";
        system("pause");
    }
}

void addMovie() {
    system("cls");

    string title, genre, releaseDate, duration;
    double rating;

    cout << "\n\n\n\n\n\n\n\n\n";
    cout << "                                                                     ***************************************************************\n";
    cout << "                                                                     *                           ADD MOVIE                          *\n";
    cout << "                                                                     ***************************************************************\n";
    cout << "\n";

    // Ask if the user wants to continue or return
    while (true) {
        cout << "                                                                     1. Continue adding a movie\n";
        cout << "                                                                     2. Go back to Manage Movies\n";
        cout << "                                                                     Enter your choice: ";
        string choice;
        cin >> choice;

        if (choice == "2") {
            cout << "\n\033[33m                                                                     Returning to Manage Movies...\033[0m\n";
            system("pause");
            manageMovies();
            return;
        }
        else if (choice == "1") {
            cin.ignore(); // Clear buffer before getting user input
            break; // Continue to adding a movie
        }
        else {
            cout << "\033[31m                                                                     Invalid choice. Please enter 1 to continue or 2 to go back.\033[0m\n";
            system("pause");
        }
    }

    cin.ignore(); // Clear the input buffer
    cout << "                                                                     Enter Movie Title: ";
    getline(cin, title);

    cout << "                                                                     Enter Movie Genre: ";
    getline(cin, genre);

    cout << "                                                                     Enter Release Date (YYYY-MM-DD): ";
    getline(cin, releaseDate);

    while (true) {
        cout << "                                                                     Enter Rating (0.0 to 10.0): ";
        cin >> rating;

        if (cin.fail() || rating < 0.0 || rating > 10.0) {
            cout << "\033[31m                                                                     Invalid rating. Please enter a value between 0.0 and 10.0.\033[0m\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
        else {
            break;
        }
    }

    cin.ignore(); // Clear the input buffer
    cout << "                                                                     Enter Duration (e.g., 2h 15m): ";
    getline(cin, duration);

    try {
        sql::PreparedStatement* pstmt = globalCon->prepareStatement(
            "INSERT INTO movies (Title, Genre, ReleaseDate, Rating, Duration) VALUES (?, ?, ?, ?, ?)");
        pstmt->setString(1, title);
        pstmt->setString(2, genre);
        pstmt->setString(3, releaseDate);
        pstmt->setDouble(4, rating);
        pstmt->setString(5, duration);

        pstmt->executeUpdate();
        delete pstmt;

        cout << "\n\033[32m                                                                     Movie added successfully!\033[0m\n";
    }
    catch (sql::SQLException& e) {
        cerr << "\033[31mSQL Error (Add Movie):\033[0m " << e.what() << endl;
    }

    system("pause");
    manageMovies();
}

void deleteMovie() {
    int minID = 0; // Start from the smallest Movie_id
    int pageSize = 10; // Number of movies to display per page

    while (true) {
        system("cls");

        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                          DELETE MOVIE                        *\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "\n";

        try {
            sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                "SELECT * FROM movies WHERE Movie_id > ? ORDER BY Movie_id ASC LIMIT ?");
            pstmt->setInt(1, minID);
            pstmt->setInt(2, pageSize);
            sql::ResultSet* res = pstmt->executeQuery();

            cout << "                                                                     +----------+------------------------------------------+------------------+--------------+--------+----------+\n";
            cout << "                                                                     | Movie ID | Title                                    | Genre            | Release Date | Rating | Duration |\n";
            cout << "                                                                     +----------+------------------------------------------+------------------+--------------+--------+----------+\n";

            bool hasMore = false;
            int lastID = minID;
            while (res->next()) {
                lastID = res->getInt("Movie_id");
                cout << "                                                                     | " << setw(8) << res->getInt("Movie_id")
                    << " | " << setw(40) << left << res->getString("Title")
                    << " | " << setw(16) << res->getString("Genre")
                    << " | " << setw(12) << res->getString("ReleaseDate")
                    << " | " << setw(6) << fixed << setprecision(1) << res->getDouble("Rating")
                    << " | " << setw(8) << res->getString("Duration") << " |\n";

                hasMore = true;
            }

            cout << "                                                                     +----------+------------------------------------------+------------------+--------------+--------+----------+\n";

            delete res;
            delete pstmt;

            // Navigation options
            cout << "\n                                                                     Options:\n";
            if (minID > 0) {
                cout << "                                                                     1. Previous Page\n";
            }
            if (hasMore) {
                cout << "                                                                     2. Next Page\n";
            }
            cout << "                                                                     3. Enter Movie ID to Delete\n";
            cout << "                                                                     4. Return to Main Menu\n";
            cout << "                                                                     Enter your choice: ";

            int choice;
            cin >> choice;

            if (cin.fail()) {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cout << "\033[31m\n                                                                     Invalid input. Please enter a valid number.\033[0m\n";
                system("pause");
                continue;
            }

            if (choice == 4) {
                return; // Go back to the main menu
            }
            if (choice == 1 && minID > 0) {
                minID -= pageSize;
                if (minID < 0) minID = 0; // Prevent negative ID
            }
            else if (choice == 2 && hasMore) {
                minID = lastID; // Move to the next page
            }
            else if (choice == 3) {
                cout << "\n                                                                     Enter the Movie ID of the movie to delete: ";
                int movieID;
                cin >> movieID;

                if (cin.fail()) {
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    cout << "\033[31m\n                                                                     Invalid input. Please enter a valid Movie ID.\033[0m\n";
                    system("pause");
                    continue;
                }

                try {
                    sql::PreparedStatement* deletePstmt = globalCon->prepareStatement("DELETE FROM movies WHERE Movie_id = ?");
                    deletePstmt->setInt(1, movieID);
                    int rowsAffected = deletePstmt->executeUpdate();
                    delete deletePstmt;

                    if (rowsAffected > 0) {
                        cout << "\n\033[32m                                                                     Movie deleted successfully!\033[0m\n";
                    }
                    else {
                        cout << "\033[31m                                                                     Movie ID not found. No movie deleted.\033[0m\n";
                    }
                }
                catch (sql::SQLException& e) {
                    cerr << "\033[31mSQL Error (Delete Movie):\033[0m " << e.what() << endl;
                }

                system("pause");
                continue;
            }
            else {
                cout << "\033[31m\n                                                                     Invalid choice. Please try again.\033[0m\n";
                system("pause");
            }
        }
        catch (sql::SQLException& e) {
            cerr << "\033[31mSQL Error (View Movies):\033[0m " << e.what() << endl;
            system("pause");
            return;
        }
    }
}

void updateMovie() {
    int minID = 0; // Start from the smallest Movie_id
    int pageSize = 10; // Number of movies to display per page

    while (true) {
        system("cls");
        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                          UPDATE MOVIES                       *\n";
        cout << "                                                                     ***************************************************************\n";

        try {
            sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                "SELECT * FROM movies WHERE Movie_id > ? ORDER BY Movie_id ASC LIMIT ?");
            pstmt->setInt(1, minID);
            pstmt->setInt(2, pageSize);
            sql::ResultSet* res = pstmt->executeQuery();

            cout << "\n                                                                     +----------+------------------------------+-----------------+--------------+--------+-----------+\n";
            cout << "                                                                     | Movie ID | Title                        | Genre           | Release Date | Rating | Duration  |\n";
            cout << "                                                                     +----------+------------------------------+-----------------+--------------+--------+-----------+\n";

            bool hasMore = false;
            int lastID = minID;
            while (res->next()) {
                lastID = res->getInt("Movie_id");
                cout << "                                                                     | " << setw(8) << res->getInt("Movie_id")
                    << " | " << setw(28) << res->getString("Title")
                    << " | " << setw(15) << res->getString("Genre")
                    << " | " << setw(12) << res->getString("ReleaseDate")
                    << " | " << setw(6) << fixed << setprecision(1) << res->getDouble("Rating")
                    << " | " << setw(9) << res->getString("Duration") << " |\n";

                hasMore = true;
            }

            cout << "                                                                     +----------+------------------------------+-----------------+--------------+--------+-----------+\n";

            delete res;
            delete pstmt;

            // Navigation options
            cout << "\n                                                                     Options:\n";
            if (minID > 0) {
                cout << "                                                                     1. Previous Page\n";
            }
            if (hasMore) {
                cout << "                                                                     2. Next Page\n";
            }
            cout << "                                                                     3. Enter Movie ID to Update\n";
            cout << "                                                                     4. Return to Main Menu\n";
            cout << "                                                                     Enter your choice: ";

            string input;
            cin >> input;

            if (input.length() != 1 || !isdigit(input[0])) {
                cout << "\033[31m\n                                                                     Invalid input. Please enter a valid number.\033[0m\n";
                system("pause");
                continue;
            }

            int choice = stoi(input);

            if (choice == 4) {
                return; // Go back to the main menu
            }
            if (choice == 1 && minID > 0) {
                minID -= pageSize;
                if (minID < 0) minID = 0; // Prevent negative ID
            }
            else if (choice == 2 && hasMore) {
                minID = lastID; // Move to the next page
            }
            else if (choice == 3) {
                int movieID;
                cout << "\n                                                                     Enter the Movie ID you want to update: ";
                cin >> movieID;

                if (cin.fail()) {
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    cout << "\033[31m\n                                                                     Invalid input. Please enter a valid Movie ID.\033[0m\n";
                    system("pause");
                    continue;
                }

                // Check if the movie ID exists
                bool movieExists = false;
                try {
                    sql::PreparedStatement* pstmt = globalCon->prepareStatement("SELECT COUNT(*) AS Count FROM movies WHERE Movie_id = ?");
                    pstmt->setInt(1, movieID);
                    sql::ResultSet* res = pstmt->executeQuery();
                    if (res->next() && res->getInt("Count") > 0) {
                        movieExists = true;
                    }
                    delete res;
                    delete pstmt;
                }
                catch (sql::SQLException& e) {
                    cerr << "SQL Error (Check Movie ID): " << e.what() << endl;
                    system("pause");
                    continue;
                }

                if (!movieExists) {
                    cout << "\033[31m\n                                                                     Movie ID not found. Please try again.\033[0m\n";
                    system("pause");
                    continue;
                }

                // Display update options
                while (true) {
                    system("cls");
                    cout << "\n                                                                     What would you like to update?\n";
                    cout << "                                                                     1. Title\n";
                    cout << "                                                                     2. Genre\n";
                    cout << "                                                                     3. Release Date\n";
                    cout << "                                                                     4. Rating\n";
                    cout << "                                                                     5. Duration\n";
                    cout << "                                                                     6. Back to Movie List\n";
                    cout << "                                                                     Enter your choice: ";

                    string updateInput;
                    cin >> updateInput;

                    if (updateInput.length() != 1 || !isdigit(updateInput[0])) {
                        cout << "\033[31m\n                                                                     Invalid input. Please enter a number between 1 and 6.\033[0m\n";
                        system("pause");
                        continue;
                    }

                    int updateChoice = stoi(updateInput);
                    if (updateChoice == 6) {
                        break; // Return to movie list
                    }

                    string field, newValue;
                    switch (updateChoice) {
                    case 1: field = "Title"; break;
                    case 2: field = "Genre"; break;
                    case 3: field = "ReleaseDate"; break;
                    case 4: field = "Rating"; break;
                    case 5: field = "Duration"; break;
                    default:
                        cout << "\033[31m\n                                                                     Invalid choice. Please try again.\033[0m\n";
                        system("pause");
                        continue;
                    }

                    cin.ignore(); // Ignore leftover newline character
                    cout << "\n                                                                     Enter the new value for " << field << ": ";
                    getline(cin, newValue);

                    // Validate input for specific fields
                    if (field == "Rating") {
                        try {
                            stod(newValue); // Ensure numeric value
                        }
                        catch (...) {
                            cout << "\033[31m\n                                                                     Invalid rating. Please enter a numeric value.\033[0m\n";
                            system("pause");
                            continue;
                        }
                    }

                    // Update the movie
                    try {
                        string query = "UPDATE movies SET " + field + " = ? WHERE Movie_id = ?";
                        sql::PreparedStatement* pstmt = globalCon->prepareStatement(query);
                        pstmt->setString(1, newValue);
                        pstmt->setInt(2, movieID);
                        pstmt->executeUpdate();
                        delete pstmt;

                        cout << "\033[32m\n                                                                     " << field << " updated successfully!\033[0m\n";
                    }
                    catch (sql::SQLException& e) {
                        cerr << "SQL Error (Update Movie): " << e.what() << endl;
                    }

                    system("pause");
                    break;
                }
            }
            else {
                cout << "\033[31m\n                                                                     Invalid choice. Please try again.\033[0m\n";
                system("pause");
            }
        }
        catch (sql::SQLException& e) {
            cerr << "SQL Error (View Movies for Update): " << e.what() << endl;
            system("pause");
            return;
        }
    }
}

//meals manage
void manageMeals() {
    while (true) {
        system("cls");
        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                         MEALS MANAGEMENT                    *\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     1. View Meals\n";
        cout << "                                                                     2. Add Meal\n";
        cout << "                                                                     3. Update Meal\n";
        cout << "                                                                     4. Return to Admin Page\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     Enter your choice: ";

        string input;
        cin >> input;

        if (input.length() != 1 || !isdigit(input[0])) {
            cout << "\033[31mInvalid input. Please enter a number between 1 and 5.\033[0m\n";
            system("pause");
            continue;
        }

        int choice = stoi(input);

        switch (choice) {
        case 1:
            viewMeals();
            break;
        case 2:
            addMeal();
            break;
        case 3:
            updateMeal();
            break;
        case 4:
            return;
        default:
            cout << "\033[31mInvalid choice. Please select a valid option.\033[0m\n";
            system("pause");
            break;
        }
    }
}

void viewMeals() {
    while (true) {
        system("cls");
        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                          VIEW MEALS                         *\n";
        cout << "                                                                     ***************************************************************\n";

        cout << "\n                                                                     Options:\n";
        cout << "                                                                     1. View All Meals\n";
        cout << "                                                                     2. Search Meal by Name\n";
        cout << "                                                                     3. Return to Main Menu\n";
        cout << "                                                                     Enter your choice: ";

        string input;
        cin >> input;

        if (input.find_first_not_of("0123456789") != string::npos || input.empty()) {
            cout << "\033[31m\n                                                                     Invalid input. Please enter a number (1, 2, or 3).\033[0m\n";
            system("pause");
            continue;
        }

        int choice = stoi(input);

        if (choice == 3) {
            return; // Go back to the main menu
        }

        if (choice == 2) {  // Search Meal by Name
            string searchMealName;
            cout << "\n                                                                     Enter Meal Name to search: ";
            cin.ignore();
            getline(cin, searchMealName);

            try {
                sql::PreparedStatement* searchPstmt = globalCon->prepareStatement(
                    "SELECT * FROM meals WHERE MealName LIKE ?");
                searchPstmt->setString(1, "%" + searchMealName + "%");
                sql::ResultSet* searchRes = searchPstmt->executeQuery();

                cout << "\n                                                                     +-------+--------------------------+-------------------+\n";
                cout << "                                                                     | MealID| Meal Name                | Meal Price (RM)   |\n";
                cout << "                                                                     +-------+--------------------------+-------------------+\n";

                bool mealFound = false;
                while (searchRes->next()) {
                    mealFound = true;
                    cout << "                                                                     | " << setw(6) << searchRes->getInt("MealID")
                        << " | " << setw(24) << searchRes->getString("MealName")
                        << " | " << setw(17) << fixed << setprecision(2) << searchRes->getDouble("MealPrice") << " |\n";
                }

                cout << "                                                                     +-------+--------------------------+-------------------+\n";

                if (!mealFound) {
                    cout << "\033[31m\n                                                                     No meal found with the specified name.\033[0m\n";
                }

                delete searchRes;
                delete searchPstmt;

                system("pause");
                continue; // Restart the loop
            }
            catch (sql::SQLException& e) {
                cerr << "\033[31mSQL Error (Search Meal): " << e.what() << "\033[0m\n";
                system("pause");
                return;
            }
        }

        if (choice == 1) { // View All Meals with Pagination
            int minID = 0; // Start from the smallest Meal ID
            int pageSize = 10; // Number of meals per page

            try {
                while (true) {
                    system("cls");
                    cout << "\n                                                                     ***************************************************************\n";
                    cout << "                                                                     *                          VIEW MEALS                         *\n";
                    cout << "                                                                     ***************************************************************\n";

                    sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                        "SELECT * FROM meals WHERE MealID >= ? ORDER BY MealID ASC LIMIT ?");
                    pstmt->setInt(1, minID);
                    pstmt->setInt(2, pageSize);
                    sql::ResultSet* res = pstmt->executeQuery();

                    cout << "\n                                                                     +-------+--------------------------+-------------------+\n";
                    cout << "                                                                     | MealID| Meal Name                | Meal Price (RM)   |\n";
                    cout << "                                                                     +-------+--------------------------+-------------------+\n";

                    bool hasMore = false;
                    int lastID = minID;
                    while (res->next()) {
                        lastID = res->getInt("MealID");
                        cout << "                                                                     | " << setw(6) << res->getInt("MealID")
                            << " | " << setw(24) << res->getString("MealName")
                            << " | " << setw(17) << fixed << setprecision(2) << res->getDouble("MealPrice") << " |\n";

                        hasMore = true;
                    }

                    cout << "                                                                     +-------+--------------------------+-------------------+\n";

                    delete res;
                    delete pstmt;

                    cout << "\n                                                                     Navigation:\n";
                    if (minID > 0) {
                        cout << "                                                                     1. Previous Page\n";
                    }
                    if (hasMore) {
                        cout << "                                                                     2. Next Page\n";
                    }
                    cout << "                                                                     3. Return to Main Menu\n";
                    cout << "                                                                     Enter your choice: ";

                    string navInput;
                    cin >> navInput;

                    if (navInput.find_first_not_of("0123456789") != string::npos || navInput.empty()) {
                        cout << "\033[31m\n                                                                     Invalid input. Please enter a valid number.\033[0m\n";
                        system("pause");
                        continue;
                    }

                    int navChoice = stoi(navInput);

                    if (navChoice == 3) {
                        break; // Exit the loop and return to Main Menu
                    }
                    if (navChoice == 1 && minID > 0) {
                        minID -= pageSize;
                        if (minID < 0) minID = 0; // Prevent negative ID
                    }
                    else if (navChoice == 2 && hasMore) {
                        minID = lastID; // Move to the next page
                    }
                    else {
                        cout << "\033[31m\n                                                                     Invalid choice. Please try again.\033[0m\n";
                        system("pause");
                    }
                }
            }
            catch (sql::SQLException& e) {
                cerr << "\033[31mSQL Error (View All Meals): " << e.what() << "\033[0m\n";
                system("pause");
                return;
            }
        }

        cout << "\033[31m\n                                                                     Invalid choice. Please try again.\033[0m\n";
        system("pause");
    }
}

void addMeal() {
    system("cls");
    string mealName;
    double mealPrice;
    int mealID;

    cout << "\n\n\n\n\n\n\n\n\n";
    cout << "                                                                     ***************************************************************\n";
    cout << "                                                                     *                          ADD NEW MEAL                       *\n";
    cout << "                                                                     ***************************************************************\n";

    cout << "\n                                                                     Enter Meal ID: ";
    cin >> mealID;
    cin.ignore(); // Clear the input buffer
    cout << "                                                                     Enter Meal Name: ";
    getline(cin, mealName);
    cout << "                                                                     Enter Meal Price (RM): ";
    cin >> mealPrice;

    try {
        sql::PreparedStatement* pstmt = globalCon->prepareStatement(
            "INSERT INTO meals (MealID, MealName, MealPrice) VALUES (?, ?, ?)");
        pstmt->setInt(1, mealID);
        pstmt->setString(2, mealName);
        pstmt->setDouble(3, mealPrice);

        pstmt->executeUpdate();
        delete pstmt;

        cout << "\033[32m\n                                                                     Meal added successfully!\033[0m\n";
    }
    catch (sql::SQLException& e) {
        cerr << "\033[31mSQL Error (Add Meal): " << e.what() << "\033[0m\n";
    }

    system("pause");
}

void updateMeal() {
    system("cls");
    int mealID;
    string newMealName;
    double newMealPrice;

    cout << "\n\n\n\n\n\n\n\n\n";
    cout << "                                                                     ***************************************************************\n";
    cout << "                                                                     *                          UPDATE MEAL                        *\n";
    cout << "                                                                     ***************************************************************\n";

    // Display the meals table
    try {
        sql::PreparedStatement* pstmt = globalCon->prepareStatement("SELECT * FROM meals");
        sql::ResultSet* res = pstmt->executeQuery();

        cout << "\n                                                                     +-------+--------------------------+-------------------+\n";
        cout << "                                                                     | MealID| Meal Name                | Meal Price (RM)   |\n";
        cout << "                                                                     +-------+--------------------------+-------------------+\n";

        while (res->next()) {
            cout << "                                                                     | " << setw(6) << res->getInt("MealID")
                << " | " << setw(24) << res->getString("MealName")
                << " | " << setw(17) << fixed << setprecision(2) << res->getDouble("MealPrice") << " |\n";
        }

        cout << "                                                                     +-------+--------------------------+-------------------+\n";

        delete res;
        delete pstmt;
    }
    catch (sql::SQLException& e) {
        cerr << "\033[31mSQL Error (View Meals for Update): " << e.what() << "\033[0m\n";
        system("pause");
        return;
    }

    cout << "\n                                                                     Enter Meal ID to update: ";
    cin >> mealID;

    // Confirm new details for the meal
    cin.ignore();
    cout << "                                                                     Enter new Meal Name: ";
    getline(cin, newMealName);
    cout << "                                                                     Enter new Meal Price (RM): ";
    cin >> newMealPrice;

    // Update the meal in the database
    try {
        sql::PreparedStatement* pstmt = globalCon->prepareStatement(
            "UPDATE meals SET MealName = ?, MealPrice = ? WHERE MealID = ?");
        pstmt->setString(1, newMealName);
        pstmt->setDouble(2, newMealPrice);
        pstmt->setInt(3, mealID);

        int rowsAffected = pstmt->executeUpdate();
        delete pstmt;

        if (rowsAffected > 0) {
            cout << "\033[32m\n                                                                     Meal updated successfully!\033[0m\n";
        }
        else {
            cout << "\033[31m\n                                                                     Meal ID not found. No meal updated.\033[0m\n";
        }
    }
    catch (sql::SQLException& e) {
        cerr << "\033[31mSQL Error (Update Meal): " << e.what() << "\033[0m\n";
    }

    system("pause");
}

//booking manage
void manageBookings() {
    int choice;
    while (true) {
        system("cls");
        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                      MANAGE BOOKINGS                         *\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     1. View All Bookings\n";
        cout << "                                                                     2. Add Booking\n";
        cout << "                                                                     3. Update Booking\n";
        cout << "                                                                     4. Return to Admin Page\n";
        cout << "\n                                                                     Enter your choice: ";

        string input;
        cin >> input;

        if (input.length() != 1 || !isdigit(input[0])) {
            cout << "\033[31mInvalid input. Please enter a number between 1 and 5.\033[0m\n";
            system("pause");
            continue;
        }

        choice = stoi(input);

        switch (choice) {
        case 1:
            viewAllBookings();
            break;
        case 2:
            addNewBooking();
            break;
        case 3:
            updateBooking();
            break;
        case 4:
            return;
        default:
            cout << "\033[31mInvalid choice. Please try again.\033[0m\n";
            system("pause");
        }
    }
}

void viewAllBookings() {
    while (true) {
        system("cls");
        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                        VIEW ALL BOOKINGS                     *\n";
        cout << "                                                                     ***************************************************************\n";

        try {
            cout << "\n                                                                     Options:\n";
            cout << "                                                                     1. View All Bookings\n";
            cout << "                                                                     2. Search Booking by ID\n";
            cout << "                                                                     3. Return to Main Menu\n";
            cout << "                                                                     Enter your choice: ";

            string input;
            cin >> input;

            if (input.find_first_not_of("0123456789") != string::npos || input.empty()) {
                cout << "\033[31m\n                                                                     Invalid input. Please enter a number (1, 2, or 3).\033[0m\n";
                system("pause");
                continue;
            }

            int choice = stoi(input);

            if (choice == 3) {
                return; // Go back to the main menu
            }

            if (choice == 2) {  // Search Booking by ID
                int searchBookingID;
                cout << "\n                                                                     Enter Booking ID to search: ";
                cin >> searchBookingID;

                if (cin.fail()) {
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    cout << "\033[31m\n                                                                     Invalid input. Please enter a valid Booking ID.\033[0m\n";
                    system("pause");
                    continue;
                }

                sql::PreparedStatement* searchPstmt = globalCon->prepareStatement(
                    "SELECT * FROM booking WHERE BookingID = ?");
                searchPstmt->setInt(1, searchBookingID);
                sql::ResultSet* searchRes = searchPstmt->executeQuery();

                cout << "\n                                                                     +-----------+----------+----------+-------------+------------+---------+------------------+---------------+------------+\n";
                cout << "                                                                     | BookingID | Movie_id | User_id  | Screen Type | BookingDate| TimeSlot| Seats Booked     | Ticket Types  | Total Price|\n";
                cout << "                                                                     +-----------+----------+----------+-------------+------------+---------+------------------+---------------+------------+\n";

                bool bookingFound = false;
                while (searchRes->next()) {
                    bookingFound = true;
                    cout << "                                                                     | " << setw(9) << searchRes->getInt("BookingID")
                        << " | " << setw(8) << searchRes->getInt("Movie_id")
                        << " | " << setw(8) << searchRes->getInt("User_id")
                        << " | " << setw(11) << searchRes->getString("ScreenType")
                        << " | " << setw(10) << searchRes->getString("BookingDate")
                        << " | " << setw(7) << searchRes->getString("TimeSlot")
                        << " | " << setw(16) << searchRes->getString("SeatsBooked")
                        << " | " << setw(13) << searchRes->getString("TicketTypes")
                        << " | " << setw(10) << fixed << setprecision(2) << searchRes->getDouble("TotalPrice") << " |\n";
                }

                cout << "                                                                     +-----------+----------+----------+-------------+------------+---------+------------------+---------------+------------+\n";

                if (!bookingFound) {
                    cout << "\033[31m\n                                                                     No booking found with the specified Booking ID.\033[0m\n";
                }

                delete searchRes;
                delete searchPstmt;

                system("pause");
                continue; // Restart the loop
            }

            if (choice == 1) {  // View All Bookings with Pagination
                int minID = 0; // Start from the smallest Booking ID
                int pageSize = 10; // Number of bookings per page

                try {
                    while (true) {
                        system("cls");
                        cout << "\n                                                                   ***************************************************************\n";
                        cout << "                                                                     *                        VIEW ALL BOOKINGS                     *\n";
                        cout << "                                                                     ***************************************************************\n";

                        sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                            "SELECT * FROM booking WHERE BookingID >= ? ORDER BY BookingID ASC LIMIT ?");
                        pstmt->setInt(1, minID);
                        pstmt->setInt(2, pageSize);
                        sql::ResultSet* res = pstmt->executeQuery();

                        cout << "\n                                                                     +-----------+----------+----------+-------------+------------+---------+------------------+---------------+------------+\n";
                        cout << "                                                                     | BookingID | Movie_id | User_id  | Screen Type | BookingDate| TimeSlot| Seats Booked     | Ticket Types  | Total Price|\n";
                        cout << "                                                                     +-----------+----------+----------+-------------+------------+---------+------------------+---------------+------------+\n";

                        bool hasMore = false;
                        int lastID = minID;
                        while (res->next()) {
                            lastID = res->getInt("BookingID");
                            cout << "                                                                     | " << setw(9) << res->getInt("BookingID")
                                << " | " << setw(8) << res->getInt("Movie_id")
                                << " | " << setw(8) << res->getInt("User_id")
                                << " | " << setw(11) << res->getString("ScreenType")
                                << " | " << setw(10) << res->getString("BookingDate")
                                << " | " << setw(7) << res->getString("TimeSlot")
                                << " | " << setw(16) << res->getString("SeatsBooked")
                                << " | " << setw(13) << res->getString("TicketTypes")
                                << " | " << setw(10) << fixed << setprecision(2) << res->getDouble("TotalPrice") << " |\n";

                            hasMore = true;
                        }

                        cout << "                                                                     +-----------+----------+----------+-------------+------------+---------+------------------+---------------+------------+\n";

                        delete res;
                        delete pstmt;

                        cout << "\n                                                                     Navigation:\n";
                        if (minID > 0) {
                            cout << "                                                                     1. Previous Page\n";
                        }
                        if (hasMore) {
                            cout << "                                                                     2. Next Page\n";
                        }
                        cout << "                                                                     3. Return to Main Menu\n";
                        cout << "                                                                     Enter your choice: ";

                        string navInput;
                        cin >> navInput;

                        if (navInput.find_first_not_of("0123456789") != string::npos || navInput.empty()) {
                            cout << "\033[31m\n                                                                     Invalid input. Please enter a valid number.\033[0m\n";
                            system("pause");
                            continue;
                        }

                        int navChoice = stoi(navInput);

                        if (navChoice == 3) {
                            break; // Exit the loop and return to Main Menu
                        }
                        if (navChoice == 1 && minID > 0) {
                            minID -= pageSize;
                            if (minID < 0) minID = 0; // Prevent negative ID
                        }
                        else if (navChoice == 2 && hasMore) {
                            minID = lastID; // Move to the next page
                        }
                        else {
                            cout << "\033[31m\n                                                                     Invalid choice. Please try again.\033[0m\n";
                            system("pause");
                        }
                    }
                }
                catch (sql::SQLException& e) {
                    cerr << "\033[31mSQL Error (View All Bookings): " << e.what() << "\033[0m\n";
                    system("pause");
                    return;
                }
            }
        }
        catch (sql::SQLException& e) {
            cerr << "\033[31mSQL Error (View All Bookings): " << e.what() << "\033[0m\n";
            system("pause");
            return;
        }
    }
}

void addNewBooking() {
    int minID = 0; // Start from the smallest Booking ID
    int pageSize = 10; // Number of bookings to display per page

    while (true) {
        system("cls");
        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                          ADD BOOKING                         *\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     Note: Enter '0' at any input prompt to return to the previous page.\n\n";

        try {
            sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                "SELECT * FROM booking WHERE BookingID > ? ORDER BY BookingID ASC LIMIT ?");
            pstmt->setInt(1, minID);
            pstmt->setInt(2, pageSize);
            sql::ResultSet* res = pstmt->executeQuery();

            cout << "\n                                                                     +-----------+----------+----------+-------------+------------+---------+------------------+---------------+------------+\n";
            cout << "                                                                     | BookingID | Movie ID | User ID  | Screen Type | BookingDate| TimeSlot| Seats Booked     | Ticket Types  | Total Price|\n";
            cout << "                                                                     +-----------+----------+----------+-------------+------------+---------+------------------+---------------+------------+\n";

            bool hasMore = false;
            int lastID = minID;
            while (res->next()) {
                lastID = res->getInt("BookingID");
                cout << "                                                                     | " << setw(9) << res->getInt("BookingID")
                    << " | " << setw(8) << res->getInt("Movie_id")
                    << " | " << setw(8) << res->getInt("User_id")
                    << " | " << setw(11) << res->getString("ScreenType")
                    << " | " << setw(10) << res->getString("BookingDate")
                    << " | " << setw(7) << res->getString("TimeSlot")
                    << " | " << setw(16) << res->getString("SeatsBooked")
                    << " | " << setw(13) << res->getString("TicketTypes")
                    << " | " << setw(10) << fixed << setprecision(2) << res->getDouble("TotalPrice") << " |\n";

                hasMore = true;
            }

            cout << "                                                                     +-----------+----------+----------+-------------+------------+---------+------------------+---------------+------------+\n";

            delete res;
            delete pstmt;

            // Navigation options
            cout << "\n                                                                     Options:\n";
            if (minID > 0) {
                cout << "                                                                     1. Previous Page\n";
            }
            if (hasMore) {
                cout << "                                                                     2. Next Page\n";
            }
            cout << "                                                                     3. Add New Booking\n";
            cout << "                                                                     4. Return to Booking Management\n";
            cout << "                                                                     Enter your choice: ";

            string input;
            cin >> input;

            if (input.length() != 1 || !isdigit(input[0])) {
                cout << "\033[31m\n                                                                     Invalid input. Please enter a valid number.\033[0m\n";
                system("pause");
                continue;
            }

            int choice = stoi(input);

            if (choice == 4) {
                return; // Go back to the booking management page
            }
            if (choice == 1 && minID > 0) {
                minID -= pageSize;
                if (minID < 0) minID = 0; // Prevent negative ID
            }
            else if (choice == 2 && hasMore) {
                minID = lastID; // Move to the next page
            }
            else if (choice == 3) {
                int movieID, userID;
                string screenType, bookingDate, timeSlot, seatsBooked, ticketTypes;
                double totalPrice;

                // **Movie ID Input**
                while (true) {
                    cout << "\n                                                                     Enter Movie ID: ";
                    cin >> movieID;
                    if (movieID == 0) return; // Go back

                    // Validate Movie ID
                    try {
                        sql::PreparedStatement* pstmt = globalCon->prepareStatement("SELECT COUNT(*) AS count FROM movies WHERE Movie_id = ?");
                        pstmt->setInt(1, movieID);
                        sql::ResultSet* res = pstmt->executeQuery();
                        if (res->next() && res->getInt("count") == 0) {
                            cout << "\033[31m                                                                     Invalid Movie ID. No such movie exists.\033[0m\n";
                            delete res;
                            delete pstmt;
                            system("pause");
                            continue;
                        }
                        delete res;
                        delete pstmt;
                        break; // Valid movie ID
                    }
                    catch (sql::SQLException& e) {
                        cerr << "SQL Error (Validate Movie ID): " << e.what() << endl;
                        system("pause");
                    }
                }

                // **User ID Input**
                while (true) {
                    cout << "                                                                     Enter User ID: ";
                    cin >> userID;
                    if (userID == 0) return; // Go back

                    // Validate User ID
                    try {
                        sql::PreparedStatement* pstmt = globalCon->prepareStatement("SELECT COUNT(*) AS count FROM users WHERE User_id = ?");
                        pstmt->setInt(1, userID);
                        sql::ResultSet* res = pstmt->executeQuery();
                        if (res->next() && res->getInt("count") == 0) {
                            cout << "\033[31m                                                                     Invalid User ID. No such user exists.\033[0m\n";
                            delete res;
                            delete pstmt;
                            system("pause");
                            continue;
                        }
                        delete res;
                        delete pstmt;
                        break; // Valid user ID
                    }
                    catch (sql::SQLException& e) {
                        cerr << "SQL Error (Validate User ID): " << e.what() << endl;
                        system("pause");
                    }
                }

                cin.ignore();
                cout << "                                                                     Enter Screen Type: ";
                getline(cin, screenType);
                cout << "                                                                     Enter Booking Date (YYYY-MM-DD): ";
                getline(cin, bookingDate);
                cout << "                                                                     Enter Time Slot: ";
                getline(cin, timeSlot);
                cout << "                                                                     Enter Seats Booked (comma-separated): ";
                getline(cin, seatsBooked);
                cout << "                                                                     Enter Ticket Types: ";
                getline(cin, ticketTypes);

                // **Total Price Input**
                while (true) {
                    cout << "                                                                     Enter Total Price: ";
                    cin >> totalPrice;
                    if (cin.fail()) {
                        cin.clear();
                        cin.ignore(numeric_limits<streamsize>::max(), '\n');
                        cout << "\033[31m\n                                                                     Invalid input. Please enter a valid numeric value.\033[0m\n";
                        system("pause");
                        continue;
                    }
                    break; // Valid price input
                }

                // Add new booking to the database
                try {
                    sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                        "INSERT INTO booking (Movie_id, User_id, ScreenType, BookingDate, TimeSlot, SeatsBooked, TicketTypes, TotalPrice) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
                    pstmt->setInt(1, movieID);
                    pstmt->setInt(2, userID);
                    pstmt->setString(3, screenType);
                    pstmt->setString(4, bookingDate);
                    pstmt->setString(5, timeSlot);
                    pstmt->setString(6, seatsBooked);
                    pstmt->setString(7, ticketTypes);
                    pstmt->setDouble(8, totalPrice);
                    pstmt->executeUpdate();
                    delete pstmt;

                    cout << "\033[32m\n                                                                     Booking added successfully!\033[0m\n";
                }
                catch (sql::SQLException& e) {
                    cerr << "SQL Error (Add Booking): " << e.what() << endl;
                }

                // Prompt admin to add another booking or go back
                cout << "\n                                                                     Would you like to add another booking? (y/n): ";
                char addMore;
                cin >> addMore;
                if (tolower(addMore) != 'y') {
                    return; // Go back to booking management
                }
            }
            else {
                cout << "\033[31m\n                                                                     Invalid choice. Please try again.\033[0m\n";
                system("pause");
            }
        }
        catch (sql::SQLException& e) {
            cerr << "SQL Error (View Bookings for Add): " << e.what() << endl;
            system("pause");
            return;
        }
    }
}

void updateBooking() {
    int minID = 0; // Start from the smallest Booking ID
    int pageSize = 10; // Number of bookings to display per page

    while (true) {
        system("cls");
        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                     ***************************************************************\n";
        cout << "                                                                     *                         UPDATE BOOKING                       *\n";
        cout << "                                                                     ***************************************************************\n";

        try {
            sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                "SELECT * FROM booking WHERE BookingID > ? ORDER BY BookingID ASC LIMIT ?");
            pstmt->setInt(1, minID);
            pstmt->setInt(2, pageSize);
            sql::ResultSet* res = pstmt->executeQuery();

            cout << "\n                                                                     +-----------+----------+----------+-------------+------------+---------+------------------+---------------+------------+\n";
            cout << "                                                                     | BookingID | Movie_id | User_id  | Screen Type | BookingDate| TimeSlot| Seats Booked     | Ticket Types  | Total Price|\n";
            cout << "                                                                     +-----------+----------+----------+-------------+------------+---------+------------------+---------------+------------+\n";

            bool hasMore = false;
            int lastID = minID;
            while (res->next()) {
                lastID = res->getInt("BookingID");
                cout << "                                                                     | " << setw(9) << res->getInt("BookingID")
                    << " | " << setw(8) << res->getInt("Movie_id")
                    << " | " << setw(8) << res->getInt("User_id")
                    << " | " << setw(11) << res->getString("ScreenType")
                    << " | " << setw(10) << res->getString("BookingDate")
                    << " | " << setw(7) << res->getString("TimeSlot")
                    << " | " << setw(16) << res->getString("SeatsBooked")
                    << " | " << setw(13) << res->getString("TicketTypes")
                    << " | " << setw(10) << fixed << setprecision(2) << res->getDouble("TotalPrice") << " |\n";

                hasMore = true;
            }

            cout << "                                                                     +-----------+----------+----------+-------------+------------+---------+------------------+---------------+------------+\n";

            delete res;
            delete pstmt;

            // Navigation options
            cout << "\n                                                                     Options:\n";
            if (minID > 0) {
                cout << "                                                                     1. Previous Page\n";
            }
            if (hasMore) {
                cout << "                                                                     2. Next Page\n";
            }
            cout << "                                                                     3. Enter Booking ID to Update\n";
            cout << "                                                                     4. Return to Booking Management\n";
            cout << "                                                                     Enter your choice: ";

            string input;
            cin >> input;

            if (input.length() != 1 || !isdigit(input[0])) {
                cout << "\033[31m\n                                                                     Invalid input. Please enter a valid number.\033[0m\n";
                system("pause");
                continue;
            }

            int choice = stoi(input);

            if (choice == 4) {
                return; // Go back to the booking management page
            }
            if (choice == 1 && minID > 0) {
                minID -= pageSize;
                if (minID < 0) minID = 0; // Prevent negative ID
            }
            else if (choice == 2 && hasMore) {
                minID = lastID; // Move to the next page
            }
            else if (choice == 3) {
                int bookingID;
                cout << "\n                                                                     Enter the Booking ID to update: ";
                cin >> bookingID;

                if (cin.fail()) {
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    cout << "\033[31m\n                                                                     Invalid input. Please enter a valid Booking ID.\033[0m\n";
                    system("pause");
                    continue;
                }

                // Check if the booking ID exists
                bool bookingExists = false;
                try {
                    sql::PreparedStatement* pstmt = globalCon->prepareStatement("SELECT COUNT(*) AS Count FROM booking WHERE BookingID = ?");
                    pstmt->setInt(1, bookingID);
                    sql::ResultSet* res = pstmt->executeQuery();
                    if (res->next() && res->getInt("Count") > 0) {
                        bookingExists = true;
                    }
                    delete res;
                    delete pstmt;
                }
                catch (sql::SQLException& e) {
                    cerr << "SQL Error (Check Booking ID): " << e.what() << endl;
                    system("pause");
                    continue;
                }

                if (!bookingExists) {
                    cout << "\033[31m\n                                                                     Booking ID not found. Please try again.\033[0m\n";
                    system("pause");
                    continue;
                }

                // Display update options
                while (true) {
                    system("cls");
                    cout << "\n                                                                     Options to Update:\n";
                    cout << "                                                                     1. Movie ID\n";
                    cout << "                                                                     2. User ID\n";
                    cout << "                                                                     3. Screen Type\n";
                    cout << "                                                                     4. Booking Date\n";
                    cout << "                                                                     5. Time Slot\n";
                    cout << "                                                                     6. Seats Booked\n";
                    cout << "                                                                     7. Ticket Types\n";
                    cout << "                                                                     8. Total Price\n";
                    cout << "                                                                     9. Back to Booking List\n";
                    cout << "                                                                     Enter your choice: ";

                    string updateInput;
                    cin >> updateInput;

                    if (updateInput.length() != 1 || !isdigit(updateInput[0])) {
                        cout << "\033[31m\n                                                                     Invalid input. Please enter a number between 1 and 9.\033[0m\n";
                        system("pause");
                        continue;
                    }

                    int updateChoice = stoi(updateInput);
                    if (updateChoice == 9) {
                        break; // Return to booking list
                    }

                    string fieldName, newValue;
                    switch (updateChoice) {
                    case 1: fieldName = "Movie_id"; break;
                    case 2: fieldName = "User_id"; break;
                    case 3: fieldName = "ScreenType"; break;
                    case 4: fieldName = "BookingDate"; break;
                    case 5: fieldName = "TimeSlot"; break;
                    case 6: fieldName = "SeatsBooked"; break;
                    case 7: fieldName = "TicketTypes"; break;
                    case 8: fieldName = "TotalPrice"; break;
                    default:
                        cout << "\033[31m\n                                                                     Invalid choice. Please try again.\033[0m\n";
                        system("pause");
                        continue;
                    }

                    cin.ignore();
                    cout << "\n                                                                     Enter the new value for " << fieldName << ": ";
                    getline(cin, newValue);

                    // Validate input for numeric fields
                    if (fieldName == "TotalPrice") {
                        try {
                            stod(newValue); // Ensure numeric value
                        }
                        catch (...) {
                            cout << "\033[31m\n                                                                     Invalid price. Please enter a numeric value.\033[0m\n";
                            system("pause");
                            continue;
                        }
                    }

                    // Update the booking in the database
                    try {
                        string query = "UPDATE booking SET " + fieldName + " = ? WHERE BookingID = ?";
                        sql::PreparedStatement* pstmt = globalCon->prepareStatement(query);
                        if (updateChoice == 8) {
                            pstmt->setDouble(1, stod(newValue)); // TotalPrice is a double
                        }
                        else {
                            pstmt->setString(1, newValue);
                        }
                        pstmt->setInt(2, bookingID);
                        pstmt->executeUpdate();
                        delete pstmt;

                        cout << "\033[32m\n                                                                     Booking updated successfully!\033[0m\n";
                    }
                    catch (sql::SQLException& e) {
                        cerr << "SQL Error (Update Booking): " << e.what() << endl;
                    }

                    system("pause");
                    break;
                }
            }
            else {
                cout << "\033[31m\n                                                                     Invalid choice. Please try again.\033[0m\n";
                system("pause");
            }
        }
        catch (sql::SQLException& e) {
            cerr << "SQL Error (View Bookings for Update): " << e.what() << endl;
            system("pause");
            return;
        }
    }
}

//report manage
void displayReportPage() {
    while (true) {
        system("cls");

        cout << "\n\n\n";
        cout << "                                      ***************************************************************\n";
        cout << "                                      *                            MANAGE REPORT                     *\n";
        cout << "                                      ***************************************************************\n";
        cout << "\n                                      Please choose an option:\n";
        cout << "                                      1. View Full Report\n";
        cout << "                                      2. Profit Margin Analysis\n";
        cout << "                                      3. Graph Sales Summary\n";
        cout << "                                      4. Bookings by Destination\n";
        cout << "                                      0. Exit\n";
        cout << "                                      ***************************************************************\n";

        int choice;
        while (true) {
            cout << "                                      Enter your choice: ";
            cin >> choice;

            if (cin.fail()) {
                cout << "\033[31m                                      Invalid input! Please enter a number between 0 and 4.\033[0m\n";
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                system("pause");
                continue;
            }

            if (choice < 0 || choice > 4) {
                cout << "\033[31m                                      Invalid choice. Please enter a number between 0 and 4.\033[0m\n";
                system("pause");
                continue;
            }

            break;
        }

        switch (choice) {
        case 1:
            viewFullReport();
            break;
        case 2:
            displayProfitMarginAnalysis();
            break;
        case 3:
            displayGraphSummary();
            break;
        case 4:
            displayBookingsByDestination();
            break;
        case 0:
            return;
        default:
            cout << "\033[31mInvalid choice. Please try again.\033[0m\n";
            system("pause");
        }
    }


}

void viewFullReport() {
    system("cls");

    cout << "\n\n\n";
    cout << "                                      ***************************************************************\n";
    cout << "                                      *                        SALES REPORT                           *\n";
    cout << "                                      ***************************************************************\n";

    string minDate, maxDate;
    try {
        sql::PreparedStatement* pstmtDateRange = globalCon->prepareStatement(
            "SELECT MIN(BookingDate) AS MinDate, MAX(BookingDate) AS MaxDate FROM booking;");
        sql::ResultSet* resDateRange = pstmtDateRange->executeQuery();

        if (resDateRange->next()) {
            minDate = resDateRange->getString("MinDate");
            maxDate = resDateRange->getString("MaxDate");
        }
        delete resDateRange;
        delete pstmtDateRange;

        cout << "\n                                      Available Booking Data Range: \033[32m" << minDate << "\033[0m to \033[32m" << maxDate << "\033[0m\n";
        cout << "                                      ***************************************************************\n";
    }
    catch (sql::SQLException& e) {
        cerr << "\033[31mSQL Error (Fetch Min/Max Dates): " << e.what() << "\033[0m\n";
    }

    try {
        sql::PreparedStatement* pstmtReport = globalCon->prepareStatement(
            "SELECT YEAR(BookingDate) AS Year, MONTH(BookingDate) AS Month, SUM(TotalPrice) AS MonthlyTotal "
            "FROM booking "
            "GROUP BY YEAR(BookingDate), MONTH(BookingDate) "
            "ORDER BY Year, Month;");
        sql::ResultSet* resReport = pstmtReport->executeQuery();

        cout << "\n                                      All Years and Months Sales Report:\n";
        cout << "                                      ***************************************************************\n";
        cout << "                                      | Year      | Month     | Total Sales (RM)                  |\n";
        cout << "                                      ***************************************************************\n";

        double totalAllYears = 0.0;
        while (resReport->next()) {
            int year = resReport->getInt("Year");
            int month = resReport->getInt("Month");
            double monthlyTotal = resReport->getDouble("MonthlyTotal");
            totalAllYears += monthlyTotal;

            cout << "                                      | " << setw(9) << left << year
                << "| " << setw(10) << left << month
                << "| RM " << setw(33) << fixed << setprecision(2) << monthlyTotal << " |\n";
        }
        cout << "                                      ***************************************************************\n";

        delete resReport;
        delete pstmtReport;
    }
    catch (sql::SQLException& e) {
        cerr << "\033[31mSQL Error (Sales Report): " << e.what() << "\033[0m\n";
    }

    try {
        sql::PreparedStatement* pstmtSummary = globalCon->prepareStatement(
            "SELECT SUM(TotalPrice) AS TotalSales, "
            "       (SUM(TotalPrice) * 0.7) AS EstimatedCosts, "
            "       ((SUM(TotalPrice) - (SUM(TotalPrice) * 0.7)) / SUM(TotalPrice) * 100) AS ProfitMargin "
            "FROM booking;");
        sql::ResultSet* resSummary = pstmtSummary->executeQuery();

        double totalSales = 0.0, estimatedCosts = 0.0, profitMargin = 0.0;

        if (resSummary->next()) {
            totalSales = resSummary->getDouble("TotalSales");
            estimatedCosts = resSummary->getDouble("EstimatedCosts");
            profitMargin = resSummary->getDouble("ProfitMargin");
        }

        delete resSummary;
        delete pstmtSummary;

        cout << "\n                                    1.Total Monthly Sales: RM " << fixed << setprecision(2) << totalSales << "\n";
        cout << "                                      2.Total Inventory Value: RM " << fixed << setprecision(2) << estimatedCosts << "\n";
        cout << "                                      3.Profit Margin: " << fixed << setprecision(2) << profitMargin << "%\n";
        cout << "                                      ***************************************************************\n";
    }
    catch (sql::SQLException& e) {
        cerr << "\033[31mSQL Error (Summary Report): " << e.what() << "\033[0m\n";
    }

    cout << "\n                                      Press any key to return to the report menu...\n";
    system("pause");
}

void displayProfitMarginAnalysis() {
    while (true) {
        system("cls");

        cout << "\n\n";
        cout << "                                      *********************************************************************\n";
        cout << "                                      *                     PROFIT MARGIN ANALYSIS                       *\n";
        cout << "                                      *********************************************************************\n";

        cout << "\n                                      Enter the range for analysis:\n";
        string startDate, endDate;
        cout << "                                      Start Date (YYYY-MM): ";
        cin >> startDate;
        cout << "                                      End Date (YYYY-MM): ";
        cin >> endDate;

        // Validate input format
        if (startDate.size() != 7 || endDate.size() != 7 || startDate[4] != '-' || endDate[4] != '-') {
            cout << "\033[31mInvalid date format! Please use YYYY-MM.\033[0m\n";
            system("pause");
            continue;
        }

        try {
            // Check if the entered range exists in the database
            sql::PreparedStatement* checkPstmt = globalCon->prepareStatement(
                "SELECT COUNT(*) AS Count FROM booking "
                "WHERE CONCAT(YEAR(BookingDate), '-', LPAD(MONTH(BookingDate), 2, '0')) BETWEEN ? AND ?;");
            checkPstmt->setString(1, startDate);
            checkPstmt->setString(2, endDate);
            sql::ResultSet* checkRes = checkPstmt->executeQuery();

            int recordCount = 0;
            if (checkRes->next()) {
                recordCount = checkRes->getInt("Count");
            }

            delete checkRes;
            delete checkPstmt;

            if (recordCount == 0) {
                cout << "\033[31mNo data available for the selected range. Please enter a valid range.\033[0m\n";
                system("pause");
                continue;
            }

            sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                "SELECT YEAR(BookingDate) AS Year, MONTH(BookingDate) AS Month, SUM(TotalPrice) AS TotalSales "
                "FROM booking "
                "WHERE CONCAT(YEAR(BookingDate), '-', LPAD(MONTH(BookingDate), 2, '0')) BETWEEN ? AND ? "
                "GROUP BY YEAR(BookingDate), MONTH(BookingDate) "
                "ORDER BY Year, Month;");
            pstmt->setString(1, startDate);
            pstmt->setString(2, endDate);
            sql::ResultSet* res = pstmt->executeQuery();

            vector<pair<string, double>> monthlySales;
            double totalCosts = 0.0;

            while (res->next()) {
                int year = res->getInt("Year");
                int month = res->getInt("Month");
                double totalSales = res->getDouble("TotalSales");

                string date = to_string(year) + "-" + (month < 10 ? "0" : "") + to_string(month);
                monthlySales.push_back({ date, totalSales });

                totalCosts += totalSales * 0.7;
            }

            delete res;
            delete pstmt;

            if (monthlySales.empty()) {
                cout << "\033[31mNo data available for the selected range.\033[0m\n";
                system("pause");
                continue;
            }

            cout << "\n                                      Profit Margin Analysis:\n";
            cout << "                                      *********************************************************************\n";
            cout << "                                      |     Period Range     |   Total Profit (RM)   |     Change (%)      |\n";
            cout << "                                      *********************************************************************\n";

            for (size_t i = 0; i < monthlySales.size() - 1; ++i) {
                string currentPeriod = monthlySales[i].first;
                string nextPeriod = monthlySales[i + 1].first;
                double currentSales = monthlySales[i].second;
                double nextSales = monthlySales[i + 1].second;

                double currentCosts = currentSales * 0.7;
                double nextCosts = nextSales * 0.7;

                double currentProfit = currentSales - currentCosts;
                double nextProfit = nextSales - nextCosts;

                double percentageChange = ((nextProfit - currentProfit) / currentProfit) * 100;

                // Color code changes
                string color = (percentageChange > 0) ? "\033[32m" : "\033[31m";

                cout << "                                      | " << setw(18) << left << (currentPeriod + " to " + nextPeriod)
                    << "| RM " << setw(20) << fixed << setprecision(2) << nextProfit
                    << "| " << color << setw(18) << fixed << setprecision(2) << (percentageChange > 0 ? "+" : "") << percentageChange << "%\033[0m |\n";
            }

            cout << "                                      *********************************************************************\n";
        }
        catch (sql::SQLException& e) {
            cerr << "\033[31mSQL Error (Profit Margin Analysis): " << e.what() << "\033[0m\n";
        }

        cout << "\n                                      Press any key to return to the main menu...\n";
        system("pause");
        break;
    }
}

void displayGraphSummary() {
    while (true) {
        system("cls");

        cout << "\n\n\n";
        cout << "                                      ***************************************************************\n";
        cout << "                                      *                        GRAPHICAL SALES SUMMARY                *\n";
        cout << "                                      ***************************************************************\n";

        try {
            sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                "SELECT YEAR(BookingDate) AS Year, SUM(TotalPrice) AS YearlyTotal "
                "FROM booking "
                "GROUP BY YEAR(BookingDate) "
                "ORDER BY Year;");
            sql::ResultSet* res = pstmt->executeQuery();

            cout << "\n                                      Sales Summary by Year:\n";
            cout << "                                      ***************************************************************\n";

            vector<int> availableYears;
            while (res->next()) {
                int year = res->getInt("Year");
                double totalSales = res->getDouble("YearlyTotal");
                availableYears.push_back(year);

                cout << "                                      " << year << " : " << string(totalSales / 1000, '*')
                    << "  (RM " << fixed << setprecision(2) << totalSales << ")\n";
            }
            cout << "                                      ***************************************************************\n";
            delete res;
            delete pstmt;

            int selectedYear;
            while (true) {
                cout << "\n                                      Enter a year to view monthly trends or 0 to go back: ";
                cin >> selectedYear;
                if (cin.fail()) {
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    cout << "\033[31mInvalid input! Please enter a valid year or 0 to go back.\033[0m\n";
                    continue;
                }
                if (selectedYear == 0) {
                    return;
                }
                if (find(availableYears.begin(), availableYears.end(), selectedYear) == availableYears.end()) {
                    cout << "\033[31mInvalid year! Please enter a year from the available list.\033[0m\n";
                    continue;
                }
                break;
            }

            system("cls");
            cout << "\n                                      Monthly Sales for Year: " << selectedYear << "\n";
            cout << "                                      ***************************************************************\n";

            sql::PreparedStatement* pstmtMonth = globalCon->prepareStatement(
                "SELECT MONTH(BookingDate) AS Month, COUNT(*) AS BookingCount, SUM(TotalPrice) AS MonthlyTotal "
                "FROM booking "
                "WHERE YEAR(BookingDate) = ? "
                "GROUP BY MONTH(BookingDate) "
                "ORDER BY Month;");
            pstmtMonth->setInt(1, selectedYear);
            sql::ResultSet* resMonth = pstmtMonth->executeQuery();

            vector<string> months = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
            vector<int> availableMonths;

            while (resMonth->next()) {
                int month = resMonth->getInt("Month");
                int bookingCount = resMonth->getInt("BookingCount");
                double monthlyTotal = resMonth->getDouble("MonthlyTotal");
                availableMonths.push_back(month);

                cout << "                                      " << months[month - 1] << " : " << string(bookingCount, '*')
                    << "  (RM " << fixed << setprecision(2) << monthlyTotal << ")\n";
            }
            cout << "                                      ***************************************************************\n";
            delete resMonth;
            delete pstmtMonth;

            int selectedMonth;
            while (true) {
                cout << "\n                                      for example (1=January, 5=May...etc ";
                cout << "\n                                      Enter a month (1-12) to view detailed bookings or 0 to go back: ";
                cin >> selectedMonth;
                if (cin.fail()) {
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    cout << "\033[31mInvalid input! Please enter a valid month (1-12) or 0 to go back.\033[0m\n";
                    continue;
                }
                if (selectedMonth == 0) {
                    return;
                }
                if (find(availableMonths.begin(), availableMonths.end(), selectedMonth) == availableMonths.end()) {
                    cout << "\033[31mInvalid month! Please select from available months.\033[0m\n";
                    continue;
                }
                break;
            }

            system("cls");
            cout << "\n                                      Detailed Bookings for " << months[selectedMonth - 1] << " " << selectedYear << "\n";
            cout << "                                      *********************************************************************************\n";
            cout << "                                      | Booking ID | User Name       | Movie Name       | Booking Date  |   Price      |\n";
            cout << "                                      **********************************************************************************\n";

            sql::PreparedStatement* pstmtDetails = globalCon->prepareStatement(
                "SELECT booking.BookingID, CONCAT(users.FirstName, ' ', users.LastName) AS UserName, "
                "movies.Title AS MovieName, booking.BookingDate, booking.TotalPrice "
                "FROM booking "
                "JOIN users ON booking.User_id = users.User_id "
                "JOIN movies ON booking.Movie_id = movies.Movie_id "
                "WHERE YEAR(booking.BookingDate) = ? AND MONTH(booking.BookingDate) = ? "
                "ORDER BY booking.BookingDate;");
            pstmtDetails->setInt(1, selectedYear);
            pstmtDetails->setInt(2, selectedMonth);
            sql::ResultSet* resDetails = pstmtDetails->executeQuery();

            while (resDetails->next()) {
                int bookingID = resDetails->getInt("BookingID");
                string userName = resDetails->getString("UserName");
                string movieName = resDetails->getString("MovieName");
                string bookingDate = resDetails->getString("BookingDate");
                double totalPrice = resDetails->getDouble("TotalPrice");

                cout << "                                      | " << setw(10) << left << bookingID
                    << "| " << setw(15) << left << userName
                    << "| " << setw(15) << left << movieName
                    << "| " << setw(13) << left << bookingDate
                    << "| RM " << setw(10) << fixed << setprecision(2) << totalPrice << " |\n";
            }
            cout << "                                      **********************************************************************************\n";
            delete resDetails;
            delete pstmtDetails;

            cout << "\n                                      Press any key to return to the monthly summary...\n";
            system("pause");
        }
        catch (sql::SQLException& e) {
            cerr << "\033[31mSQL Error (Graphical Summary): " << e.what() << "\033[0m\n";
        }
    }
}

void displayBookingsByDestination() {
    while (true) {
        system("cls");

        cout << "\n\n\n";
        cout << "                                      ***************************************************************\n";
        cout << "                                      *                        BOOKINGS BY YEAR                     *\n";
        cout << "                                      ***************************************************************\n";

        try {
            sql::PreparedStatement* pstmtYears = globalCon->prepareStatement(
                "SELECT YEAR(BookingDate) AS Year, COUNT(*) AS BookingCount "
                "FROM booking "
                "GROUP BY Year "
                "ORDER BY Year ASC;"
            );
            sql::ResultSet* resYears = pstmtYears->executeQuery();

            cout << "\n                                      Available Booking Years:\n";
            cout << "                                      ***************************************************************\n";

            vector<int> availableYears;
            while (resYears->next()) {
                int year = resYears->getInt("Year");
                int bookingCount = resYears->getInt("BookingCount");
                availableYears.push_back(year);

                cout << "                                      " << left << setw(10) << year << " | "
                    << "(" << bookingCount << " bookings)\n";
            }
            cout << "                                      ***************************************************************\n";
            delete resYears;
            delete pstmtYears;

            int selectedYear;
            cout << "\n                                      Enter a year to view monthly trends or 0 to go back: ";
            cin >> selectedYear;

            if (cin.fail()) {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cout << "\033[31mInvalid input! Please enter a valid year or 0 to go back.\033[0m\n";
                system("pause");
                return;
            }
            if (selectedYear == 0) {
                return;
            }
            if (find(availableYears.begin(), availableYears.end(), selectedYear) == availableYears.end()) {
                cout << "\033[31mInvalid year! Please select a year from the available list.\033[0m\n";
                system("pause");
                return;
            }

            system("cls");
            cout << "\n                                      Available Booking Months for Year: " << selectedYear << "\n";
            cout << "                                      ***************************************************************\n";

            sql::PreparedStatement* pstmtMonths = globalCon->prepareStatement(
                "SELECT MONTH(BookingDate) AS Month, COUNT(*) AS BookingCount "
                "FROM booking "
                "WHERE YEAR(BookingDate) = ? "
                "GROUP BY Month "
                "ORDER BY Month ASC;"
            );
            pstmtMonths->setInt(1, selectedYear);
            sql::ResultSet* resMonths = pstmtMonths->executeQuery();

            vector<int> availableMonths;
            vector<string> months = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

            while (resMonths->next()) {
                int month = resMonths->getInt("Month");
                int bookingCount = resMonths->getInt("BookingCount");
                availableMonths.push_back(month);

                cout << "                                      " << left << setw(10) << months[month - 1] << " | "
                    << string(bookingCount, '*') << " (" << bookingCount << " bookings)\n";
            }
            cout << "                                      ***************************************************************\n";
            delete resMonths;
            delete pstmtMonths;

            int selectedMonth;
            cout << "\n                                      for example (1=January, 5=May...etc ";
            cout << "\n                                      Enter the month to view booked movies or 0 to go back: ";
            cin >> selectedMonth;

            if (cin.fail()) {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cout << "\033[31mInvalid input! Please enter a valid month or 0 to go back.\033[0m\n";
                system("pause");
                return;
            }
            if (selectedMonth == 0) {
                return;
            }
            if (find(availableMonths.begin(), availableMonths.end(), selectedMonth) == availableMonths.end()) {
                cout << "\033[31mInvalid month! Please select a month from the available list.\033[0m\n";
                system("pause");
                return;
            }

            // Display Movies Booked in the Selected Month
            system("cls");
            cout << "\n                                      Movies Booked in " << months[selectedMonth - 1] << " " << selectedYear << "\n";
            cout << "                                      ***************************************************************\n";
            cout << "                                      | Movie Name                                                  |\n";
            cout << "                                      ***************************************************************\n";

            sql::PreparedStatement* pstmtMovies = globalCon->prepareStatement(
                "SELECT DISTINCT movies.Title AS MovieName "
                "FROM booking "
                "JOIN movies ON booking.Movie_id = movies.Movie_id "
                "WHERE YEAR(booking.BookingDate) = ? AND MONTH(booking.BookingDate) = ? "
                "ORDER BY movies.Title ASC;"
            );
            pstmtMovies->setInt(1, selectedYear);
            pstmtMovies->setInt(2, selectedMonth);
            sql::ResultSet* resMovies = pstmtMovies->executeQuery();

            bool hasData = false;
            while (resMovies->next()) {
                hasData = true;
                string movieName = resMovies->getString("MovieName");
                cout << "                                      | " << setw(55) << left << movieName << " |\n";
            }
            cout << "                                      ***************************************************************\n";

            if (!hasData) {
                cout << "\033[31m                                      No movies were booked in this month.\033[0m\n";
            }

            delete resMovies;
            delete pstmtMovies;

        }
        catch (sql::SQLException& e) {
            cerr << "\033[31mSQL Error (Bookings by Year & Month): " << e.what() << "\033[0m\n";
        }

        cout << "\n                                      Press any key to return to the report menu...\n";
        system("pause");
    }
}

void movieMainPage(int userID, bool isStudent) {
    cout << "Welcome to the Movie Menu!" << endl;
    displayMoviePage(userID, isStudent);
    cout << "Exiting Movie Menu..." << endl;
}

