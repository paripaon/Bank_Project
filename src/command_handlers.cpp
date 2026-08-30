#include "../include/command_handlers.hpp"
#include <cctype>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <algorithm>

// CommandParser
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

//&&&&&&&&&&&&&&&&&&&&&&&&&&&&& ADMIN &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&

//  AdminCommandContext
AdminCommandContext::AdminCommandContext(AccountService& as, TransactionService& ts, AuthService& authS, FeeManager& fm, PayaService& ps, RequestService& rs, InputQueue& iq, RankingService& rank)
    : accounts(as), transactions(ts), auth(authS), fees(fm), paya(ps), requests(rs), input(iq), rank(rank) {}

// -------------------------Admin Command Base-------------------------

void CreateBranchCommand::execute(const string& args, AdminCommandContext& ctx) {
    auto argsCheck = CommandParser::requireArgs(args);
    if (!argsCheck.isOk()) { ErrorReporter::report(argsCheck.getError()); return; }

    string name = args;
    if (!name.empty() and name.front() == '"') name = name.substr(1, name.size() - 2);
    ctx.accounts.createBranch(name);
}

void ListBranchesCommand::execute(const string&, AdminCommandContext& ctx) {
    auto result = ctx.accounts.listBranches();
    if (!result.isOk()) ErrorReporter::report(result.getError());
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
    auto result = ctx.transactions.getBalance(acc);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }
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
        ctx.paya.resetAll();
        ctx.requests.resetAll();
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

void ListPayaRequestsCommand::execute(const string&, AdminCommandContext& ctx) {
    ctx.paya.listRequests();
}

void ApprovePayaCommand::execute(const string& args, AdminCommandContext& ctx) {
    auto argsCheck = CommandParser::requireArgs(args);
    if (!argsCheck.isOk()) { ErrorReporter::report(argsCheck.getError()); return; }

    istringstream iss(args);
    int requestId;
    if (!(iss >> requestId)) {
        ErrorReporter::report(ServiceError(ServiceError::Code::InvalidArguments));
        return;
    }

    auto result = ctx.paya.approve(requestId);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }

    cout << "Paya approved. Transaction ID: " << result.getValue()->getId() << endl;
}

void RejectPayaCommand::execute(const string& args, AdminCommandContext& ctx) {
    auto argsCheck = CommandParser::requireArgs(args);
    if (!argsCheck.isOk()) { ErrorReporter::report(argsCheck.getError()); return; }

    istringstream iss(args);
    int requestId;
    if (!(iss >> requestId)) {
        ErrorReporter::report(ServiceError(ServiceError::Code::InvalidArguments));
        return;
    }

    auto result = ctx.paya.reject(requestId);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }

    cout << "Paya rejected. Amount returned to source account." << endl;
}

void BranchDashboardCommand::execute(const string& args, AdminCommandContext& ctx) {
    auto argsCheck = CommandParser::requireArgs(args);
    if (!argsCheck.isOk()) { ErrorReporter::report(argsCheck.getError()); return; }

    istringstream iss(args);
    int branchId;
    if (!(iss >> branchId)) {
        ErrorReporter::report(ServiceError(ServiceError::Code::InvalidArguments));
        return;
    }

    auto branch = ctx.accounts.getBranch(branchId);
    if (!branch) {
        ErrorReporter::report(ServiceError(ServiceError::Code::NotFound, "Branch not found."));
        return;
    }

    cout << "Branch: " << branch->getName() << endl;
    ctx.requests.branchDashboard(branchId);
}

void ListRequestsCommand::execute(const string& args, AdminCommandContext& ctx) {
    auto argsCheck = CommandParser::requireArgs(args);
    if (!argsCheck.isOk()) { ErrorReporter::report(argsCheck.getError()); return; }

    istringstream iss(args);
    int branchId;
    if (!(iss >> branchId)) {
        ErrorReporter::report(ServiceError(ServiceError::Code::InvalidArguments));
        return;
    }

    auto branch = ctx.accounts.getBranch(branchId);
    if (!branch) {
        ErrorReporter::report(ServiceError(ServiceError::Code::NotFound, "Branch not found."));
        return;
    }

    ctx.requests.listRequests(branchId);
}

