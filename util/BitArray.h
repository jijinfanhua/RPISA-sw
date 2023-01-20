//
// Created by xilinx_0 on 3/1/22.
//

#ifndef MAIN_BITARRAY_H
#define MAIN_BITARRAY_H
#include <vector>
#include <memory>
#include <iostream>
using namespace std;
using byte_ptr = unsigned char*;
using byte = unsigned char;
using bytes = vector<byte>;

/**
 * BitArray
 * @Author: Walker
 * @Date  : test done! 3/3/2022
 * simulate bits, which can calculate such as and, or, xor, etc. bit calculations.
 */
struct BitArray;
using pBitArray = std::shared_ptr<BitArray>;
struct BitArray {
private:
    bytes value;
    int len;
public:
    BitArray() {
        value = bytes();
        len = 0;
    }

    /**
     * Directly input the value.
     * @param value 
     * @param len 
     */
    BitArray(const bytes& value, int len):len(len), value(value) {}

    /**
     * Copy Constructor.
     * @param v 
     */
    BitArray(const BitArray& v) {
        value = v.value;
        len = v.len;
    }
    
    BitArray(uint32_t t, int len):len(len) {
        t = t & (((uint32_t)1 << len) - 1);
        value = bytes(ByteLength(), 0);
        memcpy(&value[0], &t, ByteLength());
    }

    void Write(byte_ptr bits, int len, int offset) {
        BitArray tmp = *this;
        auto& v = tmp.value;
        tmp.value.push_back(0);
        tmp.len += offset;
        tmp = tmp << offset;
        tmp.value[0] += bits[0] % (1 << offset);
        tmp.value[tmp.ByteLength()-1] |= bits[tmp.ByteLength()-1] & ~tmp.TailMask();
        memcpy(bits, &tmp.value[0], tmp.ByteLength());
    }

    int ByteLength() const {
        return len / 8 + (len % 8 != 0);
    }
    int TailOffset() const {
        return (TailLength()) ? (8-TailLength()) : 0;
    }
    int TailLength() const {
        return (len % 8);
    }
    byte TailMask() const {
        return (1 << TailLength()) - 1;
    }
    uint32_t Integer() const {
        uint32_t integer = *(byte*)&value[0];
        integer &= (1 << len) - 1;
        return integer;
    }

    static BitArray Combine(BitArray a, BitArray b) {
        int sum = a.len + b.len;
        b.ResetLength(sum);
        b = b << a.len;
        a.ResetLength(sum);
        return a | b;
    }



    const bytes& Value() const {
        return value;
    }
    
    static BitArray Calculate(int op, BitArray bits1, BitArray bits2) {
        
        switch (op) {
            case 1: return bits1 << bits2.Integer();
            case 2: return bits2 >> bits2.Integer();
            case 3: return bits1 & bits2;
            case 4: return bits1 | bits2;
            case 5: return bits1 ^ bits2;
            case 6: return ~bits1;
            case 7: return BitArray(bits1.Integer() + bits2.Integer(), std::max(bits1.len, 32));
            case 8: return BitArray(bits1.Integer() - bits2.Integer(), std::max(bits1.len, 32));
            default: return BitArray();
        }
        
    }

    int Length() const {
        return len;
    }

    BitArray ToLength(int len) {
        BitArray copy = *this;
        copy.ResetLength(len);
        return copy;
    }

    void ResetLength(int new_len) {
        if (len < new_len) {
            len = new_len;
            while (value.size() < ByteLength()) {
                value.push_back(0);
            } return;
        } else if (len > new_len) {
            len = new_len;
            while (value.size() > ByteLength()) {
                value.pop_back();
            } value[value.size()-1] &= TailMask();
            return;
        } else {
            value[value.size()-1] &= TailMask();
        }
    }
    
    BitArray operator >> (int shift) const {

        if (shift >= len) {
            return BitArray(bytes(value.size(), 0), len);
        }

        int offset = shift % 8;
        BitArray res = *this;
        auto& v = res.value;
        v[0] >>= offset;
        for (int i = 0; i < v.size() - 1; i++) {
            v[i] |= (v[i + 1] % (1 << offset)) << (8 - offset);
            v[i + 1] >>= offset;
        }
        int k = shift / 8;
        for (int i = k; i < v.size(); i++) {
            v[i-k] = v[i];
        } 
        for (int i = 0; i < k; i++) {
            v[v.size()-1-i] = 0;
        } res.ResetLength(this->len);
        return res;
    }
    
    BitArray operator << (int shift) const {

        if (shift >= len) {
            return BitArray(bytes(value.size(), 0), len);
        }

        int offset = shift % 8;
        BitArray res = *this;
        auto& v = res.value;
        v[v.size()-1] <<= offset;
        for (int i = v.size()-1; i > 0; i--) {
            v[i] |= v[i-1] >> (8-offset);
            v[i - 1] <<= offset;
        }
        int k = shift / 8;
        for (int i = v.size()-1; i >= k; i--) {
            v[i] = v[i-k];
        }
        for (int i = 0; i < k; i++) {
            v[i] = 0;
        } res.ResetLength(this->len);
        return res;
    }

    BitArray operator & (BitArray bits) const {
        for (int i = 0; i < len; i++) {
            bits.value[i] &= value[i];
        } return bits;
    }
    BitArray operator | (BitArray bits) const {
        for (int i = 0; i < len; i++) {
            bits.value[i] |= value[i];
        } return bits;
    }
    BitArray operator ^ (BitArray bits) const {
        for (int i = 0; i < len; i++) {
            bits.value[i] ^= value[i];
        } return bits;
    }
    BitArray operator ~ () const {
        BitArray res = *this;
        for (int i = 0; i < len; i++) {
            res.value[i] = ~res.value[i];
        } res.ResetLength(res.Length());
        return res;
    }

    void Print(int k=2) const {
        if (k == 16) {
            for (int i = 0; i < ByteLength(); i++) {
                printf("%x ", value[i]);
            } printf(" %d\n", len);
        } else {
            for (int i = 0; i < ByteLength()*8; i++) {
                if (i % 8 == 0) printf(" ");
                printf("%d", GetBit(i));
            } printf(" %d\n", len);
        }
    }

    byte GetBit(int i) const {
        int byte_id = i / 8;
        int bit_offset = i % 8;
        return (value[byte_id] & (1 << bit_offset)) > 0;
    }

    const byte& operator[](int i) const {
        return value[i];
    }

    static const byte rvs[];

};


#endif //MAIN_BITARRAY_H
