#include "../include/admin.hpp"

#include <cctype> 
#include <iomanip>
#include <sstream>
#include <fstream>
#include <iostream>
#include <chrono>
#include <ctime>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>
#include <memory>
#include <algorithm>

//••••••••••••••••••InputQueue••••••••••••••••••
// reads input lines in a background thread and stores them in a queue.
// Lets you get input either waiting or with a timeout.
// literaly made a class to handle "timeout" error in withdarw :)
InputQueue::InputQueue() {
    reader = thread(&InputQueue::readLoop, this);
    reader.detach();
}

bool InputQueue::getLine(string& outLine, int timeoutSeconds) {
    unique_lock<mutex> lock(mtx);
    bool got = cv.wait_for(lock, chrono::seconds(timeoutSeconds), [this] { return !queue_.empty() or finished; });
    if (got and !queue_.empty()) {
        outLine = queue_.front();
        queue_.pop();
        return true;
    }
    return false;
}

bool InputQueue::getLineBlocking(string& outLine) {
    unique_lock<mutex> lock(mtx);
    cv.wait(lock, [this] { return !queue_.empty() or finished; });
    if (!queue_.empty()) {
        outLine = queue_.front();
        queue_.pop();
        return true;
    }
    return false;
}

void InputQueue::readLoop() {
    string line;
    while (getline(cin, line)) {
        {
            lock_guard<mutex> lock(mtx);
            queue_.push(line);
        }
        cv.notify_all();
    }
    {
        lock_guard<mutex> lock(mtx);
        finished = true;
    }
    cv.notify_all();
}

//••••••••••••••••••Transaction••••••••••••••••••
// Represents a single bank operation such as deposit, withdrawal, or transfer.
Transaction::Transaction(int id, const string& type, double amount, const string& fromAccount, const string& toAccount, double balanceAfter)
    : id(id), type(type), amount(amount), fromAccount(fromAccount), toAccount(toAccount), balanceAfter(balanceAfter) {
    auto now = chrono::system_clock::now();
    time_t currentTime = chrono::system_clock::to_time_t(now);
    tm localTime = *localtime(&currentTime);
    stringstream ss;
    ss << put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    timestamp = ss.str();
}

int Transaction::getId() const { return id; }
string Transaction::getType() const { return type; }
double Transaction::getAmount() const { return amount; }
string Transaction::getFromAccount() const { return fromAccount; }
string Transaction::getToAccount() const { return toAccount; }
string Transaction::getTimestamp() const { return timestamp; }
double Transaction::getBalanceAfter() const { return balanceAfter; }

void Transaction::setTimestamp(const string& ts) { timestamp = ts; }
void Transaction::setBalanceAfter(double b) { balanceAfter = b; }

//••••••••••••••••••Account••••••••••••••••••
// Represents a bank account with balance, security info, and transaction history.
// Handles basic operations like deposit, withdrawal, and transfer.
Account::Account(const string& accNumber, int branchId, const string& passwordHash, double initialBalance)
    : accountNumber(accNumber), branchId(branchId), balance(initialBalance), passwordHash(passwordHash), active(true) {}

string Account::getAccountNumber() const { return accountNumber; }
int Account::getBranchId() const { return branchId; }
double Account::getBalance() const { return balance; }
bool Account::isActive() const { return active; }
string Account::getPasswordHash() const { return passwordHash; }
const vector<shared_ptr<Transaction>>& Account::getTransactions() const { return transactions; }

void Account::setActive(bool status) { active = status; }

bool Account::verifyPassword(const string& hashedPassword) const {
    return passwordHash == hashedPassword;
}

void Account::deposit(double amount) { balance += amount; }

VoidResult Account::withdraw(double amount, const string& hashedPassword) {
    if (!verifyPassword(hashedPassword)) {
        return VoidResult::failure(ServiceError(ServiceError::Code::WrongPassword));
    }
    if (balance < amount) {
        return VoidResult::failure(ServiceError(ServiceError::Code::InsufficientFunds));
    }
    balance -= amount;
    return VoidResult::success();
}

VoidResult Account::transfer(Account& toAccount, double amount, double fee, const string& hashedPassword) {
    if (!verifyPassword(hashedPassword)) {
        return VoidResult::failure(ServiceError(ServiceError::Code::WrongPassword));
    }
    if (balance < amount + fee) {
        return VoidResult::failure(ServiceError(ServiceError::Code::InsufficientFunds));
    }
    balance -= (amount + fee);
    toAccount.deposit(amount);
    return VoidResult::success();
}

void Account::addTransaction(shared_ptr<Transaction> tx) { transactions.push_back(tx); }
void Account::clearTransactions() { transactions.clear(); }

