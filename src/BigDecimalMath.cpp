
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
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
std::int32_t mathScale(std::int64_t value) {
    if (value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max())
        throw ArithmeticError(value > 0 ? "Underflow" : "Overflow");
    return static_cast<std::int32_t>(value);
}
std::int32_t preferredScale(std::int64_t value) {
    return static_cast<std::int32_t>(std::clamp(value,
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())));
}
std::int32_t wrapInt32(std::int64_t value) {
    const auto low = static_cast<std::uint32_t>(value);
    return static_cast<std::int32_t>(low <= 0x7fffffffU ?
        static_cast<std::int64_t>(low) : static_cast<std::int64_t>(low) - 4294967296LL);
}
BigDecimal integerSqrt(const BigDecimal& integer) {
    if (integer.signum() == 0) return BigDecimal();
    BigDecimal estimate("1E+" + std::to_string((integer.precision() + 1) / 2));
    const BigDecimal two("2");
    for (;;) {
        auto next = (estimate + integer.divideToScale(estimate, 0, RoundingMode::Down))
                        .divideToScale(two, 0, RoundingMode::Down);
        if (next >= estimate) return estimate;
        estimate = std::move(next);
    }
}
}

// La división entera trunca hacia cero y conserva la escala preferida;
// solo elimina los ceros necesarios para ajustarse al contexto.
BigDecimal BigDecimal::divideToIntegralValue(const BigDecimal& divisor) const {
    if (divisor.sign_ == 0) throw ArithmeticError(sign_==0 ? "Division undefined" : "Division by zero");
    const auto preferred = preferredScale(static_cast<std::int64_t>(scale_) - divisor.scale_);
    if (abs() < divisor.abs()) return BigDecimal(std::vector<std::uint32_t>{}, preferred, 0);
    // Los cocientes terminantes pueden conservar una escala negativa y evitar
    // expandir un coeficiente como 1E+1000000 en un millón de ceros.
    BigDecimal quotient;
    try {
        quotient = *this / divisor;
        if (quotient.scale_ > 0) quotient = quotient.setScale(0, RoundingMode::Down);
    } catch (const std::domain_error&) {
        quotient = divideToScale(divisor, 0, RoundingMode::Down);
    }
    if (quotient.scale_ < preferred) return quotient.setScale(preferred);
    while (quotient.scale_ > preferred && quotient.sign_ != 0 &&
           quotient.magnitude_.front() % 10 == 0)
        quotient = quotient.setScale(quotient.scale_ - 1);
    return quotient;
}

BigDecimal BigDecimal::divideToIntegralValue(const BigDecimal& divisor,
                                            const MathContext& context) const {
    if (context.precision == 0 || abs() < divisor.abs())
        return divideToIntegralValue(divisor);
    const auto preferred = preferredScale(static_cast<std::int64_t>(scale_) - divisor.scale_);
    auto quotient = divide(divisor, MathContext(context.precision, RoundingMode::Down));
    if (quotient.scale_ < 0) {
        if ((*this - quotient * divisor).abs() >= divisor.abs())
            throw ArithmeticError("Division impossible");
    } else if (quotient.scale_ > 0) {
        quotient = quotient.setScale(0, RoundingMode::Down);
    }
    if (preferred > quotient.scale_ &&
        static_cast<std::size_t>(context.precision) > quotient.precisionSize()) {
        const auto available = static_cast<std::int64_t>(context.precision) - static_cast<std::int64_t>(quotient.precisionSize());
        const auto target = std::min(static_cast<std::int64_t>(preferred),
                                     static_cast<std::int64_t>(quotient.scale_) + available);
        return quotient.setScale(mathScale(target));
    }
    while (quotient.scale_ > preferred && quotient.sign_ != 0 &&
           quotient.magnitude_.front() % 10 == 0)
        quotient = quotient.setScale(quotient.scale_ - 1);
    return quotient;
}

std::pair<BigDecimal, BigDecimal> BigDecimal::divideAndRemainder(const BigDecimal& divisor) const {
    auto quotient = divideToIntegralValue(divisor);
    auto remainder = *this - quotient * divisor;
    return {std::move(quotient), std::move(remainder)};
}
std::pair<BigDecimal, BigDecimal> BigDecimal::divideAndRemainder(
    const BigDecimal& divisor, const MathContext& context) const {
    auto quotient = divideToIntegralValue(divisor, context);
    auto remainder = *this - quotient * divisor;
    return {std::move(quotient), std::move(remainder)};
}
BigDecimal BigDecimal::remainder(const BigDecimal& divisor) const {
    return divideAndRemainder(divisor).second;
}
BigDecimal BigDecimal::remainder(const BigDecimal& divisor, const MathContext& context) const {
    return divideAndRemainder(divisor, context).second;
}

BigDecimal BigDecimal::pow(std::int32_t exponent) const {
    if (exponent < 0 || exponent > 999999999) throw ArithmeticError("Invalid operation");
    if (exponent == 0) return BigDecimal("1");
    const auto resultScale = static_cast<std::int64_t>(scale_) * exponent;
    if (sign_ == 0) return BigDecimal(std::vector<std::uint32_t>{}, preferredScale(resultScale), 0);
    mathScale(resultScale);
    auto factor = *this;
    BigDecimal result("1");
    auto power = static_cast<std::uint32_t>(exponent);
    while (power) {
        if (power & 1U) result = result * factor;
        power >>= 1U;
        if (power) factor = factor * factor;
    }
    return result;
}

