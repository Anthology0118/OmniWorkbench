#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace omni {

/**
 * Lightweight, RFC-8259 compliant JSON value representation for C++17.
 */
struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };

    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;

    Type type = Type::Null;
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    Array arr_val;
    Object obj_val;

    JsonValue() = default;
    JsonValue(std::nullptr_t) : type(Type::Null) {}
    JsonValue(bool b) : type(Type::Bool), bool_val(b) {}
    JsonValue(int i) : type(Type::Number), num_val(static_cast<double>(i)) {}
    JsonValue(int64_t i) : type(Type::Number), num_val(static_cast<double>(i)) {}
    JsonValue(size_t i) : type(Type::Number), num_val(static_cast<double>(i)) {}
    JsonValue(double d) : type(Type::Number), num_val(d) {}
    JsonValue(const char* s) : type(Type::String), str_val(s ? s : "") {}
    JsonValue(std::string s) : type(Type::String), str_val(std::move(s)) {}
    JsonValue(Array a) : type(Type::Array), arr_val(std::move(a)) {}
    JsonValue(Object o) : type(Type::Object), obj_val(std::move(o)) {}

    static JsonValue make_array() { return JsonValue(Array{}); }
    static JsonValue make_object() { return JsonValue(Object{}); }

    bool is_null() const { return type == Type::Null; }
    bool is_bool() const { return type == Type::Bool; }
    bool is_number() const { return type == Type::Number; }
    bool is_string() const { return type == Type::String; }
    bool is_array() const { return type == Type::Array; }
    bool is_object() const { return type == Type::Object; }

    std::string as_string(const std::string& def = "") const {
        return type == Type::String ? str_val : def;
    }
    bool as_bool(bool def = false) const {
        return type == Type::Bool ? bool_val : def;
    }
    double as_double(double def = 0.0) const {
        return type == Type::Number ? num_val : def;
    }
    int as_int(int def = 0) const {
        return type == Type::Number ? static_cast<int>(num_val) : def;
    }

    const JsonValue& get(const std::string& key) const;
    bool has(const std::string& key) const;
    JsonValue& operator[](const std::string& key);
    void push_back(JsonValue val);

    std::string dump(int indent = -1) const;
    static bool parse(const std::string& text, JsonValue& out, std::string* err = nullptr);
};

std::string json_escape(const std::string& s);

} // namespace omni
