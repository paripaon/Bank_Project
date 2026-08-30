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

//••••••••••••••••••AuthSession••••••••••••••••••
// Keeps track of the currently logged-in user.
// Keeping the login session in a separate class, makes it easier to manage.
bool AuthSession::isLoggedIn() const { return currentUser != nullptr; }
shared_ptr<User> AuthSession::getCurrentUser() const { return currentUser; }
void AuthSession::login(shared_ptr<User> user) { currentUser = user; }
void AuthSession::logout() { currentUser = nullptr; }

//••••••••••••••••••RankingService••••••••••••••••••
RankingService::RankingService(UserRepository& ur, FileManager& fm, vector<shared_ptr<User>>& usersRef)
    : userRepo(ur), fileManager(fm), users(usersRef) {
    fileManager.loadNextUserSeq(nextUserSeq);
}

int RankingService::assignRegistrationSeq() {
    int seq = nextUserSeq++;
    fileManager.saveNextUserSeq(nextUserSeq);
    return seq;
}

void RankingService::awardScore(const string& nationalCode, int delta) {
    for (auto& u : users) {
        if (u->getNationalCode() == nationalCode) {
            u->addScore(delta);
            userRepo.saveUsers(users);
            return;
        }
    }
}

Result<RankEntry> RankingService::getRank(const string& nationalCode) {
    auto all = getAllRankings();
    for (auto& entry : all)
        if (entry.nationalCode == nationalCode)
            return Result<RankEntry>::success(entry);
    return Result<RankEntry>::failure(ServiceError(ServiceError::Code::NotFound));
}

vector<RankEntry> RankingService::getAllRankings() {
    vector<shared_ptr<User>> sorted = users;
    sort(sorted.begin(), sorted.end(), [](const shared_ptr<User>& a, const shared_ptr<User>& b) {
        if (a->getScore() != b->getScore()) return a->getScore() > b->getScore();
        return a->getRegistrationSeq() < b->getRegistrationSeq();
    });

    vector<RankEntry> result;
    int rank = 1;
    for (auto& u : sorted) {
        result.push_back(RankEntry{
            u->getNationalCode(),
            u->getScore(),
            CreditLevel::calculate(u->getScore()),
            rank++
        });
    }
    return result;
}

//••••••••••••••••••UserService••••••••••••••••••
// Handles signup, login, logout, and user deletion for regular users.
UserService::UserService(UserRepository& ur, AuthService& as, AuthSession& sess, OwnershipRepository& own, RankingService& rank, vector<shared_ptr<User>>& usersRef)
    : userRepo(ur), authService(as), session(sess), ownership(own), ranking(rank), users(usersRef) {
    users = userRepo.loadUsers();
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
    user->setRegistrationSeq(ranking.assignRegistrationSeq());
    users.push_back(user);
    userRepo.saveUsers(users);
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
    userRepo.saveUsers(users);
    ranking.awardScore(user->getNationalCode(), ScoringRules::DELETE_USER);
    session.logout();
    return VoidResult::success();
}

Result<string> UserService::currentNationalCode() const {
    if (!session.isLoggedIn())
        return Result<string>::failure(ServiceError(ServiceError::Code::NotLoggedIn));
    return Result<string>::success(session.getCurrentUser()->getNationalCode());
}

void UserService::reload() {
    users = userRepo.loadUsers();
    session.logout();
}

//••••••••••••••••••AccounOwnerhipValidator••••••••••••••••••
// Checks if an account exists and belongs to the given user.
Result<shared_ptr<Account>> AccountOwnershipValidator::findOwnedAccount(AccountService& accountService, OwnershipRepository& ownership, const string& accountNumber, const string& nationalCode, const string& notFoundMessage) {
    auto account = accountService.getAccount(accountNumber);
    if (!account)
        return Result<shared_ptr<Account>>::failure(ServiceError(ServiceError::Code::NotFound, notFoundMessage));

    if (!ownership.isOwnedBy(accountNumber, nationalCode))
        return Result<shared_ptr<Account>>::failure(ServiceError(ServiceError::Code::NotOwner));

    return Result<shared_ptr<Account>>::success(account);
}

