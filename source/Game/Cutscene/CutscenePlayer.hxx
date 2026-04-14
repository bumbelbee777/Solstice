#pragma once

#include "../../Solstice.hxx"
#include <functional>
#include <string>
#include <vector>

namespace Solstice::Game {

// Timeline event for data-driven cutscenes (JSON).
struct CutsceneEvent {
    double TimeSeconds{0.0};
    std::string Type;  // "emit", "log", "dialogueJump", "wait"
    std::string Payload;
};

struct CutsceneDefinition {
    std::string Id;
    double DurationSeconds{0.0};
    std::vector<CutsceneEvent> Events;
};

// Lightweight cutscene player: advances timeline, fires events, supports skip to end.
class SOLSTICE_API CutscenePlayer {
public:
    CutscenePlayer() = default;

    bool LoadFromJSONString(const std::string& JSON, std::string& Err);
    bool LoadFromFile(const std::string& Path, std::string& Err);

    void Play();
    void Stop();
    void Skip();
    void Update(float DeltaTime);

    bool IsPlaying() const { return m_Playing; }
    double GetElapsed() const { return m_Elapsed; }
    const CutsceneDefinition& GetDefinition() const { return m_Def; }

    std::function<void(const std::string& EventName, const std::string& Payload)> OnEmit;
    std::function<void()> OnComplete;

private:
    CutsceneDefinition m_Def;
    bool m_Playing{false};
    double m_Elapsed{0.0};
    size_t m_NextIndex{0};
};

} // namespace Solstice::Game
