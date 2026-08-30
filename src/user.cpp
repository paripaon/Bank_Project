#include "../include/user.hpp"

#include <cctype> 
#include <iomanip>
#include <sstream>
#include <iostream>
#include <vector>
#include <memory>
#include <map>
#include <algorithm>
#include <string>
using namespace std;

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

//••••••••••••••••••User••••••••••••••••••
// Represents a regular user with a national code and password.
User::User(const string& nationalCode, const string& passwordHash)
    : nationalCode(nationalCode), passwordHash(passwordHash) {}

string User::getNationalCode() const { return nationalCode; }
string User::getPasswordHash() const { return passwordHash; }

bool User::verifyPassword(const string& hashedPassword) const {
    return passwordHash == hashedPassword;
}

//••••••••••••••••••UserFileManager••••••••••••••••••
// Handles reading and writing user data to file.
UserFileManager::UserFileManager(FileManager& fm) : fileManager(fm) {}

vector<shared_ptr<User>> UserFileManager::loadUsers() const {
    vector<shared_ptr<User>> users;
    json data = fileManager.readFile(usersPath);
    if (data.empty() or !data.contains("users")) return users;
    for (auto& u : data["users"])
        users.push_back(make_shared<User>(
            u["nationalCode"].get<string>(),
            u["passwordHash"].get<string>()
        ));
    return users;
}

void UserFileManager::saveUsers(const vector<shared_ptr<User>>& users) const {
    json data;
    data["users"] = json::array();
    for (auto& u : users) {
        json entry;
        entry["nationalCode"] = u->getNationalCode();
        entry["passwordHash"] = u->getPasswordHash();
        data["users"].push_back(entry);
    }
    fileManager.writeFile(usersPath, data);
}

//••••••••••••••••••AuthSession••••••••••••••••••
// Keeps track of the currently logged-in user.
// Keeping the login session in a separate class, makes it easier to manage.
bool AuthSession::isLoggedIn() const { return currentUser != nullptr; }
shared_ptr<User> AuthSession::getCurrentUser() const { return currentUser; }
void AuthSession::login(shared_ptr<User> user) { currentUser = user; }
void AuthSession::logout() { currentUser = nullptr; }

//••••••••••••••••••AccountOwnershipManager••••••••••••••••••
// Keeps track of which user owns each bank account.
// It stores which national code owns each account number.
AccountOwnershipManager::AccountOwnershipManager(FileManager& fm) : fileManager(fm) {
    json data = fileManager.readFile(ownersPath);
    if (data.empty() or !data.contains("owners")) return;
    for (auto& entry : data["owners"])
        ownerOf[entry["account"].get<string>()] = entry["nationalCode"].get<string>();
}