//••••••••••••••••••Branch••••••••••••••••••
// Represents a bank branch that manages accounts under it.
// Only stores and organizes accounts.
Branch::Branch(int id, const string& name) : id(id), name(name) {}

int Branch::getId() const { return id; }
string Branch::getName() const { return name; }
const vector<shared_ptr<Account>>& Branch::getAccounts() const { return accounts; }

void Branch::setName(const string& n) { name = n; }

void Branch::addAccount(shared_ptr<Account> account) {
    accounts.push_back(account);
}

void Branch::removeAccount(string& accountNumber) {
    for (auto it = accounts.begin(); it != accounts.end(); ++it) {
        if ((*it)->getAccountNumber() == accountNumber) {
            accounts.erase(it);
            break;
        }
    }
}

int Branch::getAccountCount() const { return static_cast<int>(accounts.size()); }

shared_ptr<Account> Branch::findAccount(const string& accountNumber) const {
    for (const auto& acc : accounts)
        if (acc->getAccountNumber() == accountNumber) return acc;
    return nullptr;
}

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

//••••••••••••••••••FileManager••••••••••••••••••
FileManager::FileManager()
    : branchRepo(store, "data/branches.json"),
      accountRepo(store, "data/accounts.json"),
      transactionRepo(store, "data/transactions.json"),
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

void FileManager::saveBranches(const vector<shared_ptr<Branch>>& branches) const {
    branchRepo.save(branches);
}

void FileManager::saveAccounts(const vector<shared_ptr<Account>>& accounts) const {
    accountRepo.save(accounts);
}

void FileManager::saveTransactions(const vector<shared_ptr<Transaction>>& transactions) const {
    transactionRepo.save(transactions);
}

void FileManager::saveNextIds(int nextBranchId, int nextAccountSeq) const {
    metaRepo.saveNextIds(nextBranchId, nextAccountSeq);
}

void FileManager::saveNextTransactionId(int nextTransactionId) const {
    metaRepo.saveNextTransactionId(nextTransactionId);
}

void FileManager::loadNextIds(int& nextBranchId, int& nextAccountSeq, int& nextTransactionId) const {
    metaRepo.loadNextIds(nextBranchId, nextAccountSeq, nextTransactionId);
}

void FileManager::loadNextTransactionId(int& nextTransactionId) const {
    metaRepo.loadNextTransactionId(nextTransactionId);
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
}

//••••••••••••••••••AuthService••••••••••••••••••
// Handles password hashing, verification, and user password input.
// Checks user passwords using SHA-256 hashes.
string AuthService::hashPassword(const string& rawPassword) const {
    return SHA256::hash(rawPassword);
}

string AuthService::promptPassword(InputQueue& inputQueue, string enterPass) {
    cout << enterPass << flush;
    string password;
    inputQueue.getLineBlocking(password);
    return password;
}

bool AuthService::verifyPassword(const shared_ptr<Account>& account, const string& rawPassword) {
    return hashPassword(rawPassword) == account->getPasswordHash();
}

bool AuthService::promptPasswordWithTimeout(InputQueue& inputQueue, string& outPassword, int timeoutSeconds) const {
    cout << "Enter password: " << flush;
    return inputQueue.getLine(outPassword, timeoutSeconds);
}

string AuthService::generateHash(const string& rawPassword) const {
    return hashPassword(rawPassword);
}

//••••••••••••••••••FeeManager••••••••••••••••••
// Stores and manages transfer and balance inquiry fees.
// Loads and saves fee configuration through FileManager.
FeeManager::FeeManager(FileManager& fm) : fileManager(fm) {}

double FeeManager::getTransferFee() const {
    return fileManager.getTransferFee();
}

double FeeManager::getBalanceInquiryFee() const {
    return fileManager.getBalanceInquiryFee();
}

VoidResult FeeManager::setTransferFee(double amount) {
    if (amount < 0) {
        return VoidResult::failure(ServiceError(ServiceError::Code::InvalidAmount, "Invalid fee amount."));
    }
    double currentBalanceFee = fileManager.getBalanceInquiryFee();
    fileManager.saveFees(amount, currentBalanceFee);
    cout << fixed << setprecision(2) << "Transfer fee set to " << amount << endl;
    return VoidResult::success();
}

VoidResult FeeManager::setBalanceInquiryFee(double amount) {
    if (amount < 0) {
        return VoidResult::failure(ServiceError(ServiceError::Code::InvalidAmount, "Invalid fee amount."));
    }
    double currentTransferFee = fileManager.getTransferFee();
    fileManager.saveFees(currentTransferFee, amount);
    cout << fixed << setprecision(2) << "Balance inquiry fee set to " << amount << endl;
    return VoidResult::success();
}

