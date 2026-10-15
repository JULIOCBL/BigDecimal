
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

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {

using Magnitude = std::vector<std::uint32_t>;
constexpr std::uint64_t kBase = 1'000'000'000ULL;
constexpr std::size_t kKaratsubaThreshold = 32;
// BigInteger admite como máximo INT32_MAX bits (646456993 cifras decimales).
constexpr std::uint64_t kMaxCoefficientDigits = 646456993;
std::size_t decimalDigits(const Magnitude& value);

void normalize(Magnitude& value) {
    while (!value.empty() && value.back() == 0) value.pop_back();
}

int compare(const Magnitude& lhs, const Magnitude& rhs) {
    if (lhs.size() != rhs.size()) return lhs.size() < rhs.size() ? -1 : 1;
    for (std::size_t i = lhs.size(); i != 0; --i) {
        if (lhs[i - 1] != rhs[i - 1]) return lhs[i - 1] < rhs[i - 1] ? -1 : 1;
    }
    return 0;
}

Magnitude addMagnitude(const Magnitude& lhs, const Magnitude& rhs) {
    Magnitude result;
    result.reserve(std::max(lhs.size(), rhs.size()) + 1);
    std::uint64_t carry = 0;
    const auto n = std::max(lhs.size(), rhs.size());
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint64_t a = i < lhs.size() ? lhs[i] : 0;
        const std::uint64_t b = i < rhs.size() ? rhs[i] : 0;
        const std::uint64_t sum = a + b + carry;
        result.push_back(static_cast<std::uint32_t>(sum % kBase));
        carry = sum / kBase;
    }
    if (carry) result.push_back(static_cast<std::uint32_t>(carry));
    return result;
}

// Precondición: lhs >= rhs.
Magnitude subtractMagnitude(const Magnitude& lhs, const Magnitude& rhs) {
    Magnitude result;
    result.reserve(lhs.size());
    std::int64_t borrow = 0;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        std::int64_t digit = static_cast<std::int64_t>(lhs[i]) - borrow;
        if (i < rhs.size()) digit -= rhs[i];
        if (digit < 0) {
            digit += static_cast<std::int64_t>(kBase);
            borrow = 1;
        } else {
            borrow = 0;
        }
        result.push_back(static_cast<std::uint32_t>(digit));
    }
    normalize(result);
    return result;
}

Magnitude multiplySmall(const Magnitude& value, std::uint64_t factor) {
    if (value.empty() || factor == 0) return {};
    Magnitude result;
    result.reserve(value.size() + 2);
    std::uint64_t carry = 0;
    for (const auto digit : value) {
        const std::uint64_t product = static_cast<std::uint64_t>(digit) * factor + carry;
        result.push_back(static_cast<std::uint32_t>(product % kBase));
        carry = product / kBase;
    }
    while (carry) {
        result.push_back(static_cast<std::uint32_t>(carry % kBase));
        carry /= kBase;
    }
    return result;
}

Magnitude schoolbookMultiply(const Magnitude& lhs, const Magnitude& rhs) {
    if (lhs.empty() || rhs.empty()) return {};
    Magnitude result(lhs.size() + rhs.size(), 0);
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        std::uint64_t carry = 0;
        for (std::size_t j = 0; j < rhs.size(); ++j) {
            const std::uint64_t current = static_cast<std::uint64_t>(lhs[i]) * rhs[j] +
                                          result[i + j] + carry;
            result[i + j] = static_cast<std::uint32_t>(current % kBase);
            carry = current / kBase;
        }
        std::size_t position = i + rhs.size();
        while (carry) {
            if (position == result.size()) result.push_back(0);
            const std::uint64_t current = result[position] + carry;
            result[position] = static_cast<std::uint32_t>(current % kBase);
            carry = current / kBase;
            ++position;
        }
    }
    normalize(result);
    return result;
}

void addShifted(Magnitude& destination, const Magnitude& value, std::size_t shift) {
    if (value.empty()) return;
    if (destination.size() < shift + value.size()) destination.resize(shift + value.size(), 0);
    std::uint64_t carry = 0;
    std::size_t i = 0;
    for (; i < value.size(); ++i) {
        const std::uint64_t sum = static_cast<std::uint64_t>(destination[shift + i]) + value[i] + carry;
        destination[shift + i] = static_cast<std::uint32_t>(sum % kBase);
        carry = sum / kBase;
    }
    std::size_t position = shift + i;
    while (carry) {
        if (position == destination.size()) destination.push_back(0);
        const std::uint64_t sum = destination[position] + carry;
        destination[position] = static_cast<std::uint32_t>(sum % kBase);
        carry = sum / kBase;
        ++position;
    }
}

Magnitude multiplyMagnitude(const Magnitude& lhs, const Magnitude& rhs) {
    if (std::min(lhs.size(), rhs.size()) < kKaratsubaThreshold) {
        return schoolbookMultiply(lhs, rhs);
    }
    const std::size_t split = std::max(lhs.size(), rhs.size()) / 2;
    const auto cut = [split](const Magnitude& value, bool upper) {
        const std::size_t begin = upper ? std::min(split, value.size()) : 0;
        const std::size_t end = upper ? value.size() : std::min(split, value.size());
        Magnitude part(value.begin() + static_cast<std::ptrdiff_t>(begin),
                       value.begin() + static_cast<std::ptrdiff_t>(end));
        normalize(part);
        return part;
    };
    const Magnitude lhsLow = cut(lhs, false), lhsHigh = cut(lhs, true);
    const Magnitude rhsLow = cut(rhs, false), rhsHigh = cut(rhs, true);
    const Magnitude z0 = multiplyMagnitude(lhsLow, rhsLow);
    const Magnitude z2 = multiplyMagnitude(lhsHigh, rhsHigh);
    Magnitude z1 = multiplyMagnitude(addMagnitude(lhsLow, lhsHigh), addMagnitude(rhsLow, rhsHigh));
    z1 = subtractMagnitude(subtractMagnitude(z1, z0), z2);
    Magnitude result;
    addShifted(result, z0, 0);
    addShifted(result, z1, split);
    addShifted(result, z2, split * 2);
    normalize(result);
    return result;
}

Magnitude multiplyPowerOfTen(const Magnitude& value, std::uint64_t exponent) {
    if (value.empty() || exponent == 0) return value;
    if (exponent > kMaxCoefficientDigits ||
        decimalDigits(value) > kMaxCoefficientDigits - exponent)
        throw ArithmeticError("BigInteger would overflow supported range");
    const std::uint64_t zeroLimbs = exponent / 9;
    if (zeroLimbs > value.max_size() - value.size()) {
        throw ArithmeticError("BigInteger would overflow supported range");
    }
    Magnitude result(static_cast<std::size_t>(zeroLimbs), 0);
    result.insert(result.end(), value.begin(), value.end());
    const auto remainder = static_cast<unsigned>(exponent % 9);
    if (remainder == 0) return result;
    std::uint64_t factor = 1;
    for (unsigned i = 0; i < remainder; ++i) factor *= 10;
    std::uint64_t carry = 0;
    for (std::size_t i = static_cast<std::size_t>(zeroLimbs); i < result.size(); ++i) {
        const std::uint64_t product = static_cast<std::uint64_t>(result[i]) * factor + carry;
        result[i] = static_cast<std::uint32_t>(product % kBase);
        carry = product / kBase;
    }
    if (carry) result.push_back(static_cast<std::uint32_t>(carry));
    return result;
}

