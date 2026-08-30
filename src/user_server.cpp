#include "../include/httplib.h"
#include "../include/json.hpp"
#include "../include/user.hpp"
#include "../include/session_manager.hpp"
#include <iostream>
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

//parse kardan json
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

template<typename T>
static bool optionalField(const json& body, const string& key, T& value) {
    if (!body.contains(key)) return false;
    try {
        value = body[key].get<T>();
        return true;
    } catch (...) {
        return false;
    }
}

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
    AuthSession session;
    session.logout();
    vector<shared_ptr<User>> loadedUsers = userFileManager.loadUsers();
    RankingService rankingService(userFileManager, fileManager, loadedUsers);
    UserService userService(userFileManager, authService, session, ownershipManager, rankingService, loadedUsers);
    UserAccountService userAccountService(accountService, ownershipManager, session, authService, rankingService);
    UserTransactionService userTransactionService(accountService, ownershipManager, session, transactionService, fileManager, rankingService, authService);
    UserRequestService userRequestService(requestService, accountService, ownershipManager, session, authService, rankingService);

    Server svr;
    SessionManager sessionManager(300);

    using AuthedHandler = function<void(const httplib::Request&, httplib::Response&, const string& nationalCode)>;
    auto withAuth = [&sessionManager](AuthedHandler fn) -> Server::Handler {
        return [&sessionManager, fn](const httplib::Request& req, httplib::Response& res) {
            string token = extractBearerToken(req);
            string nationalCode;
            if (!sessionManager.validateToken(token, nationalCode)) {
                sendError(res, 401, "Unauthorized");
                return;
            }
            fn(req, res, nationalCode);
        };
    };

    auto accountToJson = [](const shared_ptr<Account>& a) {
        return json{
            {"account_number", a->getAccountNumber()},
            {"branch_id", a->getBranchId()},
            {"active", a->isActive()},
            {"balance", a->getBalance()}
        };
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
    auto rankToJson = [](const RankEntry& r) {
        return json{
            {"national_code", r.nationalCode},
            {"score", r.score},
            {"level", r.level},
            {"rank", r.rank}
        };
    };

//تست
    svr.Post("/users/force-logout", [&](const httplib::Request&, httplib::Response& res) {
        session.logout();
        sendOk(res, "Session cleared.");
    });

//اهراز هویت و مدیریت حساب
    //auth/singup  1
    svr.Post("/users/signup", [&](const httplib::Request& req, httplib::Response& res) {
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string nationalCode, password;
        if (!requireField(body, "national_code", nationalCode, res)) return;
        if (!requireField(body, "password", password, res)) return;

        InputQueue iq;
        iq.pushLine(nationalCode);
        iq.pushLine(password);
        auto result = userService.signup(iq);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Signup successful.");
    });

    //auth/login   2
    svr.Post("/users/login", [&](const httplib::Request& req, httplib::Response& res) {
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string nationalCode, password;
        if (!requireField(body, "national_code", nationalCode, res)) return;
        if (!requireField(body, "password", password, res)) return;

        InputQueue iq;
        iq.pushLine(nationalCode);
        iq.pushLine(password);
        auto result = userService.login(iq);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }

        auto user = result.getValue();
        string token = sessionManager.createToken(user->getNationalCode());
        sendOk(res, "Login successful.", {{"token", token}});
    });

    //auth/session  3
    svr.Post("/users/logout", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        auto result = userService.logout();
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sessionManager.removeToken(extractBearerToken(req));
        sendOk(res, "Logged out.");
    }));

    //account/request  4
    svr.Post("/accounts/requests", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        json body;
        if (!parseJsonBody(req, res, body)) return;
        int branchId;
        if (!requireField(body, "branch_id", branchId, res)) return;

        auto result = userRequestService.requestAccount(branchId);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Request created.", {{"request", requestToJson(result.getValue())}});
    }));

    //account/request  5
    svr.Get("/accounts/requests", withAuth([&](const httplib::Request&, httplib::Response& res, const string&) {
        auto result = userRequestService.myRequests();
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        json arr = json::array();
        for (auto& r : result.getValue()) arr.push_back(requestToJson(r));
        sendOk(res, "OK", {{"requests", arr}});
    }));

    //accout/request/id  6
    svr.Post(R"(/requests/(\d+)/cancel)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        int requestId;
        if (!parseId(req.matches[1], requestId, res)) return;

        auto result = userRequestService.cancelRequest(requestId);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Request cancelled.");
    }));

    //account/id/activate 7
    svr.Post(R"(/requests/(\d+)/activate)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        int requestId;
        if (!parseId(req.matches[1], requestId, res)) return;
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string password;
        if (!requireField(body, "password", password, res)) return;

        InputQueue iq;
        iq.pushLine(password);
        auto result = userRequestService.activateAccount(requestId, iq);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Account activated.", {{"account", accountToJson(result.getValue())}});
    }));

    //accounts  8
    svr.Get("/accounts/mine", withAuth([&](const httplib::Request&, httplib::Response& res, const string&) {
        auto result = userAccountService.myAccounts();
        if (!result.isOk()) {
            const auto& err = result.getError();
            if (err.code == ServiceError::Code::NotFound) {
                sendOk(res, "OK", {{"accounts", json::array()}});
                return;
            }
            sendServiceError(res, err);
            return;
        }
        json arr = json::array();
        for (auto& a : result.getValue()) arr.push_back(accountToJson(a));
        sendOk(res, "OK", {{"accounts", arr}});
    }));

    //accounts/id   9
    svr.Delete(R"(/accounts/([\w-]+))", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        string accountNumber = req.matches[1];
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string password;
        if (!requireField(body, "password", password, res)) return;

        InputQueue iq;
        iq.pushLine(password);
        auto result = userAccountService.deleteMyAccount(accountNumber, iq);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Account deleted.");
    }));

