# BigDecimal C++

Biblioteca de aritmética decimal de precisión arbitraria, nativa y escrita en
C++17. Incluye también `BigInteger`, contextos de precisión, redondeo y un
codec para intercambiar los tipos matemáticos admitidos mediante el formato de
serialización de objetos documentado por Java. No necesita una instalación de
Java para compilar ni para ejecutar.

## Contenido

- [Características](#características)
- [Requisitos](#requisitos)
- [Compilar, probar y ejecutar](#compilar-probar-y-ejecutar)
- [Instalar](#instalar)
- [Usar desde otro proyecto CMake](#usar-desde-otro-proyecto-cmake)
- [Guía de BigDecimal](#guía-de-bigdecimal)
- [Guía de BigInteger](#guía-de-biginteger)
- [Serialización interoperable](#serialización-interoperable)
- [Redondeo](#redondeo)
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

## Compilar, probar y ejecutar

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

## Instalar

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build --prefix "$HOME/.local"
```

La instalación coloca la biblioteca, los encabezados y los archivos de
configuración de CMake bajo el prefijo elegido. Para otro prefijo, sustituye
`$HOME/.local` por su ruta.

## Usar desde otro proyecto CMake

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

## Guía de BigDecimal

### Crear valores y conservar la escala

El constructor de texto conserva la escala escrita. Por ejemplo, `"1.0"` y
`"1.00"` representan el mismo valor numérico, pero tienen escalas distintas.

```cpp
#include <bigdecimal/BigDecimal.h>
#include <iostream>

int main() {
    const BigDecimal a("1.00");
    const BigDecimal b("3");

    std::cout << a.toString() << '\n';       // 1.00
    std::cout << (a + b).toPlainString() << '\n'; // 4.00
    std::cout << (a * b).toString() << '\n'; // 3.00
}
```

Se admiten constructores desde `std::string`, `const char*`, enteros de 32 y
64 bits, `double`, `BigInteger` y texto UTF-16 (`std::u16string` o segmentos
`std::vector<char16_t>`). Para crear un decimal desde un `double`:

```cpp
const BigDecimal exacto(0.1);                   // valor binario exacto de 0.1
const BigDecimal decimalCorto = BigDecimal::valueOf(0.1); // texto decimal corto
```

`BigDecimal::ZERO`, `ONE`, `TWO` y `TEN` son constantes útiles. `unscaledValue()`
devuelve el coeficiente entero; `scale()` devuelve la escala y
`precision()` la cantidad de cifras significativas.

### Operaciones

```cpp
const BigDecimal a("5.00");
const BigDecimal b("2");

auto suma       = a + b;
auto diferencia = a - b;
auto producto   = a * b;
auto exacto     = BigDecimal("1") / BigDecimal("8"); // 0.125
auto integral   = a.divideToIntegralValue(b);          // 2.00
auto resto      = a.remainder(b);                      // 1.00
auto par        = a.divideAndRemainder(b);             // cociente y resto
auto potencia   = BigDecimal("1.25").pow(3);          // 1.953125
auto raiz       = BigDecimal("2").sqrt(MathContext::DECIMAL64);

auto plano      = a.toPlainString();
auto ingenieria = a.toEngineeringString();
auto normal     = a.toString();
```

La división exacta `/` falla si el resultado tiene una expansión decimal
infinita, como `1/3`. Usa un `MathContext` o una escala y un modo de redondeo
cuando quieras un resultado aproximado.

### Precisión significativa y escala fija

`MathContext` limita las cifras significativas. `setScale` y la sobrecarga de
división con escala fijan las cifras decimales:

```cpp
const BigDecimal uno("1");

const auto tercero = uno.divide(BigDecimal("3"), MathContext::DECIMAL128);
const auto centavos = uno.divide(BigDecimal("8"), 2, RoundingMode::HALF_EVEN);
const auto conTresDecimales = BigDecimal("2").setScale(3, RoundingMode::UNNECESSARY);
const auto contexto = MathContext(6, RoundingMode::HALF_UP);
const auto redondeado = BigDecimal("123.456789").round(contexto);
```

`MathContext` también ofrece `UNLIMITED`, `DECIMAL32`, `DECIMAL64` y
`DECIMAL128`, además de `getPrecision()`, `getRoundingMode()`, `toString()`,
`equals()` y `hashCode()`.

### Comparar valores

La igualdad de `BigDecimal` conserva la escala: `BigDecimal("1.0")` no es igual
con `==` a `BigDecimal("1.00")`. Para comparar solo el valor numérico, usa
`compareTo()`:

```cpp
const BigDecimal unoDecimal("1.0");
const BigDecimal unoDosDecimales("1.00");

bool mismaRepresentacion = unoDecimal == unoDosDecimales; // false
bool mismoValor = unoDecimal.compareTo(unoDosDecimales) == 0; // true
```

`equals()` también compara escala. `stripTrailingZeros()` elimina ceros finales
y `min()`, `max()` y `ulp()` completan las funciones de comparación y escala.

### Conversión a tipos nativos

Las funciones `intValue()`, `longValue()`, `shortValue()` y `byteValue()` siguen
la conversión truncada de los tipos enteros correspondientes. Sus variantes
`*Exact()` fallan si hay parte fraccionaria o el resultado no cabe. `floatValue()`
y `doubleValue()` realizan conversión binaria con redondeo. Para ir del decimal
a un entero arbitrario, usa `toBigInteger()` o `toBigIntegerExact()`.

También están disponibles `movePointLeft()`, `movePointRight()`,
`scaleByPowerOfTen()`, `abs()`, `negate()`, `plus()`, `round()` y
`stripTrailingZeros()`.

## Guía de BigInteger

`BigInteger` es un entero con signo y precisión arbitraria. Se puede usar como
valor independiente o como coeficiente de `BigDecimal`:

```cpp
#include <bigdecimal/BigInteger.h>

const BigInteger a("12345678901234567890");
const BigInteger b("97");

auto suma = a + b;
auto producto = a * b;
auto division = a.divideAndRemainder(b);
auto mcd = a.gcd(b);
auto modulo = a.mod(b);
auto potenciaModular = a.modPow(BigInteger("5"), b);
auto raizEntera = a.sqrt();
auto bytesComplementoDos = a.toByteArray();
```

La API incluye:

- Construcción desde texto decimal o con base entre 2 y 36, enteros con signo,
  bytes en complemento a dos y magnitud con signo.
- `add`, `subtract`, `multiply`, `divide`, `remainder` y `divideAndRemainder`;
  los operadores `+`, `-`, `*`, `/` y `%` son equivalentes.
- `pow`, `sqrt`, `sqrtAndRemainder`, `gcd`, `mod`, `modPow` y `modInverse`.
- Desplazamientos, operaciones de bits, `testBit`, `setBit`, `clearBit`,
  `flipBit`, `getLowestSetBit`, `bitLength` y `bitCount`.
- `isProbablePrime`, `nextProbablePrime` y `probablePrime`. Las funciones que
  generan números reciben un `std::mt19937_64&` para controlar la fuente aleatoria.
- Conversiones a texto, complemento a dos y tipos enteros/floating-point nativos.

`BigInteger::ZERO`, `ONE`, `TWO` y `TEN` son constantes. Para resultados que
deben caber en un tipo nativo, utiliza `intValueExact()`, `longValueExact()`,
`shortValueExact()` o `byteValueExact()`.

## Serialización interoperable

Incluye `<bigdecimal/BigDecimalSerialization.h>` y usa el espacio de nombres
`BigDecimalSerialization`. El codec puede codificar y decodificar los valores
`BigDecimal`, `BigInteger`, `MathContext` y `RoundingMode`:

```cpp
#include <bigdecimal/BigDecimalSerialization.h>

const BigDecimal original("12.3400");
const auto bytes = BigDecimalSerialization::serialize(original);
const auto copia = BigDecimalSerialization::deserializeBigDecimal(bytes);

const std::vector<BigDecimalSerialization::Value> valores = {
    BigDecimal("12.34"), BigInteger("1234"),
    MathContext::DECIMAL64, RoundingMode::HALF_EVEN
};
const auto flujo = BigDecimalSerialization::serializeAll(valores);
const auto restaurados = BigDecimalSerialization::deserializeAll(flujo);
```

`ReadLimits` permite limitar tamaño de entrada, cantidad de referencias y
profundidad al decodificar datos no confiables. Este codec implementa el formato
de serialización compatible para los tipos indicados; no restaura grafos
arbitrarios de objetos.

## Redondeo

| Modo | Comportamiento en un empate |
| --- | --- |
| `Up` / `UP` | Aleja el resultado de cero. |
| `Down` / `DOWN` | Trunca hacia cero. |
| `Ceiling` / `CEILING` | Redondea hacia `+∞`. |
| `Floor` / `FLOOR` | Redondea hacia `-∞`. |
| `HalfUp` / `HALF_UP` | Aleja de cero. |
| `HalfDown` / `HALF_DOWN` | Acerca a cero. |
| `HalfEven` / `HALF_EVEN` | Elige el último dígito par. |
| `Unnecessary` / `UNNECESSARY` | Exige resultado exacto; si no, produce error aritmético. |

Los nombres PascalCase son idiomáticos en C++; también se ofrecen los aliases
en mayúsculas para facilitar la migración.

## Errores y límites

Las operaciones inválidas lanzan excepciones C++ estándar derivadas —según el
caso— de `std::domain_error`, `std::invalid_argument`, `std::logic_error`,
`std::runtime_error` o `std::length_error`. `NativeExceptions.h` ofrece tipos
específicos como `ArithmeticError`, `NumberFormatError`, `IOError` y
`CorruptStreamError`, compatibles con esas jerarquías de captura.

La memoria y los límites de tamaño dependen de la plataforma y de la memoria
disponible. No se emulan la JVM ni sus excepciones, recolector, optimizaciones o
rendimiento interno. La compatibilidad de resultados debe evaluarse con pruebas
para las operaciones y casos límite que use cada aplicación.

## Licencia y atribución

El código matemático incluye adaptaciones de algoritmos y comportamiento de
OpenJDK 23.0.2. Consulta `NOTICE` para la atribución y `LICENSE` para GPLv2 con
Classpath Exception. Conserva los avisos de origen al redistribuir. La biblioteca
nativa no requiere un runtime Java.
