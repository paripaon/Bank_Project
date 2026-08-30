#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include "json.hpp"
#include "models.hpp"
using json = nlohmann::json;
using namespace std;

//••••••••••••••••••Json_handling••••••••••••••••••

class JsonFileStore {
public:
    json read(const string& path) const;
    void write(const string& path, const json& data) const;
};

//branchRepository
class BranchRepository {
private:
    JsonFileStore& store;
    string path;

public:
    BranchRepository(JsonFileStore& store, string path);

    vector<shared_ptr<Branch>> load() const;
    void save(const vector<shared_ptr<Branch>>& branches) const;
};

//accountRepository
class AccountRepository {
private:
    JsonFileStore& store;
    string path;

public:
    AccountRepository(JsonFileStore& store, string path);

    vector<shared_ptr<Account>> load() const;
    void save(const vector<shared_ptr<Account>>& accounts) const;
};

//transactionRepository
class TransactionRepository {
private:
    JsonFileStore& store;
    string path;

public:
    TransactionRepository(JsonFileStore& store, string path);

    vector<shared_ptr<Transaction>> load() const;
    void save(const vector<shared_ptr<Transaction>>& transactions) const;
};

//requestRepository
class RequestRepository {
private:
    JsonFileStore& store;
    string path;

public:
    RequestRepository(JsonFileStore& store, string path);

    vector<shared_ptr<Request>> load() const;
    void save(const vector<shared_ptr<Request>>& requests) const;
};

//payaRepository
class PayaRepository {
private:
    JsonFileStore& store;
    string path;

public:
    PayaRepository(JsonFileStore& store, string path);

    vector<shared_ptr<PayaRequest>> load() const;
    void save(const vector<shared_ptr<PayaRequest>>& requests) const;
};

//feeRepository
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

//metaRepository
class MetaRepository {
private:
    JsonFileStore& store;
    string path;

public:
    MetaRepository(JsonFileStore& store, string path);

    void saveNextIds(int nextBranchId, int nextAccountSeq) const;
    void saveNextTransactionId(int nextTransactionId) const;
    void saveNextRequestId(int nextRequestId) const;
    void saveNextUserSeq(int nextUserSeq) const;
    void loadNextIds(int& nextBranchId, int& nextAccountSeq, int& nextTransactionId) const;
    void loadNextTransactionId(int& nextTransactionId) const;
    void loadNextRequestId(int& nextRequestId) const;
    void loadNextUserSeq(int& nextUserSeq) const;
};

//••••••••••••••••••FileManager••••••••••••••••••
class FileManager {
private:
    JsonFileStore store;
    BranchRepository branchRepo;
    AccountRepository accountRepo;
    TransactionRepository transactionRepo;
    RequestRepository requestRepo;
    PayaRepository payaRepo;
    FeeRepository feeRepo;
    MetaRepository metaRepo;
    

public:
    FileManager();

    json readFile(const string& path) const;
    void writeFile(const string& path, const json& data) const;

    vector<shared_ptr<Branch>> loadBranches() const;
    vector<shared_ptr<Account>> loadAccounts() const;
    vector<shared_ptr<Transaction>> loadTransactions() const;
    vector<shared_ptr<Request>> loadRequests() const;
    vector<shared_ptr<PayaRequest>> loadPayaRequests() const;

    void saveBranches(const vector<shared_ptr<Branch>>& branches) const;
    void saveAccounts(const vector<shared_ptr<Account>>& accounts) const;
    void saveTransactions(const vector<shared_ptr<Transaction>>& transactions) const;
    void saveRequests(const vector<shared_ptr<Request>>& requests) const;
    void savePayaRequests(const vector<shared_ptr<PayaRequest>>& requests) const;
    void saveNextUserSeq(int nextUserSeq) const;
    
    void saveNextIds(int nextBranchId, int nextAccountSeq) const;
    void saveNextTransactionId(int nextTransactionId) const;
    void saveNextRequestId(int nextRequestId) const;
    void loadNextIds(int& nextBranchId, int& nextAccountSeq, int& nextTransactionId) const;
    void loadNextTransactionId(int& nextTransactionId) const;
    void loadNextRequestId(int& nextRequestId) const;
    void loadNextUserSeq(int& nextUserSeq) const;

    double getTransferFee() const;
    double getBalanceInquiryFee() const;
    void saveFees(double transferFee, double balanceInquiryFee) const;

    void resetFiles();
};


//UserRepository
class UserRepository {
public:
    explicit UserRepository(FileManager& fm);

    vector<shared_ptr<User>> loadUsers() const;
    void saveUsers(const vector<shared_ptr<User>>& users) const;

private:
    FileManager& fileManager;
    string usersPath = "data/users.json";
};

//OwnershipRepository
class OwnershipRepository {
public:
    explicit OwnershipRepository(FileManager& fm);

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