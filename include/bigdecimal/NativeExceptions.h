#ifndef BIGDECIMAL_NATIVE_EXCEPTIONS_H
#define BIGDECIMAL_NATIVE_EXCEPTIONS_H
#include <stdexcept>
#include <string>
#include <utility>

// Las categorías de error nativas conservan jerarquías útiles de captura de C++.
enum class NativeErrorCode {
    Arithmetic, InvalidArgument, NullPointer, Type, IO, Allocation,
    NumberFormat, InvalidObject, CorruptStream, EndOfFile, InvalidClass
};
class NativeExceptionInfo {
public:
    virtual ~NativeExceptionInfo() = default;
    [[nodiscard]] virtual NativeErrorCode errorCode() const noexcept = 0;
    [[nodiscard]] virtual bool hasMessage() const noexcept = 0;
};
#define BIGDECIMAL_NATIVE_ERROR(Name, Standard, Code) \
class Name : public Standard, public NativeExceptionInfo { \
public: \
    Name() : Standard(""), present_(false) {} \
    explicit Name(std::string message) : Standard(message), present_(true) {} \
    NativeErrorCode errorCode() const noexcept override { return NativeErrorCode::Code; } \
    bool hasMessage() const noexcept override { return present_; } \
private: bool present_ = true; \
};
BIGDECIMAL_NATIVE_ERROR(ArithmeticError, std::domain_error, Arithmetic)
BIGDECIMAL_NATIVE_ERROR(InvalidArgumentError, std::invalid_argument, InvalidArgument)
BIGDECIMAL_NATIVE_ERROR(NullPointerError, std::logic_error, NullPointer)
BIGDECIMAL_NATIVE_ERROR(TypeError, std::logic_error, Type)
BIGDECIMAL_NATIVE_ERROR(IOError, std::runtime_error, IO)
BIGDECIMAL_NATIVE_ERROR(AllocationError, std::length_error, Allocation)
#undef BIGDECIMAL_NATIVE_ERROR
class NumberFormatError : public InvalidArgumentError {
public:
    using InvalidArgumentError::InvalidArgumentError;
    NativeErrorCode errorCode() const noexcept override { return NativeErrorCode::NumberFormat; }
};
#define BIGDECIMAL_NATIVE_IO_ERROR(Name, Code) \
class Name : public IOError { \
public: using IOError::IOError; \
    NativeErrorCode errorCode() const noexcept override { return NativeErrorCode::Code; } \
};
BIGDECIMAL_NATIVE_IO_ERROR(InvalidObjectError, InvalidObject)
BIGDECIMAL_NATIVE_IO_ERROR(CorruptStreamError, CorruptStream)
BIGDECIMAL_NATIVE_IO_ERROR(EndOfFileError, EndOfFile)
BIGDECIMAL_NATIVE_IO_ERROR(InvalidClassError, InvalidClass)
#undef BIGDECIMAL_NATIVE_IO_ERROR
#endif
