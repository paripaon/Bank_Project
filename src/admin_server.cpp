#include "../include/httplib.h"
#include "../include/json.hpp"
#include "../include/admin.hpp"
#include "../include/user.hpp"
#include "../include/session_manager.hpp"
#include <iostream>
#include <sstream>
#include <functional>

using namespace std;
using namespace httplib;

static void sendOk(httplib::Response& res, const string& message, const json& data = json::object()) {
    res.status = 200;
    res.set_content(json{{"ok", true}, {"message", message}, {"data", data}}.dump(), "application/json");
}

static void sendError(httplib::Response& res, int status, const string& message) {
    res.status = status;
    string prefixed = (message.rfind("Error: ", 0) == 0) ? message : ("Error: " + message);
    res.set_content(json{{"ok", false}, {"error", prefixed}}.dump(), "application/json");
}

static void sendServiceError(httplib::Response& res, const ServiceError& err) {
    int status = 400;
    string message = err.detail;
    switch (err.code) {
        case ServiceError::Code::InvalidArguments:
            status = 400; if (message.empty()) message = "Invalid arguments."; break;
        case ServiceError::Code::Custom:
            status = 400; if (message.empty()) message = "Custom error."; break;
        case ServiceError::Code::WrongPassword:
            status = 401; if (message.empty()) message = "Wrong password."; break;
        case ServiceError::Code::NotLoggedIn:
            status = 401; if (message.empty()) message = "No user logged in."; break;
        case ServiceError::Code::NotOwner:
            status = 403; if (message.empty()) message = "Account does not belong to user."; break;
        case ServiceError::Code::NotFound:
            status = 404; if (message.empty()) message = "Not found."; break;
        case ServiceError::Code::Timeout:
            status = 408; if (message.empty()) message = "Timed out."; break;
        case ServiceError::Code::AlreadyExists:
            status = 409; if (message.empty()) message = "Already exists."; break;
        case ServiceError::Code::HasDependents:
            status = 409; if (message.empty()) message = "Has dependent records."; break;
        case ServiceError::Code::AlreadyLoggedIn:
            status = 409; if (message.empty()) message = "User already logged in."; break;
        case ServiceError::Code::InsufficientFunds:
            status = 422; if (message.empty()) message = "Insufficient funds."; break;
        case ServiceError::Code::AccountInactive:
            status = 422; if (message.empty()) message = "Account is inactive."; break;
        case ServiceError::Code::DestinationInactive:
            status = 422; if (message.empty()) message = "Destination account is inactive."; break;
        case ServiceError::Code::InvalidAmount:
            status = 422; if (message.empty()) message = "Amount must be positive."; break;

        default:
            status = 500; if (message.empty()) message = "Unknown error."; break;
    }
    sendError(res, status, "Error: " + message);
}

// Authorization: Bearer <token> -> extract token
static string extractBearerToken(const httplib::Request& req) {
    string auth = req.get_header_value("Authorization");
    if (auth.rfind("Bearer ", 0) == 0) {
        return auth.substr(7);
    }
    return "";
}

//parse json
static bool parseJsonBody(const httplib::Request& req, httplib::Response& res, json& body) {
    try {
        body = req.body.empty() ? json::object() : json::parse(req.body);
        return true;
    } catch (...) {
        sendError(res, 400, "Invalid JSON");
        return false;
    }
}

//checking args and keys
template<typename T>
static bool requireField(const json& body, const string& key, T& value, httplib::Response& res) {
    if (!body.contains(key)) {
        sendError(res, 400, "Missing field: " + key);
        return false;
    }
    try {
        value = body[key].get<T>();
        return true;
    } catch (...) {
        sendError(res, 400, "Invalid type for field: " + key);
        return false;
    }
}

//cout -> string
static string captureCout(const function<void()>& fn) {
    ostringstream oss;
    streambuf* old = cout.rdbuf(oss.rdbuf());
    fn();
    cout.rdbuf(old);
    return oss.str();
}

//string -> int
static bool parseId(const string& raw, int& out, httplib::Response& res) {
    try {
        size_t pos;
        out = stoi(raw, &pos);
        if (pos != raw.size()) throw invalid_argument("trailing chars");
        return true;
    } catch (...) {
        sendError(res, 400, "Invalid id: " + raw);
        return false;
    }
}

