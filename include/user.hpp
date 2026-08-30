#pragma once

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <map>
#include <chrono>
#include <ctime>
#include <array>
#include <cstdint>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "admin.hpp" 
#include "models.hpp"
#include "persistence.hpp"
using namespace std;

//••••••••••••••••••AuthSession••••••••••••••••••
// Keeps track of the currently logged-in user.
// Keeping the login session in a separate class, makes it easier to manage.
class AuthSession {
public:
    bool isLoggedIn() const;
    shared_ptr<User> getCurrentUser() const;
    void login(shared_ptr<User> user);
    void logout();

private:
    shared_ptr<User> currentUser = nullptr;
};

//••••••••••••••••••RankingService••••••••••••••••••
struct RankEntry {
    string nationalCode;
    int score;
    string level;
    int rank;
};

class RankingService {
    UserRepository& userRepo;
    FileManager& fileManager;
    vector<shared_ptr<User>>& users; // reference to the live list held by UserService
    int nextUserSeq;
public:
    RankingService(UserRepository& ur, FileManager& fm, vector<shared_ptr<User>>& usersRef);

    int assignRegistrationSeq(); // called on signup, returns seq and persists counter
    void awardScore(const string& nationalCode, int delta);
    Result<RankEntry> getRank(const string& nationalCode);
    vector<RankEntry> getAllRankings();
};

//••••••••••••••••••UserService••••••••••••••••••
// Handles signup, login, logout, and user deletion for regular users.
class UserService {
public:
    UserService(UserRepository& ur, AuthService& as, AuthSession& sess, OwnershipRepository& own, RankingService& rank, vector<shared_ptr<User>>& usersRef);

    VoidResult signup(InputQueue& inputQueue);
    Result<shared_ptr<User>> login(InputQueue& inputQueue);
    VoidResult logout();
    VoidResult deleteCurrentUser(InputQueue& inputQueue);

    Result<string> currentNationalCode() const;
    void reload();

private:
    UserRepository& userRepo;
    AuthService& authService;
    AuthSession& session;
    OwnershipRepository& ownership;
    RankingService& ranking;
    vector<shared_ptr<User>>& users;

    shared_ptr<User> findUser(const string& nationalCode);
};

//••••••••••••••••••AccounOwnerhipValidator••••••••••••••••••
// Checks if an account exists and belongs to the given user.
class AccountOwnershipValidator {
public:
    static Result<shared_ptr<Account>> findOwnedAccount(
        AccountService& accountService,
        OwnershipRepository& ownership,
        const string& accountNumber,
        const string& nationalCode,
        const string& notFoundMessage = "Error: Account not found.");
};

//••••••••••••••••••UserAccountService••••••••••••••••••
// Lets logged-in users open, view, and delete their own accounts.
class UserAccountService {
public:
    UserAccountService(AccountService& as, OwnershipRepository& own, AuthSession& sess, AuthService& auth, RankingService& rank);

    Result<vector<shared_ptr<Account>>> myAccounts();
    VoidResult deleteMyAccount(const string& accountNumber, InputQueue& inputQueue);
    Result<string> showIban(const string& accountNumber);

private:
    AccountService& accountService;
    OwnershipRepository& ownership;
    AuthSession& session;
    AuthService& authService;
    RankingService& rank;
};

//••••••••••••••••••UserRequestService••••••••••••••••••
class UserRequestService {
public:
    UserRequestService(RequestService& rs, AccountService& as, OwnershipRepository& own, AuthSession& sess, AuthService& auth, RankingService& rank);

    Result<shared_ptr<Request>> requestAccount(int branchId);
    Result<vector<shared_ptr<Request>>> myRequests();
    VoidResult cancelRequest(int requestId);
    Result<shared_ptr<Account>> activateAccount(int requestId, InputQueue& inputQueue);

private:
    RequestService& requestService;
    AccountService& accountService;
    OwnershipRepository& ownership;
    AuthSession& session;
    AuthService& authService;
    RankingService& rank;
};

//••••••••••••••••••UserTransactionService••••••••••••••••••
// Handles user transaction commands.
// Reuses TransactionService and adds ownership validation.
class UserTransactionService {
public:
    UserTransactionService(AccountService& as, OwnershipRepository& own, AuthSession& sess, TransactionService& ts, FileManager& fm, RankingService& rank, AuthService& authService);

    VoidResult depositTo(const string& accountNumber, double amount);
    VoidResult withdrawFrom(const string& accountNumber, double amount, InputQueue& inputQueue);
    VoidResult sendMoney(const string& from, const string& to, double amount, InputQueue& inputQueue);
    VoidResult balanceInquiry(const string& accountNumber);
    Result<string> requestOtp(const string& accountNumber, OtpService& otpService);
    VoidResult onlinePayment(const string& from, const string& to, double amount, InputQueue& inputQueue, OtpService& otpService);
    VoidResult payaTransfer(const string& from, const string& destinationIban, double amount, InputQueue& inputQueue, PayaService& payaService, AuthService& authService);
    VoidResult exportHistory(const string& accountNumber, const string& format = "json");

private:
    AccountService& accountService;
    OwnershipRepository& ownership;
    AuthSession& session;
    TransactionService& transactionService;
    FileManager& fileManager;
    RankingService& rank;
    AuthService& authService;
};