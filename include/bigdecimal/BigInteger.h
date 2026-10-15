
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



#ifndef BIG_INTEGER_H
#define BIG_INTEGER_H
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

// Entero con signo y precisión arbitraria, usado directamente y por BigDecimal.
class BigInteger {
public:
    static const BigInteger ZERO, ONE, TWO, TEN;
    BigInteger() = default;
    BigInteger(const BigInteger&) = default;
    BigInteger& operator=(const BigInteger&) = default;
    BigInteger(BigInteger&& other) noexcept;
    BigInteger& operator=(BigInteger&& other) noexcept;
    explicit BigInteger(const std::string& value);
    BigInteger(const std::string& value, int radix);
    explicit BigInteger(const std::vector<std::uint8_t>& twosComplement);
    BigInteger(const std::vector<std::uint8_t>& twosComplement, std::int32_t offset, std::int32_t length);
    BigInteger(std::int32_t signum, const std::vector<std::uint8_t>& magnitude);
    BigInteger(std::int32_t signum, const std::vector<std::uint8_t>& magnitude,
               std::int32_t offset, std::int32_t length);
    BigInteger(std::int32_t numBits, std::mt19937_64& random);
    BigInteger(std::int32_t bitLength, std::int32_t certainty, std::mt19937_64& random);
    explicit BigInteger(std::int64_t value);
    [[nodiscard]] static BigInteger valueOf(std::int64_t value);
    [[nodiscard]] static BigInteger probablePrime(std::int32_t bitLength, std::mt19937_64& random);
    [[nodiscard]] BigInteger nextProbablePrime() const;
    [[nodiscard]] bool isProbablePrime(std::int32_t certainty) const;
    [[nodiscard]] std::string toString() const;
    [[nodiscard]] std::string toString(int radix) const;
    [[nodiscard]] std::vector<std::uint8_t> toByteArray() const;
    [[nodiscard]] int signum() const noexcept { return sign_; }
    [[nodiscard]] int compareTo(const BigInteger& rhs) const noexcept;
    [[nodiscard]] bool equals(const BigInteger& rhs) const noexcept;
    [[nodiscard]] BigInteger add(const BigInteger& rhs) const;
    [[nodiscard]] BigInteger subtract(const BigInteger& rhs) const;
    [[nodiscard]] BigInteger multiply(const BigInteger& rhs) const;
    [[nodiscard]] BigInteger parallelMultiply(const BigInteger& rhs) const { return multiply(rhs); }
    [[nodiscard]] BigInteger divide(const BigInteger& rhs) const;
    [[nodiscard]] BigInteger remainder(const BigInteger& rhs) const;
    [[nodiscard]] std::pair<BigInteger, BigInteger> divideAndRemainder(const BigInteger& rhs) const;
    [[nodiscard]] BigInteger pow(std::int32_t exponent) const;
    [[nodiscard]] BigInteger sqrt() const;
    [[nodiscard]] std::pair<BigInteger, BigInteger> sqrtAndRemainder() const;
    [[nodiscard]] BigInteger gcd(const BigInteger& rhs) const;
    [[nodiscard]] BigInteger mod(const BigInteger& modulus) const;
    [[nodiscard]] BigInteger modPow(const BigInteger& exponent, const BigInteger& modulus) const;
    [[nodiscard]] BigInteger modInverse(const BigInteger& modulus) const;
    [[nodiscard]] BigInteger shiftLeft(std::int32_t distance) const;
    [[nodiscard]] BigInteger shiftRight(std::int32_t distance) const;
    [[nodiscard]] BigInteger and_(const BigInteger& rhs) const;
    [[nodiscard]] BigInteger or_(const BigInteger& rhs) const;
    [[nodiscard]] BigInteger xor_(const BigInteger& rhs) const;
    [[nodiscard]] BigInteger not_() const;
    [[nodiscard]] BigInteger andNot(const BigInteger& rhs) const;
    [[nodiscard]] bool testBit(std::int32_t index) const;
    [[nodiscard]] BigInteger setBit(std::int32_t index) const;
    [[nodiscard]] BigInteger clearBit(std::int32_t index) const;
    [[nodiscard]] BigInteger flipBit(std::int32_t index) const;
    [[nodiscard]] std::int32_t getLowestSetBit() const;
    [[nodiscard]] std::int32_t bitLength() const;
    [[nodiscard]] std::int32_t bitCount() const;
    [[nodiscard]] BigInteger abs() const;
    [[nodiscard]] BigInteger negate() const;
    [[nodiscard]] BigInteger min(const BigInteger& rhs) const;
    [[nodiscard]] BigInteger max(const BigInteger& rhs) const;
    [[nodiscard]] std::int32_t hashCode() const;
    [[nodiscard]] std::int32_t intValue() const;
    [[nodiscard]] std::int64_t longValue() const;
    [[nodiscard]] float floatValue() const;
    [[nodiscard]] double doubleValue() const;
    [[nodiscard]] std::int16_t shortValue() const;
    [[nodiscard]] std::int8_t byteValue() const;
    [[nodiscard]] std::int32_t intValueExact() const;
    [[nodiscard]] std::int64_t longValueExact() const;
    [[nodiscard]] std::int16_t shortValueExact() const;
    [[nodiscard]] std::int8_t byteValueExact() const;
    [[nodiscard]] BigInteger operator+(const BigInteger& rhs) const { return add(rhs); }
    [[nodiscard]] BigInteger operator-(const BigInteger& rhs) const { return subtract(rhs); }
    [[nodiscard]] BigInteger operator*(const BigInteger& rhs) const { return multiply(rhs); }
    [[nodiscard]] BigInteger operator/(const BigInteger& rhs) const { return divide(rhs); }
    [[nodiscard]] BigInteger operator%(const BigInteger& rhs) const { return remainder(rhs); }
    [[nodiscard]] BigInteger operator-() const { return negate(); }
    [[nodiscard]] BigInteger operator+() const { return *this; }
    [[nodiscard]] BigInteger operator<<(std::int32_t distance) const { return shiftLeft(distance); }
    [[nodiscard]] BigInteger operator>>(std::int32_t distance) const { return shiftRight(distance); }
    [[nodiscard]] BigInteger operator&(const BigInteger& rhs) const { return and_(rhs); }
    [[nodiscard]] BigInteger operator|(const BigInteger& rhs) const { return or_(rhs); }
    [[nodiscard]] BigInteger operator^(const BigInteger& rhs) const { return xor_(rhs); }
    [[nodiscard]] BigInteger operator~() const { return not_(); }
    bool operator==(const BigInteger& rhs) const noexcept { return equals(rhs); }
    bool operator!=(const BigInteger& rhs) const noexcept { return !equals(rhs); }
    bool operator<(const BigInteger& rhs) const noexcept { return compareTo(rhs) < 0; }
    bool operator>(const BigInteger& rhs) const noexcept { return compareTo(rhs) > 0; }
    bool operator<=(const BigInteger& rhs) const noexcept { return compareTo(rhs) <= 0; }
    bool operator>=(const BigInteger& rhs) const noexcept { return compareTo(rhs) >= 0; }
private:
    friend class BigDecimal;
    BigInteger(std::vector<std::uint32_t> magnitude, int sign);
    std::vector<std::uint32_t> magnitude_;
    int sign_ = 0;
};
#endif