void FeeManager::showFees() const {
    cout << fixed << setprecision(2) << "Transfer fee: " << fileManager.getTransferFee() << endl;
    cout << fixed << setprecision(2) << "Balance inquiry fee: " << fileManager.getBalanceInquiryFee() << endl;
}

//••••••••••••••••••AccountService••••••••••••••••••
// Manages bank accounts and branches.
// Creates, updates, and removes accounts.
AccountService::AccountService(FileManager& fm, AuthService& as) : fileManager(fm), authService(as) {
    int loadedNextTransactionId;
    fileManager.loadNextIds(nextBranchId, nextAccountSeq, loadedNextTransactionId);
    branches = fileManager.loadBranches();
    accounts = fileManager.loadAccounts();
}

void AccountService::createBranch(string& name) {
    branches.push_back(make_shared<Branch>(nextBranchId, name));
    cout << "Branch created. ID: " << nextBranchId << endl;
    nextBranchId++;
    fileManager.saveBranches(branches);
    fileManager.saveNextIds(nextBranchId, nextAccountSeq);
}

void AccountService::listBranches() const {
    if (branches.empty()) {
        cout << "No branches exist." << endl;
        return;
    }
    for (auto& b : branches)
        cout << b->getId() << " | " << b->getName() << endl;
}

shared_ptr<Account> AccountService::createUserAccount(const string& passwordHash) {
    string accountNumber = generateAccountNumber();
    auto account = make_shared<Account>(accountNumber, 0, passwordHash, 0.0);
    accounts.push_back(account);
    fileManager.saveAccounts(accounts);
    fileManager.saveNextIds(nextBranchId, nextAccountSeq);
    return account;
}

Result<shared_ptr<Account>> AccountService::createAccount(int branchId, InputQueue& inputQueue) {
    auto branch = findBranch(branchId);
    if (!branch) {
        return Result<shared_ptr<Account>>::failure(ServiceError(ServiceError::Code::NotFound, "Branch not found."));
    }
    string accountNumber = generateAccountNumber();
    string password = authService.promptPassword(inputQueue);
    string passwordHash = authService.generateHash(password);
    auto account = make_shared<Account>(accountNumber, branchId, passwordHash, 0.0);
    accounts.push_back(account);
    branch->addAccount(account);
    fileManager.saveAccounts(accounts);
    fileManager.saveBranches(branches);
    fileManager.saveNextIds(nextBranchId, nextAccountSeq);
    return Result<shared_ptr<Account>>::success(account);
}

VoidResult AccountService::closeAccount(string& accountNumber, InputQueue& inputQueue) {
    auto account = findAccount(accountNumber);
    if (!account) {
        return VoidResult::failure(ServiceError(ServiceError::Code::NotFound, "Account not found."));
    }
    string password = authService.promptPassword(inputQueue);
    if (!authService.verifyPassword(account, password)) {
        return VoidResult::failure(ServiceError(ServiceError::Code::WrongPassword));
    }
    account->setActive(false);
    fileManager.saveAccounts(accounts);
    return VoidResult::success();
}

VoidResult AccountService::deleteAccount(string& accountNumber, InputQueue& inputQueue) {
    auto account = findAccount(accountNumber);
    if (!account) {
        return VoidResult::failure(ServiceError(ServiceError::Code::NotFound, "Account not found."));
    }
    string password = authService.promptPassword(inputQueue);
    if (!authService.verifyPassword(account, password)) {
        return VoidResult::failure(ServiceError(ServiceError::Code::WrongPassword));
    }
    eraseAccount(accountNumber);
    return VoidResult::success();
}

void AccountService::removeAccountNoAuth(const string& accountNumber) {
    eraseAccount(accountNumber);
}

void AccountService::listAccounts() const {
    if (accounts.empty()) {
        cout << "No accounts found." << endl;
        return;
    }
    for (auto& a : accounts)
        cout << a->getAccountNumber()
             << " | Branch: " << a->getBranchId()
             << " | Active: " << (a->isActive() ? "Yes" : "No")
             << " | Balance: " << fixed << setprecision(2) << a->getBalance() << endl;
}

void AccountService::reset() {
    accounts.clear();
    branches.clear();
    nextBranchId = 10001;
    nextAccountSeq = 1;
    fileManager.saveNextIds(nextBranchId, nextAccountSeq);
}

shared_ptr<Account> AccountService::getAccount(string& accountNumber) {
    return findAccount(accountNumber);
}

shared_ptr<Account> AccountService::getAccount(const string& accountNumber) const {
    for (auto& a : accounts)
        if (a->getAccountNumber() == accountNumber) return a;
    return nullptr;
}

shared_ptr<Branch> AccountService::getBranch(int branchId) {
    return findBranch(branchId);
}

