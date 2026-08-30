#pragma once

#include <string>
#include <array>
#include <cstdint>

using namespace std;

//••••••••••••••••••SHA256••••••••••••••••••
// Generates a SHA-256 hash from a given input string.
// Creates password hashes.
class SHA256 {
public:
    static string hash(const string& input);

private:
    static uint32_t rotr(uint32_t x, uint32_t n);
};
