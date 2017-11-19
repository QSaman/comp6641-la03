#pragma once

#include <string>
#include <iostream>
#include <memory>

namespace saman
{
template<typename Value>
std::ostream& binary_write(std::ostream& out, const Value& value)
{
    static_assert(std::is_fundamental<Value>::value || std::is_enum<Value>::value, "Unsupported type");
    return out.write(reinterpret_cast<const char*>(&value), sizeof(Value));
}

//template specialization. See http://en.cppreference.com/w/cpp/language/template_specialization
template<>
std::ostream& binary_write(std::ostream& out, const std::string& value)
{
    return out.write(value.c_str(), static_cast<std::streamsize>(value.length()));
}

template<typename Value>
std::istream& binary_read(std::istream& in, Value& value)
{
    static_assert(std::is_fundamental<Value>::value || std::is_enum<Value>::value, "Unsupported type");
    return in.read(reinterpret_cast<char*>(&value), sizeof (Value));
}

std::istream& binary_read_string(std::istream& in, std::string& str, unsigned int str_len)
{
    std::unique_ptr<char> buffer(new char[str_len]);
    auto& ret = in.read(buffer.get(), str_len);
    str = std::string(buffer.release(), str_len);
    return ret;
}
}
