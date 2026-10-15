
/*
 * Copyright (c) 1996, 2024, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation. Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

/*
 * Portions Copyright IBM Corporation, 2001. All Rights Reserved.
 */

// Adaptación nativa C++17: Julio Cesar Borges Lopez, 2026.



#include "BigInteger.h"
#include "BigDecimal.h"
#include "NativeExceptions.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>

namespace {
using Bytes = std::vector<std::uint8_t>;
using Words = std::vector<std::uint32_t>;
Words fromUnsignedBytes(const Bytes& bytes) {
    Words words;
    for (const auto byte : bytes) {
        std::uint64_t carry = byte;
        for (auto& word : words) {
            const auto value = static_cast<std::uint64_t>(word) * 256 + carry;
            word = static_cast<std::uint32_t>(value % 1000000000ULL);
            carry = value / 1000000000ULL;
        }
        if (carry) words.push_back(static_cast<std::uint32_t>(carry));
    }
    while (!words.empty() && words.back() == 0) words.pop_back();
    return words;
}
Bytes toUnsignedBytes(const Words& magnitude) {
    auto words = magnitude;
    Bytes bytes;
    while (!words.empty()) {
        std::uint64_t remainder = 0;
        for (std::size_t i = words.size(); i != 0; --i) {
            const auto value = remainder * 1000000000ULL + words[i - 1];
            words[i - 1] = static_cast<std::uint32_t>(value / 256);
            remainder = value % 256;
        }
        bytes.push_back(static_cast<std::uint8_t>(remainder));
        while (!words.empty() && words.back() == 0) words.pop_back();
    }
    std::reverse(bytes.begin(), bytes.end());
    return bytes;
}
std::size_t firstNonzero(const Bytes& bytes) {
    auto it = std::find_if(bytes.begin(), bytes.end(), [](auto byte) { return byte != 0; });
    return static_cast<std::size_t>(it - bytes.begin());
}
std::int32_t wrapInt32(std::uint32_t value) {
    return value <= 0x7fffffffU ? static_cast<std::int32_t>(value)
        : static_cast<std::int32_t>(static_cast<std::int64_t>(value) - 0x100000000LL);
}
std::uint32_t popcount32(std::uint32_t value) {
    value = value - ((value >> 1U) & 0x55555555U);
    value = (value & 0x33333333U) + ((value >> 2U) & 0x33333333U);
    return (((value + (value >> 4U)) & 0x0f0f0f0fU) * 0x01010101U) >> 24U;
}
std::size_t bitLengthBytes(const Bytes& bytes) {
    if (bytes.empty()) return 0;
    auto first = bytes.front();
    unsigned leading = 0;
    for (std::uint8_t mask = 0x80U; mask && !(first & mask); mask >>= 1U) ++leading;
    return bytes.size() * 8U - leading;
}
int unicodeDecimalDigit(char32_t cp) {
    constexpr char32_t starts[]={0x30,0x660,0x6f0,0x7c0,0x966,0x9e6,0xa66,0xae6,
        0xb66,0xbe6,0xc66,0xce6,0xd66,0xe50,0xed0,0xf20,0x1040,0x1090,
        0x17e0,0x1810,0x1946,0x19d0,0x1a80,0x1a90,0x1b50,0x1bb0,0x1c40,0x1c50,
        0xa620,0xa8d0,0xa900,0xa9d0,0xa9f0,0xaa50,0xabf0,0xff10};
    for (const auto first : starts)
        if (cp >= first && cp < first + 10) return static_cast<int>(cp - first);
    return -1;
}
char32_t nextCodePoint(const std::string& text, std::size_t& position) {
    const auto lead = static_cast<unsigned char>(text[position++]);
    char32_t cp = lead;
    unsigned count = 0;
    if (lead >= 0xc2 && lead <= 0xdf) { cp = lead & 31U; count = 1; }
    else if (lead >= 0xe0 && lead <= 0xef) { cp = lead & 15U; count = 2; }
    else if (lead >= 0xf0 && lead <= 0xf4) { cp = lead & 7U; count = 3; }
    for (unsigned i = 0; i < count && position < text.size(); ++i)
        cp = (cp << 6U) | (static_cast<unsigned char>(text[position++]) & 63U);
    return cp;
}
}