void ApproveRequestCommand::execute(const string& args, AdminCommandContext& ctx) {
    auto argsCheck = CommandParser::requireArgs(args);
    if (!argsCheck.isOk()) { ErrorReporter::report(argsCheck.getError()); return; }

    istringstream iss(args);
    int requestId;
    if (!(iss >> requestId)) {
        ErrorReporter::report(ServiceError(ServiceError::Code::InvalidArguments));
        return;
    }

    auto result = ctx.requests.approveRequest(requestId);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }
    cout << "Request " << requestId << " approved. Waiting for user activation." << endl;
}

void RejectRequestCommand::execute(const string& args, AdminCommandContext& ctx) {
    auto argsCheck = CommandParser::requireArgs(args);
    if (!argsCheck.isOk()) { ErrorReporter::report(argsCheck.getError()); return; }

    istringstream iss(args);
    int requestId;
    if (!(iss >> requestId)) {
        ErrorReporter::report(ServiceError(ServiceError::Code::InvalidArguments));
        return;
    }

    auto req = ctx.requests.findRequest(requestId);
    if (!req) { ErrorReporter::report(ServiceError(ServiceError::Code::NotFound, "Request not found.")); return; }
    if (req->getStatus() != "PENDING") { ErrorReporter::report(ServiceError(ServiceError::Code::Custom, "Request is not pending.")); return; }

    cout << "Enter rejection reason: " << flush;
    string reason;
    ctx.input.getLineBlocking(reason);

    auto result = ctx.requests.rejectRequest(requestId, reason);
    if (!result.isOk()) {
        ErrorReporter::report(result.getError());
        return;
    }
    cout << "Request " << requestId << " rejected." << endl;
}

void ShowRankingsCommand::execute(const string&, AdminCommandContext& ctx) {
    auto rankings = ctx.rank.getAllRankings();
    if (rankings.empty()) {
        cout << "No users available." << endl;
        return;
    }
    for (auto& entry : rankings) {
        cout << entry.rank << " | " << entry.nationalCode
             << " | Score: " << entry.score
             << " | Level: " << entry.level << endl;
    }
}

//  HandleAdminCommand
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
    commands["list_paya_requests"] = make_unique<ListPayaRequestsCommand>();
    commands["approve_paya"] = make_unique<ApprovePayaCommand>();
    commands["reject_paya"] = make_unique<RejectPayaCommand>();
    commands["branch_dashboard"] = make_unique<BranchDashboardCommand>();
    commands["list_requests"] = make_unique<ListRequestsCommand>();
    commands["approve_request"] = make_unique<ApproveRequestCommand>();
    commands["reject_request"] = make_unique<RejectRequestCommand>();
    commands["show_rankings"] = make_unique<ShowRankingsCommand>();
}

bool HandleAdminCommand::isAdminCommand(const string& command) {
    return commands.find(command) != commands.end();
}

void HandleAdminCommand::handleCommand(string& line, AccountService& as, TransactionService& ts, AuthService& auth, FeeManager& fm, PayaService& paya,RequestService& rs, InputQueue& inputQueue, RankingService& rank) {
    istringstream iss(line);
    string command, args;
    iss >> command;
    getline(iss >> ws, args);

    auto it = commands.find(command);
    if (it == commands.end()) return;

    AdminCommandContext ctx(as, ts, auth, fm, paya, rs, inputQueue, rank);
    it->second->execute(args, ctx);
}





//&&&&&&&&&&&&&&&&&&&&&&&&&&&&& USER &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&

//  UserCommandContext
UserCommandContext::UserCommandContext(UserService& us, UserAccountService& uas, UserTransactionService& uts, UserRequestService& urs, OwnershipRepository& own, TransactionService& ts, OtpService& os, PayaService& ps, RequestService& rs, InputQueue& iq, RankingService& rank, AuthService& auth) 
    : users(us), userAccounts(uas), userTransactions(uts), userRequests(urs), ownership(own), transactions(ts), otp(os), paya(ps), requests(rs), input(iq), rank(rank), auth(auth) {}