const vector<shared_ptr<Account>>& AccountService::getAccounts() const { return accounts; }
const vector<shared_ptr<Branch>>& AccountService::getBranches() const { return branches; }

shared_ptr<Account> AccountService::findAccount(string& accountNumber) {
    for (auto& a : accounts)
        if (a->getAccountNumber() == accountNumber) return a;
    return nullptr;
}

shared_ptr<Branch> AccountService::findBranch(int branchId) {
    for (auto& b : branches)
        if (b->getId() == branchId) return b;
    return nullptr;
}

string AccountService::generateAccountNumber() {
    ostringstream os;
    int seq = nextAccountSeq++;
    os << "5022-"
       << setw(4) << setfill('0') << ((seq / 100000000) % 10000) << "-"
       << setw(4) << setfill('0') << ((seq / 10000) % 10000) << "-"
       << setw(4) << setfill('0') << (seq % 10000);
    return os.str();
}

void AccountService::eraseAccount(const string& accountNumber) {
    string accNum = accountNumber;
    for (auto& b : branches)
        b->removeAccount(accNum);
    for (auto it = accounts.begin(); it != accounts.end(); ++it) {
        if ((*it)->getAccountNumber() == accountNumber) {
            accounts.erase(it);
            break;
        }
    }
    fileManager.saveAccounts(accounts);
    fileManager.saveBranches(branches);
}

//••••••••••••••••••TransactionService••••••••••••••••••
// Handles money operations and transaction history.
TransactionService::TransactionService(FileManager& fm, AccountService& as, AuthService& auth, FeeManager& fee)
    : accountService(as), fileManager(fm), authService(auth), feeManager(fee) {
    fileManager.loadNextTransactionId(nextTransactionId);
    transactions = fileManager.loadTransactions();
    for (auto& tx : transactions) {
        auto acc = accountService.getAccount(tx->getFromAccount());
        if (acc) acc->addTransaction(tx);
        if (tx->getType() == "TRANSFER") {
            auto toAcc = accountService.getAccount(tx->getToAccount());
            if (toAcc) toAcc->addTransaction(tx);
        }
    }
}

Result<shared_ptr<Transaction>> TransactionService::deposit(const string& accountNumber, double amount) {
    auto account = accountService.getAccount(accountNumber);
    if (!account) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::NotFound, "Account not found."));
    }
    if (!account->isActive()) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::AccountInactive));
    }
    account->deposit(amount);
    auto tx = make_shared<Transaction>(nextTransactionId++, "DEPOSIT", amount, accountNumber, "", account->getBalance());
    account->addTransaction(tx);
    transactions.push_back(tx);
    fileManager.saveAccounts(accountService.getAccounts());
    fileManager.saveTransactions(transactions);
    fileManager.saveNextTransactionId(nextTransactionId);
    return Result<shared_ptr<Transaction>>::success(tx);
}

Result<shared_ptr<Transaction>> TransactionService::withdraw(const string& accountNumber, double amount, InputQueue& inputQueue, const string& wrongPasswordMessage) {
    auto account = accountService.getAccount(accountNumber);
    if (!account) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::NotFound, "Account not found."));
    }
    if (!account->isActive()) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::AccountInactive));
    }

    string password;
    bool gotPassword = authService.promptPasswordWithTimeout(inputQueue, password, 30);
    if (!gotPassword) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::Timeout, "Password timeout. Transaction cancelled."));
    }

    string hashedPassword = authService.generateHash(password);
    auto withdrawResult = account->withdraw(amount, hashedPassword);
    if (!withdrawResult.isOk()) {
        auto err = withdrawResult.getError();
        if (err.code == ServiceError::Code::WrongPassword) err.detail = wrongPasswordMessage;
        return Result<shared_ptr<Transaction>>::failure(err);
    }

    if (account->getBalance() < amount) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::InsufficientFunds));
    }

    auto tx = make_shared<Transaction>(nextTransactionId++, "WITHDRAWAL", amount, accountNumber, "", account->getBalance());
    account->addTransaction(tx);
    transactions.push_back(tx);
    fileManager.saveAccounts(accountService.getAccounts());
    fileManager.saveTransactions(transactions);
    fileManager.saveNextTransactionId(nextTransactionId);
    return Result<shared_ptr<Transaction>>::success(tx);
}

