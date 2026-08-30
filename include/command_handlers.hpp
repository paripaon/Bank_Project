#pragma once

#include <string>
#include <map>
#include <memory>
#include <vector>
#include "admin.hpp"
#include "user.hpp"
#include "models.hpp"
#include "persistence.hpp"

using namespace std;

// CommandParser
class CommandParser {
public:
    static bool isValidAccountNumber(const string& acc);
    static VoidResult requireArgs(const string& args, int count = 1);
    static VoidResult parseAccountAmount(const string& args, string& acc, double& amount);
    static VoidResult parseTransfer(const string& args, string& from, string& to, double& amount);
    static bool confirm(InputQueue& inputQueue, const string& prompt = "Are you sure? This deletes everything. (yes/no): ");
};

//&&&&&&&&&&&&&&&&&&&&&&&&&&&&& ADMIN &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&

// Admin Command Context
struct AdminCommandContext {
    AccountService& accounts;
    TransactionService& transactions;
    AuthService& auth;
    FeeManager& fees;
    PayaService& paya;
    RequestService& requests;
    InputQueue& input;
    RankingService& rank;

    AdminCommandContext(AccountService& as, TransactionService& ts, AuthService& authS,
        FeeManager& fm, PayaService& ps, RequestService& rs, InputQueue& iq, RankingService& rank);
};

// -------------------------Admin Command Base-------------------------
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

class ListPayaRequestsCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class ApprovePayaCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class RejectPayaCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class BranchDashboardCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class ListRequestsCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class ApproveRequestCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class RejectRequestCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

class ShowRankingsCommand : public AdminCommand {
public:
    void execute(const string& args, AdminCommandContext& ctx) override;
};

//  HandleAdminCommand
class HandleAdminCommand {
public:
    HandleAdminCommand();

    bool isAdminCommand(const string& command);
    void handleCommand(string& line, AccountService& as, TransactionService& ts, AuthService& auth, 
        FeeManager& fm, PayaService& paya, RequestService& rs, InputQueue& inputQueue, RankingService& rank);

private:
    map<string, unique_ptr<AdminCommand>> commands;
    void registerCommands();
};





//&&&&&&&&&&&&&&&&&&&&&&&&&&&&& USER &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&

//  User Command Context
struct UserCommandContext {
    UserService& users;
    UserAccountService& userAccounts;
    UserTransactionService& userTransactions;
    UserRequestService& userRequests;
    OwnershipRepository& ownership;
    TransactionService& transactions;
    OtpService& otp;
    PayaService& paya;
    RequestService& requests;
    InputQueue& input;
    RankingService& rank;
    AuthService& auth;

    UserCommandContext(UserService& us, UserAccountService& uas, UserTransactionService& uts, UserRequestService& urs, OwnershipRepository& own, 
        TransactionService& ts, OtpService& os, PayaService& ps, RequestService& rs, InputQueue& iq, RankingService& rank, AuthService& auth);
};

// -------------------------User Command Base-------------------------

class UserCommand : public Command<UserCommandContext> {
public:
    virtual ~UserCommand() = default;
};

class SignupCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class LoginCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class LogoutCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class DeleteMyUserCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class OpenAccountCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class DeleteMyAccountCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class MyAccountsCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class DepositToCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class WithdrawFromCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class SendMoneyCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class BalanceInquiryCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class ResetAllUserCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class ShowIbanCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class RequestOtpCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class OnlinePaymentCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class PayaTransferCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class RequestAccountCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class MyRequestsCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class CancelRequestCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class ActivateAccountCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class ExportHistoryCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

class MyRankCommand : public UserCommand {
public:
    void execute(const string& args, UserCommandContext& ctx) override;
};

// HandleUserCommand
class HandleUserCommand {
public:
    HandleUserCommand();
    explicit HandleUserCommand(HandleAdminCommand&);

    bool isUserCommand(const string& command);
    void handleCommand(string& line, UserService& us, UserAccountService& uas, UserTransactionService& uts, UserRequestService& urs, 
        OwnershipRepository& ownership, TransactionService& ts, OtpService& otp, PayaService& paya, RequestService& rs,InputQueue& inputQueue, RankingService& rank, AuthService& auth);

private:
    map<string, unique_ptr<UserCommand>> commands;
    void registerCommands();
};