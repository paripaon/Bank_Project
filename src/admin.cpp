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
#include <random>

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

//••••••••••••••••••FeeService••••••••••••••••••
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

VoidResult AccountService::listBranches() const {
    if (branches.empty()) {
        return VoidResult::failure(ServiceError(ServiceError::Code::NotFound, "No branches available."));
    }
    for (auto& b : branches)
        cout << b->getId() << " | " << b->getName() << endl;
    return VoidResult::success();
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

//••••••••••••••••••RequestService••••••••••••••••••
RequestService::RequestService(FileManager& fm, AccountService& as) : fileManager(fm), accountService(as) { 
        fileManager.loadNextRequestId(nextRequestId); 
        requests = fileManager.loadRequests();
    }

bool RequestService::hasActiveOrPendingInBranch(const string& nationalCode, int branchId) const {
    for (auto& r : requests) {
        if (r->getNationalCode() == nationalCode and r->getBranchId() == branchId) {
            if (r->getStatus() == "PENDING" or r->getStatus() == "APPROVED" or r->getStatus() == "ACTIVATED") return true;
        }
    }
    return false;
}

Result<shared_ptr<Request>> RequestService::createRequest(const string& nationalCode, int branchId) {
    auto branch = accountService.getBranch(branchId);
    if (!branch) {
        return Result<shared_ptr<Request>>::failure(ServiceError(ServiceError::Code::NotFound, "Branch not found."));
    }
    if (hasActiveOrPendingInBranch(nationalCode, branchId)) {
        return Result<shared_ptr<Request>>::failure(ServiceError(ServiceError::Code::AlreadyExists, "You already have a pending or active account in this branch."));
    }
    auto req = make_shared<Request>(nextRequestId++, nationalCode, branchId, "PENDING");
    requests.push_back(req);
    fileManager.saveRequests(requests);
    fileManager.saveNextRequestId(nextRequestId);
    return Result<shared_ptr<Request>>::success(req);
}

Result<vector<shared_ptr<Request>>> RequestService::getRequestsOf(const string& nationalCode) const {
    vector<shared_ptr<Request>> result;
    for (auto& r : requests)
        if (r->getNationalCode() == nationalCode) result.push_back(r);
    return Result<vector<shared_ptr<Request>>>::success(result);
}

VoidResult RequestService::cancelRequest(int requestId, const string& nationalCode) {
    auto req = findRequest(requestId);
    if (!req) {
        return VoidResult::failure(ServiceError(ServiceError::Code::NotFound, "Request not found."));
    }
    if (req->getNationalCode() != nationalCode) {
        return VoidResult::failure(ServiceError(ServiceError::Code::NotOwner, "Request does not belong to user."));
    }
    if (req->getStatus() != "PENDING" and req->getStatus() != "APPROVED") {
        return VoidResult::failure(ServiceError(ServiceError::Code::Custom, "Request is not cancellable."));
    }
    req->setStatus("CANCELLED");
    fileManager.saveRequests(requests);
    return VoidResult::success();
}

Result<shared_ptr<Request>> RequestService::prepareActivation(int requestId, const string& nationalCode) {
    auto req = findRequest(requestId);
    if (!req) {
        return Result<shared_ptr<Request>>::failure(ServiceError(ServiceError::Code::NotFound, "Request not found."));
    }
    if (req->getNationalCode() != nationalCode) {
        return Result<shared_ptr<Request>>::failure(ServiceError(ServiceError::Code::NotOwner, "Request does not belong to user."));
    }
    if (req->getStatus() != "APPROVED") {
        return Result<shared_ptr<Request>>::failure(ServiceError(ServiceError::Code::Custom, "Request is not approved."));
    }
    return Result<shared_ptr<Request>>::success(req);
}

void RequestService::markActivated(int requestId, const string& accountNumber) {
    auto req = findRequest(requestId);
    if (!req) return;
    req->setStatus("ACTIVATED");
    req->setAccountNumber(accountNumber);
    fileManager.saveRequests(requests);
}

void RequestService::branchDashboard(int branchId) const {
    auto activeAccounts = 0;
    for (auto& a : accountService.getAccounts())
        if (a->getBranchId() == branchId and a->isActive()) activeAccounts++;

    int pending = 0;
    int rejectedToday = 0;

    auto now = chrono::system_clock::now();
    time_t currentTime = chrono::system_clock::to_time_t(now);
    tm localTime = *localtime(&currentTime);
    stringstream todaySs;
    todaySs << put_time(&localTime, "%Y-%m-%d");
    string today = todaySs.str();

    for (auto& r : requests) {
        if (r->getBranchId() != branchId) continue;
        if (r->getStatus() == "PENDING") pending++;
        if (r->getStatus() == "REJECTED" and r->getTimestamp().substr(0, 10) == today) rejectedToday++;
    }

    cout << "Active accounts : " << activeAccounts << endl;
    cout << "Pending requests: " << pending << endl;
    cout << "Rejected (today): " << rejectedToday << endl;
}

void RequestService::listRequests(int branchId) const {
    bool found = false;
    for (auto& r : requests) {
        if (r->getBranchId() == branchId and r->getStatus() == "PENDING") {
            cout << r->getId() << " | User: " << r->getNationalCode()
                 << " | Branch: " << r->getBranchId()
                 << " | " << r->getTimestamp()
                 << " | " << r->getStatus() << endl;
            found = true;
        }
    }
    if (!found) {
        cout << "No pending requests for this branch." << endl;
    }
}

VoidResult RequestService::approveRequest(int requestId) {
    auto req = findRequest(requestId);
    if (!req) {
        return VoidResult::failure(ServiceError(ServiceError::Code::NotFound, "Request not found."));
    }
    if (req->getStatus() != "PENDING") {
        return VoidResult::failure(ServiceError(ServiceError::Code::Custom, "Request is not pending."));
    }
    req->setStatus("APPROVED");
    fileManager.saveRequests(requests);
    return VoidResult::success();
}

VoidResult RequestService::rejectRequest(int requestId, const string& reason) {
    auto req = findRequest(requestId);
    if (!req) {
        return VoidResult::failure(ServiceError(ServiceError::Code::NotFound, "Request not found."));
    }
    if (req->getStatus() != "PENDING") {
        return VoidResult::failure(ServiceError(ServiceError::Code::Custom, "Request is not pending."));
    }
    req->setStatus("REJECTED");
    req->setReason(reason);
    fileManager.saveRequests(requests);
    return VoidResult::success();
}

shared_ptr<Request> RequestService::findRequest(int requestId) const {
    for (auto& r : requests)
        if (r->getId() == requestId) return r;
    return nullptr;
}

void RequestService::resetAll() {
    requests.clear();
    nextRequestId = 2001;
    fileManager.saveNextRequestId(nextRequestId);
}

//••••••••••••••••••OtpService••••••••••••••••••
string OtpService::generateCode() const {
    static mt19937 rng(random_device{}());
    static uniform_int_distribution<int> dist(100000, 999999);
    int code = dist(rng);
    return to_string(code);
}

string OtpService::requestOtp(const string& accountNumber, int& secondsRemaining) {
    auto now = chrono::steady_clock::now();
    auto it = otps.find(accountNumber);
    if (it != otps.end() && it->second.expiresAt > now) {
        secondsRemaining = static_cast<int>(chrono::duration_cast<chrono::seconds>(it->second.expiresAt - now).count());
        return it->second.code;
    }
    OtpEntry entry;
    entry.code = generateCode();
    entry.expiresAt = now + chrono::seconds(120);
    otps[accountNumber] = entry;
    secondsRemaining = 120;
    return entry.code;
}

OtpStatus OtpService::verifyOtp(const string& accountNumber, const string& code) {
    auto it = otps.find(accountNumber);
    if (it == otps.end()) return OtpStatus::Invalid;

    if (it->second.expiresAt <= chrono::steady_clock::now()) {
        otps.erase(it);
        return OtpStatus::Expired;
    }
    if (it->second.code != code) return OtpStatus::Invalid;

    otps.erase(it);
    return OtpStatus::Valid;
}

//••••••••••••••••••TransactionService••••••••••••••••••
// Handles money operations and transaction history.
TransactionService::TransactionService(FileManager& fm, AccountService& as, RequestService& rs, AuthService& auth, FeeManager& fee)
    : accountService(as), fileManager(fm), requestService(rs), authService(auth), feeManager(fee) {
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

Result<shared_ptr<Transaction>> TransactionService::debitForPaya(const string& accountNumber, double amount) {
    auto account = accountService.getAccount(accountNumber);
    if (!account) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::NotFound, "Account not found."));
    }
    if (!account->isActive()) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::AccountInactive));
    }
    if (account->getBalance() < amount) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::InsufficientFunds));
    }
    account->deposit(-amount);
    auto tx = make_shared<Transaction>(nextTransactionId++, "PAYA", amount, accountNumber, "", account->getBalance());
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
    if (amount > TransactionLimits::MAX_TRANSFER_AMOUNT) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::Custom, "Transaction limit exceeded."));
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

