#include "Dialogue/NarrativeDocument.hxx"
#include "../../Core/Serialization/JSON.hxx"
#include <cstring>
#include <fstream>
#include <sstream>

namespace Solstice::Game {

static std::string ReadWholeFile(const std::string& Path, std::string& Err) {
    std::ifstream In(Path, std::ios::binary);
    if (!In) {
        Err = "cannot open file: " + Path;
        return {};
    }
    std::ostringstream Oss;
    Oss << In.rdbuf();
    return Oss.str();
}

void NarrativeDocumentV1::ToDialogueTree(DialogueTree& Out) const {
    Out.Clear();
    Out.SetStartNodeId(StartNodeId);
    for (const auto& N : Nodes) {
        Out.AddNode(N);
    }
}

void NarrativeDocumentV1::FromDialogueTree(const DialogueTree& Tree) {
    Format = "solstice.narrative.v1";
    StartNodeId = Tree.GetStartNodeId();
    Nodes = Tree.GetAllNodes();
}

static void NodeToJSON(Core::JSONObject& O, const DialogueNode& N) {
    O["nodeId"] = Core::JSONValue(N.NodeId);
    O["speakerName"] = Core::JSONValue(N.SpeakerName);
    O["text"] = Core::JSONValue(N.Text);
    O["nextNodeId"] = Core::JSONValue(N.NextNodeId);
    Core::JSONArray ChArr;
    for (const auto& C : N.Choices) {
        Core::JSONObject Co;
        Co["label"] = Core::JSONValue(C.Label);
        Co["targetNodeId"] = Core::JSONValue(C.TargetNodeId);
        ChArr.push_back(Core::JSONValue(Co));
    }
    O["choices"] = Core::JSONValue(ChArr);
    O["voiceAssetPath"] = Core::JSONValue(N.VoiceAssetPath);
    O["subtitleId"] = Core::JSONValue(N.SubtitleId);
    O["subtitleText"] = Core::JSONValue(N.SubtitleText);
    O["speakerStyleTag"] = Core::JSONValue(N.SpeakerStyleTag);
    O["onEnterEvent"] = Core::JSONValue(N.OnEnterEvent);
}

static bool NodeFromJSON(const Core::JSONObject& O, DialogueNode& N, std::string& Err) {
    auto GetStr = [&](const char* Key, std::string& Out, bool Required) -> bool {
        auto It = O.find(Key);
        if (It == O.end()) {
            if (Required) {
                Err = std::string("missing key: ") + Key;
                return false;
            }
            Out.clear();
            return true;
        }
        if (!It->second.IsString()) {
            Err = std::string("key not string: ") + Key;
            return false;
        }
        Out = It->second.AsString();
        return true;
    };
    if (!GetStr("nodeId", N.NodeId, true)) {
        return false;
    }
    GetStr("speakerName", N.SpeakerName, false);
    GetStr("text", N.Text, false);
    GetStr("nextNodeId", N.NextNodeId, false);
    N.Choices.clear();
    auto ChIt = O.find("choices");
    if (ChIt != O.end() && ChIt->second.IsArray()) {
        for (const auto& Cv : ChIt->second.AsArray()) {
            if (!Cv.IsObject()) {
                continue;
            }
            const auto& Co = Cv.AsObject();
            DialogueChoice C;
            auto Li = Co.find("label");
            auto Ti = Co.find("targetNodeId");
            if (Li != Co.end() && Li->second.IsString()) {
                C.Label = Li->second.AsString();
            }
            if (Ti != Co.end() && Ti->second.IsString()) {
                C.TargetNodeId = Ti->second.AsString();
            }
            N.Choices.push_back(std::move(C));
        }
    }
    GetStr("voiceAssetPath", N.VoiceAssetPath, false);
    GetStr("subtitleId", N.SubtitleId, false);
    GetStr("subtitleText", N.SubtitleText, false);
    GetStr("speakerStyleTag", N.SpeakerStyleTag, false);
    GetStr("onEnterEvent", N.OnEnterEvent, false);
    return true;
}

bool NarrativeDocumentFromJSON(const std::string& JSON, NarrativeDocumentV1& Out, std::string& Err) {
    Err.clear();
    try {
        Core::JSONValue Root = Core::JSONParser::Parse(JSON);
        if (!Root.IsObject()) {
            Err = "root must be object";
            return false;
        }
        const auto& O = Root.AsObject();
        auto F = O.find("format");
        if (F != O.end() && F->second.IsString()) {
            Out.Format = F->second.AsString();
        }
        auto S = O.find("startNodeId");
        if (S == O.end() || !S->second.IsString()) {
            Err = "missing startNodeId";
            return false;
        }
        Out.StartNodeId = S->second.AsString();
        auto Ns = O.find("nodes");
        if (Ns == O.end() || !Ns->second.IsArray()) {
            Err = "missing nodes array";
            return false;
        }
        Out.Nodes.clear();
        for (const auto& Nv : Ns->second.AsArray()) {
            if (!Nv.IsObject()) {
                continue;
            }
            DialogueNode Node;
            if (!NodeFromJSON(Nv.AsObject(), Node, Err)) {
                return false;
            }
            Out.Nodes.push_back(std::move(Node));
        }
        auto P = O.find("provenance");
        if (P != O.end() && P->second.IsObject()) {
            const auto& Po = P->second.AsObject();
            auto Ps = Po.find("source");
            if (Ps != Po.end() && Ps->second.IsString()) {
                Out.ProvenanceSource = Ps->second.AsString();
            }
        }
        return true;
    } catch (const std::exception& e) {
        Err = e.what();
        return false;
    }
}

std::string NarrativeDocumentToJSON(const NarrativeDocumentV1& Doc, bool Pretty) {
    Core::JSONObject Root;
    Root["format"] = Core::JSONValue(Doc.Format);
    Root["startNodeId"] = Core::JSONValue(Doc.StartNodeId);
    Core::JSONArray Arr;
    for (const auto& N : Doc.Nodes) {
        Core::JSONObject No;
        NodeToJSON(No, N);
        Arr.push_back(Core::JSONValue(No));
    }
    Root["nodes"] = Core::JSONValue(Arr);
    Core::JSONObject Prov;
    Prov["source"] = Core::JSONValue(Doc.ProvenanceSource);
    Root["provenance"] = Core::JSONValue(Prov);
    Core::JSONValue V(Root);
    return V.Stringify(Pretty);
}

static bool EndsWithPath(const std::string& Path, const char* Suf) {
    const size_t L = std::strlen(Suf);
    return Path.size() >= L && Path.compare(Path.size() - L, L, Suf) == 0;
}

bool NarrativeDocumentLoadFile(const std::string& Path, NarrativeDocumentV1& Out, std::string& Err) {
    Err.clear();
    std::string Data = ReadWholeFile(Path, Err);
    if (!Err.empty()) {
        return false;
    }
    if (EndsWithPath(Path, ".yaml") || EndsWithPath(Path, ".yml")) {
        return NarrativeDocumentFromYAML(Data, Out, Err);
    }
    return NarrativeDocumentFromJSON(Data, Out, Err);
}

// Minimal YAML reader for solstice.narrative.v1 only (no lib): key: value and nodes list.
bool NarrativeDocumentFromYAML(const std::string& YAML, NarrativeDocumentV1& Out, std::string& Err) {
    Err.clear();
    Out = NarrativeDocumentV1{};
    Out.Format = "solstice.narrative.v1";
    Out.ProvenanceSource = "yaml";

    std::vector<std::string> Lines;
    std::istringstream Iss(YAML);
    std::string Line;
    while (std::getline(Iss, Line)) {
        if (!Line.empty() && Line.back() == '\r') {
            Line.pop_back();
        }
        Lines.push_back(std::move(Line));
    }

    auto Trim = [](std::string& S) {
        while (!S.empty() && (S.front() == ' ' || S.front() == '\t')) {
            S.erase(S.begin());
        }
        while (!S.empty() && (S.back() == ' ' || S.back() == '\t')) {
            S.pop_back();
        }
    };

    auto CountIndent = [](const std::string& S) -> size_t {
        size_t n = 0;
        while (n < S.size() && (S[n] == ' ' || S[n] == '\t')) {
            ++n;
        }
        return n;
    };

    enum class Mode { Root, Nodes, NodeItem };
    Mode M = Mode::Root;
    DialogueNode Cur;
    bool InNode = false;

    for (size_t i = 0; i < Lines.size(); ++i) {
        std::string L = Lines[i];
        if (L.empty()) {
            continue;
        }
        size_t Ind = CountIndent(L);
        std::string Rest = L.substr(Ind);
        if (Rest.empty() || Rest[0] == '#') {
            continue;
        }

        if (M == Mode::Root) {
            size_t Col = Rest.find(':');
            if (Col == std::string::npos) {
                continue;
            }
            std::string Key = Rest.substr(0, Col);
            std::string Val = Rest.substr(Col + 1);
            Trim(Key);
            Trim(Val);
            if (Val == "|" || Val == ">") {
                // multiline not supported
                continue;
            }
            if (Key == "format") {
                Out.Format = Val;
            } else if (Key == "startNodeId") {
                Out.StartNodeId = Val;
            } else if (Key == "nodes") {
                if (Val.empty()) {
                    M = Mode::Nodes;
                }
            }
            continue;
        }

        if (M == Mode::Nodes) {
            if (Rest[0] != '-') {
                continue;
            }
            if (InNode && !Cur.NodeId.empty()) {
                Out.Nodes.push_back(Cur);
            }
            Cur = DialogueNode{};
            InNode = true;
            M = Mode::NodeItem;
            std::string AfterDash = Rest.substr(1);
            Trim(AfterDash);
            size_t Col = AfterDash.find(':');
            if (Col != std::string::npos) {
                std::string Key = AfterDash.substr(0, Col);
                std::string Val = AfterDash.substr(Col + 1);
                Trim(Key);
                Trim(Val);
                if (Key == "nodeId") {
                    Cur.NodeId = Val;
                }
            }
            continue;
        }

        if (M == Mode::NodeItem) {
            if (Ind < 2) {
                if (InNode && !Cur.NodeId.empty()) {
                    Out.Nodes.push_back(Cur);
                    Cur = DialogueNode{};
                    InNode = false;
                }
                M = Mode::Root;
                --i;
                continue;
            }
            if (Rest[0] == '-' && Ind <= 4) {
                if (InNode && !Cur.NodeId.empty()) {
                    Out.Nodes.push_back(Cur);
                }
                Cur = DialogueNode{};
                InNode = true;
                std::string AfterDash = Rest.substr(1);
                Trim(AfterDash);
                size_t Col = AfterDash.find(':');
                if (Col != std::string::npos) {
                    std::string Key = AfterDash.substr(0, Col);
                    std::string Val = AfterDash.substr(Col + 1);
                    Trim(Key);
                    Trim(Val);
                    if (Key == "nodeId") {
                        Cur.NodeId = Val;
                    }
                }
                continue;
            }
            size_t Col = Rest.find(':');
            if (Col == std::string::npos) {
                continue;
            }
            std::string Key = Rest.substr(0, Col);
            std::string Val = Rest.substr(Col + 1);
            Trim(Key);
            Trim(Val);
            if (Key == "nodeId") {
                Cur.NodeId = Val;
            } else if (Key == "speakerName") {
                Cur.SpeakerName = Val;
            } else if (Key == "text") {
                Cur.Text = Val;
            } else if (Key == "nextNodeId") {
                Cur.NextNodeId = Val;
            } else if (Key == "voiceAssetPath") {
                Cur.VoiceAssetPath = Val;
            } else if (Key == "subtitleId") {
                Cur.SubtitleId = Val;
            } else if (Key == "subtitleText") {
                Cur.SubtitleText = Val;
            } else if (Key == "speakerStyleTag") {
                Cur.SpeakerStyleTag = Val;
            } else if (Key == "onEnterEvent") {
                Cur.OnEnterEvent = Val;
            }
        }
    }
    if (InNode && !Cur.NodeId.empty()) {
        Out.Nodes.push_back(Cur);
    }

    if (Out.StartNodeId.empty()) {
        Err = "yaml: missing startNodeId";
        return false;
    }
    return true;
}

std::string NarrativeDocumentToYAML(const NarrativeDocumentV1& Doc) {
    std::ostringstream O;
    O << "format: " << Doc.Format << "\n";
    O << "startNodeId: " << Doc.StartNodeId << "\n";
    O << "nodes:\n";
    for (const auto& N : Doc.Nodes) {
        O << "  - nodeId: " << N.NodeId << "\n";
        O << "    speakerName: " << N.SpeakerName << "\n";
        O << "    text: " << N.Text << "\n";
        O << "    nextNodeId: " << N.NextNodeId << "\n";
        if (!N.Choices.empty()) {
            O << "    choices:\n";
            for (const auto& C : N.Choices) {
                O << "      - label: " << C.Label << "\n";
                O << "        targetNodeId: " << C.TargetNodeId << "\n";
            }
        }
        if (!N.VoiceAssetPath.empty()) {
            O << "    voiceAssetPath: " << N.VoiceAssetPath << "\n";
        }
        if (!N.SubtitleId.empty()) {
            O << "    subtitleId: " << N.SubtitleId << "\n";
        }
        if (!N.SubtitleText.empty()) {
            O << "    subtitleText: " << N.SubtitleText << "\n";
        }
        if (!N.SpeakerStyleTag.empty()) {
            O << "    speakerStyleTag: " << N.SpeakerStyleTag << "\n";
        }
        if (!N.OnEnterEvent.empty()) {
            O << "    onEnterEvent: " << N.OnEnterEvent << "\n";
        }
    }
    O << "provenance:\n  source: " << Doc.ProvenanceSource << "\n";
    return O.str();
}

} // namespace Solstice::Game
