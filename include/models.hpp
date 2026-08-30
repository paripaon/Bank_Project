#pragma once

#include <string>
#include <vector>
#include <memory>
#include "result.hpp"
using namespace std;

//••••••••••••••••••TransactionLimits••••••••••••••••••
namespace TransactionLimits {
    const double MAX_ONLINE_PAYMENT_AMOUNT = 5000000.0;
    const double MAX_TRANSFER_AMOUNT = 20000000.0;
    const double MAX_PAYA_AMOUNT = 50000000.0;
}

//••••••••••••••••••Transaction••••••••••••••••••
// Represents a single bank operation such as deposit, withdrawal, or transfer.
class Transaction {
public:
    Transaction(int id, const string& type, double amount, const string& fromAccount, const string& toAccount, double balanceAfter = 0.0);

    int getId() const;
    string getType() const;
    double getAmount() const;
    string getFromAccount() const;
    string getToAccount() const;
    string getTimestamp() const;
    double getBalanceAfter() const;

    void setTimestamp(const string& ts);
    void setBalanceAfter(double b);

private:
    int id;
    string type;
    double amount;
    string fromAccount;
    string toAccount;
    string timestamp;
    double balanceAfter;
};

//••••••••••••••••••Account••••••••••••••••••
// Represents a bank account with balance, security info, and transaction history.
// Handles basic operations like deposit, withdrawal, and transfer.
class Account {
public:
    Account(const string& accNumber, int branchId, const string& passwordHash, double initialBalance);

    string getAccountNumber() const;
    int getBranchId() const;
    double getBalance() const;
    bool isActive() const;
    string getPasswordHash() const;
    const vector<shared_ptr<Transaction>>& getTransactions() const;

    void setActive(bool status);
    bool verifyPassword(const string& hashedPassword) const;
    void deposit(double amount);

    VoidResult withdraw(double amount, const string& hashedPassword);
    VoidResult transfer(Account& toAccount, double amount, double fee, const string& hashedPassword);

    void addTransaction(shared_ptr<Transaction> tx);
    void clearTransactions();

private:
    string accountNumber;
    int branchId;
    double balance;
    string passwordHash;
    bool active;
    vector<shared_ptr<Transaction>> transactions;
};

//••••••••••••••••••Branch••••••••••••••••••
// Represents a bank branch that manages accounts under it.
// Only stores and organizes accounts.
class Branch {
public:
    Branch(int id, const string& name);

    int getId() const;
    string getName() const;
    const vector<shared_ptr<Account>>& getAccounts() const;

    void setName(const string& n);
    void addAccount(shared_ptr<Account> account);
    void removeAccount(string& accountNumber);
    int getAccountCount() const;
    shared_ptr<Account> findAccount(const string& accountNumber) const;

private:
    int id;
    string name;
    vector<shared_ptr<Account>> accounts;
};

//••••••••••••••••••RequestAccount••••••••••••••••••
class Request {
public:
    Request(int id, const string& nationalCode, int branchId, const string& status);

    int getId() const;
    string getNationalCode() const;
    int getBranchId() const;
    string getStatus() const;
    string getTimestamp() const;
    string getReason() const;
    string getAccountNumber() const;

    void setStatus(const string& s);
    void setTimestamp(const string& ts);
    void setReason(const string& r);
    void setAccountNumber(const string& accNum);

private:
    int id;
    string nationalCode;
    int branchId;
    string status;
    string timestamp;
    string reason;
    string accountNumber;
};

//••••••••••••••••••PayaRequest••••••••••••••••••
class PayaRequest {
public:
    PayaRequest(int id, const string& fromAccount, const string& destinationAccount, double amount, const string& status = "PENDING");

    int getId() const;
    string getFromAccount() const;
    string getDestinationAccount() const;
    double getAmount() const;
    string getStatus() const;
    void setStatus(const string& s);

private:
    int id;
    string fromAccount;
    string destinationAccount;
    double amount;
    string status;
};

//••••••••••••••••••User••••••••••••••••••
// Represents a regular user with a national code and password.
class User {
public:
    User(const string& nationalCode, const string& passwordHash);

    string getNationalCode() const;
    string getPasswordHash() const;
    bool verifyPassword(const string& hashedPassword) const;

    int getScore() const;
    int getRegistrationSeq() const;
    void setScore(int s);
    void setRegistrationSeq(int seq);
    void addScore(int delta);

private:
    string nationalCode;
    string passwordHash;
    int score;
    int registrationSeq;
};

//••••••••••••••••••NationalCodeValidator••••••••••••••••••
// Validates Iranian national codes using the modulo-11 check digit algorithm.
// This class only checks if a national code is valid and does not depend on any other classes.
class NationalCodeValidator {
public:
    static bool isValid(const string& rawCode);
    static string normalize(const string& rawCode);
};

//••••••••••••••••••IbanGenerator••••••••••••••••••
class IbanGenerator {
public:
    static string generate(const string& accountNumber);   
    static string toAccountNumber(const string& iban);
    static bool isValid(const string& iban);
private:
    static string mod97(const string& numStr);
};


//••••••••••••••••••Score••••••••••••••••••
class CreditLevel {
public:
    static string calculate(int score); 
};

class ScoringRules {
public:
    static constexpr int OPEN_ACCOUNT = 3;
    static constexpr int DEPOSIT = 1;
    static constexpr int WITHDRAW = 1;
    static constexpr int TRANSFER = 2;
    static constexpr int BALANCE_INQUIRY = 1;
    static constexpr int DELETE_ACCOUNT = -2;
    static constexpr int DELETE_USER = -3;
};