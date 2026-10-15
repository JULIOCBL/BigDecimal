#include "BigDecimal.h"
#include "NativeExceptions.h"

#include <cmath>
#include <cstring>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool condition, const std::string& detail) {
    if (!condition) throw std::runtime_error("BigDecimal contract failure: " + detail);
}

template<class Exception, class Function>
void expectThrows(Function function, const std::string& detail) {
    try {
        function();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error("Expected exception: " + detail);
}

void constructorsAndParsing() {
    struct ParseCase { const char* input; const char* canonical; const char* plain; std::int32_t scale; const char* coefficient; };
    const ParseCase cases[] = {
        {"0", "0", "0", 0, "0"},
        {"0.00", "0.00", "0.00", 2, "0"},
        {"-0.00", "0.00", "0.00", 2, "0"},
        {"+00123.4500", "123.4500", "123.4500", 4, "1234500"},
        {"1.23E3", "1.23E+3", "1230", -1, "123"},
        {"12.3E+7", "1.23E+8", "123000000", -6, "123"},
        {"-1.23e-12", "-1.23E-12", "-0.00000000000123", 14, "-123"},
        {"1234.5E-4", "0.12345", "0.12345", 5, "12345"},
        {".25", "0.25", "0.25", 2, "25"},
        {"5.", "5", "5", 0, "5"},
        {"1e2147483647", "1E+2147483647", "", -2147483647, "1"},
        {"1e2147483648", "1E+2147483648", "", (-2147483647 - 1), "1"},
        {"1e-2147483647", "1E-2147483647", "", 2147483647, "1"}
    };
    for (const auto& item : cases) {
        const BigDecimal value(item.input);
        check(value.toString() == item.canonical, std::string("canonical parse: ") + item.input);
        if (*item.plain != '\0') check(value.toPlainString() == item.plain, std::string("plain parse: ") + item.input);
        check(value.scale() == item.scale, std::string("scale parse: ") + item.input);
        check(value.unscaledValue().toString() == item.coefficient, std::string("coefficient parse: ") + item.input);
    }

    const std::vector<std::string> invalid = {
        "", "+", "-", ".", "+.", "1..0", "1.2.3", "1e", "1e+", "1e-",
        "1e--2", "1e2.0", "1_000", " 1", "1 ", "NaN", "Infinity", "0x10",
        "1e-2147483648"
    };
    for (const auto& text : invalid) {
        expectThrows<std::invalid_argument>([&] { (void)BigDecimal(text); }, "invalid decimal " + text);
    }

    const std::u16string unicodeDigits = {u'\u0661', u'\u0662', u'\u0663', u'.', u'\u0664', u'\u0665'};
    check(BigDecimal(unicodeDigits).toPlainString() == "123.45", "UTF-16 decimal digits");
    const std::vector<char16_t> slice = {u'x', u'1', u'2', u'.', u'5', u'y'};
    check(BigDecimal(slice, 1, 4).toPlainString() == "12.5", "UTF-16 slice constructor");
    check(BigDecimal(std::u16string(u"123.456"), MathContext(4, RoundingMode::HalfEven)).toString() == "123.5",
          "UTF-16 constructor with context");
    expectThrows<std::invalid_argument>([&] { (void)BigDecimal(slice, -1, 2); }, "negative UTF-16 slice offset");
    expectThrows<std::invalid_argument>([&] { (void)BigDecimal(slice, 0, 20); }, "out-of-range UTF-16 slice");
    expectThrows<std::invalid_argument>([] { (void)BigDecimal("1e-2147483648"); }, "scale overflow");

    check(BigDecimal().toString() == "0", "default constructor");
    check(BigDecimal::ZERO.equals(BigDecimal("0")) && BigDecimal::ONE.equals(BigDecimal("1")) &&
              BigDecimal::TWO.equals(BigDecimal("2")) && BigDecimal::TEN.equals(BigDecimal("10")),
          "standard constants");
    check(BigDecimal(std::int32_t{-123}).toString() == "-123", "int32 constructor");
    check(BigDecimal(std::int64_t{1234567890123LL}).toString() == "1234567890123", "int64 constructor");
    check(BigDecimal::valueOf(12345, 2).toPlainString() == "123.45", "integer valueOf with scale");
    check(BigDecimal::valueOf(std::int32_t{12}).toString() == "12", "int32 valueOf");
    check(BigDecimal::valueOf(0.1).toString() == "0.1", "double valueOf shortest decimal");
    check(BigDecimal(0.1).compareTo(BigDecimal("0.1")) != 0, "double constructor preserves exact binary value");
    check(BigDecimal(12.345, MathContext(4, RoundingMode::HalfEven)).compareTo(BigDecimal("12.35")) == 0,
          "double constructor with context");
    check(BigDecimal(BigInteger("12345"), 2).toPlainString() == "123.45", "unscaled integer constructor");
    check(BigDecimal(BigInteger("12345"), 2, MathContext(3)).toString() == "123", "unscaled constructor with context");
    check(BigDecimal(BigInteger("12345"), MathContext(3)).toString() == "1.23E+4", "BigInteger constructor with context");
    check(BigDecimal(std::vector<char16_t>{u'1', u'.', u'2', u'5'}).toString() == "1.25", "UTF-16 vector constructor");
    check(BigDecimal(std::vector<char16_t>{u'1', u'.', u'2', u'5'}, MathContext(2)).toString() == "1.3",
          "UTF-16 vector constructor with context");
    check(BigDecimal(slice, 1, 4, MathContext(2)).toString() == "13", "UTF-16 slice constructor with context");
    check(BigDecimal("12345", MathContext(3, RoundingMode::HalfUp)).toString() == "1.23E+4",
          "string constructor with context");
    expectThrows<std::invalid_argument>([] { (void)BigDecimal(std::numeric_limits<double>::infinity()); }, "infinite double");
    expectThrows<std::invalid_argument>([] { (void)BigDecimal(std::numeric_limits<double>::quiet_NaN()); }, "NaN double");
}

void contextAndRounding() {
    check(MathContext::UNLIMITED.getPrecision() == 0, "unlimited context precision");
    check(MathContext::DECIMAL32.getPrecision() == 7, "decimal32 precision");
    check(MathContext::DECIMAL64.getPrecision() == 16, "decimal64 precision");
    check(MathContext::DECIMAL128.getPrecision() == 34, "decimal128 precision");
    const MathContext parsed("precision=3 roundingMode=HALF_UP");
    check(parsed.equals(MathContext(3, RoundingMode::HalfUp)), "parse math context");
    check(parsed.toString() == "precision=3 roundingMode=HALF_UP", "format math context");
    check(parsed.hashCode() == MathContext(3, RoundingMode::HalfUp).hashCode(), "equal context hashes");
    expectThrows<std::invalid_argument>([] { (void)MathContext("precision=-1 roundingMode=HALF_UP"); }, "negative parsed precision");
    expectThrows<std::invalid_argument>([] { (void)MathContext("precision=3 roundingMode=INVALID"); }, "invalid parsed rounding mode");
    expectThrows<std::invalid_argument>([] { (void)MathContext(1, static_cast<RoundingMode>(99)); }, "invalid rounding enum");

    const struct { RoundingMode mode; const char* positive; const char* negative; } roundingCases[] = {
        {RoundingMode::Up, "2", "-2"},
        {RoundingMode::Down, "1", "-1"},
        {RoundingMode::Ceiling, "2", "-1"},
        {RoundingMode::Floor, "1", "-2"},
        {RoundingMode::HalfUp, "2", "-2"},
        {RoundingMode::HalfDown, "1", "-1"},
        {RoundingMode::HalfEven, "2", "-2"}
    };
    for (const auto& item : roundingCases) {
        check(BigDecimal("1.5").setScale(0, item.mode).toPlainString() == item.positive, "positive rounding mode");
        check(BigDecimal("-1.5").setScale(0, item.mode).toPlainString() == item.negative, "negative rounding mode");
    }
    check(BigDecimal("2.5").setScale(0, RoundingMode::HalfEven).toPlainString() == "2", "half-even even tie");
    check(BigDecimal("3.5").setScale(0, RoundingMode::HalfEven).toPlainString() == "4", "half-even odd tie");
    check(BigDecimal("-2.5").setScale(0, RoundingMode::HalfEven).toPlainString() == "-2", "negative half-even tie");
    expectThrows<ArithmeticError>([] { (void)BigDecimal("1.1").setScale(0, RoundingMode::Unnecessary); }, "unnecessary rounding");

    check(BigDecimal("12345").round(MathContext(3, RoundingMode::HalfUp)).toString() == "1.23E+4", "significant digit round");
    check(BigDecimal("999.5").round(MathContext(3, RoundingMode::HalfUp)).toString() == "1.00E+3", "rounding carry changes exponent");
    check(BigDecimal("1.234").round(std::size_t{2}, RoundingMode::HalfEven).toString() == "1.2", "precision overload");
    check(BigDecimal("1.234").round(MathContext::UNLIMITED).equals(BigDecimal("1.234")), "unlimited rounding context");
    check(BigDecimal("1").plus(MathContext(2)).equals(BigDecimal("1")), "plus with context");
    check(BigDecimal("1.239").abs(MathContext(3)).toString() == "1.24", "absolute value with context");
    check(BigDecimal("-1.239").negate(MathContext(3)).toString() == "1.24", "negation with context");
    check(BigDecimal("1.234").precision() == 4 && BigDecimal("-1.234").signum() == -1,
          "precision and signum accessors");
}

void arithmeticAndScale() {
    const BigDecimal a("1.20"), b("3.4");
    check((a + b).toString() == "4.60" && a.add(b).equals(a + b), "addition and named method");
    check((a - b).toString() == "-2.20" && a.subtract(b).equals(a - b), "subtraction and named method");
    check((a * b).toString() == "4.080" && a.multiply(b).equals(a * b), "multiplication and named method");
    check((BigDecimal("1") / BigDecimal("8")).toString() == "0.125", "terminating exact division");
    check((BigDecimal("1") / BigDecimal("40")).toString() == "0.025", "terminating division with powers of two and five");
    expectThrows<ArithmeticError>([] { (void)(BigDecimal("1") / BigDecimal("3")); }, "nonterminating exact division");
    expectThrows<ArithmeticError>([] { (void)(BigDecimal("0") / BigDecimal("0")); }, "zero divided by zero");
    expectThrows<ArithmeticError>([] { (void)(BigDecimal("1") / BigDecimal("0")); }, "division by zero");

    check(BigDecimal("1.00").divide(BigDecimal("8"), RoundingMode::HalfUp).toString() == "0.13", "divide at dividend scale");
    check(BigDecimal("1").divide(BigDecimal("8"), RoundingMode::Down).toString() == "0", "divide with mode at dividend scale");
    check(BigDecimal("1").divide(BigDecimal("8"), BigDecimal::ROUND_HALF_UP).toString() == "0", "legacy divide mode at dividend scale");
    check(BigDecimal("1").divide(BigDecimal("8"), 2, RoundingMode::HalfEven).toString() == "0.12", "divide at explicit scale");
    check(BigDecimal("1").divide(BigDecimal("8"), 2, BigDecimal::ROUND_HALF_UP).toString() == "0.13", "legacy scale/mode divide");
    check(BigDecimal("1.239").setScale(2, BigDecimal::ROUND_HALF_UP).toString() == "1.24", "legacy setScale mode");
    check(BigDecimal("10").divide(BigDecimal("3"), std::size_t{3}, RoundingMode::HalfEven).toString() == "3.33",
          "significant-digit divide overload");
    check(BigDecimal("1").divide(BigDecimal("3"), MathContext(5, RoundingMode::HalfEven)).toString() == "0.33333",
          "division with math context");
    check(BigDecimal("12345").add(BigDecimal("1"), MathContext(3)).toString() == "1.23E+4", "addition with context");
    check(BigDecimal("12345").subtract(BigDecimal("1"), MathContext(3)).toString() == "1.23E+4", "subtraction with context");
    check(BigDecimal("12.34").multiply(BigDecimal("5.67"), MathContext(4)).toString() == "69.97", "multiplication with context");
    expectThrows<std::invalid_argument>([] { (void)BigDecimal("1").divide(BigDecimal("2"), 9); }, "invalid legacy rounding mode");

    check(BigDecimal("-5.50").divideToIntegralValue(BigDecimal("2")).toString() == "-2.00", "integral quotient scale/sign");
    const auto qr = BigDecimal("-5.50").divideAndRemainder(BigDecimal("2"));
    check(qr.first.toString() == "-2.00" && qr.second.toString() == "-1.50", "quotient/remainder pair");
    check(BigDecimal("-5.50").remainder(BigDecimal("2")).toString() == "-1.50", "remainder sign follows dividend");
    check(BigDecimal("5.50").remainder(BigDecimal("2"), MathContext(4)).toString() == "1.50", "remainder with context");
    const auto contextualQr = BigDecimal("12345").divideAndRemainder(BigDecimal("1"), MathContext(5));
    check(contextualQr.first.compareTo(BigDecimal("12345")) == 0 && contextualQr.second.signum() == 0,
          "quotient/remainder with context");
    expectThrows<ArithmeticError>([] {
        (void)BigDecimal("12345").divideToIntegralValue(BigDecimal("1"), MathContext(3));
    }, "integral quotient exceeding context precision");

    check(BigDecimal("1.20").pow(3).toString() == "1.728000", "integer power preserves scale");
    check(BigDecimal("2").pow(-3, MathContext(8)).toString() == "0.125", "negative power with context");
    check(BigDecimal("7").pow(0).toString() == "1", "zero exponent");
    expectThrows<ArithmeticError>([] { (void)BigDecimal("2").pow(-1); }, "negative exact power");
    expectThrows<ArithmeticError>([] { (void)BigDecimal("2").pow(10, MathContext(1)); }, "power exponent exceeds precision");

    check(BigDecimal("4.00").sqrt(MathContext::UNLIMITED).toString() == "2.0", "exact square root and preferred scale");
    check(BigDecimal("2").sqrt(MathContext(8, RoundingMode::HalfEven)).toString() == "1.4142136", "rounded irrational square root");
    check(BigDecimal("2").sqrt(MathContext(1, RoundingMode::Up)).toString() == "2", "sqrt rounding away from zero");
    check(BigDecimal("2").sqrt(MathContext(1, RoundingMode::Down)).toString() == "1", "sqrt rounding toward zero");
    check(BigDecimal("2.25").sqrt(MathContext(1, RoundingMode::HalfUp)).toString() == "2", "sqrt half-up exact tie");
    check(BigDecimal("2.25").sqrt(MathContext(1, RoundingMode::HalfDown)).toString() == "1", "sqrt half-down exact tie");
    check(BigDecimal("2.25").sqrt(MathContext(1, RoundingMode::HalfEven)).toString() == "2", "sqrt half-even exact tie");
    check(BigDecimal("0.00").sqrt(MathContext(8)).toString() == "0.0", "square root of scaled zero");
    expectThrows<ArithmeticError>([] { (void)BigDecimal("2").sqrt(MathContext::UNLIMITED); }, "inexact unlimited square root");
    expectThrows<ArithmeticError>([] { (void)BigDecimal("2").sqrt(MathContext(8, RoundingMode::Unnecessary)); }, "inexact square root without rounding");
    expectThrows<ArithmeticError>([] { (void)BigDecimal("-1").sqrt(MathContext(8)); }, "negative square root");
    expectThrows<ArithmeticError>([] { (void)BigDecimal("0").pow(-1, MathContext(8)); }, "zero to negative power");

    check(BigDecimal("123").movePointLeft(2).toPlainString() == "1.23", "move point left");
    check(BigDecimal("1.23").movePointRight(2).toPlainString() == "123", "move point right");
    check(BigDecimal("1.23").scaleByPowerOfTen(3).toPlainString() == "1230", "scale by power of ten");
    check(BigDecimal("0E+2").scaleByPowerOfTen(std::numeric_limits<std::int32_t>::max()).scale() ==
              std::numeric_limits<std::int32_t>::min(), "zero scale saturates at lower bound");
    expectThrows<ArithmeticError>([] {
        (void)BigDecimal("1").scaleByPowerOfTen(std::numeric_limits<std::int32_t>::min());
    }, "nonzero scale underflow");
    check(BigDecimal("1.23").movePointLeft(5).toPlainString() == "0.0000123", "move point left several places");
    check(BigDecimal("1.23").movePointRight(5).toPlainString() == "123000", "move point right across scale zero");
    check(BigDecimal("1.2300").stripTrailingZeros().toString() == "1.23", "strip trailing zeros");
    check(BigDecimal("1000.00").stripTrailingZeros().toString() == "1E+3", "strip to negative scale");
    check(BigDecimal("1.23").ulp().toString() == "0.01", "unit in last place");
    check(BigDecimal("1.20").min(BigDecimal("1.2")).toString() == "1.20", "min preserves left representation on tie");
    check(BigDecimal("1.20").max(BigDecimal("1.2")).toString() == "1.20", "max preserves left representation on tie");
}

void comparisonFormattingAndConversion() {
    check(BigDecimal("0.000001").toString() == "0.000001", "plain/scientific boundary at adjusted exponent -6");
    check(BigDecimal("0.0000001").toString() == "1E-7", "scientific format below adjusted exponent -6");
    check(BigDecimal("1E+7").toEngineeringString() == "10E+6", "engineering positive exponent multiple");
    check(BigDecimal("1E-7").toEngineeringString() == "100E-9", "engineering negative exponent multiple");
    check(BigDecimal("123.4500").toPlainString() == "123.4500", "plain formatting keeps scale");

    const BigDecimal one("1.00");
    check(one == BigDecimal("1.00") && one != BigDecimal("1.0"), "representation equality");
    check(one.compareTo(BigDecimal("1.0")) == 0, "numeric comparison ignores scale");
    check(BigDecimal("-1") < BigDecimal("0") && BigDecimal("2") > BigDecimal("1"), "ordered operators");
    check(BigDecimal("1.00").hashCode() == 3102 && BigDecimal("1").hashCode() == 31, "scale-sensitive hash code");
    check(BigDecimal("-123.45").abs().toString() == "123.45", "absolute value");
    check(BigDecimal("123.45").negate().toString() == "-123.45", "negate");

    check(BigDecimal("-12.99").toBigInteger().toString() == "-12", "truncating BigInteger conversion");
    check(BigDecimal("12.00").toBigIntegerExact().toString() == "12", "exact integral conversion with zero fraction");
    expectThrows<ArithmeticError>([] { (void)BigDecimal("12.01").toBigIntegerExact(); }, "fractional exact BigInteger conversion");
    check(BigDecimal("-12.99").longValue() == -12 && BigDecimal("-12.99").intValue() == -12, "truncating primitive conversions");
    check(BigDecimal("4294967297").intValue() == 1, "int conversion keeps low 32 bits");
    check(BigDecimal("257").byteValue() == 1 && BigDecimal("32769").shortValue() == -32767, "narrow primitive conversion");
    check(BigDecimal("-9223372036854775808").longValueExact() == std::numeric_limits<std::int64_t>::min(), "long minimum exact conversion");
    check(BigDecimal("9223372036854775807").longValueExact() == std::numeric_limits<std::int64_t>::max(), "long maximum exact conversion");
    check(BigDecimal("127").byteValueExact() == 127 && BigDecimal("32767").shortValueExact() == 32767, "narrow exact conversions");
    expectThrows<ArithmeticError>([] { (void)BigDecimal("1.5").longValueExact(); }, "fractional exact primitive conversion");
    expectThrows<ArithmeticError>([] { (void)BigDecimal("2147483648").intValueExact(); }, "int exact overflow");
    expectThrows<ArithmeticError>([] { (void)BigDecimal("128").byteValueExact(); }, "byte exact overflow");

    check(BigDecimal("0.1").doubleValue() == 0.1, "decimal to binary64 conversion");
    check(BigDecimal("0.1").floatValue() == 0.1F, "decimal to binary32 conversion");
    check(std::isinf(BigDecimal("1E+400").doubleValue()), "binary64 overflow conversion");
    check(BigDecimal("1E-4000").doubleValue() == 0.0, "binary64 underflow conversion");
}

void stressAndScaleEdges() {
    const MathContext halfEven3(3, RoundingMode::HalfEven);
    const MathContext halfUp3(3, RoundingMode::HalfUp);
    check(BigDecimal("1E+100000").add(BigDecimal("5E+99997"), halfEven3).toString() == "1.00E+100000",
          "distant-scale addition with half-even sticky digit");
    check(BigDecimal("1E+100000").add(BigDecimal("5E+99997"), halfUp3).toString() == "1.01E+100000",
          "distant-scale addition with half-up sticky digit");
    check(BigDecimal("1E+100000").subtract(BigDecimal("1E+99997"), halfEven3).toString() == "9.99E+99999",
          "distant-scale subtraction across an exponent boundary");
    check(BigDecimal("1E+100000").add(BigDecimal("1E-100000"), halfEven3).toString() == "1.00E+100000",
          "addition with a very small operand without materializing its scale gap");

    const auto minScale = BigDecimal("1e2147483648");
    const auto maxScale = BigDecimal("1e-2147483647");
    check(minScale.scale() == std::numeric_limits<std::int32_t>::min(), "minimum representable scale");
    check(maxScale.scale() == std::numeric_limits<std::int32_t>::max(), "maximum representable scale");
    expectThrows<ArithmeticError>([&] { (void)(minScale * minScale); }, "scale overflow after multiplication");
    expectThrows<ArithmeticError>([&] { (void)maxScale.setScale(std::numeric_limits<std::int32_t>::min()); },
                                 "large scale reduction beyond supported power");

    std::mt19937_64 generator(0x0F3A71C5ULL);
    std::uniform_int_distribution<int> digitCount(1, 120);
    std::uniform_int_distribution<int> digit(0, 9);
    std::uniform_int_distribution<int> scaleDistribution(-100, 100);
    std::uniform_int_distribution<int> signDistribution(0, 1);
    for (int iteration = 0; iteration < 600; ++iteration) {
        const int count = digitCount(generator);
        std::string coefficient;
        coefficient.reserve(static_cast<std::size_t>(count) + 1);
        coefficient.push_back(static_cast<char>('1' + digit(generator) % 9));
        for (int index = 1; index < count; ++index) {
            coefficient.push_back(static_cast<char>('0' + digit(generator)));
        }
        if (signDistribution(generator) != 0) coefficient.insert(coefficient.begin(), '-');
        const auto scale = static_cast<std::int32_t>(scaleDistribution(generator));
        const BigDecimal value(BigInteger(coefficient), scale);

        check(BigDecimal(value.toString()).equals(value), "generated canonical string round-trip " + std::to_string(iteration));
        check(BigDecimal(value.toPlainString()).compareTo(value) == 0,
              "generated plain string numerical round-trip " + std::to_string(iteration));
        check(value.stripTrailingZeros().compareTo(value) == 0,
              "generated trailing-zero normalization " + std::to_string(iteration));
        check((value + BigDecimal("0.000")).compareTo(value) == 0,
              "generated addition identity across scales " + std::to_string(iteration));
        check((value * BigDecimal("1.00")).compareTo(value) == 0,
              "generated multiplication identity across scales " + std::to_string(iteration));
    }

    std::uniform_int_distribution<std::int64_t> rootInput(0, 1'000'000'000);
    for (int iteration = 0; iteration < 300; ++iteration) {
        const auto root = rootInput(generator);
        const auto square = root * root;
        const BigDecimal exactRoot = BigDecimal(std::to_string(square)).sqrt(MathContext::UNLIMITED);
        check(exactRoot.compareTo(BigDecimal(std::to_string(root))) == 0,
              "generated exact square root " + std::to_string(iteration));
    }

    check(BigDecimal("1.7976931348623157E308").doubleValue() == std::numeric_limits<double>::max(),
          "largest finite binary64 conversion");
    check(std::isinf(BigDecimal("1.7976931348623159E308").doubleValue()), "binary64 overflow boundary");
    check(BigDecimal("4.9406564584124654E-324").doubleValue() == std::numeric_limits<double>::denorm_min(),
          "smallest positive binary64 subnormal conversion");
    check(BigDecimal("2.4703282292062327E-324").doubleValue() == 0.0,
          "binary64 halfway-underflow rounds to even zero");
    check(BigDecimal("2.4703282292062328E-324").doubleValue() == std::numeric_limits<double>::denorm_min(),
          "binary64 above-halfway-underflow rounds to the smallest subnormal");
    check(BigDecimal("3.4028234663852886E38").floatValue() == std::numeric_limits<float>::max(),
          "largest finite binary32 conversion");
    check(BigDecimal("1.401298464324817E-45").floatValue() == std::numeric_limits<float>::denorm_min(),
          "smallest positive binary32 subnormal conversion");

    std::mt19937_64 bitGenerator(0x75432B1ULL);
    int checkedDoubles = 0;
    int checkedFloats = 0;
    while (checkedDoubles < 600 || checkedFloats < 600) {
        if (checkedDoubles < 600) {
            const std::uint64_t bits = bitGenerator();
            double original = 0;
            std::memcpy(&original, &bits, sizeof(original));
            if (std::isfinite(original) && original != 0.0) {
                const auto roundTrip = BigDecimal(original).doubleValue();
                const auto shortestRoundTrip = BigDecimal::valueOf(original).doubleValue();
                std::uint64_t roundTripBits = 0;
                std::uint64_t shortestBits = 0;
                std::memcpy(&roundTripBits, &roundTrip, sizeof(roundTrip));
                std::memcpy(&shortestBits, &shortestRoundTrip, sizeof(shortestRoundTrip));
                check(roundTripBits == bits, "random exact binary64 round-trip " + std::to_string(checkedDoubles));
                check(shortestBits == bits, "random shortest binary64 round-trip " + std::to_string(checkedDoubles));
                ++checkedDoubles;
            }
        }
        if (checkedFloats < 600) {
            const auto bits = static_cast<std::uint32_t>(bitGenerator());
            float original = 0;
            std::memcpy(&original, &bits, sizeof(original));
            if (std::isfinite(original) && original != 0.0F) {
                const float roundTrip = BigDecimal(static_cast<double>(original)).floatValue();
                std::uint32_t roundTripBits = 0;
                std::memcpy(&roundTripBits, &roundTrip, sizeof(roundTrip));
                check(roundTripBits == bits, "random binary32 round-trip " + std::to_string(checkedFloats));
                ++checkedFloats;
            }
        }
    }

    const BigDecimal zeroA("0.00");
    const BigDecimal zeroB("0.0");
    check((zeroA + zeroB).toString() == "0.00", "zero addition keeps preferred scale");
    check((zeroA * BigDecimal("1.0")).toString() == "0.000", "zero multiplication adds scales");
    check(zeroA.divide(BigDecimal("2.0")).toString() == "0.0", "zero exact division keeps preferred scale");
    check(zeroA.divide(BigDecimal("3"), MathContext(6)).toString() == "0.00", "zero contextual division");
    check(zeroA.setScale(std::numeric_limits<std::int32_t>::min()).scale() ==
              std::numeric_limits<std::int32_t>::min(), "zero can adopt minimum scale without allocation");
    check(zeroA.setScale(std::numeric_limits<std::int32_t>::max()).scale() ==
              std::numeric_limits<std::int32_t>::max(), "zero can adopt maximum scale without allocation");
    check(zeroA.setScale(-10, RoundingMode::Unnecessary).signum() == 0, "zero scale reduction is exact");
    check(zeroA.stripTrailingZeros().toString() == "0", "zero trailing-zero normalization");
    check(BigDecimal("0.000").sqrt(MathContext::UNLIMITED).scale() == 1, "zero square root uses preferred scale");
    check(BigDecimal("0").pow(0).toString() == "1", "zero to zero power");
}
} // namespace

int main() {
    constructorsAndParsing();
    contextAndRounding();
    arithmeticAndScale();
    comparisonFormattingAndConversion();
    stressAndScaleEdges();
}
