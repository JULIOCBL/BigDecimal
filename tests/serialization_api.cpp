#include "BigDecimalSerialization.h"
#include "NativeExceptions.h"
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {
void check(bool condition) { if (!condition) throw std::runtime_error("serialization API contract failed"); }
template<class Function> void rejected(Function operation) {
    bool thrown=false;
    try { operation(); } catch(const IOError&) { thrown=true; }
    check(thrown);
}
}
int main() {
    using namespace BigDecimalSerialization;
    const BigDecimal decimal("-123456789012345678901234567890.1234500");
    const auto encoded=serialize(decimal);
    check(deserializeBigDecimal(encoded).equals(decimal));
    const BigInteger integer("-9223372036854775808");
    check(deserializeBigInteger(serialize(integer)).equals(integer));
    const MathContext context(34,RoundingMode::HalfEven);
    check(deserializeMathContext(serialize(context)).equals(context));
    for(int i=0;i<8;++i) {
        const auto mode=static_cast<RoundingMode>(i);
        check(deserializeRoundingMode(serialize(mode))==mode);
    }
    std::vector<Value> values;
    values.emplace_back(decimal);values.emplace_back(integer);values.emplace_back(context);
    values.emplace_back(RoundingMode::HalfEven);values.emplace_back(RoundingMode::HalfEven);
    const auto decoded=deserializeAll(serializeAll(values));
    check(decoded.size()==values.size());
    check(std::get<BigDecimal>(decoded[0]).equals(decimal));
    check(std::get<BigInteger>(decoded[1]).equals(integer));
    check(std::get<MathContext>(decoded[2]).equals(context));
    check(std::get<RoundingMode>(decoded[3])==RoundingMode::HalfEven);
    check(std::get<RoundingMode>(decoded[4])==RoundingMode::HalfEven);
    check(deserializeAll(Bytes{0xac,0xed,0,5}).empty());
    rejected([&]{deserializeBigInteger(encoded);});
    rejected([&]{deserializeAll(Bytes{0xac,0xed,0,5,0x71,0,0x7e,0,0});});
    ReadLimits limits;
    limits.maxInputBytes=encoded.size()-1;
    rejected([&]{deserializeBigDecimal(encoded,limits);});
    limits=ReadLimits{};limits.maxDepth=1;
    rejected([&]{deserializeBigDecimal(encoded,limits);});
    limits=ReadLimits{};limits.maxHandles=0;
    rejected([&]{deserializeBigDecimal(encoded,limits);});
    // Un arreglo declarado enorme no puede reservar más memoria que la carga útil recibida.
    auto corrupted=serialize(BigInteger("123"));
    const auto length=corrupted.size()-6;
    corrupted[length]=0x7f;corrupted[length+1]=0xff;
    corrupted[length+2]=0xff;corrupted[length+3]=0xff;
    rejected([&]{deserializeBigInteger(corrupted);});
}
