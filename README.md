# BigDecimal C++

Biblioteca de aritmética decimal de precisión arbitraria, nativa y escrita en
C++17. Incluye también `BigInteger`, contextos de precisión, redondeo y un
codec de serialización binaria para intercambiar los tipos matemáticos
admitidos. Es una biblioteca nativa: no necesita ningún runtime externo ni
dependencias adicionales para compilar ni para ejecutar.

Todos los ejemplos de esta guía son programas completos y compilables: puedes
copiarlos en un archivo `.cpp` y ejecutarlos tal cual.

## Contenido

- [Características](#características)
- [Requisitos](#requisitos)
- [Compilar y probar](#compilar-y-probar)
- [Instalar](#instalar)
- [Usar desde otro proyecto](#usar-desde-otro-proyecto)
- [Inicio rápido](#inicio-rápido)
- [Guía de BigDecimal](#guía-de-bigdecimal)
- [Guía de BigInteger](#guía-de-biginteger)
- [Serialización](#serialización)
- [Manejo de errores](#manejo-de-errores)
- [Caso práctico: factura con IGV](#caso-práctico-factura-con-igv)
- [Referencia rápida](#referencia-rápida)
- [Errores y límites](#errores-y-límites)
- [Licencia y atribución](#licencia-y-atribución)

## Características

- Decimales de precisión arbitraria con escala decimal explícita.
- Suma, resta, multiplicación, división exacta y división con redondeo.
- División integral, cociente y resto, potencias y raíz cuadrada.
- Formato decimal normal, plano y de ingeniería.
- Ocho modos de redondeo y contextos `DECIMAL32`, `DECIMAL64` y `DECIMAL128`.
- Conversiones a enteros y a `float`/`double`, exactas o con truncamiento según
  la función elegida.
- `BigInteger` con operaciones aritméticas, modularidad, bits y pruebas de
  primalidad.
- API de serialización para `BigDecimal`, `BigInteger`, `MathContext` y
  `RoundingMode`.
- C++17, RAII, tipos valor y dependencias externas nulas.

## Requisitos

- CMake 3.16 o posterior.
- Un compilador con soporte para C++17 (Apple Clang, Clang, GCC o MSVC).

## Compilar y probar

Desde la carpeta raíz del repositorio:

```sh
cmake -S . -B build \
  -DBIGDECIMAL_BUILD_TESTS=ON \
  -DBIGDECIMAL_BUILD_EXAMPLE=ON \
  -DBIGDECIMAL_WARNINGS_AS_ERRORS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/bigdecimal_example
```

CTest incluye contratos de API y serialización, además de pruebas de casos
financieros y de compatibilidad matemática: redondeo, límites de escala,
conversiones, formatos, raíces y operaciones con entradas deterministas
generadas. Para repetir solo las pruebas:

```sh
ctest --test-dir build --output-on-failure
```

La salida del ejemplo es similar a:

```text
Total: 59.97
1/3: 0.3333333333333333333333333333333333
```

Para compilar solo la biblioteca, desactiva las opciones de pruebas y ejemplo o
déjalas en su valor predeterminado (`OFF`).

### Compilar a mano, sin CMake

La biblioteca son cinco archivos fuente y cuatro encabezados. Para construir un
programa propio basta con compilarlos juntos:

```sh
c++ -std=c++17 -O2 \
  -I include -I include/bigdecimal \
  mi_programa.cpp \
  src/BigDecimal.cpp src/BigDecimalMath.cpp src/BigDecimalConversions.cpp \
  src/BigInteger.cpp src/BigDecimalSerialization.cpp \
  -o mi_programa
```

## Instalar

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build --prefix "$HOME/.local"
```

La instalación coloca la biblioteca, los encabezados, los archivos de
configuración de CMake y esta documentación junto con `LICENSE` y `NOTICE` bajo
el prefijo elegido. Para otro prefijo, sustituye `$HOME/.local` por su ruta.

## Usar desde otro proyecto

### Como subdirectorio

Clona o agrega esta carpeta al proyecto y escribe en su `CMakeLists.txt`:

```cmake
add_subdirectory(external/bigdecimal-cpp)
target_link_libraries(mi_programa PRIVATE BigDecimal::BigDecimal)
```

En el código:

```cpp
#include <bigdecimal/BigDecimal.h>

int main() {
    const BigDecimal precio("19.99");
    const BigDecimal cantidad("3");
    const BigDecimal total = (precio * cantidad).setScale(2);
    return total.toPlainString() == "59.97" ? 0 : 1;
}
```

### Como paquete instalado

Con la ruta de instalación en `CMAKE_PREFIX_PATH`:

```cmake
find_package(BigDecimal CONFIG REQUIRED)
target_link_libraries(mi_programa PRIVATE BigDecimal::BigDecimal)
```

También puedes indicarla al configurar el proyecto consumidor:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/.local"
```

### Enlazando a mano contra la instalación

```sh
c++ -std=c++17 mi_programa.cpp \
  -I "$HOME/.local/include" -L "$HOME/.local/lib" -lbigdecimal \
  -o mi_programa
```

## Inicio rápido

```cpp
#include <bigdecimal/BigDecimal.h>
#include <iostream>

int main() {
    const BigDecimal precio("19.99");
    const BigDecimal cantidad("3");

    const BigDecimal subtotal = precio * cantidad;
    const BigDecimal igv = (subtotal * BigDecimal("0.18")).setScale(2, RoundingMode::HalfUp);
    const BigDecimal total = (subtotal + igv).setScale(2, RoundingMode::HalfUp);

    std::cout << "Subtotal  : " << subtotal.toPlainString() << '\n';
    std::cout << "IGV (18%) : " << igv.toPlainString() << '\n';
    std::cout << "Total     : " << total.toPlainString() << '\n';
}
```

```text
Subtotal  : 59.97
IGV (18%) : 10.79
Total     : 70.76
```

`toPlainString()` nunca usa notación científica; es la forma recomendada para
mostrar importes.

## Guía de BigDecimal

### Construir valores

Hay constructores para texto, enteros, `double`, `BigInteger` y texto UTF-16.
El constructor de texto conserva la escala escrita y el texto puede incluir
exponente (`"1E+10"`, `"1e-7"`).

```cpp
#include <bigdecimal/BigDecimal.h>
#include <bigdecimal/BigInteger.h>
#include <iostream>
#include <string>
#include <vector>

int main() {
    const BigDecimal desdeTexto("123.45");
    const BigDecimal desdeLiteral("0.001");
    const BigDecimal desdeInt32(std::int32_t(42));
    const BigDecimal desdeInt64(std::int64_t(-9000000000LL));
    const BigDecimal desdeDoubleExacto(0.5);                      // expansión binaria exacta
    const BigDecimal desdeDoubleCorto = BigDecimal::valueOf(0.1);  // texto decimal corto
    const BigDecimal desdeBigInteger(BigInteger("12345678901234567890"));
    const BigDecimal conEscala(BigInteger("1234"), 3);             // 1234 x 10^-3
    const BigDecimal desdeUtf16(std::u16string(u"12.34"));
    const BigDecimal desdeSegmento(std::vector<char16_t>{u'9', u'8', u'.', u'7', u'6'}, 1, 4);
    const BigDecimal conContexto("1.23456", MathContext(4, RoundingMode::HalfUp));

    std::cout << "texto              : " << desdeTexto.toString() << '\n';
    std::cout << "literal            : " << desdeLiteral.toPlainString() << '\n';
    std::cout << "int32              : " << desdeInt32.toString() << '\n';
    std::cout << "int64              : " << desdeInt64.toString() << '\n';
    std::cout << "double 0.5 exacto  : " << desdeDoubleExacto.toString() << '\n';
    std::cout << "valueOf(0.1)       : " << desdeDoubleCorto.toString() << '\n';
    std::cout << "BigInteger         : " << desdeBigInteger.toString() << '\n';
    std::cout << "1234 con escala 3  : " << conEscala.toPlainString() << '\n';
    std::cout << "UTF-16             : " << desdeUtf16.toPlainString() << '\n';
    std::cout << "segmento UTF-16    : " << desdeSegmento.toPlainString() << '\n';
    std::cout << "con MathContext    : " << conContexto.toString() << '\n';
    std::cout << "constantes         : "
              << BigDecimal::ZERO.toString() << ", "
              << BigDecimal::ONE.toString() << ", "
              << BigDecimal::TWO.toString() << ", "
              << BigDecimal::TEN.toString() << '\n';
}
```

```text
texto              : 123.45
literal            : 0.001
int32              : 42
int64              : -9000000000
double 0.5 exacto  : 0.5
valueOf(0.1)       : 0.1
BigInteger         : 12345678901234567890
1234 con escala 3  : 1.234
UTF-16             : 12.34
segmento UTF-16    : 8.76
con MathContext    : 1.235
constantes         : 0, 1, 2, 10
```

Diferencia entre los dos constructores de `double`:

- `BigDecimal(0.1)` guarda la expansión binaria exacta del `double`
  (`0.1000000000000000055511151231257827021181583404541015625`).
- `BigDecimal::valueOf(0.1)` guarda el texto decimal corto (`0.1`), que es lo
  que casi siempre quieres.

### Escala, precisión y comparación

Un `BigDecimal` es `coeficiente x 10^-escala`. Dos valores numéricamente
iguales pueden tener escalas distintas, y la igualdad (`==` y `equals`) tiene en
cuenta la escala; para comparar solo el valor usa `compareTo()`.

```cpp
#include <bigdecimal/BigDecimal.h>
#include <iostream>

int main() {
    const BigDecimal a("1.0");
    const BigDecimal b("1.00");

    std::cout << "a                  : " << a.toString()
              << " | escala " << a.scale()
              << " | precision " << a.precision()
              << " | coeficiente " << a.unscaledValue().toString() << '\n';
    std::cout << "b                  : " << b.toString()
              << " | escala " << b.scale()
              << " | precision " << b.precision()
              << " | coeficiente " << b.unscaledValue().toString() << '\n';
    std::cout << "a == b             : " << (a == b) << '\n';
    std::cout << "a.equals(b)        : " << a.equals(b) << '\n';
    std::cout << "a.compareTo(b)     : " << a.compareTo(b) << '\n';
    std::cout << "mismo hashCode     : " << (a.hashCode() == b.hashCode()) << '\n';
    std::cout << "a < b              : " << (a < b) << '\n';

    std::cout << "600.0 normalizado  : " << BigDecimal("600.0").stripTrailingZeros().toString() << '\n';
    std::cout << "0.00 normalizado   : " << BigDecimal("0.00").stripTrailingZeros().toString() << '\n';
    std::cout << "precision de 0.00  : " << BigDecimal("0.00").precision() << '\n';
    std::cout << "ulp de 1.00        : " << BigDecimal("1.00").ulp().toPlainString() << '\n';
    std::cout << "min y max          : " << BigDecimal("2").min(BigDecimal("3")).toString()
              << " y " << BigDecimal("2").max(BigDecimal("3")).toString() << '\n';
    std::cout << "signum             : " << BigDecimal("-7.5").signum() << ", "
              << BigDecimal("0.0").signum() << ", " << BigDecimal("7.5").signum() << '\n';
    std::cout << "abs y negate       : " << BigDecimal("-1.5").abs().toString()
              << " y " << BigDecimal("1.5").negate().toString() << '\n';
}
```

```text
a                  : 1.0 | escala 1 | precision 2 | coeficiente 10
b                  : 1.00 | escala 2 | precision 3 | coeficiente 100
a == b             : 0
a.equals(b)        : 0
a.compareTo(b)     : 0
mismo hashCode     : 0
a < b              : 0
600.0 normalizado  : 6E+2
0.00 normalizado   : 0
precision de 0.00  : 1
ulp de 1.00        : 0.01
min y max          : 2 y 3
signum             : -1, 0, 1
abs y negate       : 1.5 y -1.5
```

Usa `compareTo(rhs) == 0` (o los operadores `<`, `>`, `<=`, `>=`) para comparar
valores, y reserva `==`/`equals` para comprobar representaciones idénticas.

### Aritmética

```cpp
#include <bigdecimal/BigDecimal.h>
#include <iostream>

int main() {
    const BigDecimal a("5.00");
    const BigDecimal b("2");

    std::cout << "a + b                      : " << (a + b).toPlainString() << '\n';
    std::cout << "a - b                      : " << (a - b).toPlainString() << '\n';
    std::cout << "a * b                      : " << (a * b).toPlainString() << '\n';
    std::cout << "1 / 8 (exacta)             : " << (BigDecimal("1") / BigDecimal("8")).toPlainString() << '\n';
    std::cout << "a.divideToIntegralValue(b) : " << a.divideToIntegralValue(b).toPlainString() << '\n';
    std::cout << "a.remainder(b)             : " << a.remainder(b).toPlainString() << '\n';

    const auto par = a.divideAndRemainder(b);
    std::cout << "divideAndRemainder         : cociente " << par.first.toPlainString()
              << ", resto " << par.second.toPlainString() << '\n';

    std::cout << "1.25^3                     : " << BigDecimal("1.25").pow(3).toPlainString() << '\n';
    std::cout << "2^10                       : " << BigDecimal("2").pow(10).toPlainString() << '\n';
    std::cout << "raiz de 2 (16 cifras)      : " << BigDecimal("2").sqrt(MathContext::DECIMAL64).toString() << '\n';

    std::cout << "0.1 + 0.2                  : " << (BigDecimal("0.1") + BigDecimal("0.2")).toPlainString() << '\n';
    std::cout << "1.23 * 4.56                : " << (BigDecimal("1.23") * BigDecimal("4.56")).toPlainString() << '\n';
    std::cout << "100 - 33.33                : " << (BigDecimal("100") - BigDecimal("33.33")).toPlainString() << '\n';
}
```

```text
a + b                      : 7.00
a - b                      : 3.00
a * b                      : 10.00
1 / 8 (exacta)             : 0.125
a.divideToIntegralValue(b) : 2.00
a.remainder(b)             : 1.00
divideAndRemainder         : cociente 2.00, resto 1.00
1.25^3                     : 1.953125
2^10                       : 1024
raiz de 2 (16 cifras)      : 1.414213562373095
0.1 + 0.2                  : 0.3
1.23 * 4.56                : 5.6088
100 - 33.33                : 66.67
```

La suma y la resta toman la escala mayor; la multiplicación suma las escalas.
`0.1 + 0.2` da `0.3` exacto porque la aritmética es decimal, no binaria.

### División: exacta, con precisión y con escala

El operador `/` exige un resultado exacto: si el cociente no termina, lanza
`ArithmeticError`. Cuando necesites un resultado aproximado, indica o bien cifras
significativas con `MathContext`, o bien cifras decimales con
`divide(rhs, escala, modo)`.

```cpp
#include <bigdecimal/BigDecimal.h>
#include <bigdecimal/NativeExceptions.h>
#include <iostream>

int main() {
    std::cout << "1 / 8 (exacta)             : " << (BigDecimal("1") / BigDecimal("8")).toPlainString() << '\n';
    std::cout << "10 / 4 (exacta)            : " << (BigDecimal("10") / BigDecimal("4")).toPlainString() << '\n';
    std::cout << "20 / 4 con UNLIMITED       : " << BigDecimal("20").divide(BigDecimal("4"), MathContext::UNLIMITED).toPlainString() << '\n';

    try {
        (void)(BigDecimal("1") / BigDecimal("3"));
    } catch (const ArithmeticError&) {
        std::cout << "1 / 3 (exacta)             : ArithmeticError, la expansion no termina\n";
    }

    std::cout << "1 / 3 con DECIMAL128       : " << BigDecimal("1").divide(BigDecimal("3"), MathContext::DECIMAL128).toString() << '\n';
    std::cout << "10 / 3 con DECIMAL64       : " << BigDecimal("10").divide(BigDecimal("3"), MathContext::DECIMAL64).toString() << '\n';
    std::cout << "10 / 3 con DECIMAL32       : " << BigDecimal("10").divide(BigDecimal("3"), MathContext::DECIMAL32).toString() << '\n';

    std::cout << "7 / 2 con 3 decimales      : " << BigDecimal("7").divide(BigDecimal("2"), 3, RoundingMode::HalfUp).toPlainString() << '\n';
    std::cout << "10 / 3 con 2 decimales     : " << BigDecimal("10").divide(BigDecimal("3"), 2, RoundingMode::HalfUp).toPlainString() << '\n';
    std::cout << "1 / 8 con 2 decimales      : " << BigDecimal("1").divide(BigDecimal("8"), 2, RoundingMode::HalfEven).toPlainString() << '\n';

    const MathContext seisCifras(6, RoundingMode::HalfUp);
    std::cout << "123.456789 a 6 cifras      : " << BigDecimal("123.456789").round(seisCifras).toString() << '\n';

    std::cout << "DECIMAL32                  : " << MathContext::DECIMAL32.toString() << '\n';
    std::cout << "DECIMAL64                  : " << MathContext::DECIMAL64.toString() << '\n';
    std::cout << "DECIMAL128                 : " << MathContext::DECIMAL128.toString() << '\n';
    std::cout << "precision de DECIMAL128    : " << MathContext::DECIMAL128.getPrecision() << '\n';

    const MathContext propio(10, RoundingMode::HalfUp);
    std::cout << "MathContext propio         : " << propio.toString() << '\n';
    std::cout << "desde texto                : " << MathContext("precision=16 roundingMode=HALF_EVEN").toString() << '\n';
}
```

```text
1 / 8 (exacta)             : 0.125
10 / 4 (exacta)            : 2.5
20 / 4 con UNLIMITED       : 5
1 / 3 (exacta)             : ArithmeticError, la expansion no termina
1 / 3 con DECIMAL128       : 0.3333333333333333333333333333333333
10 / 3 con DECIMAL64       : 3.333333333333333
10 / 3 con DECIMAL32       : 3.333333
7 / 2 con 3 decimales      : 3.500
10 / 3 con 2 decimales     : 3.33
1 / 8 con 2 decimales      : 0.12
123.456789 a 6 cifras      : 123.457
DECIMAL32                  : precision=7 roundingMode=HALF_EVEN
DECIMAL64                  : precision=16 roundingMode=HALF_EVEN
DECIMAL128                 : precision=34 roundingMode=HALF_EVEN
precision de DECIMAL128    : 34
MathContext propio         : precision=10 roundingMode=HALF_UP
desde texto                : precision=16 roundingMode=HALF_EVEN
```

`MathContext` también está disponible en `add`, `subtract`, `multiply`, `abs`,
`negate`, `pow` y `divideToIntegralValue` mediante sus sobrecargas.

### Modos de redondeo

`RoundingMode` tiene ocho modos. El nombre en PascalCase es el idiomático en
C++; los alias en mayúsculas (`HALF_EVEN`, `UNNECESSARY`, ...) existen para
facilitar la migración desde otras APIs.

| Modo | Comportamiento |
| --- | --- |
| `Up` / `UP` | Aleja el resultado de cero. |
| `Down` / `DOWN` | Trunca hacia cero. |
| `Ceiling` / `CEILING` | Redondea hacia `+∞`. |
| `Floor` / `FLOOR` | Redondea hacia `-∞`. |
| `HalfUp` / `HALF_UP` | En un empate, aleja de cero. |
| `HalfDown` / `HALF_DOWN` | En un empate, acerca a cero. |
| `HalfEven` / `HALF_EVEN` | En un empate, elige el último dígito par. |
| `Unnecessary` / `UNNECESSARY` | Exige resultado exacto; si no, lanza `ArithmeticError`. |

```cpp
#include <bigdecimal/BigDecimal.h>
#include <bigdecimal/NativeExceptions.h>
#include <iostream>

namespace {
void fila(RoundingMode modo, const char* nombre) {
    const auto positivo = BigDecimal("2.5").setScale(0, modo).toPlainString();
    const auto negativo = BigDecimal("-2.5").setScale(0, modo).toPlainString();
    std::cout << nombre << " -> 2.5 = " << positivo << ", -2.5 = " << negativo << '\n';
}
}

int main() {
    fila(RoundingMode::Up, "Up");
    fila(RoundingMode::Down, "Down");
    fila(RoundingMode::Ceiling, "Ceiling");
    fila(RoundingMode::Floor, "Floor");
    fila(RoundingMode::HalfUp, "HalfUp");
    fila(RoundingMode::HalfDown, "HalfDown");
    fila(RoundingMode::HalfEven, "HalfEven");

    try {
        (void)BigDecimal("2.5").setScale(0, RoundingMode::Unnecessary);
    } catch (const ArithmeticError&) {
        std::cout << "Unnecessary -> 2.5 lanza ArithmeticError\n";
    }
    std::cout << "Unnecessary -> 2.0 = " << BigDecimal("2.0").setScale(0, RoundingMode::Unnecessary).toPlainString() << '\n';
    std::cout << "1.005 a 2 decimales (HalfUp) = " << BigDecimal("1.005").setScale(2, RoundingMode::HalfUp).toPlainString() << '\n';
}
```

```text
Up -> 2.5 = 3, -2.5 = -3
Down -> 2.5 = 2, -2.5 = -2
Ceiling -> 2.5 = 3, -2.5 = -2
Floor -> 2.5 = 2, -2.5 = -3
HalfUp -> 2.5 = 3, -2.5 = -3
HalfDown -> 2.5 = 2, -2.5 = -2
HalfEven -> 2.5 = 2, -2.5 = -2
Unnecessary -> 2.5 lanza ArithmeticError
Unnecessary -> 2.0 = 2
1.005 a 2 decimales (HalfUp) = 1.01
```

`setScale(escala)` sin modo equivale a `RoundingMode::Unnecessary`: solo sirve
cuando no hace falta redondear nada. Para importes en dos decimales lo habitual
es `setScale(2, RoundingMode::HalfUp)` o `HalfEven`.

### Conversiones y formatos

Las conversiones a enteros truncan; las variantes `*Exact()` fallan si hay parte
fraccionaria o si el valor no cabe. `toString()` puede usar notación científica,
`toPlainString()` nunca la usa y `toEngineeringString()` usa exponentes
múltiplos de tres.

```cpp
#include <bigdecimal/BigDecimal.h>
#include <bigdecimal/NativeExceptions.h>
#include <iomanip>
#include <iostream>

int main() {
    const BigDecimal valor("1234.5678");

    std::cout << "intValue          : " << valor.intValue() << '\n';
    std::cout << "longValue         : " << valor.longValue() << '\n';
    std::cout << "shortValue        : " << valor.shortValue() << '\n';
    std::cout << "byteValue         : " << static_cast<int>(valor.byteValue()) << '\n';
    std::cout << std::setprecision(10)
              << "doubleValue       : " << valor.doubleValue() << '\n'
              << "floatValue        : " << valor.floatValue() << '\n'
              << std::setprecision(6);
    std::cout << "toBigInteger      : " << valor.toBigInteger().toString() << '\n';
    std::cout << "longValueExact    : " << BigDecimal("1234").longValueExact() << '\n';
    std::cout << "intValueExact(42) : " << BigDecimal("42").intValueExact() << '\n';

    try {
        (void)valor.intValueExact();
    } catch (const ArithmeticError&) {
        std::cout << "intValueExact     : ArithmeticError, hay parte fraccionaria\n";
    }

    const BigDecimal pequeno("0.0000001");
    const BigDecimal potencia("1E+10");
    const BigDecimal grande("1234567890123456789012");

    std::cout << "grande  toString  : " << grande.toString() << '\n';
    std::cout << "grande  plano     : " << grande.toPlainString() << '\n';
    std::cout << "pequeno toString  : " << pequeno.toString() << '\n';
    std::cout << "pequeno plano     : " << pequeno.toPlainString() << '\n';
    std::cout << "pequeno ingenieria: " << pequeno.toEngineeringString() << '\n';
    std::cout << "1E+10   toString  : " << potencia.toString() << '\n';
    std::cout << "1E+10   plano     : " << potencia.toPlainString() << '\n';
    std::cout << "escala de 1E+10   : " << potencia.scale() << '\n';
    std::cout << "movePointLeft(2)  : " << BigDecimal("1.5").movePointLeft(2).toPlainString() << '\n';
    std::cout << "movePointRight(2) : " << BigDecimal("1.5").movePointRight(2).toPlainString() << '\n';
    std::cout << "scaleByPowerOfTen : " << BigDecimal("1.5").scaleByPowerOfTen(3).toPlainString() << '\n';
}
```

```text
intValue          : 1234
longValue         : 1234
shortValue        : 1234
byteValue         : -46
doubleValue       : 1234.5678
floatValue        : 1234.567749
toBigInteger      : 1234
longValueExact    : 1234
intValueExact(42) : 42
intValueExact     : ArithmeticError, hay parte fraccionaria
grande  toString  : 1234567890123456789012
grande  plano     : 1234567890123456789012
pequeno toString  : 1E-7
pequeno plano     : 0.0000001
pequeno ingenieria: 100E-9
1E+10   toString  : 1E+10
1E+10   plano     : 10000000000
escala de 1E+10   : -10
movePointLeft(2)  : 0.015
movePointRight(2) : 150
scaleByPowerOfTen : 1500
```

`byteValue()` y las demás conversiones estrechas devuelven el tipo nativo
correspondiente (`std::int8_t`), así que al imprimirlo conviene convertirlo a
`int` para ver el número y no el carácter.

## Guía de BigInteger

`BigInteger` es un entero con signo y precisión arbitraria. Se puede usar como
valor independiente o como coeficiente de un `BigDecimal` (`unscaledValue()`).

### Aritmética y modularidad

```cpp
#include <bigdecimal/BigInteger.h>
#include <iostream>
#include <vector>

int main() {
    const BigInteger a("12345678901234567890");
    const BigInteger b("97");

    std::cout << "a + b              : " << (a + b).toString() << '\n';
    std::cout << "a - b              : " << (a - b).toString() << '\n';
    std::cout << "a * b              : " << (a * b).toString() << '\n';
    std::cout << "a / b              : " << (a / b).toString() << '\n';
    std::cout << "a % b              : " << (a % b).toString() << '\n';

    const auto dr = a.divideAndRemainder(b);
    std::cout << "divideAndRemainder : cociente " << dr.first.toString()
              << ", resto " << dr.second.toString() << '\n';

    std::cout << "gcd                : " << a.gcd(b).toString() << '\n';
    std::cout << "mod                : " << a.mod(b).toString() << '\n';
    std::cout << "modPow(5, b)       : " << a.modPow(BigInteger("5"), b).toString() << '\n';
    std::cout << "modInverse(100, 7) : " << BigInteger("100").modInverse(BigInteger("7")).toString() << '\n';
    std::cout << "10^20              : " << BigInteger("10").pow(20).toString() << '\n';
    std::cout << "raiz de 10^20      : " << BigInteger("10").pow(20).sqrt().toString() << '\n';

    const auto sr = BigInteger("10").sqrtAndRemainder();
    std::cout << "sqrtAndRemainder 10: " << sr.first.toString() << " y resto " << sr.second.toString() << '\n';

    std::cout << "255 en base 16     : " << BigInteger("255").toString(16) << '\n';
    std::cout << "10 en base 2       : " << BigInteger("10").toString(2) << '\n';
    std::cout << "ff en base 16      : " << BigInteger("ff", 16).toString() << '\n';

    const auto bytes = BigInteger("255").toByteArray();
    std::cout << "toByteArray(255)   : " << bytes.size() << " bytes\n";
    std::cout << "complemento a dos  : " << BigInteger(std::vector<std::uint8_t>{0xFF}).toString() << '\n';
    std::cout << "signo y magnitud   : " << BigInteger(-1, std::vector<std::uint8_t>{0x01}).toString() << '\n';
    std::cout << "compareTo / equals : " << a.compareTo(b) << " / " << a.equals(a) << '\n';
    std::cout << "signum de -5       : " << BigInteger("-5").signum() << '\n';
    std::cout << "valueOf(42)        : " << BigInteger::valueOf(42).toString() << '\n';
}
```

```text
a + b              : 12345678901234567987
a - b              : 12345678901234567793
a * b              : 1197530853419753085330
a / b              : 127275040218913071
a % b              : 3
divideAndRemainder : cociente 127275040218913071, resto 3
gcd                : 1
mod                : 3
modPow(5, b)       : 49
modInverse(100, 7) : 4
10^20              : 100000000000000000000
raiz de 10^20      : 10000000000
sqrtAndRemainder 10: 3 y resto 1
255 en base 16     : ff
10 en base 2       : 1010
ff en base 16      : 255
toByteArray(255)   : 2 bytes
complemento a dos  : -1
signo y magnitud   : -1
compareTo / equals : 1 / 1
signum de -5       : -1
valueOf(42)        : 42
```

`toByteArray()` devuelve el valor en complemento a dos, con el byte de signo
cuando hace falta: `255` ocupa dos bytes (`0x00 0xFF`).

### Bits y desplazamientos

```cpp
#include <bigdecimal/BigInteger.h>
#include <iostream>

int main() {
    const BigInteger n("255");

    std::cout << "255 en binario       : " << n.toString(2) << '\n';
    std::cout << "bitLength            : " << n.bitLength() << '\n';
    std::cout << "bitCount             : " << n.bitCount() << '\n';
    std::cout << "getLowestSetBit de 8 : " << BigInteger("8").getLowestSetBit() << '\n';
    std::cout << "getLowestSetBit de 0 : " << BigInteger("0").getLowestSetBit() << '\n';

    std::cout << "255 & 15             : " << n.and_(BigInteger("15")).toString() << '\n';
    std::cout << "255 | 256            : " << n.or_(BigInteger("256")).toString() << '\n';
    std::cout << "255 ^ 15             : " << n.xor_(BigInteger("15")).toString() << '\n';
    std::cout << "~255                 : " << n.not_().toString() << '\n';
    std::cout << "255 & ~15            : " << n.andNot(BigInteger("15")).toString() << '\n';
    std::cout << "255 << 4             : " << n.shiftLeft(4).toString() << '\n';
    std::cout << "255 >> 4             : " << n.shiftRight(4).toString() << '\n';

    const BigInteger cinco("5");
    std::cout << "5 en binario         : " << cinco.toString(2) << '\n';
    std::cout << "testBit(0) / (1)     : " << cinco.testBit(0) << " / " << cinco.testBit(1) << '\n';
    std::cout << "setBit(1)            : " << cinco.setBit(1).toString() << '\n';
    std::cout << "clearBit(0)          : " << cinco.clearBit(0).toString() << '\n';
    std::cout << "flipBit(0)           : " << cinco.flipBit(0).toString() << '\n';
}
```

```text
255 en binario       : 11111111
bitLength            : 8
bitCount             : 8
getLowestSetBit de 8 : 3
getLowestSetBit de 0 : -1
255 & 15             : 15
255 | 256            : 511
255 ^ 15             : 240
~255                 : -256
255 & ~15            : 240
255 << 4             : 4080
255 >> 4             : 15
5 en binario         : 101
testBit(0) / (1)     : 1 / 0
setBit(1)            : 7
clearBit(0)          : 4
flipBit(0)           : 4
```

Los operadores `&`, `|`, `^`, `~`, `<<` y `>>` son equivalentes a `and_`,
`or_`, `xor_`, `not_` y los desplazamientos; llevan guion bajo para no chocar
con los operadores de bits de los enteros nativos.

### Números primos

Las funciones que generan números aleatorios reciben un `std::mt19937_64&`, de
modo que la fuente aleatoria es explícita y reproducible: con la misma semilla
se obtienen los mismos valores.

```cpp
#include <bigdecimal/BigInteger.h>
#include <iostream>
#include <random>

int main() {
    std::mt19937_64 aleatorio(42);

    const BigInteger primo = BigInteger::probablePrime(64, aleatorio);
    std::cout << "primo de 64 bits      : " << primo.toString() << '\n';
    std::cout << "bitLength             : " << primo.bitLength() << '\n';
    std::cout << "isProbablePrime(100)  : " << primo.isProbablePrime(100) << '\n';

    std::cout << "97 es primo           : " << BigInteger("97").isProbablePrime(100) << '\n';
    std::cout << "100 es primo          : " << BigInteger("100").isProbablePrime(100) << '\n';
    std::cout << "siguiente primo a 100 : " << BigInteger("100").nextProbablePrime().toString() << '\n';
    std::cout << "siguiente primo a 1000: " << BigInteger("1000").nextProbablePrime().toString() << '\n';

    std::mt19937_64 otraSemilla(7);
    const BigInteger alAzar(32, otraSemilla);
    std::cout << "entero al azar 32 bits: bitLength " << alAzar.bitLength() << '\n';
}
```

```text
primo de 64 bits      : 13146606714899012041
bitLength             : 64
isProbablePrime(100)  : 1
97 es primo           : 1
100 es primo          : 0
siguiente primo a 100 : 101
siguiente primo a 1000: 1009
entero al azar 32 bits: bitLength 32
```

La certeza se expresa como número de rondas de Miller-Rabin: `100` es un valor
habitual para uso general.

## Serialización

Incluye `<bigdecimal/BigDecimalSerialization.h>` y usa el espacio de nombres
`BigDecimalSerialization`. El codec implementa el formato de serialización de
objetos binario con encabezado `0xACED0005`, descriptores de clase y
`serialVersionUID`, de modo que los datos pueden intercambiarse con cualquier
otra implementación de ese formato para los tipos indicados.

```cpp
#include <bigdecimal/BigDecimalSerialization.h>
#include <bigdecimal/NativeExceptions.h>
#include <iomanip>
#include <iostream>
#include <vector>

int main() {
    using namespace BigDecimalSerialization;

    const auto bytes = serialize(BigDecimal("12.3400"));
    std::cout << "bytes de 12.3400      : " << bytes.size() << '\n';
    std::cout << "cabecera              : " << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < 4; ++i) {
        std::cout << std::setw(2) << static_cast<int>(bytes[i]);
    }
    std::cout << std::dec << std::setfill(' ') << '\n';

    std::cout << "BigDecimal restaurado : " << deserializeBigDecimal(bytes).toString() << '\n';

    const std::vector<Value> valores = {
        BigDecimal("12.34"), BigInteger("1234"),
        MathContext::DECIMAL64, RoundingMode::HalfEven
    };
    const auto flujo = serializeAll(valores);
    const auto restaurados = deserializeAll(flujo);
    std::cout << "valores en el flujo   : " << valores.size() << '\n';
    std::cout << "valores restaurados   : " << restaurados.size() << '\n';

    ReadLimits limites;
    limites.maxInputBytes = 1024;
    limites.maxHandles = 1000;
    limites.maxDepth = 16;
    std::cout << "con limites de lectura: " << deserializeBigDecimal(bytes, limites).toString() << '\n';

    try {
        (void)deserializeBigDecimal({0x00, 0x01, 0x02, 0x03, 0x04});
    } catch (const CorruptStreamError&) {
        std::cout << "cabecera invalida     : CorruptStreamError\n";
    }
}
```

```text
bytes de 12.3400      : 293
cabecera              : aced0005
BigDecimal restaurado : 12.3400
valores en el flujo   : 4
valores restaurados   : 4
con limites de lectura: 12.3400
cabecera invalida     : CorruptStreamError
```

`ReadLimits` permite limitar tamaño de entrada, cantidad de referencias y
profundidad al decodificar datos no confiables. El codec no restaura grafos
arbitrarios de objetos: solo los tipos indicados.

## Manejo de errores

Todas las excepciones de la biblioteca derivan de excepciones estándar de C++,
así que puedes capturarlas por su categoría específica o por la jerarquía
estándar. `NativeExceptions.h` define los tipos.

```cpp
#include <bigdecimal/BigDecimal.h>
#include <bigdecimal/BigInteger.h>
#include <bigdecimal/NativeExceptions.h>
#include <iostream>

int main() {
    try {
        const BigDecimal invalido("12,34");
        std::cout << invalido.toString() << '\n';
    } catch (const NumberFormatError& error) {
        std::cout << "texto invalido  -> NumberFormatError: " << error.what() << '\n';
    }

    try {
        (void)(BigDecimal("1") / BigDecimal("3"));
    } catch (const ArithmeticError& error) {
        std::cout << "1 / 3           -> ArithmeticError: " << error.what() << '\n';
    }

    try {
        (void)BigDecimal("1.005").setScale(2);
    } catch (const ArithmeticError& error) {
        std::cout << "setScale(2)     -> ArithmeticError: " << error.what() << '\n';
    }

    try {
        (void)BigDecimal("1E+30").longValueExact();
    } catch (const ArithmeticError& error) {
        std::cout << "longValueExact  -> ArithmeticError: " << error.what() << '\n';
    }

    try {
        const BigInteger invalido("10", 40);
        std::cout << invalido.toString() << '\n';
    } catch (const NumberFormatError& error) {
        std::cout << "base 40         -> NumberFormatError: " << error.what() << '\n';
    }

    try {
        (void)(BigDecimal("1") / BigDecimal("3"));
    } catch (const std::domain_error& error) {
        std::cout << "captura generica-> std::domain_error: " << error.what() << '\n';
    }
}
```

```text
texto invalido  -> NumberFormatError: Character , is neither a decimal digit number, decimal point, nor "e" notation exponential mark.
1 / 3           -> ArithmeticError: Non-terminating decimal expansion; no exact representable decimal result.
setScale(2)     -> ArithmeticError: Rounding necessary
longValueExact  -> ArithmeticError: Overflow
base 40         -> NumberFormatError: Radix out of range
captura generica-> std::domain_error: Non-terminating decimal expansion; no exact representable decimal result.
```

| Tipo | Hereda de | Uso típico |
| --- | --- | --- |
| `ArithmeticError` | `std::domain_error` | División no exacta, redondeo necesario, desbordamiento, raíz de un negativo. |
| `NumberFormatError` | `InvalidArgumentError` | Texto o base inválidos al construir. |
| `InvalidArgumentError` | `std::invalid_argument` | Argumentos fuera de rango. |
| `NullPointerError` | `std::logic_error` | Uso indebido de un valor nulo. |
| `IOError` | `std::runtime_error` | Errores de entrada/salida. |
| `CorruptStreamError` | `IOError` | Flujo serializado dañado o incompatible. |
| `AllocationError` | `std::length_error` | Tamaño por encima de los límites disponibles. |

## Caso práctico: factura con IGV

Ejemplo completo de un cálculo comercial: líneas de factura, descuento, IGV del
18% y totales, todo con escala 2 y redondeo `HalfUp`.

```cpp
#include <bigdecimal/BigDecimal.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Linea {
    std::string descripcion;
    BigDecimal precio;
    BigDecimal cantidad;
};

}

int main() {
    const std::vector<Linea> lineas = {
        {"Monitor 27\"", BigDecimal("899.90"), BigDecimal("2")},
        {"Teclado",      BigDecimal("129.50"), BigDecimal("3")},
        {"Cable HDMI",   BigDecimal("35.00"),  BigDecimal("5")},
    };

    std::cout << std::left << std::setw(14) << "Descripcion" << std::right
              << std::setw(10) << "Precio" << std::setw(6) << "Cant"
              << std::setw(12) << "Importe" << '\n';

    BigDecimal subtotal("0.00");
    for (const auto& linea : lineas) {
        const BigDecimal importe = (linea.precio * linea.cantidad).setScale(2, RoundingMode::HalfUp);
        subtotal = subtotal.add(importe);
        std::cout << std::left << std::setw(14) << linea.descripcion << std::right
                  << std::setw(10) << linea.precio.toPlainString()
                  << std::setw(6) << linea.cantidad.toPlainString()
                  << std::setw(12) << importe.toPlainString() << '\n';
    }

    const BigDecimal descuento = subtotal.multiply(BigDecimal("0.075"), MathContext::DECIMAL128)
                                     .setScale(2, RoundingMode::HalfUp);
    const BigDecimal base = subtotal.subtract(descuento);
    const BigDecimal igv = base.multiply(BigDecimal("0.18"), MathContext::DECIMAL128)
                               .setScale(2, RoundingMode::HalfUp);
    const BigDecimal total = base.add(igv);

    std::cout << std::string(42, '-') << '\n';
    std::cout << std::left << std::setw(32) << "Subtotal" << std::right << std::setw(10) << subtotal.toPlainString() << '\n';
    std::cout << std::left << std::setw(32) << "Descuento (7.5%)" << std::right << std::setw(10) << descuento.toPlainString() << '\n';
    std::cout << std::left << std::setw(32) << "Base imponible" << std::right << std::setw(10) << base.toPlainString() << '\n';
    std::cout << std::left << std::setw(32) << "IGV (18%)" << std::right << std::setw(10) << igv.toPlainString() << '\n';
    std::cout << std::left << std::setw(32) << "Total a pagar" << std::right << std::setw(10) << total.toPlainString() << '\n';
}
```

```text
Descripcion       Precio  Cant     Importe
Monitor 27"       899.90     2     1799.80
Teclado           129.50     3      388.50
Cable HDMI         35.00     5      175.00
------------------------------------------
Subtotal                           2363.30
Descuento (7.5%)                    177.25
Base imponible                     2186.05
IGV (18%)                           393.49
Total a pagar                      2579.54
```

Pautas que ilustra el ejemplo:

- Guarda los importes con escala 2 y redondea solo al final de cada partida.
- Usa `MathContext::DECIMAL128` en los porcentajes intermedios para no arrastrar
  errores de redondeo y aplica `setScale(2, HalfUp)` al importe definitivo.
- Muestra los importes con `toPlainString()` para evitar notación científica.
- Suma con `add` en lugar de acumular en `double`: el resultado es exacto.

## Referencia rápida

### BigDecimal

| Necesitas | API |
| --- | --- |
| Crear desde texto, entero, `double` o `BigInteger` | `BigDecimal(...)`, `BigDecimal::valueOf(...)` |
| Ver escala, precisión y coeficiente | `scale()`, `precision()`, `unscaledValue()`, `signum()` |
| Sumar, restar, multiplicar | `+`, `-`, `*`, `add`, `subtract`, `multiply` |
| Dividir con resultado exacto | `/`, `divide(rhs)` |
| Dividir con precisión o escala | `divide(rhs, MathContext)`, `divide(rhs, escala, modo)` |
| Cociente entero y resto | `divideToIntegralValue`, `remainder`, `divideAndRemainder` |
| Potencia y raíz | `pow(exp)`, `pow(exp, ctx)`, `sqrt(ctx)` |
| Ajustar decimales | `setScale(escala, modo)` |
| Redondear a cifras significativas | `round(MathContext)`, `round(cifras, modo)` |
| Comparar valor o representación | `compareTo`, `equals`, `==`, `<`, `>` |
| Convertir a nativo | `intValue`, `longValue`, `doubleValue`, `*Exact()` |
| Convertir a entero grande | `toBigInteger`, `toBigIntegerExact` |
| Formatear | `toString`, `toPlainString`, `toEngineeringString` |
| Utilidades | `abs`, `negate`, `plus`, `min`, `max`, `ulp`, `movePointLeft`, `movePointRight`, `scaleByPowerOfTen`, `stripTrailingZeros`, `hashCode` |

### BigInteger

| Necesitas | API |
| --- | --- |
| Crear desde texto, base o bytes | `BigInteger(texto)`, `BigInteger(texto, base)`, `BigInteger(bytes)`, `BigInteger(signo, magnitud)` |
| Aritmética | `+`, `-`, `*`, `/`, `%`, `add`, `subtract`, `multiply`, `divide`, `remainder` |
| Cociente y resto | `divideAndRemainder` |
| Potencia y raíz | `pow`, `sqrt`, `sqrtAndRemainder` |
| Modularidad | `mod`, `modPow`, `modInverse`, `gcd` |
| Bits | `and_`, `or_`, `xor_`, `not_`, `andNot`, `testBit`, `setBit`, `clearBit`, `flipBit`, `shiftLeft`, `shiftRight`, `bitLength`, `bitCount`, `getLowestSetBit` |
| Primalidad | `isProbablePrime`, `nextProbablePrime`, `probablePrime` |
| Conversión | `toString(base)`, `toByteArray`, `intValue`/`longValue`(+`Exact`), `doubleValue` |
| Comparación | `compareTo`, `equals`, `signum`, `min`, `max` |

### Serialización

| Necesitas | API |
| --- | --- |
| Codificar un valor | `serialize(valor)` |
| Decodificar un valor | `deserializeBigDecimal`, `deserializeBigInteger`, `deserializeMathContext`, `deserializeRoundingMode` |
| Varios valores en un flujo | `serializeAll(vector<Value>)`, `deserializeAll(bytes)` |
| Limitar datos no confiables | `ReadLimits{maxInputBytes, maxHandles, maxDepth}` |

## Errores y límites

Las operaciones inválidas lanzan excepciones C++ estándar derivadas —según el
caso— de `std::domain_error`, `std::invalid_argument`, `std::logic_error`,
`std::runtime_error` o `std::length_error`. `NativeExceptions.h` ofrece tipos
específicos como `ArithmeticError`, `NumberFormatError`, `IOError` y
`CorruptStreamError`, compatibles con esas jerarquías de captura.

La memoria y los límites de tamaño dependen de la plataforma y de la memoria
disponible. Las excepciones, la gestión de memoria y el rendimiento son los
nativos de C++: no se emula ninguna máquina virtual ni su recolector,
optimizaciones o comportamiento interno. La compatibilidad de resultados debe
evaluarse con pruebas para las operaciones y casos límite que use cada
aplicación.

## Licencia y atribución

El código matemático incluye adaptaciones de algoritmos y comportamiento de una
implementación de referencia; los avisos de origen se conservan en los
encabezados. Consulta `NOTICE` para la atribución completa y `LICENSE` para GPLv2
con Classpath Exception. Conserva los avisos de origen al redistribuir.
