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
using namespace std;

//••••••••••••••••••NationalCodeValidator••••••••••••••••••
// Validates Iranian national codes using the modulo-11 check digit algorithm.
// This class only checks if a national code is valid and does not depend on any other classes.
class NationalCodeValidator {
public:
    static bool isValid(const string& rawCode);
    static string normalize(const string& rawCode);
};

//••••••••••••••••••User••••••••••••••••••
// Represents a regular user with a national code and password.
class User {
public:
    User(const string& nationalCode, const string& passwordHash);

    string getNationalCode() const;
    string getPasswordHash() const;
    bool verifyPassword(const string& hashedPassword) const;

private:
    string nationalCode;
    string passwordHash;
};

//••••••••••••••••••UserFileManager••••••••••••••••••
// Handles reading and writing user data to file.
class UserFileManager {
public:
    explicit UserFileManager(FileManager& fm);

    vector<shared_ptr<User>> loadUsers() const;
    void saveUsers(const vector<shared_ptr<User>>& users) const;

private:
    FileManager& fileManager;
    string usersPath = "data/users.json";
};

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

//••••••••••••••••••AccountOwnershipManager••••••••••••••••••
// Keeps track of which user owns each bank account.
// It stores which national code owns each account number.
class AccountOwnershipManager {
public:
    explicit AccountOwnershipManager(FileManager& fm);

    void registerOwnership(const string& accountNumber, const string& nationalCode);
    void removeOwnership(const string& accountNumber);
    bool isOwnedBy(const string& accountNumber, const string& nationalCode) const;
    vector<string> getAccountsOf(const string& nationalCode) const;
    void reload();

private:
    FileManager& fileManager;
    string ownersPath = "data/account_owners.json";
    map<string, string> ownerOf;

    void persist() const;
};

//••••••••••••••••••UserService••••••••••••••••••
// Handles signup, login, logout, and user deletion for regular users.
class UserService {
public:
    UserService(UserFileManager& ufm, AuthService& as, AuthSession& sess, AccountOwnershipManager& own);

    VoidResult signup(InputQueue& inputQueue);
    Result<shared_ptr<User>> login(InputQueue& inputQueue);
    VoidResult logout();
    VoidResult deleteCurrentUser(InputQueue& inputQueue);
    void reload();

private:
    UserFileManager& userFileManager;
    AuthService& authService;
    AuthSession& session;
    AccountOwnershipManager& ownership;
    vector<shared_ptr<User>> users;

    shared_ptr<User> findUser(const string& nationalCode);
};

//••••••••••••••••••AccounOwnerhipValidator••••••••••••••••••
// Checks if an account exists and belongs to the given user.
class AccountOwnershipValidator {
public:
    static Result<shared_ptr<Account>> findOwnedAccount(
        AccountService& accountService,
        AccountOwnershipManager& ownership,
        const string& accountNumber,
        const string& nationalCode,
        const string& notFoundMessage = "Error: Account not found.");
};

//••••••••••••••••••UserAccountService••••••••••••••••••
// Lets logged-in users open, view, and delete their own accounts.
class UserAccountService {
public:
    UserAccountService(AccountService& as, AccountOwnershipManager& own, AuthSession& sess, AuthService& auth);

    Result<shared_ptr<Account>> openAccount(InputQueue& inputQueue);
    Result<vector<shared_ptr<Account>>> myAccounts();
    VoidResult deleteMyAccount(const string& accountNumber, InputQueue& inputQueue);

private:
    AccountService& accountService;
    AccountOwnershipManager& ownership;
    AuthSession& session;
    AuthService& authService;
};

//••••••••••••••••••UserTransactionService••••••••••••••••••
// Handles user transaction commands.
// Reuses TransactionService and adds ownership validation.
class UserTransactionService {
public:
    UserTransactionService(AccountService& as, AccountOwnershipManager& own, AuthSession& sess, TransactionService& ts, FileManager& fm);

    VoidResult depositTo(const string& accountNumber, double amount);
    VoidResult withdrawFrom(const string& accountNumber, double amount, InputQueue& inputQueue);
    VoidResult sendMoney(const string& from, const string& to, double amount, InputQueue& inputQueue);
    VoidResult balanceInquiry(const string& accountNumber);

private:
    AccountService& accountService;
    AccountOwnershipManager& ownership;
    AuthSession& session;
    TransactionService& transactionService;
    FileManager& fileManager;
};

//••••••••••••••••••Input_Handling••••••••••••••••••

struct UserCommandContext {
    UserService& users;
    UserAccountService& userAccounts;
    UserTransactionService& userTransactions;
    AccountOwnershipManager& ownership;
    TransactionService& transactions;
    InputQueue& input;

    UserCommandContext(UserService& us, UserAccountService& uas, UserTransactionService& uts, AccountOwnershipManager& own, TransactionService& ts, InputQueue& iq);
};

class UserCommand : public Command<UserCommandContext> {
public:
    virtual ~UserCommand() = default;
};

class SignupCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class LoginCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class LogoutCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class DeleteMyUserCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class OpenAccountCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class DeleteMyAccountCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class MyAccountsCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class DepositToCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class WithdrawFromCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class SendMoneyCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class BalanceInquiryCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class ResetAllUserCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class HandleUserCommand {
public:
    HandleUserCommand();                     
    explicit HandleUserCommand(HandleAdminCommand&);  

    bool isUserCommand(const string& command);
    void handleCommand(string& line, UserService& us, UserAccountService& uas, UserTransactionService& uts, AccountOwnershipManager& ownership, TransactionService& ts, InputQueue& inputQueue);

private:
    map<string, unique_ptr<UserCommand>> commands;
    void registerCommands();
};