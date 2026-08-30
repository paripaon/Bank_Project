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
#include "json.hpp"
#include "sha256.hpp"  
#include "result.hpp" 
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

//••••••••••••••••••Json_handling••••••••••••••••••

class JsonFileStore {
public:
    json read(const string& path) const;
    void write(const string& path, const json& data) const;
};

class BranchRepository {
private:
    JsonFileStore& store;
    string path;

public:
    BranchRepository(JsonFileStore& store, string path);

    vector<shared_ptr<Branch>> load() const;
    void save(const vector<shared_ptr<Branch>>& branches) const;
};

class AccountRepository {
private:
    JsonFileStore& store;
    string path;

public:
    AccountRepository(JsonFileStore& store, string path);

    vector<shared_ptr<Account>> load() const;
    void save(const vector<shared_ptr<Account>>& accounts) const;
};

class TransactionRepository {
private:
    JsonFileStore& store;
    string path;

public:
    TransactionRepository(JsonFileStore& store, string path);

    vector<shared_ptr<Transaction>> load() const;
    void save(const vector<shared_ptr<Transaction>>& transactions) const;
};

class FeeRepository {
private:
    JsonFileStore& store;
    string path;

public:
    FeeRepository(JsonFileStore& store, string path);

    double getTransferFee() const;
    double getBalanceInquiryFee() const;
    void save(double transferFee, double balanceInquiryFee) const;
};

class MetaRepository {
private:
    JsonFileStore& store;
    string path;

public:
    MetaRepository(JsonFileStore& store, string path);

    void saveNextIds(int nextBranchId, int nextAccountSeq) const;
    void saveNextTransactionId(int nextTransactionId) const;
    void loadNextIds(int& nextBranchId, int& nextAccountSeq, int& nextTransactionId) const;
    void loadNextTransactionId(int& nextTransactionId) const;
};

//••••••••••••••••••FileManager••••••••••••••••••
class FileManager {
private:
    JsonFileStore store;
    BranchRepository branchRepo;
    AccountRepository accountRepo;
    TransactionRepository transactionRepo;
    FeeRepository feeRepo;
    MetaRepository metaRepo;

public:
    FileManager();

    json readFile(const string& path) const;
    void writeFile(const string& path, const json& data) const;

    vector<shared_ptr<Branch>> loadBranches() const;
    vector<shared_ptr<Account>> loadAccounts() const;
    vector<shared_ptr<Transaction>> loadTransactions() const;

    void saveBranches(const vector<shared_ptr<Branch>>& branches) const;
    void saveAccounts(const vector<shared_ptr<Account>>& accounts) const;
    void saveTransactions(const vector<shared_ptr<Transaction>>& transactions) const;

    void saveNextIds(int nextBranchId, int nextAccountSeq) const;
    void saveNextTransactionId(int nextTransactionId) const;
    void loadNextIds(int& nextBranchId, int& nextAccountSeq, int& nextTransactionId) const;
    void loadNextTransactionId(int& nextTransactionId) const;

    double getTransferFee() const;
    double getBalanceInquiryFee() const;
    void saveFees(double transferFee, double balanceInquiryFee) const;

    void resetFiles();
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

//••••••••••••••••••FeeManager••••••••••••••••••
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
    void listBranches() const;

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

//••••••••••••••••••TransactionService••••••••••••••••••
// Handles money operations and transaction history.
class TransactionService {
public:
    TransactionService(FileManager& fm, AccountService& as, AuthService& auth, FeeManager& fee);

    Result<shared_ptr<Transaction>> deposit(const string& accountNumber, double amount);
    Result<shared_ptr<Transaction>> withdraw(const string& accountNumber, double amount, InputQueue& inputQueue, const string& wrongPasswordMessage = "Wrong password.");
    Result<shared_ptr<Transaction>> transfer(const string& from, const string& to, double amount, InputQueue& inputQueue, const string& wrongPasswordMessage = "Wrong password.");

    Result<shared_ptr<Account>> getBalance(const string& accountNumber);
    Result<shared_ptr<Account>> getBalanceForUser(const string& accountNumber);

    void getHistory(string& accountNumber) const;
    void getTransaction(int id) const;
    VoidResult clearHistory(string& accountNumber, InputQueue& inputQueue);
    void resetAll();

    const vector<shared_ptr<Transaction>>& getAllTransactions() const;

private:
    AccountService& accountService;
    FileManager& fileManager;
    AuthService& authService;
    FeeManager& feeManager;
    vector<shared_ptr<Transaction>> transactions;
    int nextTransactionId;
};

//••••••••••••••••••Input_Handling••••••••••••••••••

class CommandParser {
public:
    static bool isValidAccountNumber(const string& acc);
    static VoidResult requireArgs(const string& args, int count = 1);
    static VoidResult parseAccountAmount(const string& args, string& acc, double& amount);
    static VoidResult parseTransfer(const string& args, string& from, string& to, double& amount);
    static bool confirm(InputQueue& inputQueue, const string& prompt = "Are you sure? This deletes everything. (yes/no): ");
};

struct AdminCommandContext {
    AccountService& accounts;
    TransactionService& transactions;
    AuthService& auth;
    FeeManager& fees;
    InputQueue& input;

    AdminCommandContext(AccountService& as, TransactionService& ts, AuthService& authS, FeeManager& fm, InputQueue& iq);
};

template<typename Context>
class Command {
public:
    virtual ~Command() = default;
    virtual void execute(const string& args, Context& ctx) = 0;
};

class AdminCommand : public Command<AdminCommandContext> {};

class CreateBranchCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class ListBranchesCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class CreateAccountCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class CloseAccountCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class DeleteAccountCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class ListAccountsCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class DepositCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class WithdrawCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class TransferCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class GetBalanceCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class GetHistoryCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class GetTransactionCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class ClearHistoryCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class ResetAllCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class SetTransferFeeCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class SetBalanceInquiryFeeCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class ShowFeesCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class HandleAdminCommand {
public:
    HandleAdminCommand();

    bool isAdminCommand(const string& command);
    bool requireArgs(const string& args, int count = 1);
    void handleCommand(string& line, AccountService& as, TransactionService& ts, AuthService& auth, FeeManager& fm, InputQueue& inputQueue);

private:
    map<string, unique_ptr<AdminCommand>> commands;
    void registerCommands();
};