BigInteger::BigInteger(std::vector<std::uint32_t> magnitude, int sign)
    : magnitude_(std::move(magnitude)), sign_(sign) {
    while (!magnitude_.empty() && magnitude_.back() == 0) magnitude_.pop_back();
    if (magnitude_.empty()) sign_ = 0;
}
BigInteger::BigInteger(BigInteger&& other) noexcept
    : magnitude_(std::move(other.magnitude_)), sign_(other.sign_) {
    other.magnitude_.clear();
    other.sign_ = 0;
}
BigInteger& BigInteger::operator=(BigInteger&& other) noexcept {
    if (this != &other) {
        magnitude_ = std::move(other.magnitude_);
        sign_ = other.sign_;
        other.magnitude_.clear();
        other.sign_ = 0;
    }
    return *this;
}
BigInteger::BigInteger(std::int64_t value) : BigInteger(std::to_string(value)) {}
const BigInteger BigInteger::ZERO(0);
const BigInteger BigInteger::ONE(1);
const BigInteger BigInteger::TWO(2);
const BigInteger BigInteger::TEN(10);
BigInteger BigInteger::valueOf(std::int64_t value) { return BigInteger(value); }

BigInteger::BigInteger(const Bytes& bytes) : BigInteger(bytes, 0, static_cast<std::int32_t>(bytes.size())) {}
BigInteger::BigInteger(const Bytes& bytes, std::int32_t offset, std::int32_t length) {
    if (bytes.empty()) throw NumberFormatError("Zero length BigInteger");
    if (offset < 0 || length < 0 || static_cast<std::uint64_t>(offset) + static_cast<std::uint64_t>(length) > bytes.size())
        throw std::out_of_range("BigInteger byte range out of bounds");
    Bytes value(bytes.begin() + offset, bytes.begin() + offset + length);
    if (value.empty()) return;
    const bool negative = (value.front() & 0x80U) != 0;
    if (negative) {
        for (auto& byte : value) byte = static_cast<std::uint8_t>(~byte);
        for (std::size_t i = value.size(); i != 0; --i) {
            if (++value[i - 1] != 0) break;
        }
    }
    const auto first = firstNonzero(value);
    if (first != value.size()) {
        value.erase(value.begin(), value.begin() + static_cast<std::ptrdiff_t>(first));
        magnitude_ = fromUnsignedBytes(value);
        sign_ = negative ? -1 : 1;
    }
}
BigInteger::BigInteger(std::int32_t signum, const Bytes& bytes)
    : BigInteger(signum, bytes, 0, static_cast<std::int32_t>(bytes.size())) {}
