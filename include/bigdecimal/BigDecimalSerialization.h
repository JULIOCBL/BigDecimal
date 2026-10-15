
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



#ifndef BIGDECIMAL_SERIALIZATION_H
#define BIGDECIMAL_SERIALIZATION_H
#include "BigDecimal.h"
#include "BigInteger.h"
#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

// Protocolo de flujo de objetos para los tipos matemáticos documentados.
// Tipos raíz admitidos: BigDecimal, BigInteger, MathContext y RoundingMode.
namespace BigDecimalSerialization {
using Bytes = std::vector<std::uint8_t>;
using Value = std::variant<BigDecimal, BigInteger, MathContext, RoundingMode>;
struct ReadLimits {
    std::size_t maxInputBytes = 64 * 1024 * 1024;
    std::size_t maxHandles = 1'000'000;
    std::size_t maxDepth = 128;
};
Bytes serialize(const BigDecimal& value);
Bytes serialize(const BigInteger& value);
Bytes serialize(const MathContext& value);
Bytes serialize(RoundingMode value);
// Un único flujo de salida con objetos consecutivos (sin contenedor de colección).
Bytes serializeAll(const std::vector<Value>& values);
std::vector<Value> deserializeAll(const Bytes& bytes, ReadLimits limits = {});
BigDecimal deserializeBigDecimal(const Bytes& bytes, ReadLimits limits = {});
BigInteger deserializeBigInteger(const Bytes& bytes, ReadLimits limits = {});
MathContext deserializeMathContext(const Bytes& bytes, ReadLimits limits = {});
RoundingMode deserializeRoundingMode(const Bytes& bytes, ReadLimits limits = {});
}
#endif
