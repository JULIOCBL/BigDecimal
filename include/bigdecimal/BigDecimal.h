
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



#ifndef BIG_DECIMAL_H
#define BIG_DECIMAL_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include "BigInteger.h"

// Modos de redondeo decimal usados por MathContext y las operaciones de escala.
enum class RoundingMode {
    Up, Down, Ceiling, Floor, HalfUp, HalfDown, HalfEven, Unnecessary,
    UP = Up, DOWN = Down, CEILING = Ceiling, FLOOR = Floor,
    HALF_UP = HalfUp, HALF_DOWN = HalfDown, HALF_EVEN = HalfEven, UNNECESSARY = Unnecessary
};

struct MathContext {
    explicit MathContext(std::int32_t precision = 0,
                         RoundingMode roundingMode = RoundingMode::HalfUp);
    explicit MathContext(const std::string& text);
    explicit MathContext(const char* text);
    static const MathContext UNLIMITED, DECIMAL32, DECIMAL64, DECIMAL128;
    [[nodiscard]] std::int32_t getPrecision() const noexcept { return precision; }
    [[nodiscard]] RoundingMode getRoundingMode() const noexcept { return roundingMode; }
    [[nodiscard]] std::string toString() const;
    [[nodiscard]] bool equals(const MathContext& rhs) const noexcept;
    [[nodiscard]] std::int32_t hashCode() const noexcept;
    const std::int32_t precision;
    const RoundingMode roundingMode;
};

// Decimal de precisión arbitraria: signo * coeficiente * 10^(-escala).
// Los coeficientes usan bloques base 10^9 en orden little-endian,
// almacenados en un std::vector.
class BigDecimal {
public:
    static const BigDecimal ZERO, ONE, TWO, TEN;
    static constexpr int ROUND_UP = 0, ROUND_DOWN = 1, ROUND_CEILING = 2, ROUND_FLOOR = 3,
                         ROUND_HALF_UP = 4, ROUND_HALF_DOWN = 5, ROUND_HALF_EVEN = 6,
                         ROUND_UNNECESSARY = 7;
    BigDecimal() = default;
    explicit BigDecimal(const char* text);
    BigDecimal(const char* text, const MathContext& context);
    BigDecimal(const std::string& text, const MathContext& context);
    explicit BigDecimal(std::int32_t value);
    explicit BigDecimal(std::int64_t value);
    explicit BigDecimal(double value);
    BigDecimal(std::int32_t value, const MathContext& context);
    BigDecimal(std::int64_t value, const MathContext& context);
    BigDecimal(double value, const MathContext& context);
    explicit BigDecimal(const BigInteger& value);
    BigDecimal(const BigInteger& value, const MathContext& context);
    BigDecimal(const BigInteger& unscaled, std::int32_t scale);
    BigDecimal(const BigInteger& unscaled, std::int32_t scale, const MathContext& context);
    explicit BigDecimal(const std::u16string& text);
    BigDecimal(const std::u16string& text, const MathContext& context);
    explicit BigDecimal(const std::vector<char16_t>& text);
    BigDecimal(const std::vector<char16_t>& text, const MathContext& context);
    BigDecimal(const std::vector<char16_t>& text, std::int32_t offset, std::int32_t length);
    BigDecimal(const std::vector<char16_t>& text, std::int32_t offset, std::int32_t length, const MathContext& context);
    [[nodiscard]] static BigDecimal valueOf(std::int64_t value, std::int32_t scale = 0);
    [[nodiscard]] static BigDecimal valueOf(std::int32_t value) { return valueOf(static_cast<std::int64_t>(value)); }
    [[nodiscard]] static BigDecimal valueOf(double value);
    explicit BigDecimal(const std::string& text);
    BigDecimal(const BigDecimal&) = default;
    BigDecimal& operator=(const BigDecimal&) = default;
    // Los valores movidos quedan como cero canónico y siguen siendo utilizables.
    BigDecimal(BigDecimal&& other) noexcept;
    BigDecimal& operator=(BigDecimal&& other) noexcept;

    [[nodiscard]] std::int32_t scale() const noexcept { return scale_; }
    [[nodiscard]] int signum() const noexcept { return sign_; }
    [[nodiscard]] std::int32_t precision() const noexcept;

    [[nodiscard]] std::string toString() const;
    [[nodiscard]] std::string toPlainString() const;