BigInteger::BigInteger(std::int32_t signum, const Bytes& bytes, std::int32_t offset, std::int32_t length) {
    if (signum < -1 || signum > 1) throw NumberFormatError("Invalid signum value");
    if (offset < 0 || length < 0 || static_cast<std::uint64_t>(offset) + static_cast<std::uint64_t>(length) > bytes.size())
        throw std::out_of_range("BigInteger byte range out of bounds");
    Bytes value(bytes.begin() + offset, bytes.begin() + offset + length);
    const auto first = firstNonzero(value);
    if (first != value.size()) {
        if (signum == 0) throw NumberFormatError("signum-magnitude mismatch");
        value.erase(value.begin(), value.begin() + static_cast<std::ptrdiff_t>(first));
        magnitude_ = fromUnsignedBytes(value);
        sign_ = signum;
    }
}
BigInteger::BigInteger(std::int32_t numBits, std::mt19937_64& random) {
    if (numBits < 0) throw NumberFormatError("bitLength < 0");
    const auto count = (static_cast<std::size_t>(numBits) + 7U) / 8U;
    Bytes bytes(count);
    for (std::size_t i = 0; i < count; ++i) bytes[i] = static_cast<std::uint8_t>(random());
    if (count && (numBits & 7)) bytes.front() &= static_cast<std::uint8_t>((1U << (numBits & 7)) - 1U);
    *this = BigInteger(1, bytes);
}
BigInteger::BigInteger(std::int32_t bitLength, std::int32_t certainty, std::mt19937_64& random) {
    if (bitLength < 2) throw InvalidArgumentError("bitLength < 2");
    do {
        BigInteger candidate(bitLength, random);
        const auto byteCount = (static_cast<std::size_t>(bitLength) + 7U) / 8U;
        auto bytes = toUnsignedBytes(candidate.magnitude_);
        bytes.insert(bytes.begin(), byteCount - bytes.size(), 0);
        const auto bit = static_cast<unsigned>((bitLength - 1) & 7);
        bytes.front() |= static_cast<std::uint8_t>(1U << bit);
        bytes.back() |= 1U;
        candidate = BigInteger(1, bytes);
        if (candidate.isProbablePrime(certainty)) { *this = std::move(candidate); return; }
    } while (true);
}
BigInteger BigInteger::probablePrime(std::int32_t bitLength, std::mt19937_64& random) {
    return BigInteger(bitLength, 100, random);
}

BigInteger::BigInteger(const std::string& value, int radix) {
    if (radix < 2 || radix > 36) throw NumberFormatError("Radix out of range");
    if (radix == 10) {
        *this = BigInteger(value);
        return;
    }
    if (value.empty()) throw NumberFormatError("Zero length BigInteger");
    const std::size_t start=value[0]=='-'||value[0]=='+' ? 1 : 0;
    if (start==value.size()) throw NumberFormatError("Zero length BigInteger");
    sign_=value[0]=='-' ? -1 : 1;
    for (std::size_t i=start;i<value.size();) {
        const auto cp=nextCodePoint(value,i);
        int digit=-1;
        if (cp>='0'&&cp<='9') digit=static_cast<int>(cp-'0');
        else if (cp>='a'&&cp<='z') digit=static_cast<int>(cp-'a'+10);
        else if (cp>='A'&&cp<='Z') digit=static_cast<int>(cp-'A'+10);
        else if (cp>=0xff41&&cp<=0xff5a) digit=static_cast<int>(cp-0xff41+10);
        else if (cp>=0xff21&&cp<=0xff3a) digit=static_cast<int>(cp-0xff21+10);
        else digit=unicodeDecimalDigit(cp);
        if (digit<0||digit>=radix) throw NumberFormatError("For input string: \""+value+"\" under radix "+std::to_string(radix));
        std::uint64_t carry=static_cast<unsigned>(digit);
        for (auto& limb:magnitude_) {
            const std::uint64_t current=static_cast<std::uint64_t>(limb)*static_cast<unsigned>(radix)+carry;
            limb=static_cast<std::uint32_t>(current%1000000000ULL);
            carry=current/1000000000ULL;
        }
        if (carry) magnitude_.push_back(static_cast<std::uint32_t>(carry));
        if (magnitude_.empty() && digit) magnitude_.push_back(static_cast<std::uint32_t>(digit));
    }
    while (!magnitude_.empty() && magnitude_.back()==0) magnitude_.pop_back();
    if (magnitude_.empty()) sign_=0;
}

