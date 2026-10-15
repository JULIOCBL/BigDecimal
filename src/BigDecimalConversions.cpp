
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



#include "BigDecimal.h"
#include "NativeExceptions.h"
#include "BigInteger.h"
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
template<class Signed, class Unsigned>
Signed signedBits(Unsigned value) {
    static_assert(sizeof(Signed) == sizeof(Unsigned));
    Signed result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}
template<class T> T narrowExact(std::int64_t value) {
    if (value < std::numeric_limits<T>::min() || value > std::numeric_limits<T>::max())
        throw ArithmeticError("Overflow");
    return static_cast<T>(value);
}
}

BigInteger BigDecimal::unscaledValue() const { return BigInteger(magnitude_, sign_); }
BigInteger BigDecimal::toBigInteger() const {
    const auto integer = setScale(0, RoundingMode::Down);
    return BigInteger(integer.magnitude_, integer.sign_);
}
BigInteger BigDecimal::toBigIntegerExact() const {
    const auto integer = setScale(0, RoundingMode::Unnecessary);
    return BigInteger(integer.magnitude_, integer.sign_);
}

std::int64_t BigDecimal::longValue() const {
    // Los 64 bits inferiores de 10^64 son cero; evita materializar escalas extremas.
    if (sign_ == 0 || scale_ <= -64 ||
        (scale_ > 0 && static_cast<std::uint64_t>(scale_) >= precisionSize())) return 0;
    std::uint64_t result = 0;
    if (scale_ > 0) {
        auto digits = unscaledValue().toString();
        std::size_t begin = sign_ < 0 ? 1 : 0;
        const auto end = digits.size() - static_cast<std::size_t>(scale_);
        for (auto i = begin; i < end; ++i) result = result * 10 + (digits[i] - '0');
    } else {
        for (std::size_t i = magnitude_.size(); i != 0; --i)
            result = result * 1000000000ULL + magnitude_[i - 1];
        for (std::int64_t i = 0; i < -static_cast<std::int64_t>(scale_); ++i) result *= 10;
    }
    if (sign_ < 0) result = 0 - result;
    return signedBits<std::int64_t>(result);
}
std::int64_t BigDecimal::longValueExact() const {
    if (!sign_) return 0;
    if (scale_ > 0 && static_cast<std::uint64_t>(scale_) >= precisionSize())
        throw ArithmeticError("Rounding necessary");
    if (static_cast<std::int64_t>(precisionSize()) - scale_ > 19)
        throw ArithmeticError("Overflow");
    const auto integer = setScale(0, RoundingMode::Unnecessary);
    std::uint64_t value = 0;
    constexpr auto maximum = std::uint64_t{1} << 63;
    const auto limit = sign_ < 0 ? maximum : maximum - 1;
    for (std::size_t i = integer.magnitude_.size(); i != 0; --i) {
        const auto digit = integer.magnitude_[i - 1];
        if (value > (limit - digit) / 1000000000ULL) throw ArithmeticError("Overflow");
        value = value * 1000000000ULL + digit;
    }
    if (sign_ < 0) value = 0 - value;
    return signedBits<std::int64_t>(value);
}
std::int32_t BigDecimal::intValue() const {
    return signedBits<std::int32_t>(static_cast<std::uint32_t>(longValue()));
}
std::int16_t BigDecimal::shortValue() const {
    return signedBits<std::int16_t>(static_cast<std::uint16_t>(longValue()));
}
std::int8_t BigDecimal::byteValue() const {
    return signedBits<std::int8_t>(static_cast<std::uint8_t>(longValue()));
}
std::int32_t BigDecimal::intValueExact() const { return narrowExact<std::int32_t>(longValueExact()); }
std::int16_t BigDecimal::shortValueExact() const { return narrowExact<std::int16_t>(longValueExact()); }
std::int8_t BigDecimal::byteValueExact() const { return narrowExact<std::int8_t>(longValueExact()); }

std::int32_t BigDecimal::hashCode() const {
    // Decodifica las palabras de magnitud sin signo de 32 bits en orden
    // big-endian. El polinomio también equivale a la fórmula intCompact
    // para valores compactos.
    std::vector<std::uint32_t> binary;
    for (std::size_t i = magnitude_.size(); i != 0; --i) {
        std::uint64_t carry = magnitude_[i - 1];
        for (auto& word : binary) {
            const auto product = static_cast<std::uint64_t>(word) * 1000000000ULL + carry;
            word = static_cast<std::uint32_t>(product);
            carry = product >> 32;
        }
        if (carry) binary.push_back(static_cast<std::uint32_t>(carry));
    }
    std::uint32_t hash = 0;
    for (std::size_t i = binary.size(); i != 0; --i) hash = hash * 31 + binary[i - 1];
    if (sign_ < 0) hash = 0U - hash;
    hash = hash * 31 + static_cast<std::uint32_t>(scale_);
    return signedBits<std::int32_t>(hash);
}

std::string BigDecimal::toEngineeringString() const {
    if (scale_ == 0) return unscaledValue().toString();
    auto digits = unscaledValue().toString();
    if (sign_ < 0) digits.erase(0, 1);
    std::int64_t exponent = static_cast<std::int64_t>(digits.size()) - 1 - scale_;
    if (scale_ >= 0 && exponent >= -6) return toPlainString();
    auto remainder = exponent % 3;
    if (remainder < 0) remainder += 3;
    exponent -= remainder;
    const auto integerDigits = static_cast<std::size_t>(remainder + 1);
    std::string result = sign_ < 0 ? "-" : "";
    if (sign_ == 0) {
        if (integerDigits == 1) result += "0";
        else {
            result += integerDigits == 2 ? "0.00" : "0.0";
            exponent += 3;
        }
    } else if (integerDigits >= digits.size()) {
        result += digits;
        result.append(integerDigits - digits.size(), '0');
    } else {
        result += digits.substr(0, integerDigits) + "." + digits.substr(integerDigits);
    }
    if (exponent != 0) {
        result += exponent > 0 ? "E+" : "E";
        result += std::to_string(exponent);
    }
    return result;
}
