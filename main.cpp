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
#include <condition_variable> 
#include "include/json.hpp"
#include "src/sha256.cpp"
using json = nlohmann::json;
using namespace std;

//••••••••••••••••••InputQueue••••••••••••••••••
// reads input lines in a background thread and stores them in a queue.
// Lets you get input either waiting or with a timeout.
// literaly made a class to handle "timeout" error in withdarw :)

class InputQueue {
public:
    InputQueue() {
        reader = thread(&InputQueue::readLoop, this);
        reader.detach();  
    }

    bool getLine(string& outLine, int timeoutSeconds) {
        unique_lock<mutex> lock(mtx);
        bool got = cv.wait_for(lock, chrono::seconds(timeoutSeconds), [this] { return !queue_.empty() or finished; });
        if (got and !queue_.empty()) {
            outLine = queue_.front();
            queue_.pop();
            return true;
        }
        return false;
    }

    bool getLineBlocking(string& outLine) {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this] { return !queue_.empty() or finished; });
        if (!queue_.empty()) {
            outLine = queue_.front();
            queue_.pop();
            return true;
        }
        return false;
    }

private:
    void readLoop() {
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

    thread reader;
    queue<string> queue_;
    mutex mtx;
    condition_variable cv;
    bool finished = false;
};


//••••••••••••••••••Transaction••••••••••••••••••
// Represents a single bank operation such as deposit, withdrawal, or transfer.

class Transaction {
private:
    int id;
    string type;
    double amount;
    string fromAccount;
    string toAccount;
    string timestamp;
    double balanceAfter = 0.0;

public:
    Transaction(int id, const string& type, double amount, const string& fromAccount, const string& toAccount, double balanceAfter = 0.0)
        : id(id), type(type), amount(amount),
          fromAccount(fromAccount), toAccount(toAccount),
          balanceAfter(balanceAfter)
    {

        auto now = chrono::system_clock::now();
        time_t currentTime = chrono::system_clock::to_time_t(now);
        tm localTime = *localtime(&currentTime);
        stringstream ss;
        ss << put_time(&localTime, "%Y-%m-%d %H:%M:%S");
        timestamp = ss.str();
    }
    int getId() const { return id; }
    string getType() const { return type; }
    double getAmount() const { return amount; }
    string getFromAccount() const { return fromAccount; }
    string getToAccount() const { return toAccount; }
    string getTimestamp() const { return timestamp; }
    double getBalanceAfter() const { return balanceAfter; }

    void setTimestamp(const string& ts) { timestamp = ts; }
    void setBalanceAfter(double b) { balanceAfter = b; }
};

//••••••••••••••••••Account••••••••••••••••••
// Represents a bank account with balance, security info, and transaction history.
// Handles basic operations like deposit, withdrawal, and transfer.
class Account {
private:
    string accountNumber;
    int branchId;
    double balance;
    string passwordHash;
    bool active;
    vector<shared_ptr<Transaction>> transactions;

public:
    Account(const string& accNumber, int branchId, const string& passwordHash, double initialBalance)
        : accountNumber(accNumber), branchId(branchId), balance(initialBalance), passwordHash(passwordHash), active(true) {}

    string getAccountNumber() const { return accountNumber; }
    int getBranchId() const { return branchId; }
    double getBalance() const { return balance; }
    bool isActive() const { return active; }
    string getPasswordHash() const { return passwordHash; }
    const vector<shared_ptr<Transaction>>& getTransactions() const { return transactions; }

    void setActive(bool status) { active = status; }

    bool verifyPassword(const string& hashedPassword) const {
        return passwordHash == hashedPassword;
    }

    void deposit(double amount) { balance += amount; }

    bool withdraw(double amount, const string& hashedPassword) {
        if (!verifyPassword(hashedPassword)) {
            cout << "Error: Wrong password." << endl;
            return false;
        }
        if (balance < amount) {
            cout << "Error: Insufficient funds." << endl;
            return false;
        }
        balance -= amount;
        return true;
    }