int main() {
    FileManager fileManager;
    AuthService authService;
    FeeManager feeManager(fileManager);
    AccountService accountService(fileManager, authService);
    RequestService requestService(fileManager, accountService);
    TransactionService transactionService(fileManager, accountService, requestService, authService, feeManager);
    OtpService otpService;
    PayaService payaService(fileManager, accountService, transactionService, requestService);

    OwnershipRepository ownershipManager(fileManager);
    UserRepository userFileManager(fileManager);
    vector<shared_ptr<User>> loadedUsers = userFileManager.loadUsers();
    RankingService rankingService(userFileManager, fileManager, loadedUsers);

    Server svr;
    SessionManager sessionManager(300);

    //token checker
    using AuthedHandler = function<void(const httplib::Request&, httplib::Response&, const string& userId)>;
    auto withAuth = [&sessionManager](AuthedHandler fn) -> Server::Handler {
        return [&sessionManager, fn](const httplib::Request& req, httplib::Response& res) {
            string token = extractBearerToken(req);
            string userId;
            if (!sessionManager.validateToken(token, userId)) {
                sendError(res, 401, "Unauthorized");
                return;
            }
            fn(req, res, userId);
        };
    };

    //json result
    auto accountToJson = [](const shared_ptr<Account>& a) {
        return json{
            {"account_number", a->getAccountNumber()},
            {"branch_id", a->getBranchId()},
            {"active", a->isActive()},
            {"balance", a->getBalance()}
        };
    };
    auto branchToJson = [](const shared_ptr<Branch>& b) {
        return json{{"id", b->getId()}, {"name", b->getName()}};
    };
    auto requestToJson = [](const shared_ptr<::Request>& r) {
        return json{
            {"id", r->getId()},
            {"national_code", r->getNationalCode()},
            {"branch_id", r->getBranchId()},
            {"status", r->getStatus()},
            {"timestamp", r->getTimestamp()}
        };
    };
    auto txToJson = [](const shared_ptr<Transaction>& t) {
        return json{ //transaction == tx
            {"id", t->getId()},
            {"timestamp", t->getTimestamp()},
            {"type", t->getType()},
            {"amount", t->getAmount()},
            {"from_account", t->getFromAccount()},
            {"to_account", t->getToAccount()},
            {"balance_after", t->getBalanceAfter()}
        };
    };
    auto payaToJson = [](const shared_ptr<PayaRequest>& r) {
        return json{
            {"id", r->getId()},
            {"from_account", r->getFromAccount()},
            {"destination_account", r->getDestinationAccount()},
            {"amount", r->getAmount()},
            {"status", r->getStatus()}
        };
    };
    auto rankToJson = [](const RankEntry& r) {
        return json{
            {"national_code", r.nationalCode},
            {"score", r.score},
            {"level", r.level},
            {"rank", r.rank}
        };
    };
    auto branchExists = [&accountService](int branchId) {
        for (auto& b : accountService.getBranches()) {
            if (b->getId() == branchId) return true;
        }
        return false;
    };

//مدیریت شعبه و دسترسی
    ///admin/auth/login  1
    svr.Post("/admin/auth/login", [&](const httplib::Request& req, httplib::Response& res) {
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string username, password;
        if (!requireField(body, "username", username, res)) return;
        if (!requireField(body, "password", password, res)) return;

        if (username == "admin" && password == "admin") {
            string token = sessionManager.createToken("admin");
            sendOk(res, "Login successful", {{"token", token}});
        } else {
            sendError(res, 401, "Invalid credentials");
        }
    });

    //admin/auth/logout  2
    svr.Post("/admin/auth/logout", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        sessionManager.removeToken(extractBearerToken(req));
        sendOk(res, "Logged out.");
    }));

    ///admin/branches  3
    svr.Post("/admin/branches", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string name;
        if (!requireField(body, "name", name, res)) return;

        string output = captureCout([&]() { accountService.createBranch(name); });

        json branches = json::array();
        for (auto& b : accountService.getBranches()) branches.push_back(branchToJson(b));
        sendOk(res, output.empty() ? "Branch created." : output, {{"branches", branches}});
    }));

    ///admin/branches  4
    svr.Get("/admin/branches", withAuth([&](const httplib::Request&, httplib::Response& res, const string&) {
        json branches = json::array();
        for (auto& b : accountService.getBranches()) branches.push_back(branchToJson(b));
        sendOk(res, "OK", {{"branches", branches}});
    }));

    ///admin/branches/id/dashboard  5
    svr.Get(R"(/admin/branches/(\d+)/dashboard)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        int branchId;
        if (!parseId(req.matches[1], branchId, res)) return;
        string output = captureCout([&]() { requestService.branchDashboard(branchId); });
        sendOk(res, "OK", {{"report", output}});
    }));