// -------------------------User Command Base-------------------------
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
        ctx.paya.resetAll();
        ctx.users.reload();
        ctx.ownership.reload();
        cout << "All data cleared." << endl;
    } else {
        cout << "Cancelled." << endl;
    }
}

void ShowIbanCommand::execute(const string& args, UserCommandContext& ctx) {
    auto argsResult = CommandParser::requireArgs(args);
    if (!argsResult.isOk()) { ErrorReporter::report(argsResult.getError()); return; }

    auto result = ctx.userAccounts.showIban(args);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }
    cout << "IBAN: " << result.getValue() << endl;
}

void RequestOtpCommand::execute(const string& args, UserCommandContext& ctx) {
    auto argsResult = CommandParser::requireArgs(args);
    if (!argsResult.isOk()) { ErrorReporter::report(argsResult.getError()); return; }

    auto result = ctx.userTransactions.requestOtp(args, ctx.otp);
    if (!result.isOk()) ErrorReporter::report(result.getError());
}

void OnlinePaymentCommand::execute(const string& args, UserCommandContext& ctx) {
    auto argsResult = CommandParser::requireArgs(args, 3);
    if (!argsResult.isOk()) { ErrorReporter::report(argsResult.getError()); return; }

    istringstream iss(args);
    string from, to; double amount;
    iss >> from >> to >> amount;
    auto result = ctx.userTransactions.onlinePayment(from, to, amount, ctx.input, ctx.otp);
    if (!result.isOk()) ErrorReporter::report(result.getError());
}

void PayaTransferCommand::execute(const string& args, UserCommandContext& ctx) {
    auto argsResult = CommandParser::requireArgs(args, 3);
    if (!argsResult.isOk()) { ErrorReporter::report(argsResult.getError()); return; }

    istringstream iss(args);
    string from, iban; double amount;
    iss >> from >> iban >> amount;
    auto result = ctx.userTransactions.payaTransfer(from, iban, amount, ctx.input, ctx.paya, ctx.auth);
    if (!result.isOk()) ErrorReporter::report(result.getError());
}

void RequestAccountCommand::execute(const string& args, UserCommandContext& ctx) {
    auto argsCheck = CommandParser::requireArgs(args);
    if (!argsCheck.isOk()) { ErrorReporter::report(argsCheck.getError()); return; }

    istringstream iss(args);
    int branchId;
    if (!(iss >> branchId)) {
        ErrorReporter::report(ServiceError(ServiceError::Code::InvalidArguments));
        return;
    }

    auto result = ctx.userRequests.requestAccount(branchId);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }
    cout << "Request submitted. ID: " << result.getValue()->getId() << endl;
}

void MyRequestsCommand::execute(const string&, UserCommandContext& ctx) {
    auto result = ctx.userRequests.myRequests();
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }

    for (auto& r : result.getValue()) {
        cout << r->getId() << " | Branch: " << r->getBranchId()
             << " | Status: " << left << setw(9) << r->getStatus();
        if (r->getStatus() == "REJECTED")
            cout << " | Reason: " << r->getReason();
        else
            cout << " | " << r->getTimestamp();
        cout << endl;
    }
}

void CancelRequestCommand::execute(const string& args, UserCommandContext& ctx) {
    auto argsCheck = CommandParser::requireArgs(args);
    if (!argsCheck.isOk()) { ErrorReporter::report(argsCheck.getError()); return; }

    istringstream iss(args);
    int requestId;
    if (!(iss >> requestId)) {
        ErrorReporter::report(ServiceError(ServiceError::Code::InvalidArguments));
        return;
    }

    auto result = ctx.userRequests.cancelRequest(requestId);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }
    cout << "Request " << requestId << " cancelled." << endl;
}