std::pair<Magnitude, std::uint32_t> divideSmall(const Magnitude& value, std::uint32_t divisor) {
    Magnitude quotient(value.size(), 0);
    std::uint64_t remainder = 0;
    for (std::size_t i = value.size(); i != 0; --i) {
        const std::uint64_t current = remainder * kBase + value[i - 1];
        quotient[i - 1] = static_cast<std::uint32_t>(current / divisor);
        remainder = current % divisor;
    }
    normalize(quotient);
    return {std::move(quotient), static_cast<std::uint32_t>(remainder)};
}

// Algoritmo D de Knuth, normalizado a base 10^9; todos los productos caben en uint64_t.
std::pair<Magnitude, Magnitude> divideMagnitude(const Magnitude& dividend,
                                                const Magnitude& divisor) {
    if (divisor.empty()) throw ArithmeticError("BigInteger divide by zero");
    if (compare(dividend, divisor) < 0) return {{}, dividend};
    if (divisor.size() == 1) {
        auto result = divideSmall(dividend, divisor.front());
        return {std::move(result.first), result.second ? Magnitude{result.second} : Magnitude{}};
    }
    const auto factor = static_cast<std::uint32_t>(kBase / (divisor.back() + 1ULL));
    auto u = multiplySmall(dividend, factor);
    const auto v = multiplySmall(divisor, factor);
    const auto n = v.size();
    const auto m = dividend.size() - n;
    u.resize(dividend.size() + 1, 0);
    Magnitude quotient(m + 1, 0);
    for (std::size_t position = m + 1; position != 0; --position) {
        const auto j = position - 1;
        const std::uint64_t leading = static_cast<std::uint64_t>(u[j+n]) * kBase + u[j+n-1];
        auto estimate = leading / v[n-1];
        auto remainder = leading % v[n-1];
        while (estimate >= kBase || estimate * v[n-2] > remainder * kBase + u[j+n-2]) {
            --estimate;
            remainder += v[n-1];
            if (remainder >= kBase) break;
        }
        std::uint64_t carry = 0;
        std::int64_t borrow = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const auto product = estimate * v[i] + carry;
            carry = product / kBase;
            auto digit = static_cast<std::int64_t>(u[j+i]) -
                         static_cast<std::int64_t>(product % kBase) - borrow;
            borrow = digit < 0 ? 1 : 0;
            if (borrow) digit += static_cast<std::int64_t>(kBase);
            u[j+i] = static_cast<std::uint32_t>(digit);
        }
        auto top = static_cast<std::int64_t>(u[j+n]) - static_cast<std::int64_t>(carry) - borrow;
        const bool overestimated = top < 0;
        if (overestimated) top += static_cast<std::int64_t>(kBase);
        u[j+n] = static_cast<std::uint32_t>(top);
        if (overestimated) {
            --estimate;
            carry = 0;
            for (std::size_t i = 0; i < n; ++i) {
                const auto sum = static_cast<std::uint64_t>(u[j+i]) + v[i] + carry;
                u[j+i] = static_cast<std::uint32_t>(sum % kBase);
                carry = sum / kBase;
            }
            u[j+n] = static_cast<std::uint32_t>((u[j+n] + carry) % kBase);
        }
        quotient[j] = static_cast<std::uint32_t>(estimate);
    }
    Magnitude remainder(u.begin(), u.begin() + static_cast<std::ptrdiff_t>(n));
    normalize(remainder);
    remainder = divideSmall(remainder, factor).first;
    normalize(quotient);
    return {std::move(quotient), std::move(remainder)};
}

Magnitude powerSmall(std::uint32_t base, std::uint64_t exponent) {
    Magnitude result{1}, factor{base};
    while (exponent) {
        if (exponent & 1U) result = multiplyMagnitude(result, factor);
        exponent >>= 1U;
        if (exponent) factor = multiplyMagnitude(factor, factor);
    }
    return result;
}

std::size_t decimalDigits(const Magnitude& value) {
    if (value.empty()) return 1;
    std::size_t digits = (value.size() - 1) * 9;
    std::uint32_t mostSignificant = value.back();
    do { ++digits; mostSignificant /= 10; } while (mostSignificant);
    return digits;
}

std::string magnitudeDigits(const Magnitude& value) {
    if (value.empty()) return "0";
    std::string result = std::to_string(value.back());
    for (std::size_t i = value.size() - 1; i != 0; --i) {
        const auto part = std::to_string(value[i - 1]);
        result.append(9 - part.size(), '0');
        result += part;
    }
    return result;
}

Magnitude parseMagnitude(std::string_view digits) {
    Magnitude result;
    for (std::size_t end = digits.size(); end != 0;) {
        const std::size_t begin = end > 9 ? end - 9 : 0;
        std::uint32_t limb = 0;
        for (std::size_t i = begin; i < end; ++i)
            limb = limb * 10U + static_cast<std::uint32_t>(digits[i] - '0');
        result.push_back(limb);
        end = begin;
    }
    normalize(result);
    return result;
}

std::int32_t checkedScale(std::int64_t scale) {
    if (scale < std::numeric_limits<std::int32_t>::min() ||
        scale > std::numeric_limits<std::int32_t>::max())
        throw ArithmeticError(scale > 0 ? "Underflow" : "Overflow");
    return static_cast<std::int32_t>(scale);
}

std::int32_t checkedScaleNonZero(std::int64_t scale) {
    const auto bits=static_cast<std::uint32_t>(scale);
    const auto narrowed=bits<=0x7fffffffU ? static_cast<std::int32_t>(bits) :
        static_cast<std::int32_t>(static_cast<std::int64_t>(bits)-4294967296LL);
    if (narrowed!=scale) throw ArithmeticError(narrowed>0 ? "Underflow" : "Overflow");
    return narrowed;
}

std::int32_t saturatedScale(std::int64_t scale) {
    return static_cast<std::int32_t>(std::clamp(scale,
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())));
}

void validateRoundingMode(RoundingMode mode) {
    if (mode < RoundingMode::Up || mode > RoundingMode::Unnecessary)
        throw InvalidArgumentError("Invalid rounding mode");
}