void AccountOwnershipManager::persist() const {
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

void AccountOwnershipManager::registerOwnership(const string& accountNumber, const string& nationalCode) {
    ownerOf[accountNumber] = nationalCode;
    persist();
}

void AccountOwnershipManager::removeOwnership(const string& accountNumber) {
    ownerOf.erase(accountNumber);
    persist();
}

bool AccountOwnershipManager::isOwnedBy(const string& accountNumber, const string& nationalCode) const {
    auto it = ownerOf.find(accountNumber);
    return it != ownerOf.end() and it->second == nationalCode;
}

vector<string> AccountOwnershipManager::getAccountsOf(const string& nationalCode) const {
    vector<string> result;
    for (auto& entry : ownerOf)
        if (entry.second == nationalCode) result.push_back(entry.first);
    return result;
}

void AccountOwnershipManager::reload() {
    ownerOf.clear();
    json data = fileManager.readFile(ownersPath);
    if (data.empty() or !data.contains("owners")) return;
    for (auto& entry : data["owners"])
        ownerOf[entry["account"].get<string>()] = entry["nationalCode"].get<string>();
}

//••••••••••••••••••UserService••••••••••••••••••
// Handles signup, login, logout, and user deletion for regular users.
UserService::UserService(UserFileManager& ufm, AuthService& as, AuthSession& sess, AccountOwnershipManager& own)
    : userFileManager(ufm), authService(as), session(sess), ownership(own) {
    users = userFileManager.loadUsers();
}

shared_ptr<User> UserService::findUser(const string& nationalCode) {
    for (auto& u : users)
        if (u->getNationalCode() == nationalCode) return u;
    return nullptr;
}

VoidResult UserService::signup(InputQueue& inputQueue) {
    if (session.isLoggedIn())
        return VoidResult::failure(ServiceError(ServiceError::Code::AlreadyLoggedIn));

    cout << "Enter national code: " << endl;
    string nationalCode;
    inputQueue.getLineBlocking(nationalCode);

    if (!NationalCodeValidator::isValid(nationalCode))
        return VoidResult::failure(ServiceError(ServiceError::Code::InvalidArguments, "Invalid national code."));

    string normalized = NationalCodeValidator::normalize(nationalCode);

    if (findUser(normalized))
        return VoidResult::failure(ServiceError(ServiceError::Code::AlreadyExists, "User already exists."));

    cout << "Enter user password: " << endl;
    string password;
    inputQueue.getLineBlocking(password);
    string passwordHash = authService.generateHash(password);

    auto user = make_shared<User>(normalized, passwordHash);
    users.push_back(user);
    userFileManager.saveUsers(users);
    return VoidResult::success();
}

Result<shared_ptr<User>> UserService::login(InputQueue& inputQueue) {
    if (session.isLoggedIn())
        return Result<shared_ptr<User>>::failure(ServiceError(ServiceError::Code::AlreadyLoggedIn));

    cout << "Enter national code: " << endl;
    string nationalCode;
    inputQueue.getLineBlocking(nationalCode);
    string normalized = NationalCodeValidator::normalize(nationalCode);

    auto user = findUser(normalized);
    if (!user)
        return Result<shared_ptr<User>>::failure(ServiceError(ServiceError::Code::NotFound, "User not found."));

    cout << "Enter user password: " << endl;
    string password;
    inputQueue.getLineBlocking(password);

    string passwordHash = authService.generateHash(password);
    if (!user->verifyPassword(passwordHash))
        return Result<shared_ptr<User>>::failure(ServiceError(ServiceError::Code::WrongPassword, "Wrong user password."));

    session.login(user);
    return Result<shared_ptr<User>>::success(user);
}

VoidResult UserService::logout() {
    if (!session.isLoggedIn())
        return VoidResult::failure(ServiceError(ServiceError::Code::NotLoggedIn));
    session.logout();
    return VoidResult::success();
}

VoidResult UserService::deleteCurrentUser(InputQueue& inputQueue) {
    if (!session.isLoggedIn())
        return VoidResult::failure(ServiceError(ServiceError::Code::NotLoggedIn));

    auto user = session.getCurrentUser();
    cout << "Enter user password: " << endl;
    string password;
    inputQueue.getLineBlocking(password);
    string passwordHash = authService.generateHash(password);

    if (!user->verifyPassword(passwordHash))
        return VoidResult::failure(ServiceError(ServiceError::Code::WrongPassword, "Wrong user password."));

    if (!ownership.getAccountsOf(user->getNationalCode()).empty())
        return VoidResult::failure(ServiceError(ServiceError::Code::HasDependents, "User has accounts."));

    for (auto it = users.begin(); it != users.end(); ++it) {
        if ((*it)->getNationalCode() == user->getNationalCode()) {
            users.erase(it);
            break;
        }
    }
    userFileManager.saveUsers(users);
    session.logout();
    return VoidResult::success();
}

void UserService::reload() {
    users = userFileManager.loadUsers();
    session.logout();
}

//••••••••••••••••••AccounOwnerhipValidator••••••••••••••••••
// Checks if an account exists and belongs to the given user.
Result<shared_ptr<Account>> AccountOwnershipValidator::findOwnedAccount(AccountService& accountService, AccountOwnershipManager& ownership, const string& accountNumber, const string& nationalCode, const string& notFoundMessage) {
    auto account = accountService.getAccount(accountNumber);
    if (!account)
        return Result<shared_ptr<Account>>::failure(ServiceError(ServiceError::Code::NotFound, notFoundMessage));

    if (!ownership.isOwnedBy(accountNumber, nationalCode))
        return Result<shared_ptr<Account>>::failure(ServiceError(ServiceError::Code::NotOwner));

    return Result<shared_ptr<Account>>::success(account);
}

//••••••••••••••••••UserAccountService••••••••••••••••••
// Lets logged-in users open, view, and delete their own accounts.
UserAccountService::UserAccountService(AccountService& as, AccountOwnershipManager& own, AuthSession& sess, AuthService& auth)
    : accountService(as), ownership(own), session(sess), authService(auth) {}

Result<shared_ptr<Account>> UserAccountService::openAccount(InputQueue& inputQueue) {
    if (!session.isLoggedIn())
        return Result<shared_ptr<Account>>::failure(ServiceError(ServiceError::Code::NotLoggedIn));

    string password = authService.promptPassword(inputQueue, "Enter account password: ");
    string passwordHash = authService.generateHash(password);

    auto account = accountService.createUserAccount(passwordHash);
    ownership.registerOwnership(account->getAccountNumber(), session.getCurrentUser()->getNationalCode());
    return Result<shared_ptr<Account>>::success(account);
}

Result<vector<shared_ptr<Account>>> UserAccountService::myAccounts() {
    if (!session.isLoggedIn())
        return Result<vector<shared_ptr<Account>>>::failure(ServiceError(ServiceError::Code::NotLoggedIn));

    vector<shared_ptr<Account>> result;
    auto accountNumbers = ownership.getAccountsOf(session.getCurrentUser()->getNationalCode());
    for (auto& accNum : accountNumbers) {
        auto account = accountService.getAccount(accNum);
        if (account) result.push_back(account);
    }
    return Result<vector<shared_ptr<Account>>>::success(result);
}

VoidResult UserAccountService::deleteMyAccount(const string& accountNumber, InputQueue& inputQueue) {
    if (!session.isLoggedIn())
        return VoidResult::failure(ServiceError(ServiceError::Code::NotLoggedIn));

    auto found = AccountOwnershipValidator::findOwnedAccount(
        accountService, ownership, accountNumber, session.getCurrentUser()->getNationalCode());
    if (!found.isOk())
        return VoidResult::failure(found.getError());

    auto account = found.getValue();

    string password = authService.promptPassword(inputQueue, "Enter account password: ");
    string passwordHash = authService.generateHash(password);
    if (!account->verifyPassword(passwordHash))
        return VoidResult::failure(ServiceError(ServiceError::Code::WrongPassword, "Wrong account password."));

    if (account->getBalance() > 0)
        return VoidResult::failure(ServiceError(ServiceError::Code::Custom, "Account balance is positive."));

    accountService.removeAccountNoAuth(accountNumber);
    ownership.removeOwnership(accountNumber);
    return VoidResult::success();
}

//••••••••••••••••••UserTransactionService••••••••••••••••••
// Handles user transaction commands.
// Reuses TransactionService and adds ownership validation.
UserTransactionService::UserTransactionService(AccountService& as, AccountOwnershipManager& own, AuthSession& sess, TransactionService& ts, FileManager& fm)
    : accountService(as), ownership(own), session(sess), transactionService(ts), fileManager(fm) {}

VoidResult UserTransactionService::depositTo(const string& accountNumber, double amount) {
    auto account = accountService.getAccount(accountNumber);
    if (!account)
        return VoidResult::failure(ServiceError(ServiceError::Code::NotFound, "account not found"));

    if (amount <= 0)
        return VoidResult::failure(ServiceError(ServiceError::Code::InvalidAmount));

    auto result = transactionService.deposit(accountNumber, amount);
    if (!result.isOk())
        return VoidResult::failure(result.getError());

    auto tx = result.getValue();
    cout << "Transaction ID: " << tx->getId() << endl;
    cout << "New balance: " << fixed << setprecision(2) << tx->getBalanceAfter() << endl;
    return VoidResult::success();
}

VoidResult UserTransactionService::withdrawFrom(const string& accountNumber, double amount, InputQueue& inputQueue) {
    if (!session.isLoggedIn())
        return VoidResult::failure(ServiceError(ServiceError::Code::NotLoggedIn));

    auto found = AccountOwnershipValidator::findOwnedAccount(
        accountService, ownership, accountNumber, session.getCurrentUser()->getNationalCode());
    if (!found.isOk())
        return VoidResult::failure(found.getError());

    auto result = transactionService.withdraw(accountNumber, amount, inputQueue, "Wrong account password.");
    if (!result.isOk())
        return VoidResult::failure(result.getError());

    if (amount <= 0)
        return VoidResult::failure(ServiceError(ServiceError::Code::InvalidAmount));

    auto tx = result.getValue();
    cout << "Transaction ID: " << tx->getId() << endl;
    cout << "New balance: " << fixed << setprecision(2) << tx->getBalanceAfter() << endl;
    return VoidResult::success();
}

VoidResult UserTransactionService::sendMoney(const string& from, const string& to, double amount, InputQueue& inputQueue) {
    if (!session.isLoggedIn())
        return VoidResult::failure(ServiceError(ServiceError::Code::NotLoggedIn));

    auto foundFrom = AccountOwnershipValidator::findOwnedAccount(
        accountService, ownership, from, session.getCurrentUser()->getNationalCode(), "Source account not found.");
    if (!foundFrom.isOk())
        return VoidResult::failure(foundFrom.getError());

    auto toAccount = accountService.getAccount(to);
    if (!toAccount)
        return VoidResult::failure(ServiceError(ServiceError::Code::NotFound, "Destination account not found."));

    auto result = transactionService.transfer(from, to, amount, inputQueue, "Wrong account password.");
    if (!result.isOk())
        return VoidResult::failure(result.getError());

    if (amount <= 0)
        return VoidResult::failure(ServiceError(ServiceError::Code::InvalidAmount));

    FeeManager feeManager(fileManager);
    double fee = feeManager.getTransferFee();

    auto tx = result.getValue();
    cout << "Transaction ID: " << tx->getId() << endl;
    cout << "Transfer fee: " << fixed << setprecision(2) << fee << endl;
    cout << "New balance: " << fixed << setprecision(2) << tx->getBalanceAfter() << endl;
    return VoidResult::success();
}

VoidResult UserTransactionService::balanceInquiry(const string& accountNumber) {
    if (!session.isLoggedIn())
        return VoidResult::failure(ServiceError(ServiceError::Code::NotLoggedIn));

    auto found = AccountOwnershipValidator::findOwnedAccount(
        accountService, ownership, accountNumber, session.getCurrentUser()->getNationalCode());
    if (!found.isOk())
        return VoidResult::failure(found.getError());

    auto balanceResult = transactionService.getBalanceForUser(accountNumber);
    if (!balanceResult.isOk()) {
        return VoidResult::failure(balanceResult.getError());
    }

    return VoidResult::success();
}

//••••••••••••••••••Input_Handling••••••••••••••••••

UserCommandContext::UserCommandContext(UserService& us, UserAccountService& uas, UserTransactionService& uts, AccountOwnershipManager& own, TransactionService& ts, InputQueue& iq)
    : users(us), userAccounts(uas), userTransactions(uts), ownership(own), transactions(ts), input(iq) {}

void SignupCommand::execute(const string&, UserCommandContext& ctx) {
    auto result = ctx.users.signup(ctx.input);
    if (result.isOk()) cout << "User created." << endl;
    else ErrorReporter::report(result.getError());
}

void LoginCommand::execute(const string&, UserCommandContext& ctx) {
    auto result = ctx.users.login(ctx.input);
    if (result.isOk()) cout << "Logged in." << endl;
    else ErrorReporter::report(result.getError());
}

void LogoutCommand::execute(const string&, UserCommandContext& ctx) {
    auto result = ctx.users.logout();
    if (result.isOk()) cout << "Logged out." << endl;
    else ErrorReporter::report(result.getError());
}

void DeleteMyUserCommand::execute(const string&, UserCommandContext& ctx) {
    auto result = ctx.users.deleteCurrentUser(ctx.input);
    if (result.isOk()) cout << "User deleted." << endl;
    else ErrorReporter::report(result.getError());
}

void OpenAccountCommand::execute(const string&, UserCommandContext& ctx) {
    auto result = ctx.userAccounts.openAccount(ctx.input);
    if (result.isOk()) cout << "Account created. Number: " << result.getValue()->getAccountNumber() << endl;
    else ErrorReporter::report(result.getError());
}

void DeleteMyAccountCommand::execute(const string& args, UserCommandContext& ctx) {
    auto argsResult = CommandParser::requireArgs(args);
    if (!argsResult.isOk()) {
        ErrorReporter::report(argsResult.getError());
        return;
    }
    auto result = ctx.userAccounts.deleteMyAccount(args, ctx.input);
    if (result.isOk()) {
        cout << "Account deleted." << endl;
    } else {
        ErrorReporter::report(result.getError());
    }
}

void MyAccountsCommand::execute(const string&, UserCommandContext& ctx) {
    auto result = ctx.userAccounts.myAccounts();
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }
    for (auto& account : result.getValue())
        cout << account->getAccountNumber() << " | Balance: " << fixed << setprecision(2) << account->getBalance() << endl;
}

