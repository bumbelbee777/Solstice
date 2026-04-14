#include "JSON.hxx"
#include <Core/System/Async.hxx>
#include <Core/ML/SIMD.hxx>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <cstring>

#ifdef SOLSTICE_SIMD_SSE
#include <immintrin.h>
#endif

namespace Solstice::Core {

// --- JSONValue Implementation ---

JSONValue::JSONValue() : Data(nullptr) {}
JSONValue::JSONValue(std::nullptr_t) : Data(nullptr) {}
JSONValue::JSONValue(bool Value) : Data(Value) {}
JSONValue::JSONValue(double Value) : Data(Value) {}
JSONValue::JSONValue(int Value) : Data(static_cast<double>(Value)) {}
JSONValue::JSONValue(const std::string& Value) : Data(Value) {}
JSONValue::JSONValue(const char* Value) : Data(std::string(Value)) {}
JSONValue::JSONValue(JSONArray Value) : Data(std::make_shared<JSONArray>(std::move(Value))) {}
JSONValue::JSONValue(JSONObject Value) : Data(std::make_shared<JSONObject>(std::move(Value))) {}

JSONType JSONValue::GetType() const {
    return std::visit([](auto&& Arg) -> JSONType {
        using T = std::decay_t<decltype(Arg)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) return JSONType::Null;
        else if constexpr (std::is_same_v<T, bool>) return JSONType::Bool;
        else if constexpr (std::is_same_v<T, double>) return JSONType::Number;
        else if constexpr (std::is_same_v<T, std::string>) return JSONType::String;
        else if constexpr (std::is_same_v<T, std::shared_ptr<JSONArray>>) return JSONType::Array;
        else if constexpr (std::is_same_v<T, std::shared_ptr<JSONObject>>) return JSONType::Object;
        return JSONType::Null;
    }, Data);
}

bool JSONValue::AsBool() const { return std::get<bool>(Data); }
double JSONValue::AsDouble() const { return std::get<double>(Data); }
int JSONValue::AsInt() const { return static_cast<int>(std::get<double>(Data)); }
const std::string& JSONValue::AsString() const { return std::get<std::string>(Data); }
const JSONArray& JSONValue::AsArray() const { return *std::get<std::shared_ptr<JSONArray>>(Data); }
const JSONObject& JSONValue::AsObject() const { return *std::get<std::shared_ptr<JSONObject>>(Data); }

JSONValue& JSONValue::operator[](const std::string& Key) {
    if (!IsObject()) Data = std::make_shared<JSONObject>();
    return (*std::get<std::shared_ptr<JSONObject>>(Data))[Key];
}

const JSONValue& JSONValue::operator[](const std::string& Key) const {
    return AsObject().at(Key);
}

JSONValue& JSONValue::operator[](size_t Index) {
    if (!IsArray()) Data = std::make_shared<JSONArray>();
    auto& arr = *std::get<std::shared_ptr<JSONArray>>(Data);
    if (Index >= arr.size()) arr.resize(Index + 1);
    return arr[Index];
}

const JSONValue& JSONValue::operator[](size_t Index) const {
    return AsArray().at(Index);
}

bool JSONValue::HasKey(const std::string& Key) const {
    if (!IsObject()) return false;
    const auto& obj = AsObject();
    return obj.find(Key) != obj.end();
}

size_t JSONValue::Size() const {
    if (IsArray()) return AsArray().size();
    if (IsObject()) return AsObject().size();
    return 0;
}

// --- JSONParser Implementation ---

void JSONParser::SkipWhitespace(State& S) {
#ifdef SOLSTICE_SIMD_SSE
    while (S.Pos + 16 <= S.End) {
        __m128i Chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(S.Pos));

        // Check for ' ', '\t', '\n', '\r'
        __m128i Spaces = _mm_set1_epi8(' ');
        __m128i Tabs = _mm_set1_epi8('\t');
        __m128i Newlines = _mm_set1_epi8('\n');
        __m128i Returns = _mm_set1_epi8('\r');

        __m128i M1 = _mm_cmpeq_epi8(Chunk, Spaces);
        __m128i M2 = _mm_cmpeq_epi8(Chunk, Tabs);
        __m128i M3 = _mm_cmpeq_epi8(Chunk, Newlines);
        __m128i M4 = _mm_cmpeq_epi8(Chunk, Returns);

        __m128i Mask = _mm_or_si128(_mm_or_si128(M1, M2), _mm_or_si128(M3, M4));
        uint16_t Bitmask = _mm_movemask_epi8(Mask);

        if (Bitmask == 0xFFFF) {
            S.Pos += 16;
        } else {
            // Find first non-whitespace character
            unsigned long Index;
#ifdef _MSC_VER
            if (_BitScanForward(&Index, ~Bitmask)) {
                S.Pos += Index;
                return;
            }
#else
            S.Pos += __builtin_ctz(~Bitmask);
            return;
#endif
        }
    }
#endif
    while (S.Pos < S.End && std::isspace(*S.Pos)) S.Pos++;
}

JSONValue JSONParser::Parse(const std::string& JSON) {
    State S { JSON.c_str(), JSON.c_str() + JSON.size() };
    SkipWhitespace(S);
    return ParseValue(S);
}