void validatePrecision(std::size_t precision) {
    if (precision > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        throw InvalidArgumentError("precision is out of range");
}

std::int64_t parseExponent(std::string_view text) {
    if (text.empty()) throw NumberFormatError();
    bool negative = false;
    if (text.front() == '+' || text.front() == '-') {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    if (text.empty()) throw NumberFormatError();
    while (text.size() > 1 && text.front() == '0') text.remove_prefix(1);
    if (text.size() > 10) throw NumberFormatError("Too many nonzero exponent digits.");
    for (char digit : text)
        if (digit < '0' || digit > '9') throw NumberFormatError("Not a digit.");
    std::int64_t exponent = 0;
    const char* first = text.data();
    const char* last = first + text.size();
    const auto result = std::from_chars(first, last, exponent);
    if (result.ec != std::errc{} || result.ptr != last)
        throw NumberFormatError();
    return negative ? -exponent : exponent;
}

bool shouldIncrement(RoundingMode mode, int sign, const Magnitude& quotient,
                     const Magnitude& remainder, const Magnitude& divisor) {
    if (remainder.empty()) return false;
    switch (mode) {
        case RoundingMode::Up: return true;
        case RoundingMode::Down: return false;
        case RoundingMode::Ceiling: return sign > 0;
        case RoundingMode::Floor: return sign < 0;
        case RoundingMode::Unnecessary:
            throw ArithmeticError("Rounding necessary");
        case RoundingMode::HalfUp:
        case RoundingMode::HalfDown:
        case RoundingMode::HalfEven: {
            const int halfComparison = compare(multiplySmall(remainder, 2), divisor);
            if (halfComparison > 0) return true;
            if (halfComparison < 0) return false;
            if (mode == RoundingMode::HalfUp) return true;
            if (mode == RoundingMode::HalfDown) return false;
            return !quotient.empty() && (quotient.front() & 1U) != 0;
        }
    }
    throw InvalidArgumentError("Invalid rounding mode");
}


int decimalDigit(char32_t ch) {
    constexpr char32_t starts[] = {0x30,0x660,0x6f0,0x7c0,0x966,0x9e6,0xa66,0xae6,
        0xb66,0xbe6,0xc66,0xce6,0xd66,0xde6,0xe50,0xed0,0xf20,0x1040,0x1090,
        0x17e0,0x1810,0x1946,0x19d0,0x1a80,0x1a90,0x1b50,0x1bb0,0x1c40,0x1c50,
        0xa620,0xa8d0,0xa900,0xa9d0,0xa9f0,0xaa50,0xabf0,0xff10};
    for (const auto start : starts) if (ch >= start && ch < start + 10) return static_cast<int>(ch-start);
    return -1;
}
void appendUtf8CodePoint(std::string& text, char32_t ch) {
    const auto digit = decimalDigit(ch);
    if (digit >= 0) text.push_back(static_cast<char>('0' + digit));
    else if (ch < 128) text.push_back(static_cast<char>(ch));
    else if (ch<=0x7ff) { text.push_back(static_cast<char>(0xc0|(ch>>6)));text.push_back(static_cast<char>(0x80|(ch&63))); }
    else if (ch<=0xffff) { text.push_back(static_cast<char>(0xe0|(ch>>12)));text.push_back(static_cast<char>(0x80|((ch>>6)&63)));text.push_back(static_cast<char>(0x80|(ch&63))); }
    else { text.push_back(static_cast<char>(0xf0|(ch>>18)));text.push_back(static_cast<char>(0x80|((ch>>12)&63)));text.push_back(static_cast<char>(0x80|((ch>>6)&63)));text.push_back(static_cast<char>(0x80|(ch&63))); }
}
std::string decimalAscii(const std::string& utf8) {
    std::string text;
    text.reserve(utf8.size());
    for (std::size_t i=0; i<utf8.size();) {
        const auto lead = static_cast<unsigned char>(utf8[i++]);
        char32_t ch = lead;
        unsigned extra = 0;
        if (lead >= 0xc2 && lead <= 0xdf) { ch=lead & 0x1f; extra=1; }
        else if (lead >= 0xe0 && lead <= 0xef) { ch=lead & 0x0f; extra=2; }
        else if (lead >= 0xf0 && lead <= 0xf4) { ch=lead & 7; extra=3; }
        else if (lead >= 128) throw InvalidArgumentError("invalid UTF-8 decimal");
        if (i + extra > utf8.size()) throw InvalidArgumentError("invalid UTF-8 decimal");
        for (unsigned j=0;j<extra;++j) {
            const auto next=static_cast<unsigned char>(utf8[i++]);
            if ((next & 0xc0) != 0x80) throw InvalidArgumentError("invalid UTF-8 decimal");
            ch=(ch<<6)|(next&0x3f);
        }
        if ((extra==1 && ch<0x80)||(extra==2 && ch<0x800)||(extra==3 && ch<0x10000)||
            ch>0x10ffff)
            throw InvalidArgumentError("invalid UTF-8 decimal");
        // La entrada UTF-16 se procesa como unidades de código; por eso se
        // rechazan los dígitos suplementarios aunque Unicode los clasifique Nd.
        appendUtf8CodePoint(text,ch);
    }
    return text;
}
std::string decimalAscii(const std::u16string& utf16) {
    std::string text;
    text.reserve(utf16.size());
    for (char16_t ch : utf16) appendUtf8CodePoint(text,ch);
    return text;
}
std::string decimalSlice(const std::vector<char16_t>& text, std::int32_t offset, std::int32_t length) {
    if (offset<0 || length<0 || static_cast<std::uint64_t>(offset)+static_cast<std::uint64_t>(length)>text.size())
        throw NumberFormatError("Bad offset or len arguments for char[] input.");
    return decimalAscii(std::u16string(text.begin()+offset,text.begin()+offset+length));
}
Magnitude unsignedMagnitude(std::uint64_t value) {
    Magnitude result;
    while (value) { result.push_back(static_cast<std::uint32_t>(value%kBase)); value/=kBase; }
    return result;
}
std::uint64_t binaryBits(double value) {
    std::uint64_t bits;
    std::memcpy(&bits,&value,sizeof(bits));
    return bits;
}
const char* roundingName(RoundingMode mode) {
    constexpr const char* names[]={"UP","DOWN","CEILING","FLOOR","HALF_UP","HALF_DOWN","HALF_EVEN","UNNECESSARY"};
    validateRoundingMode(mode);
    return names[static_cast<unsigned>(mode)];
}
std::pair<std::int32_t,RoundingMode> parseContext(const std::string& text) {
    constexpr std::string_view prefix="precision=";
    constexpr std::string_view middle=" roundingMode=";
    const auto end=text.find(middle);
    if (text.compare(0,prefix.size(),prefix)!=0 || end==std::string::npos)
        throw InvalidArgumentError("bad string format");
    auto digits=std::string_view(text).substr(prefix.size(),end-prefix.size());
    if (!digits.empty() && digits.front()=='+') digits.remove_prefix(1);
    std::int32_t precision=0;
    const auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),precision);
    if (digits.empty() || parsed.ec!=std::errc{} || parsed.ptr!=digits.data()+digits.size())
        throw InvalidArgumentError("bad string format");
    const auto name=text.substr(end+middle.size());
    for (unsigned i=0;i<8;++i) if (name==roundingName(static_cast<RoundingMode>(i)))
        return precision<0 ? throw InvalidArgumentError("Digits < 0") : std::pair<std::int32_t,RoundingMode>{precision,static_cast<RoundingMode>(i)};
    throw InvalidArgumentError("bad string format");
}