//••••••••••••••••••UserAccountService••••••••••••••••••
// Lets logged-in users open, view, and delete their own accounts.
UserAccountService::UserAccountService(AccountService& as, OwnershipRepository& own, AuthSession& sess, AuthService& auth, RankingService& rank)
    : accountService(as), ownership(own), session(sess), authService(auth), rank(rank) {}

Result<vector<shared_ptr<Account>>> UserAccountService::myAccounts() {
    if (!session.isLoggedIn())
        return Result<vector<shared_ptr<Account>>>::failure(ServiceError(ServiceError::Code::NotLoggedIn));

    vector<shared_ptr<Account>> result;
    auto accountNumbers = ownership.getAccountsOf(session.getCurrentUser()->getNationalCode());
    for (auto& accNum : accountNumbers) {
        auto account = accountService.getAccount(accNum);
        if (account) result.push_back(account);
    }
    
    if (result.empty()) { 
        return Result<vector<shared_ptr<Account>>>::failure(ServiceError(ServiceError::Code::NotFound, "No account found."));
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

    accountService.removeAccountNoAuth(accountNumber);
    ownership.removeOwnership(accountNumber);
    rank.awardScore(session.getCurrentUser()->getNationalCode(), ScoringRules::DELETE_ACCOUNT);
    return VoidResult::success();

    return VoidResult::success();
}

Result<string> UserAccountService::showIban(const string& accountNumber) {

    auto account = accountService.getAccount(accountNumber);
    if (!account)
        return Result<string>::failure(ServiceError(ServiceError::Code::NotFound, "Account not found."));

    return Result<string>::success(IbanGenerator::generate(accountNumber));
}

//••••••••••••••••••UserRequestService••••••••••••••••••
UserRequestService::UserRequestService(RequestService& rs, AccountService& as, OwnershipRepository& own, AuthSession& sess, AuthService& auth, RankingService& rank)
    : requestService(rs), accountService(as), ownership(own), session(sess), authService(auth), rank(rank) {}

Result<shared_ptr<Request>> UserRequestService::requestAccount(int branchId) {
    if (!session.isLoggedIn())
        return Result<shared_ptr<Request>>::failure(ServiceError(ServiceError::Code::NotLoggedIn));

    return requestService.createRequest(session.getCurrentUser()->getNationalCode(), branchId);
}

Result<vector<shared_ptr<Request>>> UserRequestService::myRequests() {
    if (!session.isLoggedIn())
        return Result<vector<shared_ptr<Request>>>::failure(ServiceError(ServiceError::Code::NotLoggedIn));

    return requestService.getRequestsOf(session.getCurrentUser()->getNationalCode());
}

VoidResult UserRequestService::cancelRequest(int requestId) {
    if (!session.isLoggedIn())
        return VoidResult::failure(ServiceError(ServiceError::Code::NotLoggedIn));

    return requestService.cancelRequest(requestId, session.getCurrentUser()->getNationalCode());
}

Result<shared_ptr<Account>> UserRequestService::activateAccount(int requestId, InputQueue& inputQueue) {
    if (!session.isLoggedIn())
        return Result<shared_ptr<Account>>::failure(ServiceError(ServiceError::Code::NotLoggedIn));

    string nationalCode = session.getCurrentUser()->getNationalCode();
    auto prep = requestService.prepareActivation(requestId, nationalCode);
    if (!prep.isOk())
        return Result<shared_ptr<Account>>::failure(prep.getError());

    auto req = prep.getValue();
    string password = authService.promptPassword(inputQueue, "Enter account password: ");
    string passwordHash = authService.generateHash(password);

    auto account = accountService.createUserAccount(passwordHash);
    ownership.registerOwnership(account->getAccountNumber(), nationalCode);
    requestService.markActivated(requestId, account->getAccountNumber());

    rank.awardScore(nationalCode, ScoringRules::OPEN_ACCOUNT);

    return Result<shared_ptr<Account>>::success(account);
}

//••••••••••••••••••UserTransactionService••••••••••••••••••
// Handles user transaction commands.
// Reuses TransactionService and adds ownership validation.
UserTransactionService::UserTransactionService(AccountService& as, OwnershipRepository& own, AuthSession& sess, TransactionService& ts, FileManager& fm, RankingService& rank, AuthService& authService)
    : accountService(as), ownership(own), session(sess), transactionService(ts), fileManager(fm), rank(rank),  authService(authService)  {}

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

    if (session.isLoggedIn())
        rank.awardScore(session.getCurrentUser()->getNationalCode(), ScoringRules::DEPOSIT);

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

    rank.awardScore(session.getCurrentUser()->getNationalCode(), ScoringRules::WITHDRAW);

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

    rank.awardScore(session.getCurrentUser()->getNationalCode(), ScoringRules::TRANSFER);

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

     rank.awardScore(session.getCurrentUser()->getNationalCode(), ScoringRules::BALANCE_INQUIRY);

    return VoidResult::success();
}