Result<shared_ptr<Transaction>> TransactionService::transfer(const string& from, const string& to, double amount, InputQueue& inputQueue, const string& wrongPasswordMessage) {
    if (amount <= 0) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::InvalidAmount));
    }
    auto fromAccount = accountService.getAccount(from);
    if (!fromAccount) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::NotFound, "Source account not found."));
    }
    if (!fromAccount->isActive()) {
        return Result<shared_ptr<Transaction>>::failure(
            ServiceError(ServiceError::Code::AccountInactive));
    }
    auto toAccount = accountService.getAccount(to);
    if (!toAccount) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::NotFound, "Destination account not found."));
    }
    if (!toAccount->isActive()) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::DestinationInactive));
    }
    string password = authService.promptPassword(inputQueue);
    double fee = feeManager.getTransferFee();
    string hashedPassword = authService.generateHash(password);
    auto transferResult = fromAccount->transfer(*toAccount, amount, fee, hashedPassword);
    if (!transferResult.isOk()) {
        auto err = transferResult.getError();
        if (err.code == ServiceError::Code::WrongPassword) err.detail = wrongPasswordMessage;
        return Result<shared_ptr<Transaction>>::failure(err);
    }

    if (fromAccount->getBalance() < amount + fee) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::InsufficientFunds));
    }

    auto tx = make_shared<Transaction>(nextTransactionId++, "TRANSFER", amount, from, to, fromAccount->getBalance());
    fromAccount->addTransaction(tx);
    toAccount->addTransaction(tx);
    transactions.push_back(tx);

    if (fee > 0.0) {
        auto feeTx = make_shared<Transaction>(nextTransactionId++, "FEE", fee, from, "", fromAccount->getBalance());
        fromAccount->addTransaction(feeTx);
        transactions.push_back(feeTx);
    }

    fileManager.saveAccounts(accountService.getAccounts());
    fileManager.saveTransactions(transactions);
    fileManager.saveNextTransactionId(nextTransactionId);
    return Result<shared_ptr<Transaction>>::success(tx);
}

Result<shared_ptr<Account>> TransactionService::getBalance(const string& accountNumber) {
    auto account = accountService.getAccount(accountNumber);
    if (!account) {
        return Result<shared_ptr<Account>>::failure(ServiceError(ServiceError::Code::NotFound, "Account not found."));
    }
    double fee = feeManager.getBalanceInquiryFee();
    if (account->getBalance() < fee) {
        return Result<shared_ptr<Account>>::failure(ServiceError(ServiceError::Code::InsufficientFunds));
    }
    if (fee > 0.0) {
        account->deposit(-fee);
        auto tx = make_shared<Transaction>(nextTransactionId++, "FEE", fee, accountNumber, "", account->getBalance());
        account->addTransaction(tx);
        transactions.push_back(tx);
        fileManager.saveAccounts(accountService.getAccounts());
        fileManager.saveTransactions(transactions);
        fileManager.saveNextTransactionId(nextTransactionId);
    }
    cout << fixed << setprecision(2) << "Balance inquiry fee: " << fee << endl;
    cout << "Balance: " << fixed << setprecision(2) << account->getBalance() << endl;
    cout << "Active: " << (account->isActive() ? "Yes" : "No") << endl;
    cout << "Branch: " << account->getBranchId() << endl;

    return Result<shared_ptr<Account>>::success(account);
}

Result<shared_ptr<Account>> TransactionService::getBalanceForUser(const string& accountNumber) {
    auto account = accountService.getAccount(accountNumber);
    if (!account) {
        return Result<shared_ptr<Account>>::failure(ServiceError(ServiceError::Code::NotFound, "Account not found."));
    }
    double fee = feeManager.getBalanceInquiryFee();
    if (account->getBalance() < fee) {
        return Result<shared_ptr<Account>>::failure(ServiceError(ServiceError::Code::InsufficientFunds));
    }
    if (fee > 0.0) {
        account->deposit(-fee);
        auto tx = make_shared<Transaction>(nextTransactionId++, "FEE", fee, accountNumber, "", account->getBalance());
        account->addTransaction(tx);
        transactions.push_back(tx);
        fileManager.saveAccounts(accountService.getAccounts());
        fileManager.saveTransactions(transactions);
        fileManager.saveNextTransactionId(nextTransactionId);
    }
    cout << fixed << setprecision(2) << "Balance inquiry fee: " << fee << endl;
    cout << "Balance: " << fixed << setprecision(2) << account->getBalance() << endl;

    return Result<shared_ptr<Account>>::success(account);
}

