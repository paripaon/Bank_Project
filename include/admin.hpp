#pragma once

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <array>
#include <cstdint>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <map>
#include <condition_variable>
#include <random>
#include "json.hpp"
#include "sha256.hpp"  
#include "result.hpp" 
#include "models.hpp"
#include "persistence.hpp"
using json = nlohmann::json;
using namespace std;

//••••••••••••••••••InputQueue••••••••••••••••••
// reads input lines in a background thread and stores them in a queue.
// Lets you get input either waiting or with a timeout.
// literaly made a class to handle "timeout" error in withdarw :)
class InputQueue {
public:
    InputQueue();
    bool getLine(string& outLine, int timeoutSeconds);
    bool getLineBlocking(string& outLine);

private:
    void readLoop();
    thread reader;
    queue<string> queue_;
    mutex mtx;
    condition_variable cv;
    bool finished = false;
};

//••••••••••••••••••AuthService••••••••••••••••••
// Handles password hashing, verification, and user password input.
// Checks user passwords using SHA-256 hashes.
class AuthService {
public:
    string promptPassword(InputQueue& inputQueue, string enterPass = "Enter password: ");
    bool verifyPassword(const shared_ptr<Account>& account, const string& rawPassword);
    bool promptPasswordWithTimeout(InputQueue& inputQueue, string& outPassword, int timeoutSeconds) const;
    string generateHash(const string& rawPassword) const;

private:
    string hashPassword(const string& rawPassword) const;
};

//••••••••••••••••••FeeSerive••••••••••••••••••
// Stores and manages transfer and balance inquiry fees.
// Loads and saves fee configuration through FileManager.
class FeeManager {
public:
    explicit FeeManager(FileManager& fm);

    double getTransferFee() const;
    double getBalanceInquiryFee() const;
    VoidResult setTransferFee(double amount);
    VoidResult setBalanceInquiryFee(double amount);
    void showFees() const;

private:
    FileManager& fileManager;
};

//••••••••••••••••••AccountService••••••••••••••••••
// Manages bank accounts and branches.
// Creates, updates, and removes accounts.
class AccountService {
public:
    AccountService(FileManager& fm, AuthService& as);

    void createBranch(string& name);
    VoidResult listBranches() const;
    
    shared_ptr<Account> createUserAccount(const string& passwordHash);
    Result<shared_ptr<Account>> createAccount(int branchId, InputQueue& inputQueue);

    VoidResult closeAccount(string& accountNumber, InputQueue& inputQueue);
    VoidResult deleteAccount(string& accountNumber, InputQueue& inputQueue);
    void removeAccountNoAuth(const string& accountNumber);
    void listAccounts() const;
    void reset();

    shared_ptr<Account> getAccount(string& accountNumber);
    shared_ptr<Account> getAccount(const string& accountNumber) const;
    shared_ptr<Branch> getBranch(int branchId);

    const vector<shared_ptr<Account>>& getAccounts() const;
    const vector<shared_ptr<Branch>>& getBranches() const;

private:
    FileManager& fileManager;
    AuthService& authService;
    vector<shared_ptr<Account>> accounts;
    vector<shared_ptr<Branch>> branches;
    int nextBranchId;
    int nextAccountSeq;

    shared_ptr<Account> findAccount(string& accountNumber);
    shared_ptr<Branch> findBranch(int branchId);
    string generateAccountNumber();
    void eraseAccount(const string& accountNumber);
};

//••••••••••••••••••OtpService••••••••••••••••••
enum class OtpStatus { Valid, Expired, Invalid };

class OtpService {
public:
    string requestOtp(const string& accountNumber, int& secondsRemaining);
    OtpStatus verifyOtp(const string& accountNumber, const string& code);

private:
    struct OtpEntry {
        string code;
        chrono::steady_clock::time_point expiresAt;
    };
    map<string, OtpEntry> otps;
    string generateCode() const;
};

//••••••••••••••••••RequestService••••••••••••••••••
class RequestService {
public:
    RequestService(FileManager& fm, AccountService& as);

    Result<shared_ptr<Request>> createRequest(const string& nationalCode, int branchId);
    Result<vector<shared_ptr<Request>>> getRequestsOf(const string& nationalCode) const;
    VoidResult cancelRequest(int requestId, const string& nationalCode);
    Result<shared_ptr<Request>> prepareActivation(int requestId, const string& nationalCode);
    void markActivated(int requestId, const string& accountNumber);

    void branchDashboard(int branchId) const;
    void listRequests(int branchId) const;
    VoidResult approveRequest(int requestId);
    VoidResult rejectRequest(int requestId, const string& reason);

    shared_ptr<Request> findRequest(int requestId) const;
    void resetAll();

private:
    FileManager& fileManager;
    AccountService& accountService;
    vector<shared_ptr<Request>> requests;
    int nextRequestId;

    bool hasActiveOrPendingInBranch(const string& nationalCode, int branchId) const;
};

//••••••••••••••••••TransactionService••••••••••••••••••
// Handles money operations and transaction history.
class TransactionService {
public:
    TransactionService(FileManager& fm, AccountService& as, RequestService& rs, AuthService& auth, FeeManager& fee);

    Result<shared_ptr<Transaction>> deposit(const string& accountNumber, double amount);
    Result<shared_ptr<Transaction>> withdraw(const string& accountNumber, double amount, InputQueue& inputQueue, const string& wrongPasswordMessage = "Wrong password.");
    Result<shared_ptr<Transaction>> debitForPaya(const string& accountNumber, double amount);
    Result<shared_ptr<Transaction>> transfer(const string& from, const string& to, double amount, InputQueue& inputQueue, const string& wrongPasswordMessage = "Wrong password.");
    Result<shared_ptr<Transaction>> onlinePayment(const string& from, const string& to, double amount, const string& enteredOtp, OtpService& otpService);

    Result<shared_ptr<Account>> getBalance(const string& accountNumber);
    Result<shared_ptr<Account>> getBalanceForUser(const string& accountNumber);

    void getHistory(string& accountNumber) const;
    void getTransaction(int id) const;
    VoidResult clearHistory(string& accountNumber, InputQueue& inputQueue);
    void reset();

    const vector<shared_ptr<Transaction>>& getAllTransactions() const;

private:
    AccountService& accountService;
    FileManager& fileManager;
    RequestService& requestService;
    AuthService& authService;
    FeeManager& feeManager;
    vector<shared_ptr<Transaction>> transactions;
    int nextTransactionId;
};

//••••••••••••••••••PayaService••••••••••••••••••
class PayaService {
public:
    PayaService(FileManager& fm, AccountService& as, TransactionService& ts, RequestService& rs);

    Result<shared_ptr<PayaRequest>> createRequest(const string& fromAccount, const string& destinationIban, double amount);
    void listRequests() const;
    Result<shared_ptr<Transaction>> approve(int requestId);
    VoidResult reject(int requestId);

    void resetAll();

private:
    FileManager& fileManager;
    AccountService& accountService;
    TransactionService& transactionService;
    RequestService& requestService;
    vector<shared_ptr<PayaRequest>> requests;
    int nextRequestId;

    shared_ptr<PayaRequest> findRequest(int id);
};