//حساب ها و عملیات مالی

    //admin/accounts  1
    svr.Get("/admin/accounts", withAuth([&](const httplib::Request&, httplib::Response& res, const string&) {
        json arr = json::array();
        for (auto& a : accountService.getAccounts()) arr.push_back(accountToJson(a));
        sendOk(res, "OK", {{"accounts", arr}});
    }));

    //admin/accounts  2
    svr.Post("/admin/accounts", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        json body;
        if (!parseJsonBody(req, res, body)) return;
        int branchId;
        string password;
        if (!requireField(body, "branch_id", branchId, res)) return;
        if (!requireField(body, "password", password, res)) return;

        InputQueue iq;
        iq.pushLine(password);
        auto result = accountService.createAccount(branchId, iq);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Account created.", {{"account", accountToJson(result.getValue())}});
    }));

    //admin/accounts/id/status   3
    svr.Patch(R"(/admin/accounts/([\w-]+)/status)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        string accountNumber = req.matches[1];
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string status, password;
        if (!requireField(body, "status", status, res)) return;
        if (!requireField(body, "password", password, res)) return;

        if (status != "closed" && status != "inactive") {
            sendError(res, 400, "Unsupported status: " + status + " (only 'closed' is supported).");
            return;
        }

        InputQueue iq;
        iq.pushLine(password);
        auto result = accountService.closeAccount(accountNumber, iq);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Account status updated.", {{"status", "closed"}});
    }));

    //admin/accounts/id  4
    svr.Delete(R"(/admin/accounts/([\w-]+))", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        string accountNumber = req.matches[1];
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string password;
        if (!requireField(body, "password", password, res)) return;

        InputQueue iq;
        iq.pushLine(password);
        auto result = accountService.deleteAccount(accountNumber, iq);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Account deleted.");
    }));

    //admin/accounts/id/deposit  5
    svr.Post(R"(/admin/accounts/([\w-]+)/deposits)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        string accountNumber = req.matches[1];
        json body;
        if (!parseJsonBody(req, res, body)) return;
        double amount;
        if (!requireField(body, "amount", amount, res)) return;

        auto result = transactionService.deposit(accountNumber, amount);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Deposit successful.", {{"transaction", txToJson(result.getValue())}});
    }));

    //admin/accounts/id/withdraw  6
    svr.Post(R"(/admin/accounts/([\w-]+)/withdrawals)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        string accountNumber = req.matches[1];

        json body;
        if (!parseJsonBody(req, res, body)) return;

        double amount;
        string password;
        if (!requireField(body, "amount", amount, res)) return;
        if (!requireField(body, "password", password, res)) return;

        if (amount <= 0) {
            sendError(res, 422, "Amount must be positive.");
            return;
        }

        InputQueue iq;
        iq.pushLine(password);
        auto result = transactionService.withdraw(accountNumber, amount, iq, "Incorrect password.");

        if (!result.isOk()) {
            sendServiceError(res, result.getError());
            return;
        }

        auto tx = result.getValue();
        auto account = accountService.getAccount(accountNumber);
        json data = {
            {"transaction_id", tx->getId()},
            {"amount", tx->getAmount()},
            {"new_balance", account ? account->getBalance() : 0.0}
        };
        sendOk(res, "Withdrawal successful.", data);
    }));

    //admin/accounts/tansfer  7
    svr.Post("/admin/transfers", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string from, to, password;
        double amount;
        if (!requireField(body, "from", from, res)) return;
        if (!requireField(body, "to", to, res)) return;
        if (!requireField(body, "amount", amount, res)) return;
        if (!requireField(body, "password", password, res)) return;

        InputQueue iq;
        iq.pushLine(password);
        auto result = transactionService.transfer(from, to, amount, iq, "Incorrect password.");
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Transfer successful.", {{"transaction", txToJson(result.getValue())}});
    }));

    //admin/accounts/balance  8
    svr.Get(R"(/admin/accounts/([\w-]+)/balance)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        string accountNumber = req.matches[1];
        auto result = transactionService.getBalanceForUser(accountNumber);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "OK", {{"account", accountToJson(result.getValue())}});
    }));

    //admin/accounts/transaction  9
    svr.Get(R"(/admin/accounts/([\w-]+)/transactions)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        string accountNumber = req.matches[1];
        auto account = accountService.getAccount(accountNumber);
        if (!account) { sendError(res, 404, "Account not found."); return; }

        json arr = json::array();
        double runningBalance = 0.0;
        for (const auto& tx : account->getTransactions()) {
            string sign;
            if (tx->getType() == "DEPOSIT") { runningBalance += tx->getAmount(); sign = "+"; }
            else if (tx->getType() == "WITHDRAWAL") { runningBalance -= tx->getAmount(); sign = "-"; }
            else if (tx->getType() == "FEE") { runningBalance -= tx->getAmount(); sign = "-"; }
            else if (tx->getType() == "TRANSFER") {
                if (tx->getFromAccount() == accountNumber) { runningBalance -= tx->getAmount(); sign = "-"; }
                else { runningBalance += tx->getAmount(); sign = "+"; }
            } else if (tx->getType() == "PAYA") { runningBalance -= tx->getAmount(); sign = "-"; }

            json entry = txToJson(tx);
            entry["sign"] = sign;
            entry["running_balance"] = runningBalance;
            arr.push_back(entry);
        }
        sendOk(res, "OK", {{"transactions", arr}});
    }));

