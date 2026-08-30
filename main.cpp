#include <iostream>
#include "include/command_handlers.hpp"
using namespace std;

/*
 * Banking System — Phase 3
 */

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    FileManager fileManager;
    AuthService authService;
    FeeManager feeManager(fileManager);
    AccountService accountService(fileManager, authService);
    RequestService requestService(fileManager, accountService);
    TransactionService transactionService(fileManager, accountService, requestService, authService, feeManager);
    OtpService otpService;
    PayaService payaService(fileManager, accountService, transactionService, requestService);
    InputQueue inputQueue;

    OwnershipRepository ownershipManager(fileManager);
    UserRepository userFileManager(fileManager);
    AuthSession session;
    vector<shared_ptr<User>> loadedUsers = userFileManager.loadUsers();
    RankingService rankingService(userFileManager, fileManager, loadedUsers);
    UserService userService(userFileManager, authService, session, ownershipManager, rankingService, loadedUsers);
    UserAccountService userAccountService(accountService, ownershipManager, session, authService, rankingService);
    UserTransactionService userTransactionService(accountService, ownershipManager, session, transactionService, fileManager, rankingService, authService);
    UserRequestService userRequestService(requestService, accountService, ownershipManager, session, authService, rankingService);

    HandleAdminCommand admin;
    HandleUserCommand user;

    string line;
    while (inputQueue.getLineBlocking(line)) {
        if (line.empty()) continue;

        istringstream iss(line);
        string command;
        iss >> command;

        if (command == "exit") break;

        if (admin.isAdminCommand(command)) {
            admin.handleCommand(line, accountService, transactionService, authService, feeManager, payaService, requestService, inputQueue, rankingService);
        }
        else if (user.isUserCommand(command)) {
            user.handleCommand(line, userService, userAccountService, userTransactionService, userRequestService, ownershipManager, transactionService, otpService, payaService, requestService, inputQueue, rankingService, authService);
        }
        else {
            cout << "Error: Invalid command." << endl;
        }
    }

    return 0;
}