// Conversión de decimal a binario con redondeo correcto mediante cociente y resto.
// También gestiona subnormales y desbordamientos con signo, sin depender de
// la configuración regional ni de un double intermedio si el destino es float.
template<class Float, class Bits, unsigned P, int Emin, int Emax, unsigned Fraction, int Bias>
Float decimalToBinary(const Magnitude& magnitude, std::int32_t scale, int sign) {
    constexpr unsigned width=sizeof(Bits)*8;
    Bits bits=sign<0 ? (Bits{1}<<(width-1)) : 0;
    const auto pack=[&](Bits payload) {
        Float value;
        payload |= bits;
        std::memcpy(&value,&payload,sizeof(value));
        return value;
    };
    if (magnitude.empty()) return pack(0);
    const auto adjusted=static_cast<std::int64_t>(decimalDigits(magnitude))-scale-1;
    // Los límites amplios permiten tratar exactamente todos los puntos medios
    // de desbordamiento y subdesbordamiento.
    if (adjusted > (Emax==1023 ? 309 : 39)) return pack(static_cast<Bits>(2*Bias+1)<<Fraction);
    if (adjusted < (Emin==-1022 ? -325 : -46)) return pack(0);
    auto numerator=magnitude;
    Magnitude denominator{1};
    if (scale>0) denominator=powerSmall(10,static_cast<std::uint64_t>(scale));
    else if (scale<0) numerator=multiplyPowerOfTen(numerator,static_cast<std::uint64_t>(-static_cast<std::int64_t>(scale)));
    auto exponent=static_cast<int>(std::floor(
        (static_cast<std::int64_t>(decimalDigits(numerator))-static_cast<std::int64_t>(decimalDigits(denominator))) * 3.32192809488736234787L));
    const auto comparePower=[&](int e) {
        return e>=0 ? compare(numerator,multiplyMagnitude(denominator,powerSmall(2,static_cast<unsigned>(e))))
                    : compare(multiplyMagnitude(numerator,powerSmall(2,static_cast<unsigned>(-e))),denominator);
    };
    while (comparePower(exponent)<0) --exponent;
    while (comparePower(exponent+1)>=0) ++exponent;
    const bool subnormal=exponent<Emin;
    const auto shift=static_cast<int>(P-1)-std::max(exponent,Emin);
    if (shift>=0) numerator=multiplyMagnitude(numerator,powerSmall(2,static_cast<unsigned>(shift)));
    else denominator=multiplyMagnitude(denominator,powerSmall(2,static_cast<unsigned>(-shift)));
    auto [q,r]=divideMagnitude(numerator,denominator);
    if (shouldIncrement(RoundingMode::HalfEven,1,q,r,denominator)) q=addMagnitude(q,Magnitude{1});
    std::uint64_t coefficient=0;
    for (auto i=q.size();i!=0;--i) coefficient=coefficient*kBase+q[i-1];
    if (subnormal) return pack(static_cast<Bits>(coefficient));
    if (coefficient>=(std::uint64_t{1}<<P)) { coefficient>>=1; ++exponent; }
    if (exponent>Emax) return pack(static_cast<Bits>(2*Bias+1)<<Fraction);
    return pack((static_cast<Bits>(exponent+Bias)<<Fraction) |
                static_cast<Bits>(coefficient-(std::uint64_t{1}<<(P-1))));
}

} // espacio de nombres anónimo

BigDecimal::BigDecimal(const std::string& utf8, const MathContext& context) {
    const auto text = decimalAscii(utf8);
    if (text.empty()) throw NumberFormatError();
    std::size_t position = 0;
    if (text[position] == '+' || text[position] == '-') {
        sign_ = text[position] == '-' ? -1 : 1;
        ++position;
    } else {
        sign_ = 1;
    }

    std::string digits;
    digits.reserve(text.size());
    std::size_t fractionalDigits = 0;
    bool decimalPoint = false, sawDigit = false, sawExponent = false;
    std::size_t exponentPosition = text.size();
    for (; position < text.size(); ++position) {
        const char ch = text[position];
        if (ch == 'e' || ch == 'E') {
            sawExponent = true;
            exponentPosition = position + 1;
            break;
        }
        if (ch == '.') {
            if (decimalPoint) throw NumberFormatError("Character array contains more than one decimal point.");
            decimalPoint = true;
        } else if (ch >= '0' && ch <= '9') {
            digits.push_back(ch);
            sawDigit = true;
            if (decimalPoint) ++fractionalDigits;
        } else {
            std::size_t end=position+1;
            while(end<text.size() && (static_cast<unsigned char>(text[end])&0xc0)==0x80) ++end;
            std::size_t units=0;
            for(const auto byte:text) if ((static_cast<unsigned char>(byte)&0xc0)!=0x80) units+=static_cast<unsigned char>(byte)>=0xf0 ? 2 : 1;
            auto character=text.substr(position,end-position);
            if (static_cast<unsigned char>(ch)>=0xf0) {
                char32_t cp=static_cast<unsigned char>(ch)&7;
                for(auto i=position+1;i<end;++i) cp=(cp<<6)|(static_cast<unsigned char>(text[i])&63);
                const auto high=static_cast<char32_t>(0xd800+((cp-0x10000)>>10));
                character.clear();appendUtf8CodePoint(character,high);
            }
            throw NumberFormatError(units-(text.front()=='+'||text.front()=='-')<=18 ? std::string("Character ")+character+" is neither a decimal digit number, decimal point, nor \"e\" notation exponential mark." : "Character array is missing \"e\" notation exponential mark.");
        }
    }
    const auto exponent = sawExponent
        ? parseExponent(std::string_view(text).substr(exponentPosition)) : 0;
    if (!sawDigit) throw NumberFormatError("No digits found.");
    if (fractionalDigits > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()))
        throw NumberFormatError("Exponent overflow.");
    const auto fraction = static_cast<std::int64_t>(fractionalDigits);
    if (exponent < 0 && fraction > std::numeric_limits<std::int64_t>::max() + exponent)
        throw NumberFormatError("Exponent overflow.");
    auto rawScale = fraction - exponent;
    magnitude_ = parseMagnitude(digits);
    if (magnitude_.empty()) sign_ = 0;
    // El analizador redondea mientras la escala aún es long. Comprobar primero
    // la escala int32 rechazaría erróneamente, por ejemplo, 100E-2147483648 con p=1.
    const auto significant = static_cast<std::size_t>(context.precision);
    if (significant && precisionSize() > significant) {
        const auto drop = precisionSize() - significant;
        const auto divisor = powerSmall(10, drop);
        auto [quotient, remainder] = divideMagnitude(magnitude_, divisor);
        if (shouldIncrement(context.roundingMode, sign_, quotient, remainder, divisor))
            quotient = addMagnitude(quotient, Magnitude{1});
        rawScale -= static_cast<std::int64_t>(drop);
        while (decimalDigits(quotient) > significant) {
            quotient = divideSmall(quotient, 10).first;
            --rawScale;
        }
        magnitude_ = std::move(quotient);
    }
    if (rawScale<std::numeric_limits<std::int32_t>::min() || rawScale>std::numeric_limits<std::int32_t>::max())
        throw NumberFormatError("Exponent overflow.");
    scale_ = static_cast<std::int32_t>(rawScale);
}

BigDecimal::BigDecimal(std::vector<std::uint32_t> magnitude, std::int32_t scale, int sign)
    : magnitude_(std::move(magnitude)), scale_(scale), sign_(sign) {
    normalize(magnitude_);
    sign_ = magnitude_.empty() ? 0 : (sign_ < 0 ? -1 : 1);
}

BigDecimal::BigDecimal(BigDecimal&& other) noexcept
    : magnitude_(std::move(other.magnitude_)),
      scale_(std::exchange(other.scale_, 0)), sign_(std::exchange(other.sign_, 0)) {
    other.magnitude_.clear();
}

BigDecimal& BigDecimal::operator=(BigDecimal&& other) noexcept {
    if (this != &other) {
        magnitude_ = std::move(other.magnitude_);
        scale_ = std::exchange(other.scale_, 0);
        sign_ = std::exchange(other.sign_, 0);
        other.magnitude_.clear();
    }
    return *this;
}

std::int32_t BigDecimal::precision() const noexcept {
    return static_cast<std::int32_t>(precisionSize());
}

std::size_t BigDecimal::precisionSize() const noexcept { return decimalDigits(magnitude_); }

BigDecimal BigDecimal::abs() const { return BigDecimal(magnitude_, scale_, sign_ == 0 ? 0 : 1); }

BigDecimal BigDecimal::operator-() const { return BigDecimal(magnitude_, scale_, -sign_); }