void DepositToCommand::execute(const string& args, UserCommandContext& ctx) {
    auto argsResult = CommandParser::requireArgs(args, 2);
    if (!argsResult.isOk()) {
        ErrorReporter::report(argsResult.getError());
        return;
    }
    istringstream iss(args);
    string acc; double amount;
    iss >> acc >> amount;
    auto result = ctx.userTransactions.depositTo(acc, amount);
    if (!result.isOk()) ErrorReporter::report(result.getError());
}

void WithdrawFromCommand::execute(const string& args, UserCommandContext& ctx) {
    auto argsResult = CommandParser::requireArgs(args, 2);
    if (!argsResult.isOk()) {
        ErrorReporter::report(argsResult.getError());
        return;
    }
    istringstream iss(args);
    string acc; double amount;
    iss >> acc >> amount;
    auto result = ctx.userTransactions.withdrawFrom(acc, amount, ctx.input);
    if (!result.isOk()) ErrorReporter::report(result.getError());
}

void SendMoneyCommand::execute(const string& args, UserCommandContext& ctx) {
    auto argsResult = CommandParser::requireArgs(args, 3);
    if (!argsResult.isOk()) {
        ErrorReporter::report(argsResult.getError());
        return;
    }
    istringstream iss(args);
    string from, to; double amount;
    iss >> from >> to >> amount;
    auto result = ctx.userTransactions.sendMoney(from, to, amount, ctx.input);
    if (!result.isOk()) ErrorReporter::report(result.getError());
}