void TransactionService::getHistory(string& accountNumber) const {
    auto account = accountService.getAccount(accountNumber);
    if (!account) {
        cout << "Error: Account not found." << endl;
        return;
    }
    const auto& txs = account->getTransactions();
    if (txs.empty()) {
        cout << "No transactions found for this account." << endl;
        return;
    }
    double runningBalance = 0.0;
    for (const auto& tx : account->getTransactions()) {
        string sign;
        if (tx->getType() == "DEPOSIT") {
            runningBalance += tx->getAmount(); sign = "+";
        } else if (tx->getType() == "WITHDRAWAL") {
            runningBalance -= tx->getAmount(); sign = "-";
        } else if (tx->getType() == "FEE") {
            runningBalance -= tx->getAmount(); sign = "-";
        } else if (tx->getType() == "TRANSFER") {
            if (tx->getFromAccount() == accountNumber) { runningBalance -= tx->getAmount(); sign = "-"; }
            else { runningBalance += tx->getAmount(); sign = "+"; }
        }
        cout << tx->getId()
             << " | " << tx->getTimestamp()
             << " | " << left << setw(10) << tx->getType()
             << " | " << sign << fixed << setprecision(2) << tx->getAmount()
             << " | Balance: " << runningBalance << endl;
    }
}

void TransactionService::getTransaction(int id) const {
    for (const auto& tx : transactions) {
        if (tx->getId() == id) {
            cout << "ID: " << tx->getId() << endl;
            cout << "Time: " << tx->getTimestamp() << endl;
            cout << "Type: " << tx->getType() << endl;
            cout << "From: " << tx->getFromAccount() << endl;
            cout << "To: " << tx->getToAccount() << endl;
            cout << "Amount: " << fixed << setprecision(2) << tx->getAmount() << endl;
            cout << "Balance after: " << fixed << setprecision(2) << tx->getBalanceAfter() << endl;
            return;
        }
    }
    cout << "Error: Transaction not found." << endl;
}

VoidResult TransactionService::clearHistory(string& accountNumber, InputQueue& inputQueue) {
    auto account = accountService.getAccount(accountNumber);
    if (!account) {
        return VoidResult::failure(ServiceError(ServiceError::Code::NotFound, "Account not found."));
    }
    string password = authService.promptPassword(inputQueue);
    string hashedPassword = authService.generateHash(password);
    if (!account->verifyPassword(hashedPassword)) {
        return VoidResult::failure(ServiceError(ServiceError::Code::WrongPassword));
    }
    for (const auto& t : account->getTransactions()) {
        for (auto it = transactions.begin(); it != transactions.end(); ++it) {
            if ((*it)->getId() == t->getId()) {
                transactions.erase(it);
                break;
            }
        }
        account->clearTransactions();
    }
    cout << "History cleared for " << accountNumber << "." << endl;
    fileManager.saveAccounts(accountService.getAccounts());
    fileManager.saveTransactions(transactions);
    return VoidResult::success();
}

void TransactionService::resetAll() {
    transactions.clear();
    accountService.reset();
    nextTransactionId = 1001;
    fileManager.resetFiles();
}

const vector<shared_ptr<Transaction>>& TransactionService::getAllTransactions() const {
    return transactions;
}

//••••••••••••••••••Input_Handling••••••••••••••••••

bool CommandParser::isValidAccountNumber(const string& acc) {
    if (acc.size() != 19) return false;
    if (acc[4] != '-' || acc[9] != '-' || acc[14] != '-') return false;
    for (int i = 0; i < 19; i++) {
        if (i == 4 || i == 9 || i == 14) continue;
        if (!isdigit(acc[i])) return false;
    }
    return true;
}

VoidResult CommandParser::requireArgs(const string& args, int count) {
    if (args.empty()) {
        return VoidResult::failure(ServiceError(ServiceError::Code::InvalidArguments));
    }
    istringstream iss(args);
    string token;
    for (int i = 0; i < count; i++) {
        if (!(iss >> token)) {
            return VoidResult::failure(ServiceError(ServiceError::Code::InvalidArguments));
        }
    }
    return VoidResult::success();
}

VoidResult CommandParser::parseAccountAmount(const string& args, string& acc, double& amount) {
    istringstream iss(args);
    if (!(iss >> acc >> amount)) {
        return VoidResult::failure(ServiceError(ServiceError::Code::InvalidArguments));
    }
    if (!isValidAccountNumber(acc)) {
        return VoidResult::failure(ServiceError(ServiceError::Code::InvalidArguments));
    }
    if (amount <= 0) {
        return VoidResult::failure(ServiceError(ServiceError::Code::InvalidAmount));
    }
    return VoidResult::success();
}

VoidResult CommandParser::parseTransfer(const string& args, string& from, string& to, double& amount) {
    istringstream iss(args);
    if (!(iss >> from >> to >> amount)) {
        return VoidResult::failure(ServiceError(ServiceError::Code::InvalidArguments));
    }
    if (!isValidAccountNumber(from) || !isValidAccountNumber(to)) {
        return VoidResult::failure(ServiceError(ServiceError::Code::InvalidArguments));
    }
    if (amount <= 0) {
        return VoidResult::failure(ServiceError(ServiceError::Code::InvalidAmount));
    }
    return VoidResult::success();
}

