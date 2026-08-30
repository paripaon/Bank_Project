#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <random>
using namespace std;

class SessionManager {
public:
    SessionManager(int timeoutSeconds = 300);

    string createToken(const string& userId);
    bool validateToken(const string& token, string& userId);
    void removeToken(const string& token);
    void cleanExpired();

private:
    struct SessionInfo {
        string userId;
        chrono::steady_clock::time_point expiry;
    };

    unordered_map<string, SessionInfo> sessions;
    int timeoutSeconds;
    mutex mtx;

    string generateRandomToken();
};