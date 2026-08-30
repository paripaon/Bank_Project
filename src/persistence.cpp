#include "../include/persistence.hpp"

#include <fstream>
#include <sstream>
using namespace std;

//••••••••••••••••••Json_handling••••••••••••••••••

json JsonFileStore::read(const string& path) const {
    ifstream file(path);
    if (!file.is_open()) return json::object();

    stringstream buffer;
    buffer << file.rdbuf();
    string content = buffer.str();

    if (content.find_first_not_of(" \t\n\r") == string::npos)
        return json::object();

    try { return json::parse(content); }
    catch (const json::parse_error&) { return json{}; }
}

void JsonFileStore::write(const string& path, const json& data) const {
    ofstream file(path);
    file << data.dump(4);
}

//branchRepository
BranchRepository::BranchRepository(JsonFileStore& store, string path)
    : store(store), path(move(path)) {}

vector<shared_ptr<Branch>> BranchRepository::load() const {
    vector<shared_ptr<Branch>> branches;
    json data = store.read(path);

    if (data.empty() or !data.contains("branches")) return branches;
    for (auto& b : data["branches"])
        branches.push_back(make_shared<Branch>(
            b["id"].get<int>(),
            b["name"].get<string>())
        );
    return branches;
}

void BranchRepository::save(const vector<shared_ptr<Branch>>& branches) const {
    json data;
    data["branches"] = json::array();
    for (auto& b : branches) {
        json branch;
        branch["id"] = b->getId();
        branch["name"] = b->getName();
        branch["accounts"] = json::array();
        for (const auto& acc : b->getAccounts())
            branch["accounts"].push_back(acc->getAccountNumber());
        data["branches"].push_back(branch);
    }
    store.write(path, data);
}

//accountRepository
AccountRepository::AccountRepository(JsonFileStore& store, string path)
    : store(store), path(move(path)) {}

vector<shared_ptr<Account>> AccountRepository::load() const {
    vector<shared_ptr<Account>> accounts;
    json data = store.read(path);

    if (data.empty() or !data.contains("accounts")) return accounts;
    for (auto& a : data["accounts"]) {
        auto account = make_shared<Account>(
            a["number"].get<string>(),
            a["branchId"].get<int>(),
            a["passwordHash"].get<string>(),
            a["balance"].get<double>()
        );
        account->setActive(a["active"].get<bool>());
        accounts.push_back(account);
    }
    return accounts;
}

void AccountRepository::save(const vector<shared_ptr<Account>>& accounts) const {
    json data;
    data["accounts"] = json::array();
    for (auto& a : accounts) {
        json account;
        account["number"] = a->getAccountNumber();
        account["branchId"] = a->getBranchId();
        account["balance"] = a->getBalance();
        account["passwordHash"] = a->getPasswordHash();
        account["active"] = a->isActive();
        account["transactions"] = json::array();
        for (auto& tx : a->getTransactions())
            account["transactions"].push_back(tx->getId());
        data["accounts"].push_back(account);
    }
    store.write(path, data);
}

//transactionReposotory
TransactionRepository::TransactionRepository(JsonFileStore& store, string path)
    : store(store), path(move(path)) {}

vector<shared_ptr<Transaction>> TransactionRepository::load() const {
    vector<shared_ptr<Transaction>> transactions;
    json data = store.read(path);

    if (data.empty() or !data.contains("transactions")) return transactions;
    for (auto& t : data["transactions"]) {
        auto tx = make_shared<Transaction>(
            t["id"].get<int>(),
            t["type"].get<string>(),
            t["amount"].get<double>(),
            t["fromAccount"].get<string>(),
            t["toAccount"].get<string>(),
            t.value("balanceAfter", 0.0)
        );
        tx->setTimestamp(t["timestamp"].get<string>());
        transactions.push_back(tx);
    }
    return transactions;
}

