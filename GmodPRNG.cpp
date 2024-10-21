#include "GmodPRNG.h"

// this is just some function that I got from chatgpt to print 64 bits numbers
// in this format: 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 
template <typename T>
std::string toBinaryString(T number) {
    static_assert(std::is_integral<T>::value || std::is_floating_point<T>::value,
        "The type must be an integral or floating-point type.");

    std::bitset<sizeof(T) * 8> bitset;

    if constexpr (std::is_integral<T>::value) {
        bitset = std::bitset<sizeof(T) * 8>(number);
    } else {
        auto int_rep = *reinterpret_cast<std::uint64_t*>(&number);
        bitset = std::bitset<sizeof(T) * 8>(int_rep);
    }

    std::string bitstring = bitset.to_string();

    std::ostringstream oss;
    for (size_t i = 0; i < bitstring.size(); ++i) {
        oss << bitstring[i];
        if ((i + 1) % 8 == 0 && i != bitstring.size() - 1) {
            oss << ' ';
        }
    }

    return oss.str();
}

GmodPRNG::GmodPRNG()
{
    auto now = std::chrono::system_clock::now();
    double epoch_time = std::chrono::duration<double>(now.time_since_epoch()).count();
    RandomSeed(epoch_time);
}

/*  Update generator i and compute a running xor of all states. 
*   basically steps the LFSR
*/
void GmodPRNG::TW223_GEN(uint64_t& z, uint64_t& r, const int& i, const int& k, const int& q, const int& s)
{
    z = u[i];

    z = (((z << q) ^ z) >> (k - s)) ^ ((z & ((uint64_t)(int64_t)-1 << (64 - k))) << s);
    r ^= z;

    u[i] = z;
}

/*  this steps each LFSR
*   original code: https://github.com/LuaJIT/LuaJIT/blob/ae4735f621d89d84758769b76432d2319dda9827/src/lj_prng.c#L40
*   z: this uint64_t is only used to hold the state of the LFSR
*   r: this is a uint64_t that starts at 0, gets xored by each of the states and that's the output
*   i is just the index of the state
*   k, q and s are specific values to get a long period and good statistical properties
*   how they found k, q and s (for other tausworthe generators) https://www.ams.org/journals/mcom/1996-65-213/S0025-5718-96-00696-5/S0025-5718-96-00696-5.pdf
*/
void GmodPRNG::TW223_STEP(uint64_t& z, uint64_t& r)
{
    TW223_GEN(z, r, 0, 63, 31, 18);
    TW223_GEN(z, r, 1, 58, 19, 28);
    TW223_GEN(z, r, 2, 55, 24, 7);
    TW223_GEN(z, r, 3, 47, 21, 8);
}

/*  PRNG step function with uint64_t result.
*   original code: https://github.com/LuaJIT/LuaJIT/blob/ae4735f621d89d84758769b76432d2319dda9827/src/lj_prng.c#L47
*   this function is supposed to return all of the 64 bits, but it's only used when stepping 6 times after seeding
*   (see the last loop in RandomSeed() below)
*/
uint64_t GmodPRNG::Extractu64()
{
    uint64_t z, r = 0;
    TW223_STEP(z, r);
    return r;
}

/*  PRNG step function with double in uint64_t result.
*   original code: https://github.com/LuaJIT/LuaJIT/blob/ae4735f621d89d84758769b76432d2319dda9827/src/lj_prng.c#L55
*   this is what is actually used to get the random numbers, Extractu64() above is never actually used
*/
uint64_t GmodPRNG::Extractu64d()
{
    uint64_t z, r = 0;
    TW223_STEP(z, r);

    /* Returns a double bit pattern in the range 1.0 <= d < 2.0. */
    // this is a bit mask that will set the first 12 bits to 00111111 1111
    return ((r & 0x000FFFFFFFFFFFFFULL) | 0x3FF0000000000000ULL);
}

// this prints the current state of each LFSR
const void GmodPRNG::PrintStates()
{
    std::cout << "STATES:" << std::endl;
    for (int i = 0; i < 4; i++) {
       std::cout << toBinaryString(u[i]) << std::endl << std::endl;
    }
}

/*  this is math.randomseed()
*   original code: https://github.com/LuaJIT/LuaJIT/blob/ae4735f621d89d84758769b76432d2319dda9827/src/lib_math.c#L114
*   d: the seed
*/
void GmodPRNG::RandomSeed(double d)
{
    uint32_t r = 0x11090601;  /* 64-k[i] as four 8 bit constants. */
    // this stores all k positions
    // binary:  00010001 00001001 00000110 00000001
    // decimal:   (17)      (9)      (6)      (1)

    for (int i = 0; i < 4; i++) {
        U64double u64d;
        uint32_t m = 1u << (r & 255);

        r >>= 8;

        u64d.d = d = d * 3.14159265358979323846 + 2.7182818284590452354;   // not sure what pi and e do here

        if (u64d.u64 < m) {
            u64d.u64 += m;  /* Ensure k[i] MSB of u[i] are non-zero. */
        }

        u[i] = u64d.u64;
    }

    // the first few numbers are not very "random" or something like that
    for (int i = 0; i < 10; i++) {
        (void)Extractu64d();
    }
}

/*  this is basically math.random()
*   original code: https://github.com/LuaJIT/LuaJIT/blob/ae4735f621d89d84758769b76432d2319dda9827/src/lib_math.c#L132
*   0, 1 or 2 arguments, just like https://wiki.facepunch.com/gmod/math.random
*/
double GmodPRNG::MathRandom()
{
    U64double u;
    u.u64 = Extractu64d();  // get the random number
    return u.d - 1.0;       // since the double returned is [1,2[, we -1 to get [0,1[
}

// same as above but for one argument
double GmodPRNG::MathRandom(const double& r1)
{
    double d = MathRandom();
    return std::floor(d * r1) + 1.0;
}

// same as above but for two arguments
double GmodPRNG::MathRandom(const double& r1, const double& r2)
{
    double d = MathRandom();
    return std::floor(d * (r2 - r1 + 1.0)) + r1;
}

const std::string GmodPRNG::deck[52] = { "HA", "H2", "H3", "H4", "H5", "H6", "H7", "H8", "H9", "HT", "HJ", "HK", "HQ", "DA", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "DT", "DJ", "DK", "DQ", "SA", "S2", "S3", "S4", "S5", "S6", "S7", "S8", "S9", "ST", "SJ", "SK", "SQ", "CA", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9", "CT", "CJ", "CK", "CQ" };

/*  this is PerfectCasino.Cards:GetRandom() from the pcasino addons
*   it picks the card exactly how pcasino picks them
*   you can easily find a leak of the addon online
*   you will find this function in ...\lua\perfectcasino\core\sh_card.lua
*   pcasino actually uses table.random(), but you'll notice that it's basically math.random(1, 52)
*   https://github.com/Facepunch/garrysmod/blob/886991fb42e919e96a147e929962dcb73bb9f12f/garrysmod/lua/includes/extensions/table.lua#L173
*/
std::string GmodPRNG::PickRandCard()
{
    return deck[static_cast<int>(MathRandom(52)) - 1];
}