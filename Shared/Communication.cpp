#include "Communication.h"

// Reference: https://github.com/System-Glitch/SHA256
class SHA256
{
private:
    UINT8  m_data[64];
    UINT32 m_blocklen;
    UINT64 m_bitlen;
    UINT32 m_state[8]; //A, B, C, D, E, F, G, H

    static constexpr UINT32 K[] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
        0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
        0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
        0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
        0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
        0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
        0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
        0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
        0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };

    static UINT32 rotr(UINT32 x, UINT32 n) { return (x >> n) | (x << (32 - n)); }
    static UINT32 choose(UINT32 e, UINT32 f, UINT32 g) { return (e & f) ^ (~e & g); }
    static UINT32 majority(UINT32 a, UINT32 b, UINT32 c) { return (a & (b | c)) | (b & c); }
    static UINT32 sig0(UINT32 x) { return SHA256::rotr(x, 7) ^ SHA256::rotr(x, 18) ^ (x >> 3); }
    static UINT32 sig1(UINT32 x) { return SHA256::rotr(x, 17) ^ SHA256::rotr(x, 19) ^ (x >> 10); }
    void transform()
    {
        UINT32 maj, xorA, ch, xorE, sum, newA, newE, m[64];
        UINT32 state[8];

        for (UINT8 i = 0, j = 0; i < 16; i++, j += 4) { // Split data in 32 bit blocks for the 16 first words
            m[i] = (m_data[j] << 24) | (m_data[j + 1] << 16) | (m_data[j + 2] << 8) | (m_data[j + 3]);
        }

        for (UINT8 k = 16; k < 64; k++) { // Remaining 48 blocks
            m[k] = SHA256::sig1(m[k - 2]) + m[k - 7] + SHA256::sig0(m[k - 15]) + m[k - 16];
        }

        for (UINT8 i = 0; i < 8; i++) {
            state[i] = m_state[i];
        }

        for (UINT8 i = 0; i < 64; i++) {
            maj = SHA256::majority(state[0], state[1], state[2]);
            xorA = SHA256::rotr(state[0], 2) ^ SHA256::rotr(state[0], 13) ^ SHA256::rotr(state[0], 22);

            ch = choose(state[4], state[5], state[6]);

            xorE = SHA256::rotr(state[4], 6) ^ SHA256::rotr(state[4], 11) ^ SHA256::rotr(state[4], 25);

            sum = m[i] + K[i] + state[7] + ch + xorE;
            newA = xorA + maj + sum;
            newE = state[3] + sum;

            state[7] = state[6];
            state[6] = state[5];
            state[5] = state[4];
            state[4] = newE;
            state[3] = state[2];
            state[2] = state[1];
            state[1] = state[0];
            state[0] = newA;
        }

        for (UINT8 i = 0; i < 8; i++) {
            m_state[i] += state[i];
        }
    }
    void pad()
    {
        UINT64 i = m_blocklen;
        UINT8 end = m_blocklen < 56 ? 56 : 64;

        m_data[i++] = 0x80; // Append a bit 1
        while (i < end) {
            m_data[i++] = 0x00; // Pad with zeros
        }

        if (m_blocklen >= 56) {
            transform();
            memset(m_data, 0, 56);
        }

        // Append to the padding the total message's length in bits and transform.
        m_bitlen += m_blocklen * 8;
        m_data[63] = UINT8(m_bitlen);
        m_data[62] = UINT8(m_bitlen >> 8);
        m_data[61] = UINT8(m_bitlen >> 16);
        m_data[60] = UINT8(m_bitlen >> 24);
        m_data[59] = UINT8(m_bitlen >> 32);
        m_data[58] = UINT8(m_bitlen >> 40);
        m_data[57] = UINT8(m_bitlen >> 48);
        m_data[56] = UINT8(m_bitlen >> 56);
        transform();
    }
    void revert(UINT8 hash[32])
    {
        // SHA uses big endian byte ordering
        // Revert all bytes
        for (UINT8 i = 0; i < 4; i++) {
            for (UINT8 j = 0; j < 8; j++) {
                hash[i + (j * 4)] = (m_state[j] >> (24 - i * 8)) & 0x000000ff;
            }
        }
    }

 public:
    struct Digest { UINT8 data[32]; };

    SHA256() : m_blocklen(0), m_bitlen(0)
    {
        memset(m_data, 0, sizeof(m_data));
        m_state[0] = 0x6a09e667;
        m_state[1] = 0xbb67ae85;
        m_state[2] = 0x3c6ef372;
        m_state[3] = 0xa54ff53a;
        m_state[4] = 0x510e527f;
        m_state[5] = 0x9b05688c;
        m_state[6] = 0x1f83d9ab;
        m_state[7] = 0x5be0cd19;
    }
    void update(const UINT8* data, size_t length)
    {
        for (size_t i = 0; i < length; i++)
        {
            m_data[m_blocklen++] = data[i];
            if (m_blocklen == 64)
            {
                transform();
                // End of the block
                m_bitlen += 512;
                m_blocklen = 0;
            }
        }
    }
    Digest digest()
    {
        Digest ret;
        pad();
        revert(ret.data);
        return ret;
    }
};