Result<string> UserTransactionService::requestOtp(const string& accountNumber, OtpService& otpService) {
    if (!session.isLoggedIn())
        return Result<string>::failure(ServiceError(ServiceError::Code::NotLoggedIn));

    auto found = AccountOwnershipValidator::findOwnedAccount(
        accountService, ownership, accountNumber, session.getCurrentUser()->getNationalCode());
    if (!found.isOk())
        return Result<string>::failure(found.getError());

    int secondsRemaining;
    string code = otpService.requestOtp(accountNumber, secondsRemaining);
    cout << "OTP: " << code << endl;
    cout << "expires in " << secondsRemaining << " seconds" << endl;
    return Result<string>::success(code);
}

VoidResult UserTransactionService::onlinePayment(const string& from, const string& to, double amount, InputQueue& inputQueue, OtpService& otpService) {
    if (!session.isLoggedIn())
        return VoidResult::failure(ServiceError(ServiceError::Code::NotLoggedIn));

    auto found = AccountOwnershipValidator::findOwnedAccount(
        accountService, ownership, from, session.getCurrentUser()->getNationalCode(), "Source account not found.");
    if (!found.isOk()) return VoidResult::failure(found.getError());

    auto toAccount = accountService.getAccount(to);
    if (!toAccount) { return VoidResult::failure(ServiceError(ServiceError::Code::NotFound, "Destination account not found.")); }
    if (!toAccount->isActive()) { return VoidResult::failure(ServiceError(ServiceError::Code::DestinationInactive)); }

    if (amount <= 0) { return VoidResult::failure(ServiceError(ServiceError::Code::InvalidAmount)); }
    if (amount > TransactionLimits::MAX_ONLINE_PAYMENT_AMOUNT) { return VoidResult::failure(ServiceError(ServiceError::Code::Custom, "Transaction limit exceeded.")); }

    cout << "Enter OTP: " << flush;
    string otp;
    inputQueue.getLineBlocking(otp);

    auto result = transactionService.onlinePayment(from, to, amount, otp, otpService);
    if (!result.isOk())
        return VoidResult::failure(result.getError());

    auto tx = transactionService.onlinePayment(from, to, amount, otp, otpService).getValue();
    cout << "Payment successful." << endl;
    cout << "Transaction ID: " << tx->getId() << endl;
    cout << "New balance: " << fixed << setprecision(2) << tx->getBalanceAfter() << endl;
    return VoidResult::success();
}