    bool transfer(Account& toAccount, double amount, const string& hashedPassword) {
        if (!verifyPassword(hashedPassword)) {
            cout << "Error: Wrong password." << endl;
            return false;
        }
        if (balance < amount) {
            cout << "Error: Insufficient funds." <<endl;
            return false;
        }
        balance -= amount;
        toAccount.deposit(amount);
        return true;
    }
    void addTransaction(shared_ptr<Transaction> tx) { transactions.push_back(tx); }
    void clearTransactions() { transactions.clear(); }
};

//••••••••••••••••••Branch••••••••••••••••••
// Represents a bank branch that manages accounts under it.
// Only stores and organizes accounts.
class Branch {
private:
    int id;
    string name;
    vector<shared_ptr<Account>> accounts;

public:
    Branch(int id, const string& name) : id(id), name(name) {}

    int getId() const { return id; }
    string getName() const { return name; }

    const vector<shared_ptr<Account>>& getAccounts() const { return accounts; }

    void setName(const string& n) { name = n; }

    void addAccount(shared_ptr<Account> account) {
        accounts.push_back(account);
    }

    void removeAccount(string& accountNumber) {
	    for (auto it = accounts.begin(); it != accounts.end(); ++it) {
	        if ((*it)->getAccountNumber() == accountNumber) {
	            accounts.erase(it);
	            break;
	        }
	    }
	}
	
    int getAccountCount() const { return static_cast<int>(accounts.size()); }

    shared_ptr<Account> findAccount(const string& accountNumber) const {
        for (const auto& acc : accounts)
            if (acc->getAccountNumber() == accountNumber) return acc;
        return nullptr;
    }
};
//••••••••••••••••••FileManager••••••••••••••••••
// Handles reading and writing data files.
// Stores and retrieves branches, accounts, transactions, and next IDs.

class FileManager {
private:
    string branchesPath = "data/branches.json";
    string accountsPath = "data/accounts.json";
    string transactionsPath = "data/transactions.json";
    string nextIdsPath = "data/meta.json";

    json readFile(const string& path) const {
	    ifstream file(path);
	    if (!file.is_open()) return json{};
	
	    stringstream buffer;
	    buffer << file.rdbuf();
	    string content = buffer.str();
	
	    if (content.find_first_not_of(" \t\n\r") == string::npos)
	        return json{};
	
	    try { return json::parse(content); } 
		catch (const json::parse_error&) { return json{}; }
	}

    void writeFile(const string& path, const json& data) const {
        ofstream file(path);
        file << data.dump(4);
    }

public:
    vector<shared_ptr<Branch>> loadBranches() const {
        vector<shared_ptr<Branch>> branches;
        json data = readFile(branchesPath);
        
        if (data.empty() or !data.contains("branches")) return branches;
        for (auto& b : data["branches"])
            branches.push_back(make_shared<Branch>(
				b["id"].get<int>(), 
				b["name"].get<string>())
			);
        return branches;
    }