void TransactionRepository::save(const vector<shared_ptr<Transaction>>& transactions) const {
    json data;
    data["transactions"] = json::array();
    for (auto& t : transactions) {
        json tx;
        tx["id"] = t->getId();
        tx["type"] = t->getType();
        tx["amount"] = t->getAmount();
        tx["fromAccount"] = t->getFromAccount();
        tx["toAccount"] = t->getToAccount();
        tx["timestamp"] = t->getTimestamp();
        tx["balanceAfter"] = t->getBalanceAfter();
        data["transactions"].push_back(tx);
    }
    store.write(path, data);
}

//requestRepository
RequestRepository::RequestRepository(JsonFileStore& store, string path) 
    : store(store), path(move(path)) {}

vector<shared_ptr<Request>> RequestRepository::load() const {
    vector<shared_ptr<Request>> requests;
    json data = store.read(path);

    if (data.empty() or !data.contains("requests")) return requests;
    for (auto& r : data["requests"]) {
        auto req = make_shared<Request>(
            r["id"].get<int>(),
            r["nationalCode"].get<string>(),
            r["branchId"].get<int>(),
            r["status"].get<string>()
        );
        req->setTimestamp(r.value("timestamp", ""));
        req->setReason(r.value("reason", ""));
        req->setAccountNumber(r.value("accountNumber", ""));
        requests.push_back(req);
    }
    return requests;
}

void RequestRepository::save(const vector<shared_ptr<Request>>& requests) const {
    json data;
    data["requests"] = json::array();
    for (auto& r : requests) {
        json req;
        req["id"] = r->getId();
        req["nationalCode"] = r->getNationalCode();
        req["branchId"] = r->getBranchId();
        req["status"] = r->getStatus();
        req["timestamp"] = r->getTimestamp();
        req["reason"] = r->getReason();
        req["accountNumber"] = r->getAccountNumber();
        data["requests"].push_back(req);
    }
    store.write(path, data);
}

//payaRepository
PayaRepository::PayaRepository(JsonFileStore& store, string path) 
    : store(store), path(path) {}

vector<shared_ptr<PayaRequest>> PayaRepository::load() const {
    vector<shared_ptr<PayaRequest>> requests;
    json data = store.read(path);
    if (data.empty() || !data.contains("requests")) return requests;
    for (auto& r : data["requests"])
        requests.push_back(make_shared<PayaRequest>(
            r["id"].get<int>(),
            r["fromAccount"].get<string>(),
            r["destinationAccount"].get<string>(),
            r["amount"].get<double>(),
            r["status"].get<string>()
        ));
    return requests;
}

void PayaRepository::save(const vector<shared_ptr<PayaRequest>>& requests) const {
    json data;
    data["requests"] = json::array();
    for (auto& r : requests) {
        json entry;
        entry["id"] = r->getId();
        entry["fromAccount"] = r->getFromAccount();
        entry["destinationAccount"] = r->getDestinationAccount();
        entry["amount"] = r->getAmount();
        entry["status"] = r->getStatus();
        data["requests"].push_back(entry);
    }
    store.write(path, data);
}

// feeRepository
FeeRepository::FeeRepository(JsonFileStore& store, string path)
    : store(store), path(move(path)) {}

double FeeRepository::getTransferFee() const {
    json data = store.read(path);
    return data.value("transfer_fee", 0.0);
}

double FeeRepository::getBalanceInquiryFee() const {
    json data = store.read(path);
    return data.value("balance_inquiry_fee", 0.0);
}

void FeeRepository::save(double transferFee, double balanceInquiryFee) const {
    json data;
    data["transfer_fee"] = transferFee;
    data["balance_inquiry_fee"] = balanceInquiryFee;
    store.write(path, data);
}

//metaRepository
MetaRepository::MetaRepository(JsonFileStore& store, string path)
    : store(store), path(move(path)) {}

void MetaRepository::saveNextIds(int nextBranchId, int nextAccountSeq) const {
    json data = store.read(path);
    data["nextBranchId"] = nextBranchId;
    data["nextAccountSeq"] = nextAccountSeq;
    store.write(path, data);
}

void MetaRepository::saveNextTransactionId(int nextTransactionId) const {
    json data = store.read(path);
    data["nextTransactionId"] = nextTransactionId;
    store.write(path, data);
}