BigDecimal BigDecimal::operator+(const BigDecimal& rhs) const {
    const auto resultScale = std::max(scale_, rhs.scale_);
    if (sign_) checkedScale(static_cast<std::int64_t>(resultScale) - scale_);
    if (rhs.sign_) checkedScale(static_cast<std::int64_t>(resultScale) - rhs.scale_);
    const auto lhs = multiplyPowerOfTen(magnitude_, static_cast<std::uint64_t>(
        static_cast<std::int64_t>(resultScale) - scale_));
    const auto right = multiplyPowerOfTen(rhs.magnitude_, static_cast<std::uint64_t>(
        static_cast<std::int64_t>(resultScale) - rhs.scale_));
    if (sign_ == 0) return BigDecimal(right, resultScale, rhs.sign_);
    if (rhs.sign_ == 0) return BigDecimal(lhs, resultScale, sign_);
    if (sign_ == rhs.sign_) return BigDecimal(addMagnitude(lhs, right), resultScale, sign_);
    const int order = compare(lhs, right);
    if (order == 0) return BigDecimal(Magnitude{}, resultScale, 0);
    return order > 0 ? BigDecimal(subtractMagnitude(lhs, right), resultScale, sign_)
                     : BigDecimal(subtractMagnitude(right, lhs), resultScale, rhs.sign_);
}

BigDecimal BigDecimal::operator-(const BigDecimal& rhs) const { return *this + (-rhs); }

BigDecimal BigDecimal::operator*(const BigDecimal& rhs) const {
    const auto sumScale = static_cast<std::int64_t>(scale_) + rhs.scale_;
    const auto resultScale = sign_ == 0 ? saturatedScale(sumScale) : checkedScale(sumScale);
    return BigDecimal(multiplyMagnitude(magnitude_, rhs.magnitude_), resultScale, sign_ * rhs.sign_);
}

BigDecimal BigDecimal::operator/(const BigDecimal& rhs) const {
    if (rhs.sign_ == 0) throw ArithmeticError(sign_==0 ? "Division undefined" : "Division by zero");
    const auto preferredScale = saturatedScale(static_cast<std::int64_t>(scale_) - rhs.scale_);
    if (sign_ == 0) return BigDecimal(Magnitude{}, preferredScale, 0);

    // Basta un límite finito de precisión para la división exacta:
    // un cociente terminante necesita como máximo p(a) + ceil(10*p(b)/3) cifras.
    const auto bound = std::min<std::uint64_t>(
        static_cast<std::uint64_t>(precisionSize()) +
        (10 * static_cast<std::uint64_t>(rhs.precisionSize()) + 2) / 3,
        static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()));
    BigDecimal result;
    try { result = divide(rhs, static_cast<std::size_t>(bound), RoundingMode::Unnecessary); }
    catch (const ArithmeticError&) { throw ArithmeticError("Non-terminating decimal expansion; no exact representable decimal result."); }
    if (result.scale_ < preferredScale) result = result.setScale(preferredScale);
    return result;
}

BigDecimal BigDecimal::round(std::size_t significantDigits, RoundingMode mode) const {
    validatePrecision(significantDigits);
    validateRoundingMode(mode);
    if (significantDigits == 0) return *this;
    if (sign_ == 0 || precisionSize() <= significantDigits) return *this;
    const auto drop = precisionSize() - significantDigits;
    auto resultScale = checkedScaleNonZero(static_cast<std::int64_t>(scale_) - static_cast<std::int64_t>(drop));
    const auto divisor = powerSmall(10, drop);
    auto [quotient, remainder] = divideMagnitude(magnitude_, divisor);
    if (shouldIncrement(mode, sign_, quotient, remainder, divisor)) quotient = addMagnitude(quotient, Magnitude{1});
    while (!quotient.empty() && decimalDigits(quotient) > significantDigits) {
        auto divided = divideSmall(quotient, 10);
        if (divided.second != 0) break;
        quotient = std::move(divided.first);
        resultScale = checkedScaleNonZero(static_cast<std::int64_t>(resultScale) - 1);
    }
    return BigDecimal(std::move(quotient), resultScale, sign_);
}

BigDecimal BigDecimal::divide(const BigDecimal& divisor, std::size_t significantDigits,
                              RoundingMode mode) const {
    validatePrecision(significantDigits);
    validateRoundingMode(mode);
    if (divisor.sign_ == 0) throw ArithmeticError(sign_==0 ? "Division undefined" : "Division by zero");
    if (sign_ == 0) return BigDecimal(Magnitude{}, saturatedScale(static_cast<std::int64_t>(scale_) - divisor.scale_), 0);
    if (significantDigits == 0) return *this / divisor;

    const auto aDigits = static_cast<std::int64_t>(decimalDigits(magnitude_));
    const auto bDigits = static_cast<std::int64_t>(decimalDigits(divisor.magnitude_));
    std::int64_t ratioExponent = aDigits - bDigits;
    const int ratioOrder = ratioExponent >= 0
        ? compare(magnitude_, multiplyPowerOfTen(divisor.magnitude_, static_cast<std::uint64_t>(ratioExponent)))
        : compare(multiplyPowerOfTen(magnitude_, static_cast<std::uint64_t>(-ratioExponent)), divisor.magnitude_);
    if (ratioOrder < 0) --ratioExponent;
    const std::int64_t valueExponent = ratioExponent + divisor.scale_ - scale_;
    const auto targetScale = checkedScaleNonZero(static_cast<std::int64_t>(significantDigits) - 1 - valueExponent);
    // Normaliza el cociente exacto de significandos con una cifra decimal extra
    // antes de quitar ceros finales, para conservar la escala preferida.
    if (ratioOrder == 0) checkedScaleNonZero(static_cast<std::int64_t>(targetScale) + 1);
    const bool equalFastPath = ratioOrder == 0 && aDigits <= bDigits && bDigits < 18 && significantDigits < 18;
    if (equalFastPath) checkedScaleNonZero(static_cast<std::int64_t>(scale_) - divisor.scale_);
    const std::int64_t shift = static_cast<std::int64_t>(targetScale) + divisor.scale_ - scale_;
    const auto scaledNumerator = shift >= 0
        ? multiplyPowerOfTen(magnitude_, static_cast<std::uint64_t>(shift)) : magnitude_;
    const auto scaledDivisor = shift < 0
        ? multiplyPowerOfTen(divisor.magnitude_, static_cast<std::uint64_t>(-shift)) : divisor.magnitude_;
    if (!equalFastPath) checkedScaleNonZero(static_cast<std::int64_t>(scale_) - divisor.scale_);
    auto [quotient, remainder] = divideMagnitude(scaledNumerator, scaledDivisor);
    if (shouldIncrement(mode, sign_ * divisor.sign_, quotient, remainder, scaledDivisor))
        quotient = addMagnitude(quotient, Magnitude{1});
    auto resultScale = targetScale;
    while (!quotient.empty() && decimalDigits(quotient) > significantDigits) {
        auto divided = divideSmall(quotient, 10);
        if (divided.second != 0) break;
        quotient = std::move(divided.first);
        resultScale = checkedScaleNonZero(static_cast<std::int64_t>(resultScale) - 1);
    }
    if (remainder.empty()) {
        const auto preferred = static_cast<std::int64_t>(scale_) - divisor.scale_;
        while (!quotient.empty() && resultScale > preferred) {
            auto reduced = divideSmall(quotient, 10);
            if (reduced.second) break;
            quotient = std::move(reduced.first);
            resultScale = checkedScaleNonZero(static_cast<std::int64_t>(resultScale) - 1);
        }
    }
    return BigDecimal(std::move(quotient), resultScale, sign_ * divisor.sign_);
}

