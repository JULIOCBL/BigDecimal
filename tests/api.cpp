#include "BigDecimal.h"
#include "BigInteger.h"
#include "NativeExceptions.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

static_assert(std::is_same_v<decltype(std::declval<const BigDecimal&>().precision()), std::int32_t>,
              "precision() must use a signed 32-bit public result type");

void require(bool ok) {
    if (!ok) throw std::runtime_error("API assertion failed");
}
template<class Exception, class Action> void throws(Action action) {
    try { action(); } catch (const Exception&) { return; }
    throw std::runtime_error("expected exception was not thrown");
}
int main() {
    const BigInteger huge("123456789012345678901234567890");
    require((huge + BigInteger("10")).toString()=="123456789012345678901234567900");
    require((huge * BigInteger("10")).toString()=="1234567890123456789012345678900");
    const auto quotient=BigInteger("-100").divideAndRemainder(BigInteger("9"));
    require(quotient.first.toString()=="-11" && quotient.second.toString()=="-1");
    require(BigInteger("-84").gcd(BigInteger("30")).toString()=="6");
    require(BigInteger("-13").mod(BigInteger("5")).toString()=="2");
    require(BigInteger("123456789",10).toString(16)=="75bcd15");
    require(BigInteger("abcdef",16).toString()=="11259375");
    require(BigInteger("-129").toString(1)=="-129"); // Las bases no admitidas usan salida decimal.
    require(BigInteger(std::vector<std::uint8_t>{0x80}).toString()=="-128");
    require(BigInteger(1,std::vector<std::uint8_t>{0x80}).toString()=="128");
    require(BigInteger(std::vector<std::uint8_t>{0,0,0}).toString()=="0");
    require(BigInteger("-3").toByteArray()==std::vector<std::uint8_t>({0xfd}));
    require(BigInteger("-3").bitLength()==2 && BigInteger("-3").bitCount()==1);
    require(BigInteger("-3").getLowestSetBit()==0 && BigInteger("-3").testBit(100));
    require((BigInteger("-7") >> 1).toString()=="-4" && (BigInteger("7") << 12).toString()=="28672");
    require((BigInteger("-7") << std::numeric_limits<std::int32_t>::min()).toString()=="-1");
    require((BigInteger("7") >> std::numeric_limits<std::int32_t>::max()).toString()=="0");
    require((-BigInteger("7")).toString()=="-7" && (+BigInteger("7")).toString()=="7");
    require(BigInteger("-7").shiftRight(1).toString()=="-4");
    require(BigInteger("7").shiftLeft(100).shiftRight(100).toString()=="7");
    require(BigInteger("-3").and_(BigInteger("6")).toString()=="4");
    require((~BigInteger("2")).toString()=="-3");
    require(BigInteger("2").modPow(BigInteger("-1"),BigInteger("5")).toString()=="3");
    require(BigInteger("12345678987654321").sqrtAndRemainder().first.toString()=="111111111");
    require(BigInteger("257").byteValue()==1 && BigInteger("32769").shortValue()==-32767);
    require(BigInteger("17").hashCode()==17);
    throws<ArithmeticError>([] { (void)BigInteger("3").modInverse(BigInteger("6")); });
    std::mt19937_64 random(1234);
    const BigInteger randomBits(80,random);
    require(randomBits.signum()>=0 && randomBits.bitLength()<=80);
    const BigInteger randomPrime(24,100,random);
    require(randomPrime.signum()>0 && randomPrime.bitLength()==24 && randomPrime.isProbablePrime(100));
    const BigInteger smallRandomPrime(8,100,random);
    require(smallRandomPrime.signum()>0 && smallRandomPrime.bitLength()==8 && smallRandomPrime.isProbablePrime(100));
    for (std::int32_t bits=2;bits<=32;++bits) {
        const BigInteger prime(bits,100,random);
        require(prime.signum()>0 && prime.bitLength()==bits && prime.isProbablePrime(100));
    }
    require(BigInteger::TEN.pow(12).toString()=="1000000000000");
    const BigDecimal a("1.00"), b("1");
    require(a != b && a.compareTo(b) == 0 && a != BigDecimal("2"));
    require(a == BigDecimal("1.00"));
    require(!a.equals(b) && a.equals(BigDecimal("1.00")));
    require(a <= b && a >= b && !(a < b) && !(a > b));
    require(BigDecimal("-2") < a && a > BigDecimal("-2"));
    require((a + b).toString() == "2.00");
    require((a - b).toString() == "0.00");
    require((a * b).toString() == "1.00");
    require((a / BigDecimal("8")).toString() == "0.125");
    require((-a).toString() == "-1.00" && (+a).equals(a));
    require((-a).abs().equals(a));
    require(a.toString() == "1.00"); // Las operaciones conservan los operandos.
    require(BigDecimal().toString() == "0");
    require(BigDecimal("-0.00").signum() == 0);
    require(BigDecimal("1").divideToScale(BigDecimal("8"), 2, RoundingMode::HalfEven).toString() == "0.12");
    require(BigDecimal("3").divideToScale(BigDecimal("8"), 2, RoundingMode::HalfEven).toString() == "0.38");
    require(BigDecimal("1200.00").stripTrailingZeros().toString() == "1.2E+3");
    require(BigDecimal("1.230").round(0).toString() == "1.230");
    require(BigDecimal("999.5").round(MathContext(3)).toString() == "1.00E+3");
    auto copied = a;
    copied = copied + b;
    require(a.toString() == "1.00" && copied.toString() == "2.00");
    auto moved = std::move(copied);
    require(moved.toString() == "2.00");
    require(copied.toString() == "0" && copied.signum() == 0 && copied.scale() == 0);
    require((copied + b).equals(b));
    copied = std::move(moved);
    require(copied.toString() == "2.00" && moved.toString() == "0");
    throws<std::invalid_argument>([] { BigDecimal bad("1e"); });
    throws<std::invalid_argument>([] { MathContext bad(-1); });
    throws<std::invalid_argument>([] { MathContext bad(1, static_cast<RoundingMode>(8)); });
    throws<std::invalid_argument>([&] { (void)a.round(std::numeric_limits<std::size_t>::max()); });
    throws<std::domain_error>([&] { (void)(a / BigDecimal("0")); });
    throws<std::domain_error>([&] { (void)(a / BigDecimal("3")); });
    throws<std::domain_error>([&] { (void)a.setScale(-1); });
    throws<ArithmeticError>([] { (void)(BigDecimal("1e2147483647") * BigDecimal("1e2")); });
    throws<NullPointerError>([] { BigDecimal bad(static_cast<const char*>(nullptr)); });
    throws<NullPointerError>([] { BigDecimal bad(static_cast<const char*>(nullptr),MathContext(1)); });
    throws<NullPointerError>([] { MathContext bad(static_cast<const char*>(nullptr)); });
    std::cout << "Native API checks passed\n";
}