void MetaRepository::saveNextRequestId(int nextRequestId) const {
    json data = store.read(path);
    data["nextRequestId"] = nextRequestId;
    store.write(path, data);
}

void MetaRepository::saveNextUserSeq(int nextUserSeq) const {
    json data = store.read(path);
    data["nextUserSeq"] = nextUserSeq;
    store.write(path, data);
}

void MetaRepository::loadNextIds(int& nextBranchId, int& nextAccountSeq, int& nextTransactionId) const {
    json data = store.read(path);
    if (data.empty()) {
        nextBranchId = 10001;
        nextAccountSeq = 1;
        nextTransactionId = 1001;
        return;
    }
    nextBranchId = data.value("nextBranchId", 10001);
    nextAccountSeq = data.value("nextAccountSeq", 1);
    nextTransactionId = data.value("nextTransactionId", 1001);
}

void MetaRepository::loadNextTransactionId(int& nextTransactionId) const {
    json data = store.read(path);
    if (data.empty()) {
        nextTransactionId = 1001;
        return;
    }
    nextTransactionId = data.value("nextTransactionId", 1001);
}

void MetaRepository::loadNextRequestId(int& nextRequestId) const {
    json data = store.read(path);
    if (data.empty()) {
        nextRequestId = 2001;
        return;
    }
    nextRequestId = data.value("nextRequestId", 2001);
}

void MetaRepository::loadNextUserSeq(int& nextUserSeq) const {
    json data = store.read(path);
    nextUserSeq = data.value("nextUserSeq", 1);
}

//••••••••••••••••••FileManager••••••••••••••••••
FileManager::FileManager()
    : branchRepo(store, "data/branches.json"),
      accountRepo(store, "data/accounts.json"),
      transactionRepo(store, "data/transactions.json"),
      requestRepo(store, "data/requests.json"),
      payaRepo(store, "data/paya_requests.json"),
      feeRepo(store, "data/fees.json"),
      metaRepo(store, "data/meta.json") {}

json FileManager::readFile(const string& path) const {
    return store.read(path);
}

void FileManager::writeFile(const string& path, const json& data) const {
    store.write(path, data);
}

vector<shared_ptr<Branch>> FileManager::loadBranches() const {
    return branchRepo.load();
}

vector<shared_ptr<Account>> FileManager::loadAccounts() const {
    return accountRepo.load();
}

vector<shared_ptr<Transaction>> FileManager::loadTransactions() const {
    return transactionRepo.load();
}

vector<shared_ptr<Request>> FileManager::loadRequests() const {
    return requestRepo.load();
}

vector<shared_ptr<PayaRequest>> FileManager::loadPayaRequests() const {
    return payaRepo.load();
}

void FileManager::saveBranches(const vector<shared_ptr<Branch>>& branches) const {
    branchRepo.save(branches);
}

void FileManager::saveAccounts(const vector<shared_ptr<Account>>& accounts) const {
    accountRepo.save(accounts);
}

void FileManager::saveTransactions(const vector<shared_ptr<Transaction>>& transactions) const {
    transactionRepo.save(transactions);
}

void FileManager::saveRequests(const vector<shared_ptr<Request>>& requests) const {
    requestRepo.save(requests);
}

void FileManager::savePayaRequests(const vector<shared_ptr<PayaRequest>>& requests) const {
    payaRepo.save(requests);
}

void FileManager::saveNextIds(int nextBranchId, int nextAccountSeq) const {
    metaRepo.saveNextIds(nextBranchId, nextAccountSeq);
}

void FileManager::saveNextTransactionId(int nextTransactionId) const {
    metaRepo.saveNextTransactionId(nextTransactionId);
}

void FileManager::saveNextRequestId(int nextRequestId) const {
    metaRepo.saveNextRequestId(nextRequestId);
}

void FileManager::saveNextUserSeq(int nextUserSeq) const {
    metaRepo.saveNextUserSeq(nextUserSeq);
}

void FileManager::loadNextIds(int& nextBranchId, int& nextAccountSeq, int& nextTransactionId) const {
    metaRepo.loadNextIds(nextBranchId, nextAccountSeq, nextTransactionId);
}