bool BigDecimal::operator==(const BigDecimal& rhs) const { return equals(rhs); }

bool BigDecimal::operator<(const BigDecimal& rhs) const {
    if (sign_ != rhs.sign_) return sign_ < rhs.sign_;
    if (sign_ == 0) return false;
    const auto lhsDigits = static_cast<std::int64_t>(decimalDigits(magnitude_));
    const auto rhsDigits = static_cast<std::int64_t>(decimalDigits(rhs.magnitude_));
    const auto lhsAdjusted = lhsDigits - scale_;
    const auto rhsAdjusted = rhsDigits - rhs.scale_;
    int order = 0;
    if (lhsAdjusted != rhsAdjusted) {
        order = lhsAdjusted < rhsAdjusted ? -1 : 1;
    } else {
        const auto commonScale = std::max(scale_, rhs.scale_);
        order = compare(multiplyPowerOfTen(magnitude_, static_cast<std::uint64_t>(
                           static_cast<std::int64_t>(commonScale) - scale_)),
                        multiplyPowerOfTen(rhs.magnitude_, static_cast<std::uint64_t>(
                           static_cast<std::int64_t>(commonScale) - rhs.scale_)));
    }
    return sign_ > 0 ? order < 0 : order > 0;
}

std::string BigDecimal::toPlainString() const {
    if (sign_ == 0 && scale_ < 0) return "0";
    std::string digits = magnitudeDigits(magnitude_);
    std::string result = sign_ < 0 ? "-" : "";
    const std::int64_t scale = scale_;
    if (scale <= 0) {
        if (scale<0) checkedScaleNonZero(-scale);
        if (static_cast<std::uint64_t>(digits.size())+static_cast<std::uint64_t>(-scale)+(sign_<0)>2147483647ULL)
            throw AllocationError("too large to fit in a String");
        result += digits;
        result.append(static_cast<std::size_t>(-scale), '0');
    } else if (digits.size() > static_cast<std::size_t>(scale)) {
        const auto point = digits.size() - static_cast<std::size_t>(scale);
        result.append(digits, 0, point);
        result.push_back('.');
        result.append(digits, point, std::string::npos);
    } else {
        if (static_cast<std::uint64_t>(scale)+2+(sign_<0)>2147483647ULL)
            throw AllocationError("too large to fit in a String");
        result += "0.";
        result.append(static_cast<std::size_t>(scale) - digits.size(), '0');
        result += digits;
    }
    return result;
}

std::string BigDecimal::toString() const {
    const auto digits = magnitudeDigits(magnitude_);
    const auto adjustedExponent = static_cast<std::int64_t>(digits.size()) - scale_ - 1;
    if (scale_ >= 0 && adjustedExponent >= -6) return toPlainString();

    std::string result = sign_ < 0 ? "-" : "";
    result.push_back(digits.front());
    if (digits.size() > 1) {
        result.push_back('.');
        result.append(digits.begin() + 1, digits.end());
    }
    result.push_back('E');
    if (adjustedExponent >= 0) result.push_back('+');
    result += std::to_string(adjustedExponent);
    return result;
}

MathContext::MathContext(std::int32_t digits, RoundingMode mode)
    : precision(digits), roundingMode(mode) {
    if (digits < 0) throw InvalidArgumentError("Digits < 0");
    validateRoundingMode(mode);
}

BigDecimal BigDecimal::round(const MathContext& context) const {
    return round(static_cast<std::size_t>(context.precision), context.roundingMode);
}

BigDecimal BigDecimal::add(const BigDecimal& rhs, const MathContext& context) const {
    if (context.precision == 0) return *this + rhs;
    if (sign_ == 0 || rhs.sign_ == 0) {
        const auto preferred = std::max(scale_, rhs.scale_);
        if (sign_ == 0 && rhs.sign_ == 0) return BigDecimal(Magnitude{}, preferred, 0);
        auto result = (sign_ == 0 ? rhs : *this).round(context);
        if (result.scale_ > preferred) {
            while (result.scale_ > preferred) {
                auto reduced = divideSmall(result.magnitude_, 10);
                if (reduced.second) break;
                result.magnitude_ = std::move(reduced.first);
                --result.scale_;
            }
        } else {
            checkedScale(static_cast<std::int64_t>(preferred) - result.scale_);
            const auto extra = static_cast<std::int64_t>(context.precision) -
                               static_cast<std::int64_t>(result.precisionSize());
            const auto target = std::min(static_cast<std::int64_t>(preferred),
                                        static_cast<std::int64_t>(result.scale_) + extra);
            result = result.setScale(checkedScale(target));
        }
        return result;
    }
    // Un operando pequeño, separado e invisible se convierte en un dígito
    // residual con signo. Así se conserva el redondeo dirigido sin reservar
    // miles de millones de ceros.
    const BigDecimal* big = scale_ < rhs.scale_ ? this : &rhs;
    const BigDecimal* small = scale_ < rhs.scale_ ? &rhs : this;
    const auto ulpScale = static_cast<std::int64_t>(big->scale_) -
                          static_cast<std::int64_t>(big->precisionSize()) + context.precision;
    const auto smallHigh = static_cast<std::int64_t>(small->scale_) -
                           static_cast<std::int64_t>(small->precisionSize()) + 1;
    if (smallHigh > static_cast<std::int64_t>(big->scale_) + 2 && smallHigh > ulpScale + 2) {
        const auto stickyScale = checkedScale(std::max(static_cast<std::int64_t>(big->scale_), ulpScale) + 3);
        return (*big + BigDecimal(Magnitude{1}, stickyScale, small->sign_)).round(context);
    }
    return (*this + rhs).round(context);
}

BigDecimal BigDecimal::subtract(const BigDecimal& rhs, const MathContext& context) const {
    return add(-rhs, context);
}

BigDecimal BigDecimal::multiply(const BigDecimal& rhs, const MathContext& context) const {
    return (*this * rhs).round(context);
}

BigDecimal BigDecimal::divide(const BigDecimal& rhs, const MathContext& context) const {
    return divide(rhs, static_cast<std::size_t>(context.precision), context.roundingMode);
}

BigDecimal BigDecimal::setScale(std::int32_t target, RoundingMode mode) const {
    validateRoundingMode(mode);
    if (target == scale_) return *this;
    if (sign_ == 0) return BigDecimal(Magnitude{}, target, 0);
    const auto shift = static_cast<std::int64_t>(target) - scale_;
    checkedScale(shift >= 0 ? shift : -shift);
    if (shift >= 0)
        return BigDecimal(multiplyPowerOfTen(magnitude_, static_cast<std::uint64_t>(shift)), target, sign_);
    const auto drop = static_cast<std::uint64_t>(-shift);
    // Construye el factor de escala antes de dividir, incluso si el cociente
    // es cero, para comprobar la magnitud siempre en el mismo orden.
    if (drop >= kMaxCoefficientDigits)
        throw ArithmeticError("BigInteger would overflow supported range");
    // Si se descartan más cifras que las que tiene el coeficiente, la fracción es < 1/2.
    if (drop > precisionSize()) {
        const bool increment = shouldIncrement(mode, sign_, {}, Magnitude{1}, Magnitude{10});
        return BigDecimal(increment ? Magnitude{1} : Magnitude{}, target, sign_);
    }
    const auto denominator = powerSmall(10, drop);
    auto [quotient, remainder] = divideMagnitude(magnitude_, denominator);
    if (shouldIncrement(mode, sign_, quotient, remainder, denominator))
        quotient = addMagnitude(quotient, Magnitude{1});
    return BigDecimal(std::move(quotient), target, sign_);
}

