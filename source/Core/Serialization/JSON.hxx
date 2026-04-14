#pragma once

#include "Solstice.hxx"
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <memory>
#include <future>
#include <optional>

namespace Solstice::Core {

enum class JSONType {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

class JSONValue;
using JSONArray = std::vector<JSONValue>;
using JSONObject = std::map<std::string, JSONValue>;

class SOLSTICE_API JSONValue {
public:
    JSONValue();
    JSONValue(std::nullptr_t);
    JSONValue(bool Value);
    JSONValue(double Value);
    JSONValue(int Value);
    JSONValue(const std::string& Value);
    JSONValue(const char* Value);
    JSONValue(JSONArray Value);
    JSONValue(JSONObject Value);

    JSONType GetType() const;

    bool IsNull() const { return GetType() == JSONType::Null; }
    bool IsBool() const { return GetType() == JSONType::Bool; }
    bool IsNumber() const { return GetType() == JSONType::Number; }
    bool IsString() const { return GetType() == JSONType::String; }
    bool IsArray() const { return GetType() == JSONType::Array; }
    bool IsObject() const { return GetType() == JSONType::Object; }

    bool AsBool() const;
    double AsDouble() const;
    int AsInt() const;
    const std::string& AsString() const;
    const JSONArray& AsArray() const;
    const JSONObject& AsObject() const;

    JSONValue& operator[](const std::string& Key);
    const JSONValue& operator[](const std::string& Key) const;
    JSONValue& operator[](size_t Index);
    const JSONValue& operator[](size_t Index) const;

    bool HasKey(const std::string& Key) const;
    size_t Size() const;

    std::string Stringify(bool Pretty = false) const;

private:
    std::variant<
        std::nullptr_t,
        bool,
        double,
        std::string,
        std::shared_ptr<JSONArray>,
        std::shared_ptr<JSONObject>
    > Data;
};

class SOLSTICE_API JSONParser {
public:
    static JSONValue Parse(const std::string& JSON);
    static std::future<JSONValue> ParseAsync(const std::string& JSON);

private:
    struct State {
        const char* Pos;
        const char* End;
    };

    static JSONValue ParseValue(State& S);
    static JSONValue ParseObject(State& S);
    static JSONValue ParseArray(State& S);
    static JSONValue ParseString(State& S);
    static JSONValue ParseNumber(State& S);
    static JSONValue ParseLiteral(State& S);

    static void SkipWhitespace(State& S);
};

class SOLSTICE_API JSONWriter {
public:
    static std::string Write(const JSONValue& Value, bool Pretty = false);
    static std::future<std::string> WriteAsync(const JSONValue& Value, bool Pretty = false);
};

} // namespace Solstice::Core