void FileManager::loadNextTransactionId(int& nextTransactionId) const {
    metaRepo.loadNextTransactionId(nextTransactionId);
}

void FileManager::loadNextRequestId(int& nextRequestId) const {
    metaRepo.loadNextRequestId(nextRequestId);
}

void FileManager::loadNextUserSeq(int& nextUserSeq) const {
    metaRepo.loadNextUserSeq(nextUserSeq);
}

double FileManager::getTransferFee() const {
    return feeRepo.getTransferFee();
}

double FileManager::getBalanceInquiryFee() const {
    return feeRepo.getBalanceInquiryFee();
}

void FileManager::saveFees(double transferFee, double balanceInquiryFee) const {
    feeRepo.save(transferFee, balanceInquiryFee);
}

void FileManager::resetFiles() {
    writeFile("data/transactions.json", json::object());
    writeFile("data/meta.json", json::object());
    writeFile("data/fees.json", json::object());
    writeFile("data/branches.json", json::object());
    writeFile("data/accounts.json", json::object());
    writeFile("data/users.json", json::object());
    writeFile("data/account_owners.json", json::object());
    writeFile("data/paya_requests.json", json::object());
    writeFile("data/requests.json", json::object());
}

//UserRepository
UserRepository::UserRepository(FileManager& fm) : fileManager(fm) {}

vector<shared_ptr<User>> UserRepository::loadUsers() const {
    vector<shared_ptr<User>> users;
    json data = fileManager.readFile(usersPath);
    if (data.empty() or !data.contains("users")) return users;
    for (auto& u : data["users"]) {
        auto user = make_shared<User>(
            u["nationalCode"].get<string>(),
            u["passwordHash"].get<string>()
        );
        user->setScore(u.value("score", 0));
        user->setRegistrationSeq(u.value("registrationSeq", 0));
        users.push_back(user);
    }
    return users;
}

void UserRepository::saveUsers(const vector<shared_ptr<User>>& users) const {
    json data;
    data["users"] = json::array();
    for (auto& u : users) {
        json entry;
        entry["nationalCode"] = u->getNationalCode();
        entry["passwordHash"] = u->getPasswordHash();
        entry["score"] = u->getScore();
        entry["registrationSeq"] = u->getRegistrationSeq();
        data["users"].push_back(entry);
    }
    fileManager.writeFile(usersPath, data);
}

//OwnershipRepositor
OwnershipRepository::OwnershipRepository(FileManager& fm) : fileManager(fm) {
    json data = fileManager.readFile(ownersPath);
    if (data.empty() or !data.contains("owners")) return;
    for (auto& entry : data["owners"])
        ownerOf[entry["account"].get<string>()] = entry["nationalCode"].get<string>();
}

void OwnershipRepository::persist() const {
    json data;
    data["owners"] = json::array();
    for (auto& entry : ownerOf) {
        json e;
        e["account"] = entry.first;
        e["nationalCode"] = entry.second;
        data["owners"].push_back(e);
    }
    fileManager.writeFile(ownersPath, data);
}

void OwnershipRepository::registerOwnership(const string& accountNumber, const string& nationalCode) {
    ownerOf[accountNumber] = nationalCode;
    persist();
}

void OwnershipRepository::removeOwnership(const string& accountNumber) {
    ownerOf.erase(accountNumber);
    persist();
}

bool OwnershipRepository::isOwnedBy(const string& accountNumber, const string& nationalCode) const {
    auto it = ownerOf.find(accountNumber);
    return it != ownerOf.end() and it->second == nationalCode;
}

vector<string> OwnershipRepository::getAccountsOf(const string& nationalCode) const {
    vector<string> result;
    for (auto& entry : ownerOf)
        if (entry.second == nationalCode) result.push_back(entry.first);
    return result;
}

void OwnershipRepository::reload() {
    ownerOf.clear();
    json data = fileManager.readFile(ownersPath);
    if (data.empty() or !data.contains("owners")) return;
    for (auto& entry : data["owners"])
        ownerOf[entry["account"].get<string>()] = entry["nationalCode"].get<string>();
}