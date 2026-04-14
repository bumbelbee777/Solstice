#include "Cutscene/CutscenePlayer.hxx"
#include "Dialogue/DialogueTree.hxx"
#include "Dialogue/NarrativeBridge.hxx"
#include "Dialogue/NarrativeRuntime.hxx"
#include "../../Core/Serialization/JSON.hxx"
#include <algorithm>
#include <functional>
#include <fstream>
#include <sstream>

namespace Solstice::Game {

static void FireCutsceneEvent(
    const CutsceneEvent& E,
    const std::function<void(const std::string&, const std::string&)>& OnEmit) {
    if (E.Type == "emit" && OnEmit) {
        OnEmit(E.Payload, "");
    } else if (E.Type == "log" && OnEmit) {
        OnEmit("log", E.Payload);
    } else if (E.Type == "dialogueJump") {
        DialogueTree* Tree = NarrativeBridge::GetDialogueTree();
        if (Tree != nullptr) {
            Tree->JumpToNodeId(E.Payload);
        }
        NarrativeRuntime* NR = NarrativeBridge::GetNarrativeRuntime();
        if (NR != nullptr) {
            NR->OnCurrentLineEntered();
        }
    }
}

static std::string ReadFile(const std::string& Path, std::string& Err) {
    std::ifstream In(Path, std::ios::binary);
    if (!In) {
        Err = "cannot open: " + Path;
        return {};
    }
    std::ostringstream Oss;
    Oss << In.rdbuf();
    return Oss.str();
}

bool CutscenePlayer::LoadFromJSONString(const std::string& JSON, std::string& Err) {
    Err.clear();
    m_Def = CutsceneDefinition{};
    try {
        Core::JSONValue Root = Core::JSONParser::Parse(JSON);
        if (!Root.IsObject()) {
            Err = "cutscene root must be object";
            return false;
        }
        const auto& O = Root.AsObject();
        auto Id = O.find("id");
        if (Id != O.end() && Id->second.IsString()) {
            m_Def.Id = Id->second.AsString();
        }
        auto Dur = O.find("durationSeconds");
        if (Dur != O.end() && Dur->second.IsNumber()) {
            m_Def.DurationSeconds = Dur->second.AsDouble();
        }
        auto Ev = O.find("events");
        if (Ev != O.end() && Ev->second.IsArray()) {
            for (const auto& Evv : Ev->second.AsArray()) {
                if (!Evv.IsObject()) {
                    continue;
                }
                const auto& Eo = Evv.AsObject();
                CutsceneEvent E;
                auto T = Eo.find("timeSeconds");
                if (T != Eo.end() && T->second.IsNumber()) {
                    E.TimeSeconds = T->second.AsDouble();
                }
                auto Ty = Eo.find("type");
                if (Ty != Eo.end() && Ty->second.IsString()) {
                    E.Type = Ty->second.AsString();
                }
                auto Pl = Eo.find("payload");
                if (Pl != Eo.end() && Pl->second.IsString()) {
                    E.Payload = Pl->second.AsString();
                }
                m_Def.Events.push_back(std::move(E));
            }
        }
        std::sort(m_Def.Events.begin(), m_Def.Events.end(), [](const CutsceneEvent& A, const CutsceneEvent& B) {
            return A.TimeSeconds < B.TimeSeconds;
        });
        if (m_Def.DurationSeconds <= 0.0 && !m_Def.Events.empty()) {
            m_Def.DurationSeconds = m_Def.Events.back().TimeSeconds + 0.25;
        }
        return true;
    } catch (const std::exception& e) {
        Err = e.what();
        return false;
    }
}

bool CutscenePlayer::LoadFromFile(const std::string& Path, std::string& Err) {
    std::string S = ReadFile(Path, Err);
    if (!Err.empty() && S.empty()) {
        return false;
    }
    return LoadFromJSONString(S, Err);
}

void CutscenePlayer::Play() {
    m_Playing = true;
    m_Elapsed = 0.0;
    m_NextIndex = 0;
}

void CutscenePlayer::Stop() {
    m_Playing = false;
    m_Elapsed = 0.0;
    m_NextIndex = 0;
}

void CutscenePlayer::Skip() {
    if (!m_Playing) {
        return;
    }
    m_Elapsed = m_Def.DurationSeconds > 0.0 ? m_Def.DurationSeconds : m_Elapsed + 1.0;
    while (m_NextIndex < m_Def.Events.size()) {
        FireCutsceneEvent(m_Def.Events[m_NextIndex], OnEmit);
        ++m_NextIndex;
    }
    m_Playing = false;
    if (OnComplete) {
        OnComplete();
    }
}

void CutscenePlayer::Update(float DeltaTime) {
    if (!m_Playing) {
        return;
    }
    m_Elapsed += static_cast<double>(DeltaTime);
    while (m_NextIndex < m_Def.Events.size() && m_Def.Events[m_NextIndex].TimeSeconds <= m_Elapsed) {
        FireCutsceneEvent(m_Def.Events[m_NextIndex], OnEmit);
        ++m_NextIndex;
    }
    double EndT = m_Def.DurationSeconds;
    if (EndT > 0.0 && m_Elapsed >= EndT) {
        m_Playing = false;
        if (OnComplete) {
            OnComplete();
        }
    }
}

} // namespace Solstice::Game