BigDecimal BigDecimal::divideToScale(const BigDecimal& rhs, std::int32_t target, RoundingMode mode) const {
    validateRoundingMode(mode);
    // Comprueba las escalas intermedias antes de aplicar la diferencia de
    // potencias con signo de 32 bits, incluido su caso de desbordamiento circular.
    const auto wrap32 = [](std::int64_t value) -> std::int32_t {
        const auto bits = static_cast<std::uint32_t>(value);
        return bits <= 0x7fffffffU ? static_cast<std::int32_t>(bits)
            : static_cast<std::int32_t>(static_cast<std::int64_t>(bits) - 0x100000000LL);
    };
    const auto scaleFor = [](int sign, std::int64_t value) {
        return sign == 0 ? saturatedScale(value) : checkedScale(value);
    };
    const auto raised = [&](const Magnitude& value, std::int32_t power) {
        if (power <= 0) return value;
        // Construye el factor de escala antes de multiplicar por cero cuando
        // la potencia es al menos 19, para mantener el orden de comprobación.
        if (value.empty() && power >= 19 && static_cast<std::uint64_t>(power) > kMaxCoefficientDigits)
            throw ArithmeticError("BigInteger would overflow supported range");
        return multiplyPowerOfTen(value, static_cast<std::uint64_t>(power));
    };
    auto numerator = magnitude_;
    auto denominator = rhs.magnitude_;
    const auto fitsCompact = [](const Magnitude& value) {
        const auto digits=magnitudeDigits(value);
        return digits.size()<19 || (digits.size()==19 && digits<="9223372036854775807");
    };
    bool compactDivision=fitsCompact(numerator) && fitsCompact(denominator);
    const auto adjusted = scaleFor(sign_, static_cast<std::int64_t>(target) + rhs.scale_);
    if (adjusted > scale_) {
        const auto power = wrap32(static_cast<std::int64_t>(target) + rhs.scale_ - scale_);
        numerator = raised(numerator, power);
        compactDivision=compactDivision && power<19 && fitsCompact(numerator);
    } else {
        const auto adjustedDenominator = scaleFor(rhs.sign_, static_cast<std::int64_t>(scale_) - target);
        const auto power = wrap32(static_cast<std::int64_t>(adjustedDenominator) - rhs.scale_);
        denominator = raised(denominator, power);
        compactDivision=compactDivision && power<19 && fitsCompact(denominator);
    }
    if (denominator.empty()) throw ArithmeticError(compactDivision ? "/ by zero" : "BigInteger divide by zero");
    auto [quotient, remainder] = divideMagnitude(numerator, denominator);
    if (shouldIncrement(mode, sign_ * rhs.sign_, quotient, remainder, denominator))
        quotient = addMagnitude(quotient, Magnitude{1});
    return BigDecimal(std::move(quotient), target, sign_ * rhs.sign_);
}

BigDecimal BigDecimal::stripTrailingZeros() const {
    if (sign_ == 0) return BigDecimal();
    auto result = *this;
    for (;;) {
        auto reduced = divideSmall(result.magnitude_, 10);
        if (reduced.second) return result;
        result.scale_ = checkedScale(static_cast<std::int64_t>(result.scale_) - 1);
        result.magnitude_ = std::move(reduced.first);
    }
}

int BigDecimal::compareTo(const BigDecimal& rhs) const {
    return *this < rhs ? -1 : (rhs < *this ? 1 : 0);
}

bool BigDecimal::equals(const BigDecimal& rhs) const noexcept {
    return sign_ == rhs.sign_ && scale_ == rhs.scale_ && magnitude_ == rhs.magnitude_;
}

const BigDecimal BigDecimal::ZERO("0");
const BigDecimal BigDecimal::ONE("1");
const BigDecimal BigDecimal::TWO("2");
const BigDecimal BigDecimal::TEN("10");
const MathContext MathContext::UNLIMITED(0, RoundingMode::HalfUp);
const MathContext MathContext::DECIMAL32(7, RoundingMode::HalfEven);
const MathContext MathContext::DECIMAL64(16, RoundingMode::HalfEven);
const MathContext MathContext::DECIMAL128(34, RoundingMode::HalfEven);

MathContext::MathContext(const std::string& text)
    : MathContext(parseContext(text).first,parseContext(text).second) {}
std::string MathContext::toString() const {
    return "precision="+std::to_string(precision)+" roundingMode="+roundingName(roundingMode);
}
bool MathContext::equals(const MathContext& rhs) const noexcept {
    return precision==rhs.precision && roundingMode==rhs.roundingMode;
}
std::int32_t MathContext::hashCode() const noexcept {
    // Los hashes basados en la identidad de un enum dependen del proceso.
    // Usa un hash estable y conserva el contrato de igualdad/hash de MathContext.
    const auto bits=static_cast<std::uint32_t>(precision)+59U*static_cast<std::uint32_t>(roundingMode);
    return static_cast<std::int32_t>(bits<=0x7fffffffU ? static_cast<std::int64_t>(bits) : static_cast<std::int64_t>(bits)-4294967296LL);
}

BigDecimal::BigDecimal(const char* text)
    : BigDecimal(text ? std::string(text) : throw NullPointerError("Cannot invoke \"String.toCharArray()\" because \"val\" is null")) {}
BigDecimal::BigDecimal(const std::string& text)
    : BigDecimal(text,MathContext(0)) {}
BigDecimal::BigDecimal(std::int32_t value) : BigDecimal(static_cast<std::int64_t>(value)) {}
BigDecimal::BigDecimal(std::int64_t value) : BigDecimal(std::to_string(value)) {}
BigDecimal::BigDecimal(std::int32_t value, const MathContext& context)
    : BigDecimal(BigDecimal(value).round(context)) {}
BigDecimal::BigDecimal(std::int64_t value, const MathContext& context)
    : BigDecimal(BigDecimal(value).round(context)) {}
BigDecimal::BigDecimal(const BigInteger& value) : BigDecimal(value,0) {}
BigDecimal::BigDecimal(const BigInteger& value, const MathContext& context)
    : BigDecimal(BigDecimal(value).round(context)) {}
BigDecimal::BigDecimal(const BigInteger& value, std::int32_t scale)
    : BigDecimal(value.magnitude_,scale,value.sign_) {}
BigDecimal::BigDecimal(const BigInteger& value, std::int32_t scale, const MathContext& context)
    : BigDecimal(BigDecimal(value,scale).round(context)) {}
BigDecimal::BigDecimal(const std::u16string& text) : BigDecimal(decimalAscii(text)) {}
BigDecimal::BigDecimal(const std::u16string& text, const MathContext& context)
    : BigDecimal(decimalAscii(text),context) {}
BigDecimal::BigDecimal(const std::vector<char16_t>& text)
    : BigDecimal(std::u16string(text.begin(),text.end())) {}
BigDecimal::BigDecimal(const std::vector<char16_t>& text, const MathContext& context)
    : BigDecimal(std::u16string(text.begin(),text.end()),context) {}
BigDecimal::BigDecimal(const std::vector<char16_t>& text, std::int32_t offset, std::int32_t length)
    : BigDecimal(text,offset,length,MathContext(0)) {}
