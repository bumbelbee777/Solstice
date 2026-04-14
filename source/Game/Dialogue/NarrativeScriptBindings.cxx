#include "Dialogue/NarrativeScriptBindings.hxx"
#include "Dialogue/NarrativeBridge.hxx"
#include "Dialogue/NarrativeDocument.hxx"
#include "Dialogue/NarrativeRuntime.hxx"
#include "Cutscene/CutscenePlayer.hxx"
#include "../../Scripting/Bindings/ScriptBindings.hxx"
#include "../../Scripting/VM/BytecodeVM.hxx"

namespace Solstice::Game {

void RegisterNarrativeScriptBindings(Scripting::BytecodeVM& VM) {
    using Solstice::Scripting::GetString;
    using Solstice::Scripting::GetInt;

    VM.RegisterNative("Dialogue.LoadFromFile", [](const std::vector<Scripting::Value>& args) -> Scripting::Value {
        if (args.empty()) {
            return static_cast<int64_t>(0);
        }
        std::string Path = GetString(args[0]);
        DialogueTree* Tree = NarrativeBridge::GetDialogueTree();
        if (Tree == nullptr) {
            return static_cast<int64_t>(0);
        }
        NarrativeDocumentV1 Doc;
        std::string Err;
        if (!NarrativeDocumentLoadFile(Path, Doc, Err)) {
            (void)Err;
            return static_cast<int64_t>(0);
        }
        Doc.ToDialogueTree(*Tree);
        return static_cast<int64_t>(1);
    });

    VM.RegisterNative("Dialogue.Start", [](const std::vector<Scripting::Value>&) -> Scripting::Value {
        DialogueTree* Tree = NarrativeBridge::GetDialogueTree();
        if (Tree == nullptr) {
            return static_cast<int64_t>(0);
        }
        Tree->Start();
        NarrativeRuntime* NR = NarrativeBridge::GetNarrativeRuntime();
        if (NR != nullptr) {
            NR->OnCurrentLineEntered();
        }
        return static_cast<int64_t>(1);
    });

    VM.RegisterNative("Dialogue.Advance", [](const std::vector<Scripting::Value>&) -> Scripting::Value {
        DialogueTree* Tree = NarrativeBridge::GetDialogueTree();
        if (Tree == nullptr) {
            return static_cast<int64_t>(0);
        }
        Tree->Advance();
        NarrativeRuntime* NR = NarrativeBridge::GetNarrativeRuntime();
        if (NR != nullptr) {
            NR->OnCurrentLineEntered();
        }
        return static_cast<int64_t>(1);
    });

    VM.RegisterNative("Dialogue.AdvanceChoice", [](const std::vector<Scripting::Value>& args) -> Scripting::Value {
        if (args.empty()) {
            return static_cast<int64_t>(0);
        }
        size_t Idx = static_cast<size_t>(GetInt(args[0]));
        DialogueTree* Tree = NarrativeBridge::GetDialogueTree();
        if (Tree == nullptr) {
            return static_cast<int64_t>(0);
        }
        Tree->Advance(Idx);
        NarrativeRuntime* NR = NarrativeBridge::GetNarrativeRuntime();
        if (NR != nullptr) {
            NR->OnCurrentLineEntered();
        }
        return static_cast<int64_t>(1);
    });

    VM.RegisterNative("Dialogue.JumpTo", [](const std::vector<Scripting::Value>& args) -> Scripting::Value {
        if (args.empty()) {
            return static_cast<int64_t>(0);
        }
        std::string Id = GetString(args[0]);
        DialogueTree* Tree = NarrativeBridge::GetDialogueTree();
        if (Tree == nullptr) {
            return static_cast<int64_t>(0);
        }
        bool Ok = Tree->JumpToNodeId(Id);
        NarrativeRuntime* NR = NarrativeBridge::GetNarrativeRuntime();
        if (NR != nullptr && Ok) {
            NR->OnCurrentLineEntered();
        }
        return static_cast<int64_t>(Ok ? 1 : 0);
    });

    VM.RegisterNative("Dialogue.CurrentNodeId", [](const std::vector<Scripting::Value>&) -> Scripting::Value {
        DialogueTree* Tree = NarrativeBridge::GetDialogueTree();
        if (Tree == nullptr) {
            return std::string("");
        }
        return Tree->GetCurrentNodeId();
    });

    VM.RegisterNative("Dialogue.IsAtEnd", [](const std::vector<Scripting::Value>&) -> Scripting::Value {
        DialogueTree* Tree = NarrativeBridge::GetDialogueTree();
        if (Tree == nullptr) {
            return static_cast<int64_t>(1);
        }
        return static_cast<int64_t>(Tree->IsAtEnd() ? 1 : 0);
    });

    VM.RegisterNative("Cutscene.LoadFromFile", [](const std::vector<Scripting::Value>& args) -> Scripting::Value {
        if (args.empty()) {
            return static_cast<int64_t>(0);
        }
        CutscenePlayer* P = NarrativeBridge::GetCutscenePlayer();
        if (P == nullptr) {
            return static_cast<int64_t>(0);
        }
        std::string Err;
        bool Ok = P->LoadFromFile(GetString(args[0]), Err);
        return static_cast<int64_t>(Ok ? 1 : 0);
    });

    VM.RegisterNative("Cutscene.Play", [](const std::vector<Scripting::Value>&) -> Scripting::Value {
        CutscenePlayer* P = NarrativeBridge::GetCutscenePlayer();
        if (P == nullptr) {
            return static_cast<int64_t>(0);
        }
        P->Play();
        return static_cast<int64_t>(1);
    });

    VM.RegisterNative("Cutscene.Stop", [](const std::vector<Scripting::Value>&) -> Scripting::Value {
        CutscenePlayer* P = NarrativeBridge::GetCutscenePlayer();
        if (P == nullptr) {
            return static_cast<int64_t>(0);
        }
        P->Stop();
        return static_cast<int64_t>(1);
    });

    VM.RegisterNative("Cutscene.Skip", [](const std::vector<Scripting::Value>&) -> Scripting::Value {
        CutscenePlayer* P = NarrativeBridge::GetCutscenePlayer();
        if (P == nullptr) {
            return static_cast<int64_t>(0);
        }
        P->Skip();
        return static_cast<int64_t>(1);
    });

    VM.RegisterNative("Cutscene.IsPlaying", [](const std::vector<Scripting::Value>&) -> Scripting::Value {
        CutscenePlayer* P = NarrativeBridge::GetCutscenePlayer();
        if (P == nullptr) {
            return static_cast<int64_t>(0);
        }
        return static_cast<int64_t>(P->IsPlaying() ? 1 : 0);
    });
}

} // namespace Solstice::Game