//درخواست ها دسترسی ها و کارمزد

    // admin/branches/id/account-requests 1
    svr.Get(R"(/admin/branches/(\d+)/account-requests)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        int branchId;
        if (!parseId(req.matches[1], branchId, res)) return;
        if (!branchExists(branchId)) { sendError(res, 404, "Branch not found."); return; }
        string output = captureCout([&]() { requestService.listRequests(branchId); });
        sendOk(res, "OK", {{"report", output}});
    }));

    // aadmin/branches/id/account-requests 2
    svr.Post("/admin/account-requests", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string nationalCode;
        int branchId;
        if (!requireField(body, "national_code", nationalCode, res)) return;
        if (!requireField(body, "branch_id", branchId, res)) return;

        auto result = requestService.createRequest(nationalCode, branchId);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Request created.", {{"request", requestToJson(result.getValue())}});
    }));

    //admin/account-requests/id/approve 3
    svr.Post(R"(/admin/account-requests/(\d+)/approve)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        int requestId;
        if (!parseId(req.matches[1], requestId, res)) return;
        auto result = requestService.approveRequest(requestId);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Request approved.");
    }));

    //admin/account-requests/id/reject  4
    svr.Post(R"(/admin/account-requests/(\d+)/reject)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        int requestId;
        if (!parseId(req.matches[1], requestId, res)) return;
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string reason;
        if (body.contains("reason")) {
            reason = body["reason"].get<string>();
        }

        auto result = requestService.rejectRequest(requestId, reason);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Request rejected.");
    }));

    // activate account 5
    svr.Post(R"(/admin/account-requests/(\d+)/activate)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        int requestId;
        if (!parseId(req.matches[1], requestId, res)) return;
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string nationalCode, accountNumber;
        if (!requireField(body, "national_code", nationalCode, res)) return;
        if (!requireField(body, "account_number", accountNumber, res)) return;

        auto result = requestService.prepareActivation(requestId, nationalCode);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }

        requestService.markActivated(requestId, accountNumber);
        sendOk(res, "Request activated.", {{"request", requestToJson(result.getValue())}});
    }));

    //admin/transfers/paya  6
    svr.Get("/admin/paya/transfers", withAuth([&](const httplib::Request&, httplib::Response& res, const string&) {
        string output = captureCout([&]() { payaService.listRequests(); });
        sendOk(res, "OK", {{"report", output}});
    }));

    //admin/transfers/paya/id/approve  7
    svr.Post(R"(/admin/transfers/paya/(\d+)/approve)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        int requestId;
        if (!parseId(req.matches[1], requestId, res)) return;
        auto result = payaService.approve(requestId);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Paya request approved.", {{"transaction", txToJson(result.getValue())}});
    }));

    //admin/transfers/paya/id/reject  8
    svr.Post(R"(/admin/transfers/paya/(\d+)/reject)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        int requestId;
        if (!parseId(req.matches[1], requestId, res)) return;
        auto result = payaService.reject(requestId);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Paya request rejected.");
    }));

    //admin/fee  9
    svr.Put("/admin/fees", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string type;
        double amount;
        if (!requireField(body, "type", type, res)) return;
        if (!requireField(body, "amount", amount, res)) return;

        if (type == "transfer") {
            auto result = feeManager.setTransferFee(amount);
            if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
            sendOk(res, "Transfer fee updated.", {{"transfer_fee", amount}});
        } else if (type == "balance_inquiry") {
            auto result = feeManager.setBalanceInquiryFee(amount);
            if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
            sendOk(res, "Balance inquiry fee updated.", {{"balance_inquiry_fee", amount}});
        } else {
            sendError(res, 400, "Unknown fee type: " + type + " (use 'transfer' or 'balance_inquiry').");
        }
    }));

    //admin/fees  10
    svr.Get("/admin/fees", withAuth([&](const httplib::Request&, httplib::Response& res, const string&) {
        sendOk(res, "OK", {
            {"transfer_fee", feeManager.getTransferFee()},
            {"balance_inquiry_fee", feeManager.getBalanceInquiryFee()}
        });
    }));

    //admin/rankings   11
    svr.Get("/admin/rankings", withAuth([&](const httplib::Request&, httplib::Response& res, const string&) {
        json arr = json::array();
        for (auto& r : rankingService.getAllRankings()) arr.push_back(rankToJson(r));
        sendOk(res, "OK", {{"rankings", arr}});
    }));

    //admin/history  12
    svr.Delete(R"(/admin/accounts/([\w-]+)/history)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        string accountNumber = req.matches[1];
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string password;
        if (!requireField(body, "password", password, res)) return;

        InputQueue iq;
        iq.pushLine(password);
        auto result = transactionService.clearHistory(accountNumber, iq);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "History cleared.");
    }));

    //admin/system/reset  13
    svr.Post("/admin/system/reset", withAuth([&](const httplib::Request&, httplib::Response& res, const string&) {
        payaService.resetAll();
        sendOk(res, "System fully reset.");
    }));

