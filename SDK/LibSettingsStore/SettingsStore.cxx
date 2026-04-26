#include <Solstice/SettingsStore/SettingsStore.hxx>

#include <cctype>
#include <fstream>
#include <sstream>

namespace Solstice::SettingsStore {

namespace {

std::string JsonEscape(std::string_view s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '\\':
            o += "\\\\";
            break;
        case '"':
            o += "\\\"";
            break;
        case '\b':
            o += "\\b";
            break;
        case '\f':
            o += "\\f";
            break;
        case '\n':
            o += "\\n";
            break;
        case '\r':
            o += "\\r";
            break;
        case '\t':
            o += "\\t";
            break;
        default:
            o += c;
            break;
        }
    }
    return o;
}

bool ReadQuotedString(std::string_view s, size_t& i, std::string& out) {
    out.clear();
    if (i >= s.size() || s[i] != '"') {
        return false;
    }
    ++i;
    while (i < s.size()) {
        char c = s[i++];
        if (c == '"') {
            return true;
        }
        if (c == '\\' && i < s.size()) {
            const char e = s[i++];
            switch (e) {
            case '"':
                out += '"';
                break;
            case '\\':
                out += '\\';
                break;
            case '/':
                out += '/';
                break;
            case 'b':
                out += '\b';
                break;
            case 'f':
                out += '\f';
                break;
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            default:
                out += e;
                break;
            }
            continue;
        }
        out += c;
    }
    return false;
}

void SkipWs(std::string_view s, size_t& i) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
}

bool ParseJsonStringMap(std::string_view json, std::unordered_map<std::string, std::string>& out) {
    out.clear();
    size_t i = 0;
    SkipWs(json, i);
    if (i >= json.size() || json[i] != '{') {
        return false;
    }
    ++i;
    while (true) {
        SkipWs(json, i);
        if (i < json.size() && json[i] == '}') {
            ++i;
            return true;
        }
        std::string key;
        if (!ReadQuotedString(json, i, key)) {
            return false;
        }
        SkipWs(json, i);
        if (i >= json.size() || json[i] != ':') {
            return false;
        }
        ++i;
        SkipWs(json, i);
        std::string val;
        if (!ReadQuotedString(json, i, val)) {
            return false;
        }
        out[std::move(key)] = std::move(val);
        SkipWs(json, i);
        if (i < json.size() && json[i] == ',') {
            ++i;
            continue;
        }
        if (i < json.size() && json[i] == '}') {
            ++i;
            return true;
        }
        return false;
    }
}

static bool TruthyString(std::string_view v) {
    if (v == "1" || v == "true" || v == "True" || v == "TRUE" || v == "yes" || v == "Yes") {
        return true;
    }
    return false;
}

} // namespace

std::filesystem::path PathNextToExecutable(const char* sdlBasePathUtf8OrNull, std::string_view appSlug) {
    std::string name = "solstice_settings_";
    name.append(appSlug);
    name.append(".json");
    if (sdlBasePathUtf8OrNull && sdlBasePathUtf8OrNull[0] != '\0') {
        return std::filesystem::path(sdlBasePathUtf8OrNull) / name;
    }
    return std::filesystem::path(std::move(name));
}

Store::Store(std::filesystem::path path) : m_Path(std::move(path)) {}

void Store::Clear() {
    m_Values.clear();
}

std::optional<std::string> Store::GetString(std::string_view key) const {
    std::string k(key);
    auto it = m_Values.find(k);
    if (it == m_Values.end()) {
        return std::nullopt;
    }
    return it->second;
}

void Store::SetString(std::string_view key, std::string value) {
    m_Values[std::string(key)] = std::move(value);
}

std::optional<bool> Store::GetBool(std::string_view key) const {
    auto s = GetString(key);
    if (!s) {
        return std::nullopt;
    }
    if (TruthyString(*s)) {
        return true;
    }
    if (*s == "0" || *s == "false" || *s == "False" || *s == "FALSE" || *s == "no" || *s == "No") {
        return false;
    }
    return std::nullopt;
}

void Store::SetBool(std::string_view key, bool value) {
    SetString(key, value ? "true" : "false");
}

std::optional<std::int64_t> Store::GetInt64(std::string_view key) const {
    auto s = GetString(key);
    if (!s || s->empty()) {
        return std::nullopt;
    }
    try {
        size_t idx = 0;
        const long long v = std::stoll(*s, &idx, 10);
        if (idx != s->size()) {
            return std::nullopt;
        }
        return static_cast<std::int64_t>(v);
    } catch (...) {
        return std::nullopt;
    }
}

void Store::SetInt64(std::string_view key, std::int64_t value) {
    SetString(key, std::to_string(value));
}

bool Store::Load(std::string* outError) {
    m_Values.clear();
    std::ifstream in(m_Path, std::ios::binary);
    if (!in) {
        return true;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    if (text.empty()) {
        return true;
    }
    if (!ParseJsonStringMap(text, m_Values)) {
        if (outError) {
            *outError = "Invalid settings JSON.";
        }
        m_Values.clear();
        return false;
    }
    return true;
}

bool Store::Save(std::string* outError) {
    std::string body = "{";
    bool first = true;
    for (const auto& kv : m_Values) {
        if (!first) {
            body += ',';
        }
        first = false;
        body += '"';
        body += JsonEscape(kv.first);
        body += "\":\"";
        body += JsonEscape(kv.second);
        body += '"';
    }
    body += '}';

    std::error_code ec;
    const auto parent = m_Path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
    }
    std::ofstream out(m_Path, std::ios::binary | std::ios::trunc);
    if (!out) {
        if (outError) {
            *outError = "Failed to write settings file.";
        }
        return false;
    }
    out.write(body.data(), static_cast<std::streamsize>(body.size()));
    return static_cast<bool>(out);
}

} // namespace Solstice::SettingsStore