std::future<JSONValue> JSONParser::ParseAsync(const std::string& JSON) {
    return JobSystem::Instance().SubmitAsync([JSON]() {
        return Parse(JSON);
    });
}

JSONValue JSONParser::ParseValue(State& S) {
    SkipWhitespace(S);
    if (S.Pos >= S.End) return JSONValue();

    char C = *S.Pos;
    if (C == '{') return ParseObject(S);
    if (C == '[') return ParseArray(S);
    if (C == '"') return ParseString(S);
    if (std::isdigit(C) || C == '-') return ParseNumber(S);
    if (C == 't' || C == 'f' || C == 'n') return ParseLiteral(S);

    throw std::runtime_error("Unexpected character in JSON: " + std::string(1, C));
}

JSONValue JSONParser::ParseObject(State& S) {
    S.Pos++; // '{'
    JSONObject Obj;
    while (true) {
        SkipWhitespace(S);
        if (S.Pos >= S.End || *S.Pos == '}') {
            if (S.Pos < S.End) S.Pos++;
            break;
        }

        JSONValue KeyVal = ParseString(S);
        SkipWhitespace(S);
        if (S.Pos >= S.End || *S.Pos != ':') throw std::runtime_error("Expected ':' in object");
        S.Pos++;

        Obj[KeyVal.AsString()] = ParseValue(S);

        SkipWhitespace(S);
        if (S.Pos < S.End && *S.Pos == ',') {
            S.Pos++;
        } else if (S.Pos >= S.End || *S.Pos != '}') {
            throw std::runtime_error("Expected ',' or '}' in object");
        }
    }
    return JSONValue(std::move(Obj));
}

JSONValue JSONParser::ParseArray(State& S) {
    S.Pos++; // '['
    JSONArray Arr;
    while (true) {
        SkipWhitespace(S);
        if (S.Pos >= S.End || *S.Pos == ']') {
            if (S.Pos < S.End) S.Pos++;
            break;
        }

        Arr.push_back(ParseValue(S));

        SkipWhitespace(S);
        if (S.Pos < S.End && *S.Pos == ',') {
            S.Pos++;
        } else if (S.Pos >= S.End || *S.Pos != ']') {
            throw std::runtime_error("Expected ',' or ']' in array");
        }
    }
    return JSONValue(std::move(Arr));
}

JSONValue JSONParser::ParseString(State& S) {
    if (S.Pos >= S.End || *S.Pos != '"') throw std::runtime_error("Expected '\"' for string");
    S.Pos++;
    const char* Start = S.Pos;
    while (S.Pos < S.End && *S.Pos != '"') {
        if (*S.Pos == '\\') S.Pos++; // Simple escape handling
        S.Pos++;
    }
    std::string Val(Start, S.Pos - Start);
    if (S.Pos < S.End) S.Pos++; // '"'
    return JSONValue(Val);
}

JSONValue JSONParser::ParseNumber(State& S) {
    char* EndPtr;
    double Val = std::strtod(S.Pos, &EndPtr);
    S.Pos = EndPtr;
    return JSONValue(Val);
}

JSONValue JSONParser::ParseLiteral(State& S) {
    if (std::strncmp(S.Pos, "true", 4) == 0) { S.Pos += 4; return JSONValue(true); }
    if (std::strncmp(S.Pos, "false", 5) == 0) { S.Pos += 5; return JSONValue(false); }
    if (std::strncmp(S.Pos, "null", 4) == 0) { S.Pos += 4; return JSONValue(nullptr); }
    throw std::runtime_error("Unknown literal in JSON");
}

// --- JSONWriter Implementation ---

std::string JSONWriter::Write(const JSONValue& Value, bool Pretty) {
    return Value.Stringify(Pretty);
}

std::future<std::string> JSONWriter::WriteAsync(const JSONValue& Value, bool Pretty) {
    return JobSystem::Instance().SubmitAsync([Value, Pretty]() {
        return Write(Value, Pretty);
    });
}

std::string JSONValue::Stringify(bool Pretty) const {
    std::stringstream SS;
    std::visit([&](auto&& Arg) {
        using T = std::decay_t<decltype(Arg)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) SS << "null";
        else if constexpr (std::is_same_v<T, bool>) SS << (Arg ? "true" : "false");
        else if constexpr (std::is_same_v<T, double>) SS << Arg;
        else if constexpr (std::is_same_v<T, std::string>) SS << "\"" << Arg << "\"";
        else if constexpr (std::is_same_v<T, std::shared_ptr<JSONArray>>) {
            SS << "[";
            for (size_t i = 0; i < Arg->size(); i++) {
                SS << (*Arg)[i].Stringify(Pretty);
                if (i < Arg->size() - 1) SS << ",";
            }
            SS << "]";
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<JSONObject>>) {
            SS << "{";
            size_t i = 0;
            for (auto it = Arg->begin(); it != Arg->end(); ++it, ++i) {
                SS << "\"" << it->first << "\":" << it->second.Stringify(Pretty);
                if (i < Arg->size() - 1) SS << ",";
            }
            SS << "}";
        }
    }, Data);
    return SS.str();
}

} // namespace Solstice::Core
