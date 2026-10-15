#include <bigdecimal/BigDecimal.h>
#include <iostream>

int main() {
    const BigDecimal price("19.99"), quantity("3");
    const auto total = (price * quantity).setScale(2);
    const auto third = BigDecimal("1").divide(BigDecimal("3"), MathContext(34, RoundingMode::HalfEven));
    std::cout << "Total: " << total.toPlainString() << '\n';
    std::cout << "1/3: " << third.toString() << '\n';
}