    vector<shared_ptr<Account>> loadAccounts() const {
        vector<shared_ptr<Account>> accounts;
        json data = readFile(accountsPath);
        
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

    vector<shared_ptr<Transaction>> loadTransactions() const {
        vector<shared_ptr<Transaction>> transactions;
        json data = readFile(transactionsPath);
        
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

    void saveBranches(const vector<shared_ptr<Branch>>& branches) const {
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
        writeFile(branchesPath, data);
    }

    void saveAccounts(const vector<shared_ptr<Account>>& accounts) const {
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
        writeFile(accountsPath, data);
    }

    void saveTransactions(const vector<shared_ptr<Transaction>>& transactions) const {
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
        writeFile(transactionsPath, data);
    }

    void saveNextIds(int nextBranchId, int nextAccountSeq) const {
        json data = readFile(nextIdsPath);
        data["nextBranchId"] = nextBranchId;
        data["nextAccountSeq"] = nextAccountSeq;
        writeFile(nextIdsPath, data);
    }

    void saveNextTransactionId(int nextTransactionId) const {
        json data = readFile(nextIdsPath);
        data["nextTransactionId"] = nextTransactionId;
        writeFile(nextIdsPath, data);
    }

    void loadNextIds(int& nextBranchId, int& nextAccountSeq, int& nextTransactionId) const {
        json data = readFile(nextIdsPath);
        if (data.empty()) {
            nextBranchId = 10001;
            nextAccountSeq = 1;
            nextTransactionId = 1001;
            return;
        }
        nextBranchId = data["nextBranchId"].get<int>();
        nextAccountSeq = data["nextAccountSeq"].get<int>();
        nextTransactionId = data["nextTransactionId"].get<int>();
    }
    
    void loadNextTransactionId(int& nextTransactionId) const {
    	json data = readFile(nextIdsPath);
        if (data.empty()) {
            nextTransactionId = 1001;
            return;
        }
        nextTransactionId = data["nextTransactionId"].get<int>();
    }
};
//••••••••••••••••••AuthService••••••••••••••••••
// Handles password hashing, verification, and user password input.
// Checks user passwords using SHA-256 hashes.

class AuthService {
private:
    string hashPassword(const string& rawPassword) const {
	    return SHA256::hash(rawPassword); 
	}

public:
    string promptPassword(InputQueue& inputQueue) const {
        cout << "Enter password: " << flush;
        string password;
        inputQueue.getLineBlocking(password);
        return password;
    }

    bool verifyPassword(const shared_ptr<Account>& account, const string& rawPassword) const {
        if (hashPassword(rawPassword) == account->getPasswordHash())
            return true;
        cout << "Error: Wrong password." << endl;
        return false;
    }

    bool promptPasswordWithTimeout(InputQueue& inputQueue, string& outPassword, int timeoutSeconds) const {
        cout << "Enter password: " << flush;
        return inputQueue.getLine(outPassword, timeoutSeconds);
    }

    string generateHash(const string& rawPassword) const {
        return hashPassword(rawPassword);
    }
};
//••••••••••••••••••AccountService••••••••••••••••••
// Manages bank accounts and branches.
// Creates, updates, and removes accounts.

class AccountService {
private:
    FileManager& fileManager;
    AuthService& authService;

    vector<shared_ptr<Account>> accounts;
    vector<shared_ptr<Branch>>  branches;

    int nextBranchId = 10001;
    int nextAccountSeq = 1;

    shared_ptr<Account> findAccount(string& accountNumber) {
        for (auto& a : accounts)
            if (a->getAccountNumber() == accountNumber) return a;
        return nullptr;
    }

    shared_ptr<Branch> findBranch(int branchId) {
        for (auto& b : branches)
            if (b->getId() == branchId) return b;
        return nullptr;
    }

    string generateAccountNumber() {
        ostringstream os;
        int seq = nextAccountSeq++;
        os << "5022-"
            << setw(4) << setfill('0') << ((seq / 100000000) % 10000) << "-"
            << setw(4) << setfill('0') << ((seq / 10000) % 10000) << "-"
            << setw(4) << setfill('0') << (seq % 10000);
        return os.str();
    }

public:
    AccountService(FileManager& fm, AuthService& as) : fileManager(fm), authService(as) {
        int loadedNextTransactionId;
        fileManager.loadNextIds(nextBranchId, nextAccountSeq, loadedNextTransactionId);
        branches = fileManager.loadBranches();
        accounts = fileManager.loadAccounts();
    }

    void createBranch(string& name) {
        branches.push_back(make_shared<Branch>(nextBranchId, name));
        cout << "Branch created. ID: " << nextBranchId << endl;
        nextBranchId++;
        fileManager.saveBranches(branches);
        fileManager.saveNextIds(nextBranchId, nextAccountSeq);
    }

    void listBranches() const {
    	if (branches.empty()) {
	        cout << "No branches exist." << endl;
	        return;
	    }
        for (auto& b : branches)
            cout << b->getId() << " | " << b->getName() << endl;
    }

    void createAccount(int branchId, InputQueue& inputQueue) {
        auto branch = findBranch(branchId);
        if (!branch) { 
			cout << "Error: Branch not found." << endl; 
			return; 
		}
        string accountNumber = generateAccountNumber();
        string password = authService.promptPassword(inputQueue);
        string passwordHash = authService.generateHash(password);
        auto account = make_shared<Account>(accountNumber, branchId, passwordHash, 0.0);
        accounts.push_back(account);
        branch->addAccount(account);
        cout << "Account created. Number: " << accountNumber << endl;
        fileManager.saveAccounts(accounts);
        fileManager.saveBranches(branches);
        fileManager.saveNextIds(nextBranchId, nextAccountSeq);
    }

    void closeAccount(string& accountNumber, InputQueue& inputQueue) {
        auto account = findAccount(accountNumber);
        if (!account) { 
			cout << "Error: Account not found." << endl; 
			return; 
		}
        string password = authService.promptPassword(inputQueue);
        if (!authService.verifyPassword(account, password)) return;
        account->setActive(false);
        cout << "Account closed." << endl;
        fileManager.saveAccounts(accounts);
    }

    void deleteAccount(string& accountNumber, InputQueue& inputQueue) {
        auto account = findAccount(accountNumber);
        if (!account) { 
			cout << "Error: Account not found." << endl; 
			return; 
		}
		string password = authService.promptPassword(inputQueue);
        if (!authService.verifyPassword(account, password)) return;
        auto branch = findBranch(account->getBranchId());
        if (branch) branch->removeAccount(accountNumber);
        
        for (auto it = accounts.begin(); it != accounts.end(); ++it) {
		    if ((*it)->getAccountNumber() == accountNumber) {
		        accounts.erase(it);
		        break;
		    }
		}
        cout << "Account deleted." << endl;
        fileManager.saveAccounts(accounts);
        fileManager.saveBranches(branches);
    }

    void listAccounts() const {
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

    void reset() {
        accounts.clear();
        branches.clear();
        nextBranchId = 10001;
        nextAccountSeq = 1;
        fileManager.saveBranches({});
        fileManager.saveAccounts({});
        fileManager.saveNextIds(nextBranchId, nextAccountSeq);
    }

	shared_ptr<Account> getAccount(string& accountNumber) {
        return findAccount(accountNumber);
    }
    shared_ptr<Account> getAccount(const string& accountNumber) const {
        for (auto& a : accounts)
            if (a->getAccountNumber() == accountNumber) return a;
        return nullptr;
    }
    shared_ptr<Branch> getBranch(int branchId) { return findBranch(branchId); }

    const vector<shared_ptr<Account>>& getAccounts() const { return accounts; }
    const vector<shared_ptr<Branch>>&  getBranches() const { return branches; }
};
//••••••••••••••••••TransactionService••••••••••••••••••
// Handles money operations and transaction history.

class TransactionService {
private:
    AccountService& accountService;
    FileManager& fileManager;
    AuthService& authService;
    vector<shared_ptr<Transaction>> transactions;
    int nextTransactionId = 1001;

public:
    TransactionService(FileManager& fm, AccountService& as, AuthService& auth) : accountService(as), fileManager(fm), authService(auth) {
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

    void deposit(string& accountNumber, double amount) {
        auto account = accountService.getAccount(accountNumber);
        if (!account) { 
			cout << "Error: Account not found." << endl; 
			return; 
		}
        if (!account->isActive()) {
			cout << "Error: Account is inactive." << endl; 
			return; 
		}
        account->deposit(amount);
        auto tx = make_shared<Transaction>(nextTransactionId++, "DEPOSIT", amount, accountNumber, "", account->getBalance());
        account->addTransaction(tx);
        transactions.push_back(tx);
        cout << "Transaction ID: " << tx->getId() << endl;
        cout << "New balance: " << fixed << setprecision(2) << account->getBalance() << endl;
        fileManager.saveAccounts(accountService.getAccounts());
        fileManager.saveTransactions(transactions);
        fileManager.saveNextTransactionId(nextTransactionId);
    }

    void withdraw(string& accountNumber, double amount, InputQueue& inputQueue) {
        auto account = accountService.getAccount(accountNumber);
        if (!account) { 
			cout << "Error: Account not found." << endl; 
			return; 
		}
        if (!account->isActive()) { 
			cout << "Error: Account is inactive." << endl; 
			return; 
		}
        if (account->getBalance() < amount) { 
			cout << "Error: Insufficient funds." << endl; 
			return; 
		}

		string password;
		bool gotPassword = authService.promptPasswordWithTimeout(inputQueue, password, 30);
		if (!gotPassword) {
			cout << "Error: Password timeout. Transaction cancelled." << endl;
			return;
		}

		string hashedPassword = authService.generateHash(password);
        if (!account->withdraw(amount, hashedPassword)) return;
        auto tx = make_shared<Transaction>(nextTransactionId++, "WITHDRAWAL", amount, accountNumber, "", account->getBalance());
        account->addTransaction(tx);
        transactions.push_back(tx);
        cout << "Transaction ID: " << tx->getId() << endl;
        cout << "New balance: " << fixed << setprecision(2) << account->getBalance() << endl;
        fileManager.saveAccounts(accountService.getAccounts());
        fileManager.saveTransactions(transactions);
        fileManager.saveNextTransactionId(nextTransactionId);
    }

    void transfer(string& from, string& to, double amount, InputQueue& inputQueue) {
        auto fromAccount = accountService.getAccount(from);
        if (!fromAccount) { 
			cout << "Error: Source account not found." << endl; 
			return; 
		}
        if (!fromAccount->isActive()) { 
			cout << "Error: Account is inactive." << endl; 
			return; 
			}
        auto toAccount = accountService.getAccount(to);
        if (!toAccount) { 
			cout << "Error: Destination account not found." << endl; 
			return; 
		}
        if (!toAccount->isActive()) { 
			cout << "Error: Destination account is inactive." << endl; 
			return; 
		}
		string password = authService.promptPassword(inputQueue);
        if (fromAccount->getBalance() < amount) { 
			cout << "Error: Insufficient funds." << endl; 
			return; 
		}
		string hashedPassword = authService.generateHash(password);
        if (!fromAccount->transfer(*toAccount, amount, hashedPassword)) return;
        auto tx = make_shared<Transaction>(nextTransactionId++, "TRANSFER", amount, from, to, fromAccount->getBalance());
        fromAccount->addTransaction(tx);
        toAccount->addTransaction(tx);
        transactions.push_back(tx);
        cout << "Transaction ID: " << tx->getId() << endl;
        cout << "New balance: " << fixed << setprecision(2) << fromAccount->getBalance() << endl;
        fileManager.saveAccounts(accountService.getAccounts());
        fileManager.saveTransactions(transactions);
        fileManager.saveNextTransactionId(nextTransactionId);
    }

    void getBalance(string& accountNumber) const {
        auto account = accountService.getAccount(accountNumber);
        if (!account) { 
			cout << "Error: Account not found." << endl; 
			return; 
		}
        cout << "Balance: " << fixed << setprecision(2) << account->getBalance() << endl;
        cout << "Active: " << (account->isActive() ? "Yes" : "No") << endl;
        cout << "Branch: " << account->getBranchId() << endl;
    }

    void getHistory(string& accountNumber) const {
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

    void getTransaction(int id) const {
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

    void clearHistory(string& accountNumber, InputQueue& inputQueue) {
        auto account = accountService.getAccount(accountNumber);
        if (!account) { 
			cout << "Error: Account not found." << endl; 
			return; 
		}
		string password = authService.promptPassword(inputQueue);
		string hashedPassword = authService.generateHash(password);
        if (!account->verifyPassword(hashedPassword)) { 
			cout << "Error: Wrong password." << endl; 
			return; 
		}
        for (const auto& t : account->getTransactions()) {
			for (auto it = transactions.begin();
		        it != transactions.end(); ++it) {
		        if ((*it)->getId() == t->getId()) {
			        transactions.erase(it);
			        break;
			    }
		    }
			account->clearTransactions();
		}
        cout << "History cleared for " << accountNumber << "." <<endl;
        fileManager.saveAccounts(accountService.getAccounts());
        fileManager.saveTransactions(transactions);
    }

    void resetAll() {
        transactions.clear();
        accountService.reset();
        fileManager.saveTransactions({});
        nextTransactionId = 1001;
        fileManager.saveNextTransactionId(nextTransactionId);
    }

    const vector<shared_ptr<Transaction>>& getAllTransactions() const {
        return transactions;
    }
};

static void handleCreateBranch(string& args, AccountService& as) {
    string name = args;
    if (!name.empty() and name.front() == '"') name = name.substr(1, name.size() - 2);
    as.createBranch(name);
}

static void handleDeposit(string& args, TransactionService& ts) {
    istringstream iss(args);
    string acc; 
	double amount;
    iss >> acc >> amount;
    ts.deposit(acc, amount);
}

static void handleWithdraw(string& args, TransactionService& ts, InputQueue& inputQueue) {
    istringstream iss(args);
    string acc; 
	double amount;
    iss >> acc >> amount;
    ts.withdraw(acc, amount, inputQueue);
}

static void handleTransfer(string& args, TransactionService& ts, InputQueue& inputQueue) {
    istringstream iss(args);
    string from, to; 
	double amount;
    iss >> from >> to >> amount;
    ts.transfer(from, to, amount, inputQueue);
}

static void handleResetAll(TransactionService& ts, InputQueue& inputQueue) {
    string confirm;
    cout << "Are you sure? This deletes everything. (yes/no): " << flush;
    inputQueue.getLineBlocking(confirm);
    if (confirm == "yes") { 
		ts.resetAll(); 
		cout << "All data cleared." << endl; 
	}
    else cout << "Cancelled." << endl;
}

static void handleCommand(string& line, AccountService& as, TransactionService& ts, AuthService& auth, InputQueue& inputQueue) {
    istringstream iss(line);
    string command, args;
    iss >> command;
    getline(iss >> ws, args);

    if      (command == "create_branch")    handleCreateBranch(args, as);
    else if (command == "list_branches")    as.listBranches();
    else if (command == "create_account")   as.createAccount(stoi(args), inputQueue);
    else if (command == "close_account")    as.closeAccount(args, inputQueue);
    else if (command == "delete_account")   as.deleteAccount(args, inputQueue);
    else if (command == "list_accounts")    as.listAccounts();
    else if (command == "deposit")          handleDeposit(args, ts);
    else if (command == "withdraw")         handleWithdraw(args, ts, inputQueue);
    else if (command == "transfer")         handleTransfer(args, ts, inputQueue);
    else if (command == "get_balance")      ts.getBalance(args);
    else if (command == "get_history")      ts.getHistory(args);
    else if (command == "get_transaction")  ts.getTransaction(stoi(args));
    else if (command == "clear_history")    ts.clearHistory(args, inputQueue);
    else if (command == "reset_all")        handleResetAll(ts, inputQueue);
    else cout << "Error: Unknown command." << endl;
}
// Starts the banking system
int main() {
    FileManager fileManager;
    AuthService authService;
    AccountService accountService(fileManager, authService);
    TransactionService transactionService(fileManager, accountService, authService);
    InputQueue inputQueue; 

    string line;
    while (inputQueue.getLineBlocking(line)) {
        if (line.empty()) continue;
        handleCommand(line, accountService, transactionService, authService, inputQueue);
    }
    
    return 0;
}