// La exponenciación por cuadrados usa un orden fijo y precisión de guarda;
// el orden de multiplicación importa porque cada paso redondea.
BigDecimal BigDecimal::pow(std::int32_t exponent, const MathContext& context) const {
    if (context.precision == 0) return pow(exponent);
    if (exponent < -999999999 || exponent > 999999999)
        throw ArithmeticError("Invalid operation");
    if (exponent == 0) return BigDecimal("1");
    auto bits = static_cast<std::uint32_t>(exponent < 0 ? -exponent : exponent);
    const auto digits = static_cast<std::int32_t>(std::to_string(bits).size());
    if (digits > context.precision) throw ArithmeticError("Invalid operation");
    const auto workPrecision = static_cast<std::int64_t>(context.precision) + digits + 1;
    if (workPrecision > std::numeric_limits<std::int32_t>::max())
        throw InvalidArgumentError("Digits < 0");
    const MathContext work(static_cast<std::int32_t>(workPrecision), context.roundingMode);
    BigDecimal result("1");
    bool seen = false;
    for (int i = 1; i <= 31; ++i) {
        bits <<= 1U;
        if (bits & 0x80000000U) {
            seen = true;
            result = result.multiply(*this, work);
        }
        if (i != 31 && seen) result = result.multiply(result, work);
    }
    if (exponent < 0) result = BigDecimal("1").divide(result, work);
    return result.round(context);
}

// La raíz cuadrada entera exacta y las comparaciones con el punto medio
// cumplen las reglas de redondeo sin depender de una aproximación double.
BigDecimal BigDecimal::sqrt(const MathContext& context) const {
    if (sign_ < 0) throw ArithmeticError("Attempted square root of negative BigDecimal");
    const auto preferred = scale_ / 2;
    if (sign_ == 0) return BigDecimal(std::vector<std::uint32_t>{}, preferred, 0);
    const auto stripped = stripTrailingZeros();
    if (stripped.magnitude_.size() == 1 && stripped.magnitude_.front() == 1 &&
        stripped.scale_ % 2 == 0) {
        BigDecimal result(std::vector<std::uint32_t>{1}, stripped.scale_ / 2, 1);
        return result.scale_ == preferred ? result :
            result.add(BigDecimal(std::vector<std::uint32_t>{}, preferred, 0), context);
    }
    // Normaliza con aritmética de 32 bits, incluidos el desbordamiento y la
    // negación de INT32_MIN. Conserva estos casos límite observables.
    const auto normal = wrapInt32(static_cast<std::int64_t>(stripped.scale_) -
                                static_cast<std::int64_t>(stripped.precisionSize()) + 1);
    const auto evenNormal = normal % 2 == 0 ? normal : wrapInt32(static_cast<std::int64_t>(normal) - 1);
    const auto normalizedScale = mathScale(static_cast<std::int64_t>(stripped.scale_) - evenNormal);
    const auto shiftBack = wrapInt32(-static_cast<std::int64_t>(evenNormal)) / 2;
    const auto effectiveScale = static_cast<std::int64_t>(normalizedScale) - 2LL * shiftBack;
    std::int64_t target;
    if (context.precision == 0) {
        target = effectiveScale > 0 ? (effectiveScale + 1) / 2 : effectiveScale / 2;
    } else {
        auto adjusted = static_cast<std::int64_t>(stripped.precisionSize()) - 1 - effectiveScale;
        auto exponent = adjusted / 2;
        if (adjusted < 0 && adjusted % 2) --exponent;
        target = static_cast<std::int64_t>(context.precision) - 1 - exponent;
    }
    const auto resultScale = mathScale(target);
    const auto scaledScale = mathScale(effectiveScale - 2 * target);
    const BigDecimal scaled(stripped.magnitude_, scaledScale, 1);
    auto root = integerSqrt(scaled.setScale(0, RoundingMode::Down));
    const bool exact = (root * root).compareTo(scaled) == 0;
    bool increment = false;
    if (!exact) {
        if (context.precision == 0 || context.roundingMode == RoundingMode::Unnecessary) {
            // Eleva al cuadrado el candidato redondeado antes de indicar que
            // el resultado es inexacto. Los errores de escala intermedios
            // tienen prioridad sobre ese mensaje.
            mathScale(2LL*resultScale);
            throw ArithmeticError("Computed square root not exact.");
        }
        switch (context.roundingMode) {
        case RoundingMode::Up:
        case RoundingMode::Ceiling: increment = true; break;
        case RoundingMode::Down:
        case RoundingMode::Floor: break;
        default: {
            const BigDecimal midpoint = root * BigDecimal("2") + BigDecimal("1");
            const int comparison = (scaled * BigDecimal("4")).compareTo(midpoint * midpoint);
            increment = comparison > 0 || (comparison == 0 &&
                (context.roundingMode == RoundingMode::HalfUp ||
                 (context.roundingMode == RoundingMode::HalfEven && root.magnitude_.front() % 2)));
            break;
        }
        }
    }
    if (increment) root = root + BigDecimal("1");
    BigDecimal result(std::move(root.magnitude_), resultScale, 1);
    result = result.round(context);
    // Este cuadrado es necesario para el redondeo dirigido o exacto. El
    // desbordamiento de escala forma parte del comportamiento observable.
    if (context.precision == 0 || context.roundingMode == RoundingMode::Unnecessary ||
        context.roundingMode == RoundingMode::Down || context.roundingMode == RoundingMode::Floor ||
        context.roundingMode == RoundingMode::Up || context.roundingMode == RoundingMode::Ceiling) {
        const auto square = result * result;
        if ((context.precision == 0 || context.roundingMode == RoundingMode::Unnecessary) &&
            square.compareTo(*this) != 0) throw ArithmeticError("Computed square root not exact.");
    }
    if (result.scale_ != preferred)
        result = result.stripTrailingZeros().add(BigDecimal(std::vector<std::uint32_t>{}, preferred, 0),
                    MathContext(context.precision, RoundingMode::Unnecessary));
    return result;
}
