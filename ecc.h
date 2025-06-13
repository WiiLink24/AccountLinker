// Based off of Dolphin Emulator who based off of twintig http://git.infradead.org/?p=users/segher/wii.git
// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.tx

#pragma once

#include <algorithm>
#include <gctypes.h>
#include <array>
#include <string>

struct SignatureECC
{
    u32 type;
    std::array<u8, 60> sig;
    u8 fill[0x40];
    char issuer[0x40];
};

struct CertHeader
{
    u32 public_key_type;
    char name[0x40];
    u32 id;
};


struct CertECC
{
    SignatureECC signature;
    CertHeader header;
    std::array<u8, 60> public_key;
    std::array<u8, 60> padding;
};

namespace ec {
    static const u8 ec_N[30] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0xe9, 0x74, 0xe7, 0x2f,
                            0x8a, 0x69, 0x22, 0x03, 0x1d, 0x26, 0x03, 0xcf, 0xe0, 0xd7};

    struct Elt;
    static Elt operator*(const Elt& a, const Elt& b);

    static const u8 square[16] = {0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15,
                              0x40, 0x41, 0x44, 0x45, 0x50, 0x51, 0x54, 0x55};

    struct Elt
    {
        bool IsZero() const
        {
            return std::ranges::all_of(data, [](u8 b) { return b == 0; });
        }

        void MulX()
        {
            u8 carry = data[0] & 1;
            u8 x = 0;
            for (std::size_t i = 0; i < data.size() - 1; i++)
            {
                u8 y = data[i + 1];
                data[i] = x ^ (y >> 7);
                x = y << 1;
            }
            data[29] = x ^ carry;
            data[20] ^= carry << 2;
        }

        Elt Square() const
        {
            std::array<u8, 60> wide;
            for (std::size_t i = 0; i < data.size(); i++)
            {
                wide[2 * i] = square[data[i] >> 4];
                wide[2 * i + 1] = square[data[i] & 15];
            }
            for (std::size_t i = 0; i < data.size(); i++)
            {
                u8 x = wide[i];

                wide[i + 19] ^= x >> 7;
                wide[i + 20] ^= x << 1;

                wide[i + 29] ^= x >> 1;
                wide[i + 30] ^= x << 7;
            }

            u8 x = wide[30] & ~1;
            wide[49] ^= x >> 7;
            wide[50] ^= x << 1;
            wide[59] ^= x >> 1;
            wide[30] &= 1;

            Elt result;
            std::copy(wide.cbegin() + 30, wide.cend(), result.data.begin());
            return result;
        }

        Elt ItohTsujii(const Elt& b, std::size_t j) const
        {
            Elt t = *this;
            while (j--)
                t = t.Square();
            return t * b;
        }

        Elt Inv() const
        {
            Elt t = ItohTsujii(*this, 1);
            Elt s = t.ItohTsujii(*this, 1);
            t = s.ItohTsujii(s, 3);
            s = t.ItohTsujii(*this, 1);
            t = s.ItohTsujii(s, 7);
            s = t.ItohTsujii(t, 14);
            t = s.ItohTsujii(*this, 1);
            s = t.ItohTsujii(t, 29);
            t = s.ItohTsujii(s, 58);
            s = t.ItohTsujii(t, 116);
            return s.Square();
        }

        std::array<u8, 30> data{};
    };

    static Elt operator+(const Elt& a, const Elt& b)
    {
        Elt d;
        for (std::size_t i = 0; i < std::tuple_size<decltype(Elt::data)>{}; i++)
            d.data[i] = a.data[i] ^ b.data[i];
        return d;
    }

    static Elt operator*(const Elt& a, const Elt& b)
    {
        Elt d;
        std::size_t i = 0;
        u8 mask = 1;
        for (std::size_t n = 0; n < 233; n++)
        {
            d.MulX();

            if ((a.data[i] & mask) != 0)
                d = d + b;

            mask >>= 1;
            if (mask == 0)
            {
                mask = 0x80;
                i++;
            }
        }
        return d;
    }

    static Elt operator/(const Elt& dividend, const Elt& divisor)
    {
        return dividend * divisor.Inv();
    }


    struct Point
    {
        Point() = default;
        constexpr explicit Point(Elt x, Elt y) : m_data{{std::move(x), std::move(y)}} {}
        explicit Point(const u8* data) { std::copy_n(data, sizeof(m_data), Data()); }

        bool IsZero() const { return X().IsZero() && Y().IsZero(); }
        Elt& X() { return m_data[0]; }
        Elt& Y() { return m_data[1]; }
        u8* Data() { return m_data[0].data.data(); }
        const Elt& X() const { return m_data[0]; }
        const Elt& Y() const { return m_data[1]; }
        const u8* Data() const { return m_data[0].data.data(); }

        Point Double() const
        {
            Point r;
            if (X().IsZero())
                return r;

            const auto s = Y() / X() + X();
            r.X() = s.Square() + s;
            r.X().data[29] ^= 1;
            r.Y() = s * r.X() + r.X() + X().Square();
            return r;
        }

    private:
        std::array<Elt, 2> m_data{};
        static_assert(sizeof(decltype(m_data)) == 60, "Wrong size for m_data");
    };

    static Point operator+(const Point& a, const Point& b)
    {
        if (a.IsZero())
            return b;
        if (b.IsZero())
            return a;

        Elt u = a.X() + b.X();
        if (u.IsZero())
        {
            u = a.Y() + b.Y();
            if (u.IsZero())
                return a.Double();
            return Point{};
        }

        const Elt s = (a.Y() + b.Y()) / u;
        Elt t = s.Square() + s + b.X();
        t.data[29] ^= 1;

        const Elt rx = t + a.X();
        const Elt ry = s * t + a.Y() + rx;
        return Point{rx, ry};
    }

    static Point operator*(const u8* a, const Point& b)
    {
        Point d;
        for (std::size_t i = 0; i < 30; i++)
        {
            for (u8 mask = 0x80; mask != 0; mask >>= 1)
            {
                d = d.Double();
                if ((a[i] & mask) != 0)
                    d = d + b;
            }
        }
        return d;
    }

    constexpr Point ec_G{
        {{{0x00, 0xfa, 0xc9, 0xdf, 0xcb, 0xac, 0x83, 0x13, 0xbb, 0x21, 0x39, 0xf1, 0xbb, 0x75, 0x5f,
           0xef, 0x65, 0xbc, 0x39, 0x1f, 0x8b, 0x36, 0xf8, 0xf8, 0xeb, 0x73, 0x71, 0xfd, 0x55, 0x8b}}},
        {{{0x01, 0x00, 0x6a, 0x08, 0xa4, 0x19, 0x03, 0x35, 0x06, 0x78, 0xe5, 0x85, 0x28, 0xbe, 0xbf,
           0x8a, 0x0b, 0xef, 0xf8, 0x67, 0xa7, 0xca, 0x36, 0x71, 0x6f, 0x7e, 0x01, 0xf8, 0x10, 0x52}}}};

    std::array<u8, 60> PrivToPub(const u8* key);
    std::array<u8, 60> Sign(const u8* key, const u8* hash);
}