//جهت باگ یابی

    //otp
    svr.Post(R"(/admin/accounts/([\w-]+)/otp)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        string accountNumber = req.matches[1];
        int secondsRemaining = 0;
        string code = otpService.requestOtp(accountNumber, secondsRemaining);
        sendOk(res, "OTP generated.", {{"code", code}, {"expires_in_seconds", secondsRemaining}});
    }));

    //transaction -> id
    svr.Get(R"(/admin/transactions/(\d+))", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        int id;
        if (!parseId(req.matches[1], id, res)) return;
        string output = captureCout([&]() { transactionService.getTransaction(id); });
        sendOk(res, "OK", {{"report", output}});
    }));

    //account reset
    svr.Post("/admin/accounts/reset", withAuth([&](const httplib::Request&, httplib::Response& res, const string&) {
        accountService.reset();
        sendOk(res, "Accounts reset.");
    }));

    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.status == 404 && res.body.empty()) {
            sendError(res, 404, "Invalid route");
        }
    });

    //account requests -> by national code
    svr.Get("/admin/account-requests", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        if (!req.has_param("national_code")) {
            sendError(res, 400, "Missing query param: national_code");
            return;
        }
        string nationalCode = req.get_param_value("national_code");
        auto result = requestService.getRequestsOf(nationalCode);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        json arr = json::array();
        for (auto& r : result.getValue()) arr.push_back(requestToJson(r));
        sendOk(res, "OK", {{"requests", arr}});
    }));

    cout << "Admin server listening on http://127.0.0.1:47001" << endl;
    svr.listen("127.0.0.1", 47001);

    return 0;
}