BigInteger::BigInteger(const std::string& value) {
    if (value.empty()) throw NumberFormatError("Zero length BigInteger");
    const std::size_t start=value[0]=='-'||value[0]=='+' ? 1 : 0;
    if (value.find('+',start)!=std::string::npos || value.find('-',start)!=std::string::npos)
        throw NumberFormatError("Illegal embedded sign character");
    if (start==value.size()) throw NumberFormatError("Zero length BigInteger");
    sign_=value[0]=='-' ? -1 : 1;
    struct Digit { int digit; std::string text; };
    std::vector<Digit> digits;
    for (std::size_t i=start;i<value.size();) {
        const auto begin=i;
        const auto cp=nextCodePoint(value,i);
        const auto digit=unicodeDecimalDigit(cp);
        digits.push_back({digit,value.substr(begin,i-begin)});
    }
    std::size_t cursor=0;
    while(cursor<digits.size() && digits[cursor].digit==0) ++cursor;
    if(cursor==digits.size()) {sign_=0;return;}
    std::string ascii;
    const auto first=(digits.size()-cursor)%9;
    for(std::size_t group=cursor;group<digits.size();) {
        const auto count=group==cursor && first ? first : std::min<std::size_t>(9,digits.size()-group);
        std::string original;bool invalid=false;
        for(auto i=group;i<group+count;++i) {original+=digits[i].text;invalid|=digits[i].digit<0;}
        if(invalid) throw NumberFormatError("For input string: \""+original+"\"");
        for(auto i=group;i<group+count;++i) ascii.push_back(static_cast<char>('0'+digits[i].digit));
        group+=count;
    }
    for(std::size_t end=ascii.size();end;) {
        const auto begin=end>9 ? end-9 : 0;
        std::uint32_t word=0;
        for(auto i=begin;i<end;++i) word=word*10+static_cast<unsigned>(ascii[i]-'0');
        magnitude_.push_back(word);end=begin;
    }
}
std::string BigInteger::toString() const {
    if (!sign_) return "0";
    std::string result = sign_ < 0 ? "-" : "";
    result += std::to_string(magnitude_.back());
    for (std::size_t i = magnitude_.size() - 1; i != 0; --i) {
        const auto word = std::to_string(magnitude_[i - 1]);
        result.append(9 - word.size(), '0');
        result += word;
    }
    return result;
}
std::string BigInteger::toString(int radix) const {
    if (radix < 2 || radix > 36) radix = 10;
    if (radix==10) return toString();
    if (!sign_) return "0";
    auto value=magnitude_;
    std::string digits;
    constexpr char alphabet[]="0123456789abcdefghijklmnopqrstuvwxyz";
    while (!value.empty()) {
        std::uint64_t remainder=0;
        for (std::size_t i=value.size();i>0;--i) {
            const std::uint64_t current=remainder*1000000000ULL+value[i-1];
            value[i-1]=static_cast<std::uint32_t>(current/static_cast<unsigned>(radix));
            remainder=current%static_cast<unsigned>(radix);
        }
        digits.push_back(alphabet[remainder]);
        while (!value.empty()&&value.back()==0) value.pop_back();
    }
    if (sign_<0) digits.push_back('-');
    std::reverse(digits.begin(),digits.end());
    return digits;
}
Bytes BigInteger::toByteArray() const {
    auto bytes = toUnsignedBytes(magnitude_);
    if (bytes.empty()) return Bytes{0};
    if (sign_ > 0) {
        if (bytes.front() & 0x80U) bytes.insert(bytes.begin(), 0);
        return bytes;
    }
    for (auto& byte : bytes) byte = static_cast<std::uint8_t>(~byte);
    for (std::size_t i = bytes.size(); i != 0; --i) {
        if (++bytes[i - 1] != 0) break;
    }
    if (!(bytes.front() & 0x80U)) bytes.insert(bytes.begin(), 0xff);
    return bytes;
}
int BigInteger::compareTo(const BigInteger& rhs) const noexcept {
    if (sign_ != rhs.sign_) return sign_ < rhs.sign_ ? -1 : 1;
    if (magnitude_.size() != rhs.magnitude_.size())
        return magnitude_.size() < rhs.magnitude_.size() ? -sign_ : sign_;
    for (std::size_t i = magnitude_.size(); i != 0; --i)
        if (magnitude_[i - 1] != rhs.magnitude_[i - 1])
            return magnitude_[i - 1] < rhs.magnitude_[i - 1] ? -sign_ : sign_;
    return 0;
}
bool BigInteger::equals(const BigInteger& rhs) const noexcept {
    return sign_ == rhs.sign_ && magnitude_ == rhs.magnitude_;
}