bool CommandParser::confirm(InputQueue& inputQueue, const string& prompt) {
    string answer;
    cout << prompt << flush;
    inputQueue.getLineBlocking(answer);
    return answer == "yes";
}

AdminCommandContext::AdminCommandContext(AccountService& as, TransactionService& ts, AuthService& authS, FeeManager& fm, InputQueue& iq)
    : accounts(as), transactions(ts), auth(authS), fees(fm), input(iq) {}

void CreateBranchCommand::execute(const string& args, AdminCommandContext& ctx) {
    string name = args;
    if (!name.empty() and name.front() == '"') name = name.substr(1, name.size() - 2);
    ctx.accounts.createBranch(name);
}

void ListBranchesCommand::execute(const string&, AdminCommandContext& ctx) {
    ctx.accounts.listBranches();
}

void CreateAccountCommand::execute(const string& args, AdminCommandContext& ctx) {
    auto argsCheck = CommandParser::requireArgs(args);
    if (!argsCheck.isOk()) { ErrorReporter::report(argsCheck.getError()); return; }

    istringstream iss(args);
    int branchId;
    if (!(iss >> branchId)) {
        ErrorReporter::report(ServiceError(ServiceError::Code::InvalidArguments));
        return;
    }

    auto result = ctx.accounts.createAccount(branchId, ctx.input);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }
    cout << "Account created. Number: " << result.getValue()->getAccountNumber() << endl;
}

void CloseAccountCommand::execute(const string& args, AdminCommandContext& ctx) {
    auto argsCheck = CommandParser::requireArgs(args);
    if (!argsCheck.isOk()) { ErrorReporter::report(argsCheck.getError()); return; }

    string acc = args;
    auto result = ctx.accounts.closeAccount(acc, ctx.input);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }
    cout << "Account closed." << endl;
}

void DeleteAccountCommand::execute(const string& args, AdminCommandContext& ctx) {
    auto argsCheck = CommandParser::requireArgs(args);
    if (!argsCheck.isOk()) { ErrorReporter::report(argsCheck.getError()); return; }

    string acc = args;
    auto result = ctx.accounts.deleteAccount(acc, ctx.input);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }
    cout << "Account deleted." << endl;
}

void ListAccountsCommand::execute(const string&, AdminCommandContext& ctx) {
    ctx.accounts.listAccounts();
}

void DepositCommand::execute(const string& args, AdminCommandContext& ctx) {
    string acc; double amount;
    auto parseCheck = CommandParser::parseAccountAmount(args, acc, amount);
    if (!parseCheck.isOk()) { ErrorReporter::report(parseCheck.getError()); return; }

    auto result = ctx.transactions.deposit(acc, amount);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }

    auto tx = result.getValue();
    cout << "Transaction ID: " << tx->getId() << endl;
    cout << "New balance: " << fixed << setprecision(2) << tx->getBalanceAfter() << endl;
}

void WithdrawCommand::execute(const string& args, AdminCommandContext& ctx) {
    string acc; double amount;
    auto parseCheck = CommandParser::parseAccountAmount(args, acc, amount);
    if (!parseCheck.isOk()) { ErrorReporter::report(parseCheck.getError()); return; }

    auto result = ctx.transactions.withdraw(acc, amount, ctx.input);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }

    auto tx = result.getValue();
    cout << "Transaction ID: " << tx->getId() << endl;
    cout << "New balance: " << fixed << setprecision(2) << tx->getBalanceAfter() << endl;
}

void TransferCommand::execute(const string& args, AdminCommandContext& ctx) {
    string from, to; double amount;
    auto parseCheck = CommandParser::parseTransfer(args, from, to, amount);
    if (!parseCheck.isOk()) { ErrorReporter::report(parseCheck.getError()); return; }

    double feeCharged = ctx.fees.getTransferFee();
    auto result = ctx.transactions.transfer(from, to, amount, ctx.input);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }

    auto tx = result.getValue();
    cout << "Transaction ID: " << tx->getId() << endl;
    cout << fixed << setprecision(2) << "Transfer fee: " << feeCharged << endl;
    cout << "New balance: " << fixed << setprecision(2) << tx->getBalanceAfter() << endl;
}

void GetBalanceCommand::execute(const string& args, AdminCommandContext& ctx) {
    auto argsCheck = CommandParser::requireArgs(args);
    if (!argsCheck.isOk()) { ErrorReporter::report(argsCheck.getError()); return; }

    string acc = args;
    // double fee = ctx.fees.getBalanceInquiryFee();
    auto result = ctx.transactions.getBalance(acc);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }

    // auto account = result.getValue();
    // cout << fixed << setprecision(2) << "Balance inquiry fee: " << fee << endl;
    // cout << "Balance: " << fixed << setprecision(2) << account->getBalance() << endl;
    // cout << "Active: " << (account->isActive() ? "Yes" : "No") << endl;
    // cout << "Branch: " << account->getBranchId() << endl;
}

