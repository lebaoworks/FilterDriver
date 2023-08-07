#pragma once

#include <basetsd.h>

namespace Hash
{
    namespace SHA256
    {
        struct Digest
        {
            UINT8 data[32];
        };

        class Hasher
        {
        private:
            UINT8  m_data[64];
            UINT32 m_blocklen;
            UINT64 m_bitlen;
            UINT32 m_state[8];
            void transform();
            void pad();
            void revert(UINT8 hash[32]);
        public:
            Hasher();
            void feed(const UINT8* data, size_t length);
            Digest digest();
        };
    }
}
