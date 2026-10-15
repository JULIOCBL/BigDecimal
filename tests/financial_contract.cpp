#include "BigDecimal.h"
#include "NativeExceptions.h"

#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

namespace {
constexpr std::int64_t kCoefficientLimit = 1'000'000'000;
constexpr std::int64_t kDivisionScaleFactor = 1'000'000;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string fixedDecimal(std::int64_t coefficient, std::int32_t scale) {
    const bool negative = coefficient < 0;
    std::string digits = std::to_string(negative ? -coefficient : coefficient);
    const auto fractionalDigits = static_cast<std::size_t>(scale);
    if (digits.size() <= fractionalDigits) {
        digits.insert(0, fractionalDigits + 1 - digits.size(), '0');
    }
    if (scale != 0) {
        digits.insert(digits.size() - fractionalDigits, 1, '.');
    }
    if (negative) {
        digits.insert(digits.begin(), '-');
    }
    return digits;
}

std::int64_t roundHalfEvenQuotient(std::int64_t numerator, std::int64_t denominator) {
    std::int64_t quotient = numerator / denominator;
    const std::int64_t remainder = numerator % denominator;
    if (remainder == 0) {
        return quotient;
    }

    const std::int64_t twiceRemainder = (remainder < 0 ? -remainder : remainder) * 2;
    const std::int64_t absoluteDenominator = denominator < 0 ? -denominator : denominator;
    const bool greaterThanHalf = twiceRemainder > absoluteDenominator;
    const bool tieAndOdd = twiceRemainder == absoluteDenominator && quotient % 2 != 0;
    if (greaterThanHalf || tieAndOdd) {
        const bool negative = (numerator < 0) != (denominator < 0);
        quotient += negative ? -1 : 1;
    }
    return quotient;
}

void checkRoundingBoundaries() {
    require(BigDecimal("1.005").setScale(2, RoundingMode::HalfEven).toPlainString() == "1.00",
            "HALF_EVEN must round an exact tie toward the even digit");
    require(BigDecimal("1.015").setScale(2, RoundingMode::HalfEven).toPlainString() == "1.02",
            "HALF_EVEN must round an odd tie upward");
    require(BigDecimal("-1.005").setScale(2, RoundingMode::HalfEven).toPlainString() == "-1.00",
            "HALF_EVEN must handle negative ties symmetrically");
    require(BigDecimal("-1.005").setScale(2, RoundingMode::HalfUp).toPlainString() == "-1.01",
            "HALF_UP must round a negative tie away from zero");
    require(BigDecimal("1.001").setScale(2, RoundingMode::Up).toPlainString() == "1.01",
            "UP must increase magnitude for discarded nonzero digits");
    require(BigDecimal("1.009").setScale(2, RoundingMode::Down).toPlainString() == "1.00",
            "DOWN must truncate toward zero");
    require(BigDecimal("-1.001").setScale(2, RoundingMode::Ceiling).toPlainString() == "-1.00",
            "CEILING must round toward positive infinity");
    require(BigDecimal("-1.001").setScale(2, RoundingMode::Floor).toPlainString() == "-1.01",
            "FLOOR must round toward negative infinity");

    bool rejectedInexact = false;
    try {
        (void)BigDecimal("1.001").setScale(2, RoundingMode::Unnecessary);
    } catch (const ArithmeticError&) {
        rejectedInexact = true;
    }
    require(rejectedInexact, "UNNECESSARY must reject discarded nonzero digits");

    require(BigDecimal("1").divideToScale(BigDecimal("8"), 2, RoundingMode::HalfEven)
                .toPlainString() == "0.12",
            "division must round 0.125 to 0.12 using HALF_EVEN");
    require(BigDecimal("3").divideToScale(BigDecimal("8"), 2, RoundingMode::HalfEven)
                .toPlainString() == "0.38",
            "division must round 0.375 to 0.38 using HALF_EVEN");
}

void checkFinancialCases() {
    const BigDecimal taxBase("123456789012345678901234.56");
    const BigDecimal taxRate("0.0825");
    require((taxBase * taxRate).toPlainString() == "10185185093518518509351.851200",
            "large exact tax multiplication must preserve scale");

    const auto cents = BigDecimal("12.34").divideToScale(BigDecimal("3"), 2, RoundingMode::HalfEven);
    require(cents.toPlainString() == "4.11", "fixed-scale division must produce expected cents");
    require((BigDecimal("0.10") + BigDecimal("0.20")).toPlainString() == "0.30",
            "decimal addition must avoid binary floating-point artifacts");

    bool zeroDivisorRejected = false;
    try {
        (void)BigDecimal("1.00").divideToScale(BigDecimal("0.00"), 2, RoundingMode::HalfEven);
    } catch (const std::domain_error&) {
        zeroDivisorRejected = true;
    }
    require(zeroDivisorRejected, "division by zero must throw a domain error");

    bool fractionalExactConversionRejected = false;
    try {
        (void)BigDecimal("1.01").intValueExact();
    } catch (const ArithmeticError&) {
        fractionalExactConversionRejected = true;
    }
    require(fractionalExactConversionRejected, "exact integer conversion must reject a fraction");

    bool overflowConversionRejected = false;
    try {
        (void)BigDecimal("2147483648").intValueExact();
    } catch (const ArithmeticError&) {
        overflowConversionRejected = true;
    }
    require(overflowConversionRejected, "exact integer conversion must reject overflow");

    require(BigDecimal("1.0") != BigDecimal("1.00") &&
                BigDecimal("1.0").compareTo(BigDecimal("1.00")) == 0,
            "representation equality and numerical comparison must remain distinct");
}

void checkGeneratedArithmetic() {
    std::mt19937_64 generator(0xB16DEC1A1ULL);
    std::uniform_int_distribution<std::int64_t> coefficients(-kCoefficientLimit, kCoefficientLimit);

    constexpr std::int32_t inputScale = 2;
    constexpr std::int32_t divisionScale = 6;
    for (std::int32_t iteration = 0; iteration < 1200; ++iteration) {
        const std::int64_t leftCoefficient = coefficients(generator);
        const std::int64_t rightCoefficient = coefficients(generator);
        const BigDecimal left(fixedDecimal(leftCoefficient, inputScale));
        const BigDecimal right(fixedDecimal(rightCoefficient, inputScale));

        require((left + right).toPlainString() ==
                    fixedDecimal(leftCoefficient + rightCoefficient, inputScale),
                "generated addition mismatch at case " + std::to_string(iteration));
        require((left - right).toPlainString() ==
                    fixedDecimal(leftCoefficient - rightCoefficient, inputScale),
                "generated subtraction mismatch at case " + std::to_string(iteration));
        require((left * right).toPlainString() ==
                    fixedDecimal(leftCoefficient * rightCoefficient, inputScale * 2),
                "generated multiplication mismatch at case " + std::to_string(iteration));
        require(left.compareTo(right) ==
                    (leftCoefficient < rightCoefficient ? -1 :
                     leftCoefficient > rightCoefficient ? 1 : 0),
                "generated comparison mismatch at case " + std::to_string(iteration));
        require(((left + right) - right).compareTo(left) == 0,
                "generated add/subtract identity mismatch at case " + std::to_string(iteration));

        if (rightCoefficient != 0) {
            const std::int64_t scaledNumerator = leftCoefficient * kDivisionScaleFactor;
            const std::int64_t expected = roundHalfEvenQuotient(scaledNumerator, rightCoefficient);
            const auto actual = left.divideToScale(right, divisionScale, RoundingMode::HalfEven);
            require(actual.toPlainString() == fixedDecimal(expected, divisionScale),
                    "generated HALF_EVEN division mismatch at case " + std::to_string(iteration));
        }
    }
}
} // namespace

int main() {
    checkRoundingBoundaries();
    checkFinancialCases();
    checkGeneratedArithmetic();
    std::cout << "Financial contract and deterministic arithmetic checks passed (1200 cases)\n";
}
