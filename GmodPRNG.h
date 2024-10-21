#pragma once
#include <iostream>
#include <bitset>
#include <sstream> // for toBinaryString()
#include <chrono>

typedef union { uint64_t u64; double d; } U64double;

class GmodPRNG {
private:
	uint64_t u[4];
    static const std::string deck[52];

    void TW223_GEN(uint64_t& z, uint64_t& r, const int& i, const int& k, const int& q, const int& s);
    void TW223_STEP(uint64_t& z, uint64_t& r);
    uint64_t Extractu64();
    uint64_t Extractu64d();

public:
    GmodPRNG();

    const void PrintStates();
    void RandomSeed(double d);
    double MathRandom();
    double MathRandom(const double& r1);
    double MathRandom(const double& r1, const double& r2);
    std::string PickRandCard();
};