/**
    \file alu.h
    \author Natesh Narain <nnaraindev@gmail.com>
*/

#ifndef GAMEBOYCORE_ALU_H
#define GAMEBOYCORE_ALU_H

#include <cstdint>

namespace gb
{
    /*!
        \class ALU
        \brief Arithmetic and logic unit
    */
    class ALU
    {
    public:
        enum Flags
        {
            Z = 1 << 7,
            N = 1 << 6,
            H = 1 << 5,
            C = 1 << 4
        };

    public:
        ALU(uint8_t& flags);
        ~ALU();

        /**
            ADD
        */
        void add(uint8_t& a, uint8_t n);
        void add(uint16_t& hl, uint16_t n);
        void addr(uint16_t& sp, int8_t n);

        /**
            ADC
        */
        void addc(uint8_t& a, uint8_t n);

        /**
            SUB
        */
        void sub(uint8_t& a, uint8_t n);

        /**
            SUBC
        */
        void subc(uint8_t& a, uint8_t n);

        /**
            AND
        */
        void anda(uint8_t& a, uint8_t n);

        /**
            OR
        */
        void ora(uint8_t& a, uint8_t n);

        /**
            XOR
        */
        void xora(uint8_t& a, uint8_t n);

        /**
            Compare
        */
        void compare(uint8_t& a, uint8_t n);

    private:
        void setFlag(uint8_t mask, bool set);

    private:
        uint8_t& flags_;
    };

}
#endif // GAMEBOYCORE_ALU_H