void BalanceInquiryCommand::execute(const string& args, UserCommandContext& ctx) {
    auto argsResult = CommandParser::requireArgs(args);
    if (!argsResult.isOk()) {
        ErrorReporter::report(argsResult.getError());
        return;
    }
    auto result = ctx.userTransactions.balanceInquiry(args);
    if (!result.isOk()) ErrorReporter::report(result.getError());
}

void ResetAllUserCommand::execute(const string&, UserCommandContext& ctx) {
    if (CommandParser::confirm(ctx.input)) {
        ctx.transactions.resetAll();
        ctx.users.reload();
        ctx.ownership.reload();
        cout << "All data cleared." << endl;
    } else {
        cout << "Cancelled." << endl;
    }
}

HandleUserCommand::HandleUserCommand() {
    registerCommands();
}

HandleUserCommand::HandleUserCommand(HandleAdminCommand&) {
    registerCommands();  
}

void HandleUserCommand::registerCommands() {
    commands["signup"] = make_unique<SignupCommand>();
    commands["login"] = make_unique<LoginCommand>();
    commands["logout"] = make_unique<LogoutCommand>();
    commands["delete_my_user"] = make_unique<DeleteMyUserCommand>();
    commands["open_account"] = make_unique<OpenAccountCommand>();
    commands["delete_my_account"] = make_unique<DeleteMyAccountCommand>();
    commands["my_accounts"] = make_unique<MyAccountsCommand>();
    commands["deposit_to"] = make_unique<DepositToCommand>();
    commands["withdraw_from"] = make_unique<WithdrawFromCommand>();
    commands["send_money"] = make_unique<SendMoneyCommand>();
    commands["balance_inquiry"] = make_unique<BalanceInquiryCommand>();
    commands["reset_all_user"] = make_unique<ResetAllUserCommand>();
}

bool HandleUserCommand::isUserCommand(const string& command) {
    return commands.find(command) != commands.end();
}

void HandleUserCommand::handleCommand(string& line, UserService& us, UserAccountService& uas, UserTransactionService& uts, AccountOwnershipManager& ownership, TransactionService& ts, InputQueue& inputQueue) {
    istringstream iss(line);
    string command, args;
    iss >> command;
    getline(iss >> ws, args);

    auto it = commands.find(command);
    if (it == commands.end()) return;

    UserCommandContext ctx(us, uas, uts, ownership, ts, inputQueue);
    it->second->execute(args, ctx);
}