VoidResult UserTransactionService::payaTransfer(const string& from, const string& destinationIban, double amount, InputQueue& inputQueue, PayaService& payaService, AuthService& authService) {
    if (!session.isLoggedIn())
        return VoidResult::failure(ServiceError(ServiceError::Code::NotLoggedIn));

    auto found = AccountOwnershipValidator::findOwnedAccount(
        accountService, ownership, from, session.getCurrentUser()->getNationalCode(), "Source account not found.");
    if (!found.isOk())
        return VoidResult::failure(found.getError());

    auto result = payaService.createRequest(from, destinationIban, amount);
    if (!result.isOk())
        return VoidResult::failure(result.getError());

    string password = authService.promptPassword(inputQueue);
    if (!authService.verifyPassword(found.getValue(), password)) {
        return VoidResult::failure(ServiceError(ServiceError::Code::WrongPassword));
    }

    auto req = result.getValue();
    cout << "Paya request registered" << endl;
    cout << "Request ID: " << req->getId() << endl;

    string status = req->getStatus();
    string displayStatus = status;
    if (status == "PENDING") displayStatus = "Pending";
    else if (status == "APPROVED") displayStatus = "Approved";
    else if (status == "REJECTED") displayStatus = "Rejected";
    cout << "Status: " << displayStatus << endl;
        return VoidResult::success();
}

VoidResult UserTransactionService::exportHistory(const string& accountNumber, const string& format) {
    if (!session.isLoggedIn())
        return VoidResult::failure(ServiceError(ServiceError::Code::NotLoggedIn));

    auto found = AccountOwnershipValidator::findOwnedAccount(
        accountService, ownership, accountNumber, session.getCurrentUser()->getNationalCode());
    if (!found.isOk())
        return VoidResult::failure(found.getError());

    if (format != "json" && format != "csv")
        return VoidResult::failure(ServiceError(ServiceError::Code::NotFound, "Unsupported format."));

    auto account = found.getValue();
    const auto& txs = account->getTransactions();

    string filename = "history_" + accountNumber + "." + format;
    ofstream outFile(filename);
    if (!outFile.is_open())
        return VoidResult::failure(ServiceError(ServiceError::Code::NotFound, "Could not create export file."));

    double runningBalance = 0.0;

    if (format == "csv") {
        outFile << "id,timestamp,type,amount,balance_after,from,to\n";
    } else {
        outFile << "[\n";
    }

    for (size_t i = 0; i < txs.size(); ++i) {
        const auto& tx = txs[i];
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
        } else if (tx->getType() == "PAYA") {
            runningBalance -= tx->getAmount(); sign = "-";
        }

        if (format == "csv") {
            outFile << tx->getId() << ","
                << tx->getTimestamp() << ","
                << tx->getType() << ","
                << sign << fixed << setprecision(2) << tx->getAmount() << ","
                << fixed << setprecision(2) << runningBalance << ",";
            if (tx->getType() == "TRANSFER")
                outFile << tx->getFromAccount() << "," << tx->getToAccount();
            outFile << "\n";
        } else {
            outFile << "  {\n";
            outFile << "    \"id\": " << tx->getId() << ",\n";
            outFile << "    \"timestamp\": \"" << tx->getTimestamp() << "\",\n";
            outFile << "    \"type\": \"" << tx->getType() << "\",\n";
            outFile << "    \"amount\": \"" << sign << fixed << setprecision(2) << tx->getAmount() << "\",\n";
            outFile << "    \"balance_after\": " << fixed << setprecision(2) << runningBalance;
            if (tx->getType() == "TRANSFER") {
                outFile << ",\n    \"from\": \"" << tx->getFromAccount() << "\",\n";
                outFile << "    \"to\": \"" << tx->getToAccount() << "\"\n";
            } else {
                outFile << "\n";
            }
            outFile << "  }" << (i + 1 < txs.size() ? "," : "") << "\n";
        }
    }

    if (format == "json") {
        outFile << "]\n";
    }

    outFile.close();
    return VoidResult::success();
}