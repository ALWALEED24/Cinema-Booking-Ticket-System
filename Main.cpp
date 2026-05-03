#include "Database.h"
#include <iostream>
#include <conio.h> // For masking password
#include <regex>   
#include <windows.h> 

using namespace std;

void setConsoleColor(int color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

string getPassword(const string& prompt) {
    string password;
    char ch;

    cout << prompt;
    while ((ch = _getch()) != '\r') { 
        if (ch == '\b') { 
            if (!password.empty()) {
                cout << "\b \b";
                password.pop_back();
            }
        }
        else if (isprint(ch)) { 
            password += ch;
            cout << '*';
        }
    }

    cout << endl;
    return password;
}

bool isValidPhoneNumber(const string& phoneNumber) {
    return phoneNumber.length() == 11 && regex_match(phoneNumber, regex("^[0-9]{11}$"));
}


// Check if email already exists
bool isEmailAlreadyRegistered(const string& email) {
    try {
        sql::PreparedStatement* pstmt = globalCon->prepareStatement(
            "SELECT COUNT(*) FROM users WHERE email = ?");
        pstmt->setString(1, email);

        sql::ResultSet* res = pstmt->executeQuery();
        res->next();
        int count = res->getInt(1);

        delete res;
        delete pstmt;

        return count > 0; 
    }
    catch (sql::SQLException& e) {
        cout << "SQL Error (isEmailAlreadyRegistered): " << e.what() << endl;
        return false;
    }
}

// User registration page
void registerUserPage() {
    string firstName, lastName, phoneNumber, email, userName, password, confirmPassword, matricNumber;
    bool isStudentFlag = false;
    int year, month, day;

    cout << "\n\n\n\n\n\n\n\n\n";
    cout << "                                                                                    *******************************************\n";
    cout << "                                                                                    *                  REGISTER                *\n";
    cout << "                                                                                    *******************************************\n\n";
    cout << "                                                                     Note: Enter '0' at any input prompt to Exit from this page.\n\n";

    cout << "                                                                                        Enter First Name: ";
    getline(cin, firstName);
    if (firstName == "0") return;

    cout << "                                                                                        Enter Last Name: ";
    getline(cin, lastName);
    if (lastName == "0") return;

    // Enter Username with Duplicate Check
    do {
        cout << "                                                                                        Enter Username: ";
        getline(cin, userName);
        if (userName == "0") return;

        try {
            sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                "SELECT COUNT(*) AS UserCount FROM users WHERE UserName = ?");
            pstmt->setString(1, userName);
            sql::ResultSet* res = pstmt->executeQuery();

            if (res->next() && res->getInt("UserCount") > 0) {
                setConsoleColor(12);
                cout << "                                                                                        This username is already taken. Please choose a different one.\n";
                setConsoleColor(7);
            }
            else {
                delete res;
                delete pstmt;
                break;
            }

            delete res;
            delete pstmt;
        }
        catch (sql::SQLException& e) {
            cerr << "\033[31mSQL Error (Check Username):\033[0m " << e.what() << endl;
            system("pause");
            return;
        }
    } while (true);

    // Enter Phone Number with Duplicate Check
    do {
        cout << "                                                                                        Enter Phone Number: ";
        getline(cin, phoneNumber);
        if (phoneNumber == "0") return;

        if (!isValidPhoneNumber(phoneNumber)) {
            setConsoleColor(12);
            cout << "                                                                                        Invalid phone number. It must be 11 digits.\n";
            setConsoleColor(7);
        }
        else {
            try {
                sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                    "SELECT COUNT(*) AS PhoneCount FROM users WHERE PhoneNumber = ?");
                pstmt->setString(1, phoneNumber);
                sql::ResultSet* res = pstmt->executeQuery();

                if (res->next() && res->getInt("PhoneCount") > 0) {
                    setConsoleColor(12);
                    cout << "                                                                                        This phone number is already registered. Please use different one.\n";
                    setConsoleColor(7);
                }
                else {
                    delete res;
                    delete pstmt;
                    break;
                }

                delete res;
                delete pstmt;
            }
            catch (sql::SQLException& e) {
                cerr << "\033[31mSQL Error (Check Phone Number):\033[0m " << e.what() << endl;
                system("pause");
                return;
            }
        }
    } while (true);

    do {
        cout << "                                                                                        Enter Email: ";
        getline(cin, email);
        if (email == "0") return;

        if (isEmailAlreadyRegistered(email)) {
            setConsoleColor(12);
            cout << "                                                                                        This email is already registered. Please try a different one.\n";
            setConsoleColor(7);
        }
        else if (email.length() < 10 || email.substr(email.length() - 10) != "@gmail.com") {
            setConsoleColor(12);
            cout << "                                                                                        Invalid email. Please use a Gmail address (@gmail.com).\n";
            setConsoleColor(7);
        }
        else {
            break;
        }
    } while (true);

    // Enter and Confirm Password
    do {
        password = getPassword("                                                                                        Enter Password: ");
        if (password == "0") return;

        confirmPassword = getPassword("                                                                                        Confirm Password: ");
        if (confirmPassword == "0") return;

        if (password != confirmPassword) {
            setConsoleColor(12);
            cout << "                                                                                        Passwords do not match. Please try again.\n";
            setConsoleColor(7);
        }
    } while (password != confirmPassword);

    // Enter Date 
    do {
        cout << "                                                                                        Enter Year (YYYY): ";
        cin >> year;
        if (year == 0) return;

        cout << "                                                                                        Enter Month (MM): ";
        cin >> month;
        if (month == 0) return;

        cout << "                                                                                        Enter Day (DD): ";
        cin >> day;
        if (day == 0) return;

        cin.ignore(); // Clear buffer


        if (year < 1900 || year > 2100) {
            setConsoleColor(12);
            cout << "                                                                                        Invalid year. Please enter a correct year.\n";
            setConsoleColor(7);
        }
        else if (month < 1 || month > 12) {
            setConsoleColor(12);
            cout << "                                                                                        Invalid month. Please enter a valid month (1-12).\n";
            setConsoleColor(7);
        }
        else if (day < 1 || day > 31 || (month == 2 && day > 29) ||
            ((month == 4 || month == 6 || month == 9 || month == 11) && day > 30)) {
            setConsoleColor(12);
            cout << "                                                                                        Invalid day. Please enter a valid day for the selected month.\n";
            setConsoleColor(7);
        }
        else {
            break; 
        }
    } while (true);

    string date = to_string(year) + "-" + (month < 10 ? "0" : "") + to_string(month) + "-" + (day < 10 ? "0" : "") + to_string(day);

    char isStudent;
    cout << "                                                                                        Are you a student? (Y/N): ";
    cin >> isStudent;
    cin.ignore();
    if (isStudent == '0') return;

    if (toupper(isStudent) == 'Y') {
        isStudentFlag = true;
        cout << "                                                                                        Enter your Matric Number: ";
        getline(cin, matricNumber);
        if (matricNumber == "0") return;
    }
    else {
        isStudentFlag = false;
        matricNumber = "";
    }

    // Register the User
    registerUser(firstName, lastName, phoneNumber, email, userName, password, date, matricNumber, isStudentFlag);

    // Success Message
    cout << "                                                                                        Registration successful! Returning to the main menu...\n";
    system("pause");
    system("cls");
}


// User login page
void loginUserPage() {
    string emailOrUsername, password;
    bool isStudent = false;
    int userID = -1;       
    bool isAdmin = false;  

    while (true) {
        system("cls");
        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                                    *****************************************\n";
        cout << "                                                                                    *                  LOGIN                *\n";
        cout << "                                                                                    *****************************************\n\n";

        cout << "                                                                                    Enter Email or Username: ";
        getline(cin, emailOrUsername);

        password = getPassword("                                                                                    Enter Password: ");

        try {
            sql::PreparedStatement* pstmt = globalCon->prepareStatement(
                "SELECT User_id, IsStudent, Admin FROM users WHERE (Email = ? OR UserName = ?) AND Password = ?");
            pstmt->setString(1, emailOrUsername);
            pstmt->setString(2, emailOrUsername);
            pstmt->setString(3, password); 

            sql::ResultSet* res = pstmt->executeQuery();

            if (res->next()) {
                userID = res->getInt("User_id");
                isStudent = res->getBoolean("IsStudent");
                isAdmin = res->getBoolean("Admin");

                delete res;
                delete pstmt;

                if (isAdmin) {
                    cout << "\n\033[32mLogged in as Admin. Redirecting to Admin Page...\033[0m\n";
                    system("pause");
                    adminPage(); 
                }
                else {
                    cout << "\nLogged in as User. Redirecting to Movie Page...\n";
                    if (isStudent) {
                        cout << "You are logged in as a student. Enjoy a 40% discount on tickets!\n";
                    }
                    system("pause");
                    movieMainPage(userID, isStudent); 
                }
                return; 
            }
            else {
                delete res;
                delete pstmt;
                cout << "\033[31m\nLogin failed. Invalid email/username or password. Please try again.\033[0m\n";
            }
        }
        catch (sql::SQLException& e) {
            cout << "\n\033[31mSQL Error (loginUserPage): " << e.what() << "\033[0m\n";
        }

        system("pause");
    }
}



int main() {
    connectToDatabase();

    // main menu page
    string input; 
    int choice = 0;

    do {
        system("cls");
        cout << "\n\n\n\n\n\n\n\n\n";
        cout << "                                                                                    *****************************************\n";
        cout << "                                                                                    *       CINEMA TICKET BOOKING SYSTEM    *\n";
        cout << "                                                                                    *****************************************\n\n";
        cout << "                                                                                    1. REGISTER\n";
        cout << "                                                                                    2. LOGIN\n";
        cout << "                                                                                    3. EXIT\n\n";
        cout << "                                                                                    Choose an option: ";
        getline(cin, input);

        if (input.length() == 1 && isdigit(input[0])) {
            choice = stoi(input); 
        }
        else {
            choice = 0; 
        }

        switch (choice) {
        case 1:
            system("cls");
            registerUserPage();
            break;
        case 2:
            system("cls");
            loginUserPage();
            break;
        case 3:
            cout << "Exiting the system. Goodbye!\n";
            break;
        default:
            setConsoleColor(12); 
            cout << "\033[31mInvalid choice. Please choose 1, 2, or 3.\033[0m\n";
            setConsoleColor(7); 
            system("pause");    
        }
    } while (choice != 3);

    closeDatabaseConnection();
    return 0;
}