    [[nodiscard]] BigDecimal abs() const;
    [[nodiscard]] BigDecimal abs(const MathContext& context) const;
    [[nodiscard]] BigDecimal plus() const { return *this; }
    [[nodiscard]] BigDecimal plus(const MathContext& context) const { return round(context); }
    [[nodiscard]] BigDecimal negate() const { return -*this; }
    [[nodiscard]] BigDecimal negate(const MathContext& context) const;
    [[nodiscard]] BigDecimal min(const BigDecimal& rhs) const;
    [[nodiscard]] BigDecimal max(const BigDecimal& rhs) const;
    [[nodiscard]] BigDecimal ulp() const;
    [[nodiscard]] BigDecimal movePointLeft(std::int32_t places) const;
    [[nodiscard]] BigDecimal movePointRight(std::int32_t places) const;
    [[nodiscard]] BigDecimal scaleByPowerOfTen(std::int32_t exponent) const;
    [[nodiscard]] BigDecimal pow(std::int32_t exponent) const;
    [[nodiscard]] BigDecimal pow(std::int32_t exponent, const MathContext& context) const;
    [[nodiscard]] BigDecimal sqrt(const MathContext& context) const;
    [[nodiscard]] BigDecimal divideToIntegralValue(const BigDecimal& rhs) const;
    [[nodiscard]] BigDecimal divideToIntegralValue(const BigDecimal& rhs, const MathContext& context) const;
    [[nodiscard]] BigDecimal remainder(const BigDecimal& rhs) const;
    [[nodiscard]] BigDecimal remainder(const BigDecimal& rhs, const MathContext& context) const;
    [[nodiscard]] std::pair<BigDecimal, BigDecimal> divideAndRemainder(const BigDecimal& rhs) const;
    [[nodiscard]] std::pair<BigDecimal, BigDecimal> divideAndRemainder(const BigDecimal& rhs, const MathContext& context) const;
    [[nodiscard]] std::string toEngineeringString() const;
    [[nodiscard]] std::int32_t hashCode() const;
    [[nodiscard]] BigInteger unscaledValue() const;
    [[nodiscard]] BigInteger toBigInteger() const;
    [[nodiscard]] BigInteger toBigIntegerExact() const;
    [[nodiscard]] std::int64_t longValue() const;
    [[nodiscard]] std::int64_t longValueExact() const;
    [[nodiscard]] std::int32_t intValue() const;
    [[nodiscard]] std::int32_t intValueExact() const;
    [[nodiscard]] std::int16_t shortValue() const;
    [[nodiscard]] std::int16_t shortValueExact() const;
    [[nodiscard]] std::int8_t byteValue() const;
    [[nodiscard]] std::int8_t byteValueExact() const;
    [[nodiscard]] double doubleValue() const;
    [[nodiscard]] float floatValue() const;
    [[nodiscard]] BigDecimal add(const BigDecimal& rhs) const { return *this + rhs; }
    [[nodiscard]] BigDecimal subtract(const BigDecimal& rhs) const { return *this - rhs; }
    [[nodiscard]] BigDecimal multiply(const BigDecimal& rhs) const { return *this * rhs; }
    [[nodiscard]] BigDecimal divide(const BigDecimal& rhs) const { return *this / rhs; }
    [[nodiscard]] BigDecimal divide(const BigDecimal& rhs, RoundingMode mode) const { return divideToScale(rhs, scale_, mode); }
    [[nodiscard]] BigDecimal divide(const BigDecimal& rhs, int legacyMode) const;
    [[nodiscard]] BigDecimal divide(const BigDecimal& rhs, std::int32_t scale, int legacyMode) const;
    [[nodiscard]] BigDecimal divide(const BigDecimal& rhs, std::int32_t scale, RoundingMode mode) const { return divideToScale(rhs, scale, mode); }
    [[nodiscard]] BigDecimal setScale(std::int32_t scale, int legacyMode) const;

    [[nodiscard]] BigDecimal round(const MathContext& context) const;
    [[nodiscard]] BigDecimal add(const BigDecimal& rhs, const MathContext& context) const;
    [[nodiscard]] BigDecimal subtract(const BigDecimal& rhs, const MathContext& context) const;
    [[nodiscard]] BigDecimal multiply(const BigDecimal& rhs, const MathContext& context) const;
    [[nodiscard]] BigDecimal divide(const BigDecimal& rhs, const MathContext& context) const;
    [[nodiscard]] BigDecimal setScale(std::int32_t scale,
                                     RoundingMode mode = RoundingMode::Unnecessary) const;
    [[nodiscard]] BigDecimal divideToScale(const BigDecimal& rhs, std::int32_t scale,
                                          RoundingMode mode) const;
    [[nodiscard]] BigDecimal stripTrailingZeros() const;
    [[nodiscard]] int compareTo(const BigDecimal& rhs) const;
    [[nodiscard]] bool equals(const BigDecimal& rhs) const noexcept;

    [[nodiscard]] BigDecimal round(std::size_t significantDigits,
                                  RoundingMode mode = RoundingMode::HalfUp) const;
    // Extensión de compatibilidad: size_t selecciona la precisión significativa;
    // int32_t selecciona la sobrecarga de escala fija. Usa MathContext para
    // indicar cifras significativas.
    [[nodiscard]] BigDecimal divide(const BigDecimal& divisor,
                                    std::size_t significantDigits,
                                    RoundingMode mode) const;

    [[nodiscard]] BigDecimal operator+() const { return *this; }
    [[nodiscard]] BigDecimal operator-() const;
    [[nodiscard]] BigDecimal operator+(const BigDecimal& rhs) const;
    [[nodiscard]] BigDecimal operator-(const BigDecimal& rhs) const;
    [[nodiscard]] BigDecimal operator*(const BigDecimal& rhs) const;
    // División exacta; lanza std::domain_error si el decimal no termina.
    [[nodiscard]] BigDecimal operator/(const BigDecimal& rhs) const;

    // La igualdad incluye la escala, por lo que dos valores numéricamente
    // iguales pueden resultar distintos. Usa compareTo(rhs) == 0 para
    // comprobar equivalencia numérica.
    [[nodiscard]] bool operator==(const BigDecimal& rhs) const;
    [[nodiscard]] bool operator!=(const BigDecimal& rhs) const { return !(*this == rhs); }
    [[nodiscard]] bool operator<(const BigDecimal& rhs) const;
    [[nodiscard]] bool operator>(const BigDecimal& rhs) const { return rhs < *this; }
    [[nodiscard]] bool operator<=(const BigDecimal& rhs) const { return !(rhs < *this); }
    [[nodiscard]] bool operator>=(const BigDecimal& rhs) const { return !(*this < rhs); }

private:
    std::vector<std::uint32_t> magnitude_;
    std::int32_t scale_ = 0;
    int sign_ = 0;

    [[nodiscard]] std::size_t precisionSize() const noexcept;
    BigDecimal(std::vector<std::uint32_t> magnitude, std::int32_t scale, int sign);
};

#endif // BIG_DECIMAL_H