Result<shared_ptr<Transaction>> TransactionService::onlinePayment(const string& from, const string& to, double amount, const string& enteredOtp, OtpService& otpService) {
    if (amount <= 0) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::InvalidAmount));
    }
    if (amount > TransactionLimits::MAX_ONLINE_PAYMENT_AMOUNT) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::Custom, "Transaction limit exceeded."));
    }
    auto fromAccount = accountService.getAccount(from);
    if (!fromAccount) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::NotFound, "Source account not found."));
    }
    if (!fromAccount->isActive()) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::AccountInactive));
    }
    auto toAccount = accountService.getAccount(to);
    if (!toAccount) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::NotFound, "Destination account not found."));
    }
    if (!toAccount->isActive()) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::DestinationInactive));
    }

    OtpStatus status = otpService.verifyOtp(from, enteredOtp);
    if (status == OtpStatus::Expired) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::Custom, "OTP expired."));
    }
    if (status == OtpStatus::Invalid) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::Custom, "Invalid OTP."));
    }

    double fee = feeManager.getTransferFee();
    if (fromAccount->getBalance() < amount + fee) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::InsufficientFunds));
    }

    fromAccount->deposit(-(amount + fee));
    toAccount->deposit(amount);

    auto tx = make_shared<Transaction>(nextTransactionId++, "ONLINE_PAYMENT", amount, from, to, fromAccount->getBalance());
    fromAccount->addTransaction(tx);
    toAccount->addTransaction(tx);
    transactions.push_back(tx);

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
        else if (tx->getType() == "PAYA") {
            runningBalance -= tx->getAmount();
            sign = "-";
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

void TransactionService::reset() {
    transactions.clear();
    nextTransactionId = 1001;
}

//••••••••••••••••••PayaService••••••••••••••••••
PayaService::PayaService(FileManager& fm, AccountService& as, TransactionService& ts, RequestService& rs)
    : fileManager(fm), accountService(as), transactionService(ts), requestService(rs) {
    requests = fileManager.loadPayaRequests();
    nextRequestId = 5001;
    for (auto& r : requests) nextRequestId = max(nextRequestId, r->getId() + 1);
}

shared_ptr<PayaRequest> PayaService::findRequest(int id) {
    for (auto& r : requests) if (r->getId() == id) return r;
    return nullptr;
}

Result<shared_ptr<PayaRequest>> PayaService::createRequest(const string& fromAccount, const string& destinationIban, double amount) {
    if (amount <= 0) {
        return Result<shared_ptr<PayaRequest>>::failure(ServiceError(ServiceError::Code::InvalidAmount));
    }
    if (amount > TransactionLimits::MAX_PAYA_AMOUNT) {
        return Result<shared_ptr<PayaRequest>>::failure(ServiceError(ServiceError::Code::Custom, "Transaction limit exceeded."));
    }
    auto from = accountService.getAccount(fromAccount);
    if (!from) {
        return Result<shared_ptr<PayaRequest>>::failure(ServiceError(ServiceError::Code::NotFound, "Source account not found."));
    }
    if (!from->isActive()) {
        return Result<shared_ptr<PayaRequest>>::failure(ServiceError(ServiceError::Code::AccountInactive));
    }
    if (!IbanGenerator::isValid(destinationIban)) {
        return Result<shared_ptr<PayaRequest>>::failure(ServiceError(ServiceError::Code::NotFound, "Invalid destination IBAN"));
    }

    string destinationAccount = IbanGenerator::toAccountNumber(destinationIban);
    auto to = accountService.getAccount(destinationAccount);
    if (!to) {
        return Result<shared_ptr<PayaRequest>>::failure(ServiceError(ServiceError::Code::NotFound, "Invalid destination IBAN"));
    }
    auto debitResult = transactionService.debitForPaya(fromAccount, amount);
    if (!debitResult.isOk()) {
        return Result<shared_ptr<PayaRequest>>::failure(debitResult.getError());
    }

    auto req = make_shared<PayaRequest>(nextRequestId++, fromAccount, destinationAccount, amount);
    requests.push_back(req);
    fileManager.savePayaRequests(requests);
    return Result<shared_ptr<PayaRequest>>::success(req);
}

void PayaService::listRequests() const {
    if (requests.empty()) {
        cout << "No paya requests." << endl;
        return;
    }
    for (size_t i = 0; i < requests.size(); i++) {
        auto& r = requests[i];
        string status = r->getStatus();
        string displayStatus = status;
        if (status == "PENDING") displayStatus = "Pending";
        else if (status == "APPROVED") displayStatus = "Completed";        
        else if (status == "REJECTED") displayStatus = "Rejected";

        string iban = IbanGenerator::generate(r->getDestinationAccount());
        iban.erase(remove(iban.begin(), iban.end(), ' '), iban.end());

        cout << "Source Account: " << r->getFromAccount() << endl;
        cout << "Request ID: " << r->getId() << endl;
        cout << "Destination IBAN: " << iban << endl;
        cout << "Amount: " << fixed << setprecision(2) << r->getAmount() << endl;
        cout << "Status: " << displayStatus << endl;

        if (i + 1 < requests.size()) cout << endl;
    }
}

Result<shared_ptr<Transaction>> PayaService::approve(int requestId) {
    auto req = findRequest(requestId);
    if (!req || req->getStatus() != "PENDING") {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::NotFound, "Request not found."));
    }
    auto to = accountService.getAccount(req->getDestinationAccount());
    if (!to) {
        return Result<shared_ptr<Transaction>>::failure(ServiceError(ServiceError::Code::NotFound, "Destination account not found."));
    }
    auto txResult = transactionService.deposit(req->getDestinationAccount(), req->getAmount());
    if (!txResult.isOk()) {
        return txResult;
    }

    req->setStatus("APPROVED");
    fileManager.savePayaRequests(requests);
    return txResult;
}

VoidResult PayaService::reject(int requestId) {
    auto req = findRequest(requestId);
    if (!req || req->getStatus() != "PENDING") {
        return VoidResult::failure(ServiceError(ServiceError::Code::NotFound, "Request not found."));
    }
    auto from = accountService.getAccount(req->getFromAccount());
    if (from) {
        transactionService.deposit(req->getFromAccount(), req->getAmount());
    }
    req->setStatus("REJECTED");
    fileManager.savePayaRequests(requests);
    return VoidResult::success();
}

void PayaService::resetAll(){
    requests.clear();
    nextRequestId = 5001;
    accountService.reset();
    transactionService.reset();
    requestService.resetAll();
    fileManager.resetFiles();
}