//تراکنش و انتقال مالی

    //accounts/id/deposit   1
    svr.Post("/accounts/deposit", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string accountNumber;
        double amount;
        if (!requireField(body, "account_number", accountNumber, res)) return;
        if (!requireField(body, "amount", amount, res)) return;

        auto result = userTransactionService.depositTo(accountNumber, amount);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Deposit successful.");
    }));

    //accounts/id/withdraw  2
    svr.Post(R"(/accounts/([\w-]+)/withdrawals)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        string accountNumber = req.matches[1];
        json body;
        if (!parseJsonBody(req, res, body)) return;
        double amount;
        string password;
        if (!requireField(body, "amount", amount, res)) return;
        if (!requireField(body, "password", password, res)) return;

        InputQueue iq;
        iq.pushLine(password);
        auto result = userTransactionService.withdrawFrom(accountNumber, amount, iq);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Withdrawal successful.");
    }));


    //transfer card to card  3
    svr.Post("/accounts/transfer", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
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
        auto result = userTransactionService.sendMoney(from, to, amount, iq);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Transfer successful.");
    }));

    //accounts/id/balance_inquiries   4
    svr.Get(R"(/accounts/([\w-]+)/balance)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        string accountNumber = req.matches[1];
        auto result = userTransactionService.balanceInquiry(accountNumber);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }

        auto account = accountService.getAccount(accountNumber);
        sendOk(res, "OK", {{"account", account ? accountToJson(account) : json::object()}});
    }));

    //auth/otp    5 
    svr.Post(R"(/accounts/([\w-]+)/otp)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        string accountNumber = req.matches[1];
        auto result = userTransactionService.requestOtp(accountNumber, otpService);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "OTP generated.", {{"code", result.getValue()}});
    }));

    //payment/online  6
    svr.Post("/transactions/online-payment", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string from, to, otp;
        double amount;
        if (!requireField(body, "from", from, res)) return;
        if (!requireField(body, "to", to, res)) return;
        if (!requireField(body, "amount", amount, res)) return;
        if (!requireField(body, "otp", otp, res)) return;

        InputQueue iq;
        iq.pushLine(otp);
        auto result = userTransactionService.onlinePayment(from, to, amount, iq, otpService);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Online payment successful.");
    }));

    //accounts/id/iban  7
    svr.Get(R"(/accounts/([\w-]+)/iban)", [&](const httplib::Request& req, httplib::Response& res) {
        string accountNumber = req.matches[1];
        auto result = userAccountService.showIban(accountNumber);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "OK", {{"iban", result.getValue()}});
    });

    //transfer/paya  8
    svr.Post("/paya/requests", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string fromAccount, destinationIban, password;
        double amount;
        if (!requireField(body, "from_account", fromAccount, res)) return;
        if (!requireField(body, "destination_iban", destinationIban, res)) return;
        if (!requireField(body, "amount", amount, res)) return;
        if (!requireField(body, "password", password, res)) return;

        InputQueue iq;
        iq.pushLine(password);
        auto result = userTransactionService.payaTransfer(fromAccount, destinationIban, amount, iq, payaService, authService);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "Paya request registered.");
    }));

//سایر امکانات

    //user/me  1
    svr.Delete("/users/delete", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string password;
        if (!requireField(body, "password", password, res)) return;

        InputQueue iq;
        iq.pushLine(password);
        auto result = userService.deleteCurrentUser(iq);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sessionManager.removeToken(extractBearerToken(req));
        sendOk(res, "User deleted.");
    }));

    //user/me/rank  2
    svr.Get("/rankings/me", withAuth([&](const httplib::Request&, httplib::Response& res, const string& nationalCode) {
        auto result = rankingService.getRank(nationalCode);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "OK", {{"rank", rankToJson(result.getValue())}});
    }));

    //accounts/id/statement  3
    svr.Post(R"(/accounts/([\w-]+)/history/export)", withAuth([&](const httplib::Request& req, httplib::Response& res, const string&) {
        string accountNumber = req.matches[1];
        json body;
        if (!parseJsonBody(req, res, body)) return;
        string format;
        if (!requireField(body, "format", format, res)) return;

        auto result = userTransactionService.exportHistory(accountNumber, format);
        if (!result.isOk()) { sendServiceError(res, result.getError()); return; }
        sendOk(res, "History exported.", {{"file", "history_" + accountNumber + "." + format}});
    }));

    //rankings 4
    svr.Get("/rankings", withAuth([&](const httplib::Request&, httplib::Response& res, const string&) {
        json arr = json::array();
        for (auto& r : rankingService.getAllRankings()) arr.push_back(rankToJson(r));
        sendOk(res, "OK", {{"rankings", arr}});
    }));

    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.status == 404 && res.body.empty()) {
            sendError(res, 404, "Invalid route");
        }
    });

    cout << "User server listening on http://127.0.0.1:47002" << endl;
    svr.listen("127.0.0.1", 47002);

    return 0;
}