void ActivateAccountCommand::execute(const string& args, UserCommandContext& ctx) {
    auto argsCheck = CommandParser::requireArgs(args);
    if (!argsCheck.isOk()) { ErrorReporter::report(argsCheck.getError()); return; }

    istringstream iss(args);
    int requestId;
    if (!(iss >> requestId)) {
        ErrorReporter::report(ServiceError(ServiceError::Code::InvalidArguments));
        return;
    }

    auto result = ctx.userRequests.activateAccount(requestId, ctx.input);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }
    cout << "Account activated. Number: " << result.getValue()->getAccountNumber() << endl;
}

void ExportHistoryCommand::execute(const string& args, UserCommandContext& ctx) {
    auto argsCheck = CommandParser::requireArgs(args);
    if (!argsCheck.isOk()) { ErrorReporter::report(argsCheck.getError()); return; }

    istringstream iss(args);
    string accountNumber, format;
    iss >> accountNumber >> format;

    if (accountNumber.empty()) {
        ErrorReporter::report(ServiceError(ServiceError::Code::InvalidArguments));
        return;
    }

    if (format.empty()) {
        format = "json";
    }

    auto result = ctx.userTransactions.exportHistory(accountNumber, format);
    if (!result.isOk()) { ErrorReporter::report(result.getError()); return; }

    cout << "History exported to history_" << accountNumber << "." << format << endl;
}

void MyRankCommand::execute(const string&, UserCommandContext& ctx) {
    auto codeResult = ctx.users.currentNationalCode();
    if (!codeResult.isOk()) { ErrorReporter::report(codeResult.getError()); return; }

    auto rankResult = ctx.rank.getRank(codeResult.getValue());
    if (!rankResult.isOk()) { ErrorReporter::report(rankResult.getError()); return; }

    auto entry = rankResult.getValue();
    cout << "Rank : " << entry.rank << endl;
    cout << "Score: " << entry.score << endl;
    cout << "Level: " << entry.level << endl;
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
    commands["request_account"] = make_unique<RequestAccountCommand>();
    commands["my_requests"] = make_unique<MyRequestsCommand>();
    commands["cancel_request"] = make_unique<CancelRequestCommand>();
    commands["activate_account"] = make_unique<ActivateAccountCommand>();
    commands["delete_my_account"] = make_unique<DeleteMyAccountCommand>();
    commands["my_accounts"] = make_unique<MyAccountsCommand>();
    commands["deposit_to"] = make_unique<DepositToCommand>();
    commands["withdraw_from"] = make_unique<WithdrawFromCommand>();
    commands["send_money"] = make_unique<SendMoneyCommand>();
    commands["balance_inquiry"] = make_unique<BalanceInquiryCommand>();
    commands["reset_all_user"] = make_unique<ResetAllUserCommand>();
    commands["show_iban"] = make_unique<ShowIbanCommand>();
    commands["request_OTP"] = make_unique<RequestOtpCommand>();
    commands["online_payment"] = make_unique<OnlinePaymentCommand>();
    commands["paya_transfer"] = make_unique<PayaTransferCommand>();
    commands["export_history"] = make_unique<ExportHistoryCommand>();
    commands["my_rank"] = make_unique<MyRankCommand>();
}

bool HandleUserCommand::isUserCommand(const string& command) {
    return commands.find(command) != commands.end();
}

void HandleUserCommand::handleCommand(string& line, UserService& us, UserAccountService& uas, UserTransactionService& uts, UserRequestService& urs, 
    OwnershipRepository& ownership, TransactionService& ts, OtpService& otp, PayaService& paya, RequestService& rs, InputQueue& inputQueue, RankingService& rank, AuthService& authService) {
    istringstream iss(line);
    string command, args;
    iss >> command;
    getline(iss >> ws, args);

    auto it = commands.find(command);
    if (it == commands.end()) return;

    UserCommandContext ctx(us, uas, uts, urs, ownership, ts, otp, paya, rs, inputQueue, rank, authService);
    it->second->execute(args, ctx);
}