BigDecimal::BigDecimal(const std::vector<char16_t>& text, std::int32_t offset, std::int32_t length, const MathContext& context)
    : BigDecimal() {
    const auto slice=decimalSlice(text,offset,length);
    if (length==0 && static_cast<std::size_t>(offset)<text.size()) throw NumberFormatError("No digits found.");
    try { *this=BigDecimal(slice,context); }
    catch(const NumberFormatError& e) {
        if (!e.hasMessage() && static_cast<std::size_t>(offset)+static_cast<std::size_t>(length)<text.size() &&
            (!slice.empty() && (slice.back()=='e'||slice.back()=='E'||slice.back()=='+'||slice.back()=='-')))
            throw NumberFormatError("No exponent digits.");
        throw;
    }
}

BigDecimal::BigDecimal(double value) {
    static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559,"IEEE 754 binary64 is required");
    const auto bits=binaryBits(value);
    const auto biased=static_cast<unsigned>((bits>>52)&0x7ff);
    if (biased==0x7ff) throw NumberFormatError("Infinite or NaN");
    auto coefficient=bits & ((std::uint64_t{1}<<52)-1);
    auto exponent=biased ? static_cast<int>(biased)-1075 : -1074;
    if (biased) coefficient |= std::uint64_t{1}<<52;
    if (!coefficient) return;
    while ((coefficient&1U)==0) { coefficient>>=1; ++exponent; }
    sign_=(bits>>63) ? -1 : 1;
    magnitude_=unsignedMagnitude(coefficient);
    if (exponent<0) {
        scale_=-exponent;
        magnitude_=multiplyMagnitude(magnitude_,powerSmall(5,static_cast<unsigned>(-exponent)));
    } else magnitude_=multiplyMagnitude(magnitude_,powerSmall(2,static_cast<unsigned>(exponent)));
}
BigDecimal::BigDecimal(double value, const MathContext& context)
    : BigDecimal(BigDecimal(value).round(context)) {}
BigDecimal BigDecimal::valueOf(std::int64_t value, std::int32_t scale) {
    auto result=BigDecimal(value);
    result.scale_=scale;
    return result;
}

BigDecimal BigDecimal::valueOf(double value) {
    if (!std::isfinite(value)) return BigDecimal(std::isnan(value) ? "NaN" : (std::signbit(value) ? "-Infinity" : "Infinity"));
    if (value==0.0) return BigDecimal("0.0");
    const auto exact=BigDecimal(value).abs();
    const auto target=binaryBits(std::abs(value));
    const auto adjusted=static_cast<std::int64_t>(exact.precisionSize())-exact.scale_-1;
    BigDecimal shortest;
    bool found=false;
    // El formato más corto de ida y vuelta elige el decimal más cercano con al
    // menos dos cifras. También prueba los valores vecinos: los intervalos
    // binarios son asimétricos en las potencias de dos. Todas las comparaciones
    // siguientes son comparaciones exactas entre enteros.
    for (std::size_t digits=2; digits<=17 && !found; ++digits) {
        const auto nearest=exact.round(digits,RoundingMode::HalfEven);
        const auto step=BigDecimal(Magnitude{1},checkedScale(static_cast<std::int64_t>(digits)-1-adjusted),1);
        BigDecimal bestDistance;
        for (const auto& candidate : {nearest,nearest-step,nearest+step}) {
            if (candidate.sign_<=0 || candidate.stripTrailingZeros().precisionSize()>digits || binaryBits(candidate.doubleValue())!=target) continue;
            const auto distance=(candidate-exact).abs();
            if (!found || distance<bestDistance ||
                (distance==bestDistance && !candidate.magnitude_.empty() && candidate.magnitude_.front()%2==0)) {
                shortest=candidate;
                bestDistance=distance;
                found=true;
            }
        }
    }
    if (!found) throw std::logic_error("no binary64 decimal representation");
    shortest=shortest.stripTrailingZeros();
    const auto exponent=static_cast<std::int64_t>(shortest.precisionSize())-shortest.scale_-1;
    if (exponent>=-3 && exponent<7) {
        const auto text=shortest.toPlainString();
        shortest=BigDecimal(text.find('.')==std::string::npos ? text+".0" : text);
    } else if (shortest.precisionSize()==1) {
        shortest=shortest.setScale(checkedScale(static_cast<std::int64_t>(shortest.scale_)+1));
    }
    return std::signbit(value) ? -shortest : shortest;
}

double BigDecimal::doubleValue() const {
    return decimalToBinary<double,std::uint64_t,53,-1022,1023,52,1023>(magnitude_,scale_,sign_);
}
float BigDecimal::floatValue() const {
    static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559,"IEEE 754 binary32 is required");
    return decimalToBinary<float,std::uint32_t,24,-126,127,23,127>(magnitude_,scale_,sign_);
}
BigDecimal BigDecimal::abs(const MathContext& context) const { return abs().round(context); }
BigDecimal BigDecimal::negate(const MathContext& context) const { return (-*this).round(context); }
BigDecimal BigDecimal::min(const BigDecimal& rhs) const { return rhs<*this ? rhs : *this; }
BigDecimal BigDecimal::max(const BigDecimal& rhs) const { return *this<rhs ? rhs : *this; }
BigDecimal BigDecimal::ulp() const { return BigDecimal(Magnitude{1},scale_,1); }
BigDecimal BigDecimal::scaleByPowerOfTen(std::int32_t exponent) const {
    const auto scale=static_cast<std::int64_t>(scale_)-exponent;
    return BigDecimal(magnitude_,sign_ ? checkedScale(scale) : saturatedScale(scale),sign_);
}
BigDecimal BigDecimal::movePointLeft(std::int32_t places) const {
    const auto scale=static_cast<std::int64_t>(scale_)+places;
    auto result=BigDecimal(magnitude_,sign_ ? checkedScale(scale) : saturatedScale(scale),sign_);
    return result.scale_<0 ? result.setScale(0) : result;
}
BigDecimal BigDecimal::movePointRight(std::int32_t places) const {
    const auto scale=static_cast<std::int64_t>(scale_)-places;
    auto result=BigDecimal(magnitude_,sign_ ? checkedScale(scale) : saturatedScale(scale),sign_);
    return result.scale_<0 ? result.setScale(0) : result;
}
BigDecimal BigDecimal::divide(const BigDecimal& rhs, int legacyMode) const {
    if (legacyMode<0 || legacyMode>7) throw InvalidArgumentError("Invalid rounding mode");
    return divideToScale(rhs,scale_,static_cast<RoundingMode>(legacyMode));
}
BigDecimal BigDecimal::divide(const BigDecimal& rhs, std::int32_t scale, int legacyMode) const {
    if (legacyMode<0 || legacyMode>7) throw InvalidArgumentError("Invalid rounding mode");
    return divideToScale(rhs,scale,static_cast<RoundingMode>(legacyMode));
}
BigDecimal BigDecimal::setScale(std::int32_t scale, int legacyMode) const {
    if (legacyMode<0 || legacyMode>7) throw InvalidArgumentError("Invalid rounding mode");
    return setScale(scale,static_cast<RoundingMode>(legacyMode));
}

MathContext::MathContext(const char* text)
    : MathContext(text ? std::string(text) : throw NullPointerError("null String")) {}
BigDecimal::BigDecimal(const char* text, const MathContext& context)
    : BigDecimal(text ? std::string(text) : throw NullPointerError("Cannot invoke \"String.toCharArray()\" because \"val\" is null"), context) {}