BigInteger BigInteger::add(const BigInteger& rhs) const {
    return (BigDecimal(*this)+BigDecimal(rhs)).toBigInteger();
}
BigInteger BigInteger::subtract(const BigInteger& rhs) const {
    return (BigDecimal(*this)-BigDecimal(rhs)).toBigInteger();
}
BigInteger BigInteger::multiply(const BigInteger& rhs) const {
    return (BigDecimal(*this)*BigDecimal(rhs)).toBigInteger();
}
std::pair<BigInteger,BigInteger> BigInteger::divideAndRemainder(const BigInteger& rhs) const {
    if (!rhs.sign_) throw ArithmeticError("BigInteger divide by zero");
    const auto result=BigDecimal(*this).divideAndRemainder(BigDecimal(rhs));
    return {result.first.toBigInteger(),result.second.toBigInteger()};
}
BigInteger BigInteger::divide(const BigInteger& rhs) const { return divideAndRemainder(rhs).first; }
BigInteger BigInteger::remainder(const BigInteger& rhs) const { return divideAndRemainder(rhs).second; }
BigInteger BigInteger::pow(std::int32_t exponent) const {
    if (exponent<0) throw ArithmeticError("Negative exponent");
    BigInteger result(1),base(*this);
    auto power=static_cast<std::uint32_t>(exponent);
    while (power) {
        if (power&1U) result=result.multiply(base);
        power>>=1;
        if (power) base=base.multiply(base);
    }
    return result;
}
BigInteger BigInteger::gcd(const BigInteger& rhs) const {
    auto a=abs(),b=rhs.abs();
    while (b.sign_) { auto r=a.remainder(b);a=std::move(b);b=std::move(r); }
    return a;
}
BigInteger BigInteger::mod(const BigInteger& modulus) const {
    if (modulus.sign_<=0) throw ArithmeticError("BigInteger: modulus not positive");
    auto result=remainder(modulus);
    if (result.sign_<0) result=result.add(modulus);
    return result;
}
BigInteger BigInteger::abs() const { auto result=*this;if(result.sign_<0)result.sign_=1;return result; }
BigInteger BigInteger::negate() const { auto result=*this;result.sign_=-result.sign_;return result; }
BigInteger BigInteger::min(const BigInteger& rhs) const { return compareTo(rhs)<=0?*this:rhs; }
BigInteger BigInteger::max(const BigInteger& rhs) const { return compareTo(rhs)>=0?*this:rhs; }
BigInteger BigInteger::sqrt() const {
    if (sign_ < 0) throw ArithmeticError("Negative BigInteger");
    if (compareTo(ONE) < 0) return *this;
    auto estimate = ONE.shiftLeft((bitLength() + 1) / 2);
    for (;;) {
        auto next = (estimate + divide(estimate)).shiftRight(1);
        if (next >= estimate) return estimate;
        estimate = std::move(next);
    }
}
std::pair<BigInteger, BigInteger> BigInteger::sqrtAndRemainder() const {
    auto root = sqrt();
    return {root, subtract(root.multiply(root))};
}
BigInteger BigInteger::modInverse(const BigInteger& modulus) const {
    if (modulus.sign_ <= 0) throw ArithmeticError("BigInteger: modulus not positive");
    if (modulus == ONE) return ZERO;
    auto oldR = modulus;
    auto r = mod(modulus);
    auto oldT = ZERO;
    auto t = ONE;
    while (r.sign_) {
        auto [q, remainderValue] = oldR.divideAndRemainder(r);
        oldR = std::move(r);
        r = std::move(remainderValue);
        auto next = oldT.subtract(q.multiply(t));
        oldT = std::move(t);
        t = std::move(next);
    }
    if (oldR != ONE) throw ArithmeticError("BigInteger not invertible.");
    return oldT.mod(modulus);
}
BigInteger BigInteger::modPow(const BigInteger& exponent, const BigInteger& modulus) const {
    if (modulus.sign_ <= 0) throw ArithmeticError("BigInteger: modulus not positive");
    if (modulus == ONE) return ZERO;
    if (exponent.sign_ < 0) return modInverse(modulus).modPow(exponent.negate(), modulus);
    auto result = ONE.mod(modulus);
    auto base = mod(modulus);
    auto power = exponent;
    while (power.sign_) {
        if (power.testBit(0)) result = result.multiply(base).mod(modulus);
        power = power.shiftRight(1);
        if (power.sign_) base = base.multiply(base).mod(modulus);
    }
    return result;
}
BigInteger BigInteger::shiftLeft(std::int32_t distance) const {
    if (!sign_ || distance == 0) return *this;
    if (distance < 0) {
        if (distance == std::numeric_limits<std::int32_t>::min()) return sign_ < 0 ? BigInteger(-1) : ZERO;
        return shiftRight(-distance);
    }
    auto bytes = toUnsignedBytes(magnitude_);
    const auto whole = static_cast<std::size_t>(distance / 8);
    const auto bits = static_cast<unsigned>(distance % 8);
    if (whole > bytes.max_size() - bytes.size() - 1) throw std::length_error("BigInteger would overflow supported range");
    Bytes shifted(bytes.size() + 1, 0);
    std::uint16_t carry = 0;
    for (std::size_t i = bytes.size(); i != 0; --i) {
        const auto value = static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[i - 1]) << bits) | carry);
        shifted[i] = static_cast<std::uint8_t>(value);
        carry = static_cast<std::uint16_t>(value >> 8U);
    }
    shifted[0] = static_cast<std::uint8_t>(carry);
    const auto first = firstNonzero(shifted);
    shifted.erase(shifted.begin(), shifted.begin() + static_cast<std::ptrdiff_t>(first));
    shifted.insert(shifted.end(), whole, 0);
    return BigInteger(sign_, shifted);
}
BigInteger BigInteger::shiftRight(std::int32_t distance) const {
    if (!sign_ || distance == 0) return *this;
    if (distance < 0) {
        if (distance == std::numeric_limits<std::int32_t>::min()) throw std::length_error("BigInteger would overflow supported range");
        return shiftLeft(-distance);
    }
    if (static_cast<std::size_t>(distance) >= bitLength() + 1U)
        return sign_ < 0 ? BigInteger(-1) : ZERO;
    auto bytes = toUnsignedBytes(magnitude_);
    const auto whole = static_cast<std::size_t>(distance / 8);
    const auto bits = static_cast<unsigned>(distance % 8);
    bool discarded = false;
    for (std::size_t i = bytes.size() - whole; i < bytes.size(); ++i) discarded |= bytes[i] != 0;
    bytes.resize(bytes.size() - whole);
    if (bits) {
        discarded |= (bytes.back() & ((1U << bits) - 1U)) != 0;
        unsigned carry = 0;
        for (auto& byte : bytes) {
            const auto value = static_cast<unsigned>(byte);
            byte = static_cast<std::uint8_t>((carry << (8U - bits)) | (value >> bits));
            carry = value & ((1U << bits) - 1U);
        }
    }
    const auto first = firstNonzero(bytes);
    bytes.erase(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(first));
    auto quotient = BigInteger(1, bytes);
    if (sign_ < 0) {
        if (discarded) quotient = quotient.add(ONE);
        return quotient.negate();
    }
    return quotient;
}
namespace {
BigInteger bitOperation(const BigInteger& lhs, const BigInteger& rhs, char operation) {
    auto a = lhs.toByteArray(), b = rhs.toByteArray();
    const auto size = std::max(a.size(), b.size());
    Bytes result(size);
    const auto aPad = (a.front() & 0x80U) ? 0xffU : 0U;
    const auto bPad = (b.front() & 0x80U) ? 0xffU : 0U;
    for (std::size_t i = 0; i < size; ++i) {
        const auto av = i < size - a.size() ? aPad : a[i - (size - a.size())];
        const auto bv = i < size - b.size() ? bPad : b[i - (size - b.size())];
        result[i] = operation == '&' ? static_cast<std::uint8_t>(av & bv)
            : operation == '|' ? static_cast<std::uint8_t>(av | bv)
            : static_cast<std::uint8_t>(av ^ bv);
    }
    return BigInteger(result);
}
}
BigInteger BigInteger::and_(const BigInteger& rhs) const { return bitOperation(*this, rhs, '&'); }
BigInteger BigInteger::or_(const BigInteger& rhs) const { return bitOperation(*this, rhs, '|'); }
BigInteger BigInteger::xor_(const BigInteger& rhs) const { return bitOperation(*this, rhs, '^'); }
BigInteger BigInteger::not_() const {
    auto bytes = toByteArray();
    for (auto& byte : bytes) byte = static_cast<std::uint8_t>(~byte);
    return BigInteger(bytes);
}
BigInteger BigInteger::andNot(const BigInteger& rhs) const { return and_(rhs.not_()); }
bool BigInteger::testBit(std::int32_t index) const {
    if (index < 0) throw ArithmeticError("Negative bit address");
    const auto bytes = toByteArray();
    const auto byteFromEnd = static_cast<std::size_t>(index / 8);
    if (byteFromEnd >= bytes.size()) return sign_ < 0;
    return ((bytes[bytes.size() - byteFromEnd - 1] >> (index % 8)) & 1U) != 0;
}
BigInteger BigInteger::setBit(std::int32_t index) const {
    if (index < 0) throw ArithmeticError("Negative bit address");
    return or_(ONE.shiftLeft(index));
}
BigInteger BigInteger::clearBit(std::int32_t index) const {
    if (index < 0) throw ArithmeticError("Negative bit address");
    return andNot(ONE.shiftLeft(index));
}
BigInteger BigInteger::flipBit(std::int32_t index) const {
    if (index < 0) throw ArithmeticError("Negative bit address");
    return xor_(ONE.shiftLeft(index));
}
std::int32_t BigInteger::getLowestSetBit() const {
    if (!sign_) return -1;
    const auto bytes = toUnsignedBytes(magnitude_);
    std::size_t result = 0;
    for (auto i = bytes.rbegin(); i != bytes.rend() && *i == 0; ++i) result += 8;
    auto value = bytes[bytes.size() - result / 8 - 1];
    while ((value & 1U) == 0) { ++result; value >>= 1U; }
    return static_cast<std::int32_t>(result);
}
std::int32_t BigInteger::bitLength() const {
    auto bytes = toUnsignedBytes(magnitude_);
    auto length = bitLengthBytes(bytes);
    if (sign_ < 0 && getLowestSetBit() == static_cast<std::int32_t>(length - 1)) --length;
    return static_cast<std::int32_t>(length);
}
std::int32_t BigInteger::bitCount() const {
    if (sign_ < 0) {
        auto absoluteMinusOne = abs().subtract(ONE);
        return absoluteMinusOne.bitCount();
    }
    const auto bytes = toUnsignedBytes(magnitude_);
    std::uint64_t count = 0;
    for (const auto byte : bytes) count += popcount32(byte);
    if (count > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
        throw std::length_error("BigInteger bit count overflow");
    return static_cast<std::int32_t>(count);
}
std::int32_t BigInteger::hashCode() const {
    const auto bytes = toUnsignedBytes(magnitude_);
    const auto words = (bytes.size() + 3U) / 4U;
    std::uint32_t hash = 0;
    const auto padding = words * 4U - bytes.size();
    for (std::size_t i = 0; i < words; ++i) {
        std::uint32_t word = 0;
        for (std::size_t j = 0; j < 4; ++j) {
            const auto byteIndex = i * 4U + j;
            const auto value = byteIndex < padding ? 0U : bytes[byteIndex - padding];
            word = (word << 8U) | value;
        }
        hash = hash * 31U + word;
    }
    return wrapInt32(hash * static_cast<std::uint32_t>(sign_));
}
std::int32_t BigInteger::intValue() const { return BigDecimal(*this).intValue(); }
std::int64_t BigInteger::longValue() const { return BigDecimal(*this).longValue(); }
float BigInteger::floatValue() const { return BigDecimal(*this).floatValue(); }
double BigInteger::doubleValue() const { return BigDecimal(*this).doubleValue(); }
std::int16_t BigInteger::shortValue() const {
    const auto bits = static_cast<std::uint16_t>(intValue());
    return bits <= 0x7fffU ? static_cast<std::int16_t>(bits)
        : static_cast<std::int16_t>(static_cast<std::int32_t>(bits) - 0x10000);
}
std::int8_t BigInteger::byteValue() const {
    const auto bits = static_cast<std::uint8_t>(intValue());
    return bits <= 0x7fU ? static_cast<std::int8_t>(bits)
        : static_cast<std::int8_t>(static_cast<std::int16_t>(bits) - 0x100);
}
std::int32_t BigInteger::intValueExact() const {
    if (bitLength() > 31) throw ArithmeticError("BigInteger out of int range");
    return intValue();
}
std::int64_t BigInteger::longValueExact() const {
    if (bitLength() > 63) throw ArithmeticError("BigInteger out of long range");
    return longValue();
}
std::int16_t BigInteger::shortValueExact() const {
    if (compareTo(BigInteger(std::numeric_limits<std::int16_t>::min())) < 0 ||
        compareTo(BigInteger(std::numeric_limits<std::int16_t>::max())) > 0)
        throw ArithmeticError("BigInteger out of short range");
    return static_cast<std::int16_t>(intValue());
}
std::int8_t BigInteger::byteValueExact() const {
    if (compareTo(BigInteger(std::numeric_limits<std::int8_t>::min())) < 0 ||
        compareTo(BigInteger(std::numeric_limits<std::int8_t>::max())) > 0)
        throw ArithmeticError("BigInteger out of byte range");
    return static_cast<std::int8_t>(intValue());
}
bool BigInteger::isProbablePrime(std::int32_t certainty) const {
    if (certainty <= 0) return true;
    const auto number = abs();
    if (number == TWO) return true;
    if (number < BigInteger(2) || !number.testBit(0)) return false;
    for (const std::int64_t prime : {3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47}) {
        const BigInteger p(prime);
        if (number == p) return true;
        if (number.remainder(p).sign_ == 0) return false;
    }
    auto d = number.subtract(ONE);
    const auto powersOfTwo = d.getLowestSetBit();
    d = d.shiftRight(powersOfTwo);
    std::mt19937_64 random(std::random_device{}());
    const auto rounds = std::max<std::int32_t>(1, certainty / 2 + certainty % 2);
    const auto range = number.subtract(BigInteger(3));
    for (std::int32_t round = 0; round < rounds; ++round) {
        auto base = BigInteger(number.bitLength(), random).mod(range).add(TWO);
        auto witness = base.modPow(d, number);
        if (witness == ONE || witness == number.subtract(ONE)) continue;
        bool probablyPrime = false;
        for (std::int32_t i = 1; i < powersOfTwo; ++i) {
            witness = witness.multiply(witness).mod(number);
            if (witness == number.subtract(ONE)) { probablyPrime = true; break; }
            if (witness == ONE) return false;
        }
        if (!probablyPrime) return false;
    }
    return true;
}
BigInteger BigInteger::nextProbablePrime() const {
    if (sign_ < 0) throw ArithmeticError("start < 0: " + toString());
    auto candidate = compareTo(ONE) < 0 ? BigInteger(2) : add(ONE);
    if (!candidate.testBit(0) && candidate != BigInteger(2)) candidate = candidate.add(ONE);
    while (!candidate.isProbablePrime(100)) candidate = candidate.add(BigInteger(2));
    return candidate;
}
