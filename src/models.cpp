#include "../include/models.hpp"

#include <cctype>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <algorithm>
using namespace std;

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

//••••••••••••••••••RequestAccount••••••••••••••••••
Request::Request(int id, const string& nationalCode, int branchId, const string& status) : id(id), nationalCode(nationalCode), branchId(branchId), status(status) {
    auto now = chrono::system_clock::now();
    time_t currentTime = chrono::system_clock::to_time_t(now);
    tm localTime = *localtime(&currentTime);
    stringstream ss;
    ss << put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    timestamp = ss.str();
}

int Request::getId() const { return id; }
string Request::getNationalCode() const { return nationalCode; }
int Request::getBranchId() const { return branchId; }
string Request::getStatus() const { return status; }
string Request::getTimestamp() const { return timestamp; }
string Request::getReason() const { return reason; }
string Request::getAccountNumber() const { return accountNumber; }

void Request::setStatus(const string& s) { status = s; }
void Request::setTimestamp(const string& ts) { timestamp = ts; }
void Request::setReason(const string& r) { reason = r; }
void Request::setAccountNumber(const string& accNum) { accountNumber = accNum; }

//••••••••••••••••••PayaRequest••••••••••••••••••
PayaRequest::PayaRequest(int id, const string& fromAccount, const string& destinationAccount, double amount, const string& status)
    : id(id), fromAccount(fromAccount), destinationAccount(destinationAccount), amount(amount), status(status) {}

int PayaRequest::getId() const { return id; }
string PayaRequest::getFromAccount() const { return fromAccount; }
string PayaRequest::getDestinationAccount() const { return destinationAccount; }
double PayaRequest::getAmount() const { return amount; }
string PayaRequest::getStatus() const { return status; }
void PayaRequest::setStatus(const string& s) { status = s; }

//••••••••••••••••••User••••••••••••••••••
// Represents a regular user with a national code and password.
User::User(const string& nationalCode, const string& passwordHash)
    : nationalCode(nationalCode), passwordHash(passwordHash) {}

string User::getNationalCode() const { return nationalCode; }
string User::getPasswordHash() const { return passwordHash; }
bool User::verifyPassword(const string& hashedPassword) const {
    return passwordHash == hashedPassword;
}
int User::getScore() const { return score; }
int User::getRegistrationSeq() const { return registrationSeq; }
void User::setScore(int s) { score = s; }
void User::setRegistrationSeq(int seq) { registrationSeq = seq; }
void User::addScore(int delta) { score += delta; }



//••••••••••••••••••NationalCodeValidator••••••••••••••••••
// Validates Iranian national codes using the modulo-11 check digit algorithm.
// This class only checks if a national code is valid and does not depend on any other classes.
bool NationalCodeValidator::isValid(const string& rawCode) {
    if (rawCode.empty()) return false;
    if (!all_of(rawCode.begin(), rawCode.end(), [](unsigned char c) { return isdigit(c); })) return false;
    if (rawCode.size() < 8 or rawCode.size() > 10) return false;

    string code = rawCode;
    while (code.size() < 10) code = "0" + code;
    int sum = 0;
    for (int i = 0; i < 9; i++) {
        int digit = code[i] - '0';
        sum += digit * (10 - i);
    }
    int remainder = sum % 11;
    int controlDigit = code[9] - '0';

    if (remainder < 2) return controlDigit == remainder;
    return controlDigit == (11 - remainder);
}

string NationalCodeValidator::normalize(const string& rawCode) {
    string code = rawCode;
    while (code.size() < 10) code = "0" + code;
    return code;
}

//••••••••••••••••••IbanGenerator••••••••••••••••••
string IbanGenerator::mod97(const string& numStr) {
    long long rem = 0;
    for (char c : numStr) {
        rem = (rem * 10 + (c - '0')) % 97;
    }
    ostringstream os;
    os << rem;
    return os.str();
}

string IbanGenerator::generate(const string& accountNumber) {
    string digits;
    for (char c : accountNumber)
        if (isdigit(static_cast<unsigned char>(c))) digits += c;

    string body22 = string(22 - digits.size(), '0') + digits;
    string forMod = body22 + "182700"; 

    int check = 98 - stoi(mod97(forMod));

    ostringstream os;
    os << "IR" << setw(2) << setfill('0') << check << body22;
    string raw = os.str();

    string formatted;
    for (size_t i = 0; i < raw.size(); i += 4) {
        if (i > 0) formatted += " ";
        formatted += raw.substr(i, 4);
    }
    return formatted;
}

string IbanGenerator::toAccountNumber(const string& iban) {
    string raw;
    for (char c : iban) if (c != ' ') raw += c;
    string body22 = raw.substr(4);
    string digits16 = body22.substr(body22.size() - 16);
    return digits16.substr(0,4) + "-" + digits16.substr(4,4) + "-" + digits16.substr(8,4) + "-" + digits16.substr(12,4);
}
bool IbanGenerator::isValid(const string& iban) {
    string raw;
    for (char c : iban) if (c != ' ') raw += c;

    if (raw.size() != 26) return false;
    if (raw.substr(0, 2) != "IR") return false;

    string checkStr = raw.substr(2, 2);
    if (!isdigit(static_cast<unsigned char>(checkStr[0])) || !isdigit(static_cast<unsigned char>(checkStr[1])))
        return false;

    string body22 = raw.substr(4);
    string forMod = body22 + "182700";

    int actualCheck = stoi(checkStr);
    int expectedCheck = 98 - stoi(mod97(forMod));

    return actualCheck == expectedCheck;
}

//••••••••••••••••••CreditLevel••••••••••••••••••
string CreditLevel::calculate(int score) {
    if (score >= 15) return "Diamond";
    if (score >= 10) return "Gold";
    if (score >= 5)  return "Silver";
    return "Bronze";
}