void GetHistoryCommand::execute(const string& args, AdminCommandContext& ctx) {
    auto argsCheck = CommandParser::requireArgs(args);
    if (!argsCheck.isOk()) { ErrorReporter::report(argsCheck.getError()); return; }
    string acc = args;
    ctx.transactions.getHistory(acc);
}

void GetTransactionCommand::execute(const string& args, AdminCommandContext& ctx) {
    auto argsCheck = CommandParser::requireArgs(args);
    if (!argsCheck.isOk()) { ErrorReporter::report(argsCheck.getError()); return; }
    istringstream iss(args);
    int id;
    if (!(iss >> id)) {
        ErrorReporter::report(ServiceError(ServiceError::Code::InvalidArguments));
        return;
    }
    ctx.transactions.getTransaction(id);
}

void ClearHistoryCommand::execute(const string& args, AdminCommandContext& ctx) {
    auto argsCheck = CommandParser::requireArgs(args);
    if (!argsCheck.isOk()) { ErrorReporter::report(argsCheck.getError()); return; }

    string acc = args;
    auto result = ctx.transactions.clearHistory(acc, ctx.input);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }
    // cout << "History cleared for " << acc << "." << endl;
}

void ResetAllCommand::execute(const string&, AdminCommandContext& ctx) {
    if (CommandParser::confirm(ctx.input)) {
        ctx.transactions.resetAll();
        cout << "All data cleared." << endl;
    } else {
        cout << "Cancelled." << endl;
    }
}

void SetTransferFeeCommand::execute(const string& args, AdminCommandContext& ctx) {
    istringstream iss(args);
    double amount;
    if (!(iss >> amount)) {
        ErrorReporter::report(ServiceError(ServiceError::Code::InvalidAmount, "Invalid fee amount."));
        return;
    }
    auto result = ctx.fees.setTransferFee(amount);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }
}

void SetBalanceInquiryFeeCommand::execute(const string& args, AdminCommandContext& ctx) {
    istringstream iss(args);
    double amount;
    if (!(iss >> amount)) {
        ErrorReporter::report(ServiceError(ServiceError::Code::InvalidAmount, "Invalid fee amount."));
        return;
    }
    auto result = ctx.fees.setBalanceInquiryFee(amount);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }
}

void ShowFeesCommand::execute(const string&, AdminCommandContext& ctx) {
    ctx.fees.showFees();
}

HandleAdminCommand::HandleAdminCommand() {
    registerCommands();
}

void HandleAdminCommand::registerCommands() {
    commands["create_branch"] = make_unique<CreateBranchCommand>();
    commands["list_branches"] = make_unique<ListBranchesCommand>();
    commands["create_account"] = make_unique<CreateAccountCommand>();
    commands["close_account"] = make_unique<CloseAccountCommand>();
    commands["delete_account"] = make_unique<DeleteAccountCommand>();
    commands["list_accounts"] = make_unique<ListAccountsCommand>();
    commands["deposit"] = make_unique<DepositCommand>();
    commands["withdraw"] = make_unique<WithdrawCommand>();
    commands["transfer"] = make_unique<TransferCommand>();
    commands["get_balance"] = make_unique<GetBalanceCommand>();
    commands["get_history"] = make_unique<GetHistoryCommand>();
    commands["get_transaction"] = make_unique<GetTransactionCommand>();
    commands["clear_history"] = make_unique<ClearHistoryCommand>();
    commands["reset_all"] = make_unique<ResetAllCommand>();
    commands["set_transfer_fee"] = make_unique<SetTransferFeeCommand>();
    commands["set_balance_inquiry_fee"] = make_unique<SetBalanceInquiryFeeCommand>();
    commands["show_fees"] = make_unique<ShowFeesCommand>();
}

bool HandleAdminCommand::isAdminCommand(const string& command) {
    return commands.find(command) != commands.end();
}

bool HandleAdminCommand::requireArgs(const string& args, int count) {
    auto result = CommandParser::requireArgs(args, count);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return false; }
    return true;
}

void HandleAdminCommand::handleCommand(string& line, AccountService& as, TransactionService& ts, AuthService& auth, FeeManager& fm, InputQueue& inputQueue) {
    istringstream iss(line);
    string command, args;
    iss >> command;
    getline(iss >> ws, args);

    auto it = commands.find(command);
    if (it == commands.end()) return;

    AdminCommandContext ctx(as, ts, auth, fm, inputQueue);
    it->second->execute(args, ctx);
}