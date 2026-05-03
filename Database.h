#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <vector>
#include <utility> 
#include <map>     

using namespace std;

extern sql::Connection* globalCon;

void connectToDatabase();
void closeDatabaseConnection();

void registerUser(const string& firstName, const string& lastName, const string& phoneNumber,
    const string& email, const string& userName, const string& password,
    const string& date, const string& matricNumber, bool isStudent);

bool loginUser(const string& emailOrUsername, const string& password, bool& isStudent, int& userID);

int insertBooking(int userID, int movieID, const string& screenType,
    const string& bookingDate, const string& timeSlot,
    const string& seatsBooked, const string& ticketType, double totalPrice,
    const string& meals);

void movieMainPage(int userID, bool isStudent);
void updateUserProfile(int userID);
void viewUserBookings(int userID);

void displayMovies(const string& searchKeyword = "");

void browseAndSearchMovies(int userID, bool isStudent);

void displaySearchMoviesPage();

void displayDaysPage(const std::string& chosenMovie, int userID, int movieID, bool isStudent);
void displayScreenAndTimePage(int userID, int movieID, const string& chosenDay, bool isStudent);

void displaySeatsPage(const string& chosenDay, const string& chosenTimeSlot, const string& screenType, int userID, int movieID, bool allowSelection);
string getUserName(int userID);
void cancelBooking(int bookingID);

void displayTicketTypeSelectionPage(int userID, int movieID, const string& chosenDay, const string& chosenTimeSlot,
    const string& screenType, const vector<string>& selectedSeats, bool isStudent);

void displayStudentTicketPage(int userID, int movieID, const string& chosenDay, const string& chosenTimeSlot,
    const string& screenType, const vector<string>& selectedSeats);

void displayMealPage(int movieID, int userID, const string& chosenDay, const string& chosenTimeSlot,
    const string& screenType, const vector<string>& selectedSeats,
    const vector<string>& selectedTicketTypes, bool isStudent, bool allowMealRemoval = true);

void displayPaymentPage(int userID, int bookingID, int movieID, const string& chosenDay, const string& chosenTimeSlot,
    const string& screenType, const vector<string>& selectedSeats,
    const vector<string>& selectedTicketTypes, double totalMealPrice, bool isStudent,
    const vector<pair<string, double>>& selectedMeals);

bool checkUserIDExists(int userID);
string getMovieTitle(int movieID);

void displayCardInformationPage(int userID, int bookingID, double grandTotal,
    const vector<string>& selectedTicketTypes,
    const vector<pair<string, double>>& selectedMeals,
    double totalMealPrice, bool isStudent,
    const string& bookingDate, const string& movieTitle);


void displayReceipt(const string& userName, const string& movieTitle,
    const vector<string>& ticketTypes, const vector<pair<string, double>>& selectedMeals,
    double totalAmount, double discountedAmount, const string& bookingDate);

// Admin page
void adminPage();

// Payment Management 
void managePayments();
void viewAllPayments();
void addNewPayment();
void updatePayment();
void deletePayment();

// Movie Management 
void manageMovies();
void addMovie();
void deleteMovie();
void viewAllMovies();
void updateMovie();

// Users Management 
void manageUsers();
void viewAllUsers();
void addNewUser();
void updateUser();
void deleteUser();

// Booking Management 
void manageBookings();
void viewAllBookings();
void addNewBooking();
void updateBooking();

// Report
void displayReportPage();

// Meal Management Functions
void manageMeals();
void viewMeals();
void addMeal();
void updateMeal();

// New function for saving MealID to Booking table
void saveMealsToBooking(int bookingID, const vector<int>& mealIDs);

void viewFullReport();
void displayProfitMarginAnalysis();
void displayGraphSummary();
void displayBookingsByDestination();

// Utility functions
std::string vectorToCommaSeparatedString(const std::vector<std::string>& inputVector);

#endif
