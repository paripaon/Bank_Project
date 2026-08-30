#include <iostream>
#include "include/user.hpp"
using namespace std;

/*
 * Banking System — Phase 2
 */

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    FileManager fileManager;
    AuthService authService;
    FeeManager feeManager(fileManager); 
    AccountService accountService(fileManager, authService);
    TransactionService transactionService(fileManager, accountService, authService, feeManager);
    InputQueue inputQueue;
    AccountOwnershipManager ownershipManager(fileManager);
    UserFileManager userFileManager(fileManager);
    AuthSession session;
    UserService userService(userFileManager, authService, session, ownershipManager);
    UserAccountService userAccountService(accountService, ownershipManager, session, authService);
    UserTransactionService userTransactionService(accountService, ownershipManager, session, transactionService, fileManager);
    HandleAdminCommand admin;
    HandleUserCommand user;

    string line;
    while (inputQueue.getLineBlocking(line)) {
        if (line.empty()) continue;

        istringstream iss(line);
        string command;
        iss >> command;

        if (command == "exit") {
            break;
        }

        if (admin.isAdminCommand(command)) {
            admin.handleCommand(line, accountService, transactionService, authService, feeManager, inputQueue);
        }
        else if (user.isUserCommand(command)) {
            user.handleCommand(line, userService, userAccountService, userTransactionService, ownershipManager, transactionService, inputQueue);
        }
        else cout << "Error: Invalid command." << endl;
    }

    return 0;
}