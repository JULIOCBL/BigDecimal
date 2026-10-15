
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



#include "BigDecimalSerialization.h"
#include "NativeExceptions.h"
#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace BigDecimalSerialization {
namespace {
constexpr std::uint8_t Null = 0x70, Reference = 0x71, ClassDesc = 0x72,
    Object = 0x73, String = 0x74, Array = 0x75, BlockData = 0x77,
    EndBlock = 0x78, Reset = 0x79, BlockDataLong = 0x7a,
    LongString = 0x7c, Enum = 0x7e;
constexpr std::uint32_t BaseHandle = 0x7e0000;
constexpr std::size_t NoIndex = std::numeric_limits<std::size_t>::max();
constexpr std::uint64_t DecimalUid = 0x54c71557f981284fULL,
    IntegerUid = 0x8cfc9f1fa93bfb1dULL, NumberUid = 0x86ac951d0b94e08bULL,
    ContextUid = 0x4d6f25c81f7601ffULL, ByteArrayUid = 0xacf317f8060854e0ULL;
const std::array<std::string, 8> ModeNames = {
    "UP", "DOWN", "CEILING", "FLOOR", "HALF_UP", "HALF_DOWN", "HALF_EVEN", "UNNECESSARY"
};
[[noreturn]] void invalid(const std::string& message) {
    throw CorruptStreamError("Invalid Java serialization stream: " + message);
}
std::string modeName(RoundingMode mode) {
    const auto index = static_cast<int>(mode);
    if (index < 0 || index >= 8) throw std::invalid_argument("Invalid rounding mode");
    return ModeNames[static_cast<std::size_t>(index)];
}
RoundingMode modeValue(const std::string& text) {
    const auto found = std::find(ModeNames.begin(), ModeNames.end(), text);
    if (found == ModeNames.end())
        throw InvalidObjectError("enum constant " + text + " does not exist in class java.math.RoundingMode");
    return static_cast<RoundingMode>(found - ModeNames.begin());
}
// El texto decimal sirve como adaptador de intercambio para el coeficiente
// nativo privado. La magnitud serializada es un arreglo de bytes sin signo
// en orden big-endian.
Bytes integerBytes(const BigInteger& value) {
    const auto text = value.toString();
    Bytes little;
    for (std::size_t i = value.signum() < 0 ? 1 : 0; i < text.size(); ++i) {
        unsigned carry = static_cast<unsigned>(text[i] - '0');
        for (auto& byte : little) {
            const auto product = static_cast<unsigned>(byte) * 10 + carry;
            byte = static_cast<std::uint8_t>(product);
            carry = product >> 8;
        }
        if (carry) little.push_back(static_cast<std::uint8_t>(carry));
    }
    std::reverse(little.begin(), little.end());
    return little;
}
BigInteger integerValue(const Bytes& bytes, int sign) {
    if (sign < -1 || sign > 1) throw CorruptStreamError("BigInteger: Invalid signum value");
    std::vector<std::uint32_t> decimal;
    for (const auto byte : bytes) {
        std::uint64_t carry = byte;
        for (auto& word : decimal) {
            const auto product = static_cast<std::uint64_t>(word) * 256 + carry;
            word = static_cast<std::uint32_t>(product % 1000000000ULL);
            carry = product / 1000000000ULL;
        }
        if (carry) decimal.push_back(static_cast<std::uint32_t>(carry));
    }
    if (decimal.empty() != (sign == 0)) throw CorruptStreamError("BigInteger: signum-magnitude mismatch");
    if (decimal.empty()) return BigInteger{};
    std::string text = sign < 0 ? "-" : "";
    text += std::to_string(decimal.back());
    for (std::size_t i = decimal.size() - 1; i != 0; --i) {
        const auto word = std::to_string(decimal[i - 1]);
        text.append(9 - word.size(), '0');
        text += word;
    }
    return BigInteger(text);
}
struct Field { char type; std::string name; std::string signature; };
struct Descriptor {
    std::string name;
    std::uint64_t uid;
    std::uint8_t flags;
    std::vector<Field> fields;
    std::string parent;
};
const std::map<std::string, Descriptor>& descriptors() {
    static const std::map<std::string, Descriptor> table = {
        {"java.math.BigDecimal", {"java.math.BigDecimal", DecimalUid, 3,
            {{'I', "scale", ""}, {'L', "intVal", "Ljava/math/BigInteger;"}}, "java.lang.Number"}},
        {"java.math.BigInteger", {"java.math.BigInteger", IntegerUid, 3,
            {{'I', "bitCount", ""}, {'I', "bitLength", ""}, {'I', "firstNonzeroByteNum", ""},
             {'I', "lowestSetBit", ""}, {'I', "signum", ""}, {'[', "magnitude", "[B"}}, "java.lang.Number"}},
        {"java.lang.Number", {"java.lang.Number", NumberUid, 2, {}, ""}},
        {"java.math.MathContext", {"java.math.MathContext", ContextUid, 2,
            {{'I', "precision", ""}, {'L', "roundingMode", "Ljava/math/RoundingMode;"}}, ""}},
        {"java.math.RoundingMode", {"java.math.RoundingMode", 0, 0x12, {}, "java.lang.Enum"}},
        {"java.lang.Enum", {"java.lang.Enum", 0, 0x12, {}, ""}},
        {"[B", {"[B", ByteArrayUid, 2, {}, ""}}
    };
    return table;
}
class Writer {
    Bytes output_ = {0xac, 0xed, 0x00, 0x05};
    std::map<std::string, std::uint32_t> classes_, strings_, modes_;
    std::map<std::string, std::uint32_t> smallIntegers_;
    std::uint32_t nextHandle_ = BaseHandle;
    void byte(std::uint8_t value) { output_.push_back(value); }
    void number(std::uint64_t value, unsigned size) {
        for (unsigned i = size; i != 0; --i) byte(static_cast<std::uint8_t>(value >> ((i - 1) * 8)));
    }
    void utf(const std::string& text) {
        if (text.size() > 65535) throw std::length_error("Java UTF descriptor too long");
        number(text.size(), 2);
        output_.insert(output_.end(), text.begin(), text.end());
    }
    void reference(std::uint32_t handle) { byte(Reference); number(handle, 4); }
    std::uint32_t handle() {
        if (nextHandle_ == std::numeric_limits<std::uint32_t>::max()) throw std::length_error("Too many serialization handles");
        return nextHandle_++;
    }
    void string(const std::string& text) {
        const auto found = strings_.find(text);
        if (found != strings_.end()) { reference(found->second); return; }
        byte(String); strings_.emplace(text, handle()); utf(text);
    }
    void descriptor(const std::string& name) {
        if (name.empty()) { byte(Null); return; }
        const auto found = classes_.find(name);
        if (found != classes_.end()) { reference(found->second); return; }
        const auto& desc = descriptors().at(name);
        byte(ClassDesc); utf(desc.name); number(desc.uid, 8);
        classes_.emplace(name, handle());
        byte(desc.flags); number(desc.fields.size(), 2);
        for (const auto& field : desc.fields) {
            byte(static_cast<std::uint8_t>(field.type)); utf(field.name);
            if (field.type == 'L' || field.type == '[') string(field.signature);
        }
        byte(EndBlock); descriptor(desc.parent);
    }
    void byteArray(const Bytes& bytes) {
        if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
            throw std::length_error("Serialized byte array exceeds Java size range");
        byte(Array); descriptor("[B"); handle(); number(bytes.size(), 4);
        output_.insert(output_.end(), bytes.begin(), bytes.end());
    }
    void integer(const BigInteger& value, bool decimalCache = false) {
        const auto text = value.toString();
        const bool small = decimalCache && text.size() <= 3 &&
            (text[0] == '-' ? text.size() <= 3 && std::stoi(text) >= -16 : std::stoi(text) <= 16);
        if (small) {
            const auto found = smallIntegers_.find(text);
            if (found != smallIntegers_.end()) { reference(found->second); return; }
        }
        byte(Object); descriptor("java.math.BigInteger");
        const auto own = handle();
        if (small) smallIntegers_.emplace(text, own);
        number(0xffffffffU, 4); number(0xffffffffU, 4);
        number(0xfffffffeU, 4); number(0xfffffffeU, 4);
        number(static_cast<std::uint32_t>(value.signum()), 4);
        byteArray(integerBytes(value)); byte(EndBlock);
    }
public:
    void write(const BigInteger& value) { integer(value); }
    void write(const BigDecimal& value) {
        byte(Object); descriptor("java.math.BigDecimal"); handle();
        number(static_cast<std::uint32_t>(value.scale()), 4);
        integer(value.unscaledValue(), true); byte(EndBlock);
    }
    void write(RoundingMode value) {
        const auto name = modeName(value);
        const auto found = modes_.find(name);
        if (found != modes_.end()) { reference(found->second); return; }
        byte(Enum); descriptor("java.math.RoundingMode");
        modes_.emplace(name, handle()); string(name);
    }
    void write(const MathContext& value) {
        byte(Object); descriptor("java.math.MathContext"); handle();
        number(static_cast<std::uint32_t>(value.precision), 4); write(value.roundingMode);
    }
    Bytes finish() { return std::move(output_); }
};
struct ReadField { char type; std::string name; std::string signature; };
struct ReadDescriptor {
    std::string name;
    std::uint8_t flags = 0;
    std::vector<ReadField> fields;
    std::size_t parent = NoIndex;
};
struct FieldValue {
    char type = 'L';
    std::uint64_t bits = 0;
    std::size_t object = NoIndex;
};
struct Node {
    std::uint8_t kind = Null;
    std::size_t descriptor = NoIndex;
    std::string text;
    Bytes bytes;
    std::map<std::string, FieldValue> fields;
};
struct Handle { bool descriptor; std::size_t index; };
class Reader {
    const Bytes& input_;
    ReadLimits limits_;
    std::size_t position_ = 0, depth_ = 0;
    std::vector<Handle> handles_;
    std::vector<std::unique_ptr<ReadDescriptor>> classes_;
    std::vector<std::unique_ptr<Node>> nodes_;
    struct Depth {
        Reader& reader;
        explicit Depth(Reader& value) : reader(value) {
            if (++reader.depth_ > reader.limits_.maxDepth) invalid("nesting limit exceeded");
        }
        ~Depth() { --reader.depth_; }
    };
    void available(std::size_t count) const {
        if (count > input_.size() - position_) throw EndOfFileError();
    }
    std::uint8_t byte() { available(1); return input_[position_++]; }
    std::uint64_t number(unsigned width) {
        available(width); std::uint64_t result = 0;
        for (unsigned i = 0; i < width; ++i) result = (result << 8) | input_[position_++];
        return result;
    }
    std::string rawString(std::uint64_t size) {
        if (size > input_.size() - position_) invalid("truncated UTF string");
        const auto length = static_cast<std::size_t>(size);
        std::string result(input_.begin() + static_cast<std::ptrdiff_t>(position_),
                           input_.begin() + static_cast<std::ptrdiff_t>(position_ + length));
        position_ += length;
        // Las cadenas del flujo usan UTF-8 modificado, incluidas las cadenas
        // de anotación que se omiten. Rechaza codificaciones mal formadas
        // antes de conservarlas.
        for (std::size_t i = 0; i < result.size();) {
            const auto first = static_cast<unsigned char>(result[i]);
            const auto count = first <= 0x7f ? 1U : (first >> 5) == 6 ? 2U : (first >> 4) == 14 ? 3U : 0U;
            if (!count || count > result.size() - i) invalid("malformed modified UTF-8");
            for (unsigned j = 1; j < count; ++j)
                if ((static_cast<unsigned char>(result[i + j]) & 0xc0) != 0x80)
                    invalid("malformed modified UTF-8");
            i += count;
        }
        return result;
    }
    std::string utf() { return rawString(number(2)); }
    void addHandle(bool descriptor, std::size_t index) {
        if (nodes_.size() + classes_.size() > limits_.maxHandles ||
            handles_.size() >= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max() - BaseHandle))
            invalid("handle limit exceeded");
        handles_.push_back({descriptor, index});
    }
    Handle reference() {
        const auto raw = number(4);
        if (raw < BaseHandle || raw - BaseHandle >= handles_.size()) invalid("invalid reference handle");
        return handles_[static_cast<std::size_t>(raw - BaseHandle)];
    }
    void annotation() {
        for (;;) {
            available(1);
            const auto tag = input_[position_];
            if (tag == EndBlock) { ++position_; return; }
            if (tag == BlockData || tag == BlockDataLong) {
                ++position_;
                const auto count = number(tag == BlockData ? 1 : 4);
                if (count > input_.size() - position_) invalid("truncated block data");
                position_ += static_cast<std::size_t>(count);
            } else {
                if (tag == Reset) invalid("reset inside object data");
                value();
            }
        }
    }
    std::size_t descriptor() {
        Depth depth(*this);
        const auto tag = byte();
        if (tag == Null) return NoIndex;
        if (tag == Reference) {
            const auto handle = reference();
            if (!handle.descriptor) invalid("object handle used as class descriptor");
            return handle.index;
        }
        if (tag != ClassDesc) invalid("expected non-proxy class descriptor");
        auto desc = std::make_unique<ReadDescriptor>();
        desc->name = utf();
        const auto uid = number(8);
        const auto known = descriptors().find(desc->name);
        if (known == descriptors().end()) invalid("unsupported class descriptor");
        if (uid != known->second.uid) {
            const auto signedUid = [](std::uint64_t bits) {
                if (bits <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                    return std::to_string(bits);
                return std::string("-") + std::to_string(~bits + 1);
            };
            throw InvalidClassError(desc->name + "; local class incompatible: stream classdesc serialVersionUID = " +
                signedUid(uid) + ", local class serialVersionUID = " + signedUid(known->second.uid));
        }
        const auto index = classes_.size();
        classes_.push_back(std::move(desc)); addHandle(true, index);
        auto& result = *classes_[index];
        result.flags = byte();
        if (result.flags != known->second.flags &&
            !((result.flags == 2 || result.flags == 3) &&
              (known->second.flags == 2 || known->second.flags == 3) && result.name != "[B"))
            invalid("invalid class flags");
        const auto count = number(2);
        if (count > (input_.size() - position_) / 3) invalid("invalid field count");
        std::set<std::string> fieldNames;
        for (std::uint64_t i = 0; i < count; ++i) {
            ReadField field{static_cast<char>(byte()), utf(), ""};
            if (!fieldNames.insert(field.name).second) invalid("duplicate field name");
            if (field.type == 'L' || field.type == '[') {
                const auto object = value();
                if (object == NoIndex || nodes_[object]->kind != String) invalid("invalid field signature");
                field.signature = nodes_[object]->text;
            } else if (std::string("BCDFIJSZ").find(field.type) == std::string::npos) invalid("invalid primitive field type");
            const auto expected = std::find_if(known->second.fields.begin(), known->second.fields.end(),
                [&](const Field& item) { return item.name == field.name; });
            if (expected != known->second.fields.end() &&
                (expected->type != field.type || expected->signature != field.signature)) invalid("incompatible field type");
            result.fields.push_back(std::move(field));
        }
        annotation(); result.parent = descriptor();
        const auto parent = result.parent == NoIndex ? std::string{} : classes_[result.parent]->name;
        if (parent != known->second.parent && !(parent.empty() && known->second.parent == "java.lang.Number"))
            invalid("incompatible serializable superclass");
        return index;
    }
    std::size_t newNode(std::uint8_t kind, std::size_t descriptorIndex = NoIndex) {
        const auto index = nodes_.size();
        auto node = std::make_unique<Node>(); node->kind = kind; node->descriptor = descriptorIndex;
        nodes_.push_back(std::move(node)); addHandle(false, index);
        return index;
    }
    std::size_t value() {
        Depth depth(*this);
        const auto tag = byte();
        if (tag == Null) return NoIndex;
        if (tag == Reference) {
            const auto handle = reference();
            if (handle.descriptor) invalid("class descriptor handle used as object");
            return handle.index;
        }
        if (tag == String || tag == LongString) {
            const auto length = number(tag == String ? 2 : 8);
            const auto index = newNode(String);
            nodes_[index]->text = rawString(length);
            return index;
        }
        if (tag != Object && tag != Array && tag != Enum) invalid("unsupported content tag");
        const auto classIndex = descriptor();
        if (classIndex == NoIndex) invalid("null class descriptor");
        const auto index = newNode(tag, classIndex);
        auto& node = *nodes_[index];
        const auto& desc = *classes_[classIndex];
        if (tag == Array) {
            if (desc.name != "[B") invalid("unsupported array type");
            const auto count = number(4);
            if (count > 0x7fffffffULL || count > input_.size() - position_) invalid("invalid byte array length");
            node.bytes.assign(input_.begin() + static_cast<std::ptrdiff_t>(position_),
                              input_.begin() + static_cast<std::ptrdiff_t>(position_ + count));
            position_ += static_cast<std::size_t>(count);
            return index;
        }
        if (tag == Enum) {
            if (desc.name != "java.math.RoundingMode") invalid("unsupported enum type");
            const auto name = value();
            if (name == NoIndex || nodes_[name]->kind != String) invalid("invalid enum constant name");
            node.text = nodes_[name]->text;
            modeValue(node.text);
            return index;
        }
        if (desc.name != "java.math.BigDecimal" && desc.name != "java.math.BigInteger" && desc.name != "java.math.MathContext")
            invalid("unsupported object class");
        std::vector<std::size_t> hierarchy;
        std::set<std::size_t> visited;
        for (auto current = classIndex; current != NoIndex; current = classes_[current]->parent) {
            if (!visited.insert(current).second) invalid("cyclic superclass descriptors");
            hierarchy.push_back(current);
        }
        std::reverse(hierarchy.begin(), hierarchy.end());
        for (const auto current : hierarchy) {
            const auto& level = *classes_[current];
            for (const auto& field : level.fields) {
                FieldValue item; item.type = field.type;
                switch (field.type) {
                    case 'B': case 'Z': item.bits = number(1); break;
                    case 'C': case 'S': item.bits = number(2); break;
                    case 'F': case 'I': item.bits = number(4); break;
                    case 'D': case 'J': item.bits = number(8); break;
                    case 'L': case '[': item.object = value(); break;
                    default: invalid("invalid field type");
                }
                if (current == classIndex) node.fields.emplace(field.name, item);
            }
            if (level.flags & 1) annotation();
        }
        validateObject(index);
        return index;
    }
    static std::int32_t signed32(std::uint64_t raw) {
        const auto bits = static_cast<std::uint32_t>(raw);
        return bits <= 0x7fffffffU ? static_cast<std::int32_t>(bits)
            : static_cast<std::int32_t>(static_cast<std::int64_t>(bits) - 0x100000000LL);
    }
    std::int32_t integerField(const Node& node, const std::string& name, std::int32_t fallback) const {
        const auto field = node.fields.find(name);
        if (field == node.fields.end()) return fallback;
        if (field->second.type != 'I') invalid("field is not an int");
        return signed32(field->second.bits);
    }
    std::size_t objectField(const Node& node, const std::string& name) const {
        const auto field = node.fields.find(name);
        if (field == node.fields.end()) return NoIndex;
        if (field->second.type != 'L' && field->second.type != '[') invalid("field is not a reference");
        return field->second.object;
    }
    const Node& requiredNode(std::size_t index, const std::string& name, std::uint8_t kind) const {
        if (index == NoIndex) invalid("null or missing " + name);
        const auto& node = *nodes_[index];
        if (node.kind != kind || node.descriptor == NoIndex || classes_[node.descriptor]->name != name)
            invalid("incompatible reference type for " + name);
        return node;
    }
    void validateObject(std::size_t index) const {
        const auto& node = *nodes_[index];
        const auto& name = classes_[node.descriptor]->name;
        if (name == "java.math.BigInteger") {
            const auto sign = integerField(node, "signum", -2);
            if (sign < -1 || sign > 1)
                throw CorruptStreamError(node.fields.count("signum") ?
                    "BigInteger: Invalid signum value" : "BigInteger: Signum not present in stream");
            const auto magnitude = objectField(node, "magnitude");
            if (magnitude == NoIndex)
                throw NullPointerError("Cannot invoke \"[B.clone()\" because \"<local4>\" is null");
            const auto& bytes = requiredNode(magnitude, "[B", Array).bytes;
            const auto first = std::find_if(bytes.begin(), bytes.end(), [](std::uint8_t b) { return b != 0; });
            const auto length = static_cast<std::size_t>(bytes.end() - first);
            if ((length == 0) != (sign == 0))
                throw CorruptStreamError("BigInteger: signum-magnitude mismatch");
            if (length > 268435456 || (length == 268435456 && *first >= 0x80))
                throw CorruptStreamError("BigInteger: Out of the supported range");
        } else if (name == "java.math.BigDecimal") {
            const auto integer = objectField(node, "intVal");
            if (integer == NoIndex)
                throw CorruptStreamError("Null or missing intVal in BigDecimal stream");
            requiredNode(integer, "java.math.BigInteger", Object);
        } else if (name == "java.math.MathContext") {
            if (integerField(node, "precision", 0) < 0)
                throw CorruptStreamError("MathContext: invalid digits in stream");
            const auto rounding = objectField(node, "roundingMode");
            if (rounding == NoIndex)
                throw CorruptStreamError("MathContext: null roundingMode in stream");
            mode(rounding);
        }
    }
    BigInteger integer(std::size_t index) const {
        const auto& node = requiredNode(index, "java.math.BigInteger", Object);
        validateObject(index);
        return integerValue(requiredNode(objectField(node, "magnitude"), "[B", Array).bytes,
                            integerField(node, "signum", -2));
    }
    RoundingMode mode(std::size_t index) const {
        return modeValue(requiredNode(index, "java.math.RoundingMode", Enum).text);
    }
    Value convert(std::size_t index) const {
        if (index == NoIndex) invalid("null root is not a supported native value");
        const auto& node = *nodes_[index];
        if (node.descriptor == NoIndex) invalid("unsupported root value");
        const auto& name = classes_[node.descriptor]->name;
        if (name == "java.math.BigInteger") return integer(index);
        if (name == "java.math.BigDecimal") {
            requiredNode(index, name, Object);
            return BigDecimal(integer(objectField(node, "intVal")), integerField(node, "scale", 0));
        }
        if (name == "java.math.MathContext") {
            requiredNode(index, name, Object);
            const auto digits = integerField(node, "precision", 0);
            if (digits < 0) throw CorruptStreamError("MathContext: invalid digits in stream");
            return MathContext(digits, mode(objectField(node, "roundingMode")));
        }
        if (name == "java.math.RoundingMode") return mode(index);
        invalid("unsupported root class");
    }
public:
    Reader(const Bytes& input, ReadLimits limits) : input_(input), limits_(limits) {
        if (input.size() > limits.maxInputBytes) invalid("input size limit exceeded");
        const auto header = number(4);
        if (header != 0xaced0005U) {
            static constexpr char hex[] = "0123456789ABCDEF";
            std::string encoded(8, '0');
            for (unsigned i = 0; i < 8; ++i)
                encoded[i] = hex[(header >> ((7 - i) * 4)) & 15];
            throw CorruptStreamError("invalid stream header: " + encoded);
        }
    }
    std::vector<Value> read() {
        std::vector<Value> output;
        while (position_ < input_.size()) {
            if (input_[position_] == Reset) { ++position_; handles_.clear(); continue; }
            output.emplace_back(convert(value()));
        }
        return output;
    }
};

template<class T> T deserializeOne(const Bytes& bytes, ReadLimits limits) {
    auto values = Reader(bytes, limits).read();
    if (values.size() != 1 || !std::holds_alternative<T>(values.front())) invalid("expected one object of the requested type");
    return std::get<T>(std::move(values.front()));
}
}
Bytes serialize(const BigDecimal& value) { Writer writer; writer.write(value); return writer.finish(); }
Bytes serialize(const BigInteger& value) { Writer writer; writer.write(value); return writer.finish(); }
Bytes serialize(const MathContext& value) { Writer writer; writer.write(value); return writer.finish(); }
Bytes serialize(RoundingMode value) { Writer writer; writer.write(value); return writer.finish(); }
Bytes serializeAll(const std::vector<Value>& values) {
    Writer writer;
    for (const auto& value : values) std::visit([&](const auto& item) { writer.write(item); }, value);
    return writer.finish();
}
std::vector<Value> deserializeAll(const Bytes& bytes, ReadLimits limits) { return Reader(bytes, limits).read(); }
BigDecimal deserializeBigDecimal(const Bytes& bytes, ReadLimits limits) { return deserializeOne<BigDecimal>(bytes, limits); }
BigInteger deserializeBigInteger(const Bytes& bytes, ReadLimits limits) { return deserializeOne<BigInteger>(bytes, limits); }
MathContext deserializeMathContext(const Bytes& bytes, ReadLimits limits) { return deserializeOne<MathContext>(bytes, limits); }
RoundingMode deserializeRoundingMode(const Bytes& bytes, ReadLimits limits) { return deserializeOne<RoundingMode>(bytes, limits); }
}
