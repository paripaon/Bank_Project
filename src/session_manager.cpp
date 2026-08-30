#include "../include/session_manager.hpp"
#include <sstream>
#include <iomanip>
#include <chrono>
using namespace std;

SessionManager::SessionManager(int timeo) : timeoutSeconds(timeo) {}

string SessionManager::generateRandomToken() {
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_int_distribution<> dis(0, 15);
    
    stringstream ss;
    for (int i = 0; i < 32; ++i) {
        ss << hex << dis(gen);
    }
    return ss.str();
}

string SessionManager::createToken(const string& userId) {
    lock_guard<mutex> lock(mtx);  
    
    string token = generateRandomToken();
    auto now = chrono::steady_clock::now();
    sessions[token] = {userId, now + chrono::seconds(timeoutSeconds)};
    return token;
}

bool SessionManager::validateToken(const string& token, string& userId) {
    lock_guard<mutex> lock(mtx);
    
    auto it = sessions.find(token);
    if (it == sessions.end()) return false; 
    
    if (chrono::steady_clock::now() > it->second.expiry) {
        sessions.erase(it);  
        return false;
    }
    
    userId = it->second.userId;
    it->second.expiry = chrono::steady_clock::now() + chrono::seconds(timeoutSeconds);
    return true;
}

void SessionManager::removeToken(const string& token) {
    lock_guard<mutex> lock(mtx);
    sessions.erase(token);
}

void SessionManager::cleanExpired() {
    lock_guard<mutex> lock(mtx);
    
    auto now = chrono::steady_clock::now();
    for (auto it = sessions.begin(); it != sessions.end(); ) {
        if (now > it->second.expiry) {
            it = sessions.erase(it); 
        } else {
            ++it;
        }
    }
}