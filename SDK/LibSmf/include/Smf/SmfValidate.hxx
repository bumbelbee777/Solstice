#pragma once

#include "SmfBinary.hxx"
#include "SmfMapEditor.hxx"
#include "SmfUtil.hxx"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

namespace Solstice::Smf {

/// Result of ``ValidateSmfBytes`` / ``ValidateSmfFile`` / ``ValidateSmfMap``.
struct SmfValidationReport {
    SmfError loadError{SmfError::None};
    bool loadOk{false};
    bool roundTripOk{false};
    std::string loadStageNote;
    std::string roundTripStageNote;
    SmfFileHeader header{};
    bool haveHeader{false};
    std::vector<SmfMapValidationMessage> structure;

    /// Load and round-trip succeeded, and no structure messages at Error severity.
    bool IsFullyValid() const {
        if (!loadOk || !roundTripOk) {
            return false;
        }
        for (const auto& m : structure) {
            if (m.Level == SmfMapValidationMessage::Severity::Error) {
                return false;
            }
        }
        return true;
    }
};

/// Parse ``bytes`` as SMF v1, run structure checks, and verify serialize→parse round-trip (uncompressed save).
/// Returns ``true`` if the document **loaded** successfully (``report.loadOk``). Round-trip and structure details
/// are always filled when load succeeds.
inline bool ValidateSmfBytes(std::span<const std::byte> bytes, SmfValidationReport& report, SmfMap* outMap = nullptr) {
    report = SmfValidationReport{};
    SmfMap map;
    SmfError err = SmfError::None;
    SmfFileHeader fh{};
    if (!LoadSmfFromBytes(map, bytes, &fh, &err)) {
        report.loadError = err;
        report.loadStageNote = std::string("Load failed: ") + SmfErrorMessage(err);
        return false;
    }
    report.loadOk = true;
    report.loadError = SmfError::None;
    report.header = fh;
    report.haveHeader = true;

    ValidateMapStructure(map, report.structure);

    SmfError verr = SmfError::None;
    std::vector<std::byte> serialized;
    if (!SaveSmfToBytes(map, serialized, &verr, false)) {
        report.roundTripOk = false;
        report.roundTripStageNote = std::string("Serialize after load failed: ") + SmfErrorMessage(verr);
    } else {
        SmfMap round;
        SmfError err2 = SmfError::None;
        if (!LoadSmfFromBytes(round, serialized, nullptr, &err2)) {
            report.roundTripOk = false;
            report.roundTripStageNote = std::string("Round-trip parse failed: ") + SmfErrorMessage(err2);
        } else {
            report.roundTripOk = true;
            report.roundTripStageNote = "LibSmf codec: OK (serialize → parse round-trip).";
        }
    }

    if (outMap != nullptr) {
        *outMap = std::move(map);
    }
    return true;
}

/// Read a file and run ``ValidateSmfBytes`` on its contents.
inline bool ValidateSmfFile(const std::filesystem::path& path, SmfValidationReport& report, SmfMap* outMap = nullptr) {
    report = SmfValidationReport{};
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        report.loadError = SmfError::IoOpenFailed;
        report.loadStageNote = std::string("Open failed: ") + SmfErrorMessage(SmfError::IoOpenFailed);
        return false;
    }
    const auto sz = f.tellg();
    if (sz < 0) {
        report.loadError = SmfError::IoReadFailed;
        report.loadStageNote = SmfErrorMessage(SmfError::IoReadFailed);
        return false;
    }
    f.seekg(0);
    std::vector<std::byte> buf(static_cast<size_t>(sz));
    if (!buf.empty()) {
        f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        if (f.gcount() != static_cast<std::streamsize>(buf.size()) || f.fail()) {
            report.loadError = SmfError::IoReadFailed;
            report.loadStageNote = SmfErrorMessage(SmfError::IoReadFailed);
            return false;
        }
    }
    return ValidateSmfBytes(buf, report, outMap);
}

/// Serialize ``map`` (optionally ZSTD-compressed tail) and run the same pipeline as ``ValidateSmfBytes``.
/// Use this for in-memory maps (e.g. level editor validate).
inline bool ValidateSmfMap(const SmfMap& map, SmfValidationReport& report, bool compressTail = false) {
    report = SmfValidationReport{};
    std::vector<std::byte> bytes;
    SmfError verr = SmfError::None;
    if (!SaveSmfToBytes(map, bytes, &verr, compressTail)) {
        report.loadError = verr;
        report.loadStageNote = std::string("Serialize failed: ") + SmfErrorMessage(verr);
        return false;
    }
    return ValidateSmfBytes(bytes, report, nullptr);
}

} // namespace Solstice::Smf
