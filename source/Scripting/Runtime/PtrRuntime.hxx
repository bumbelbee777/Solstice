#pragma once

#include "../VM/BytecodeVM.hxx"

namespace Solstice::Scripting {

    // Helpers for creating and manipulating PtrValue instances at runtime.
    // These are intentionally small wrappers so that the VM and future
    // analyses have a single place to evolve pointer semantics.

    // Create a new PtrValue that owns the given payload value.
    PtrValue MakePtr(const Value& payload);

    // Mark the underlying pointer as logically freed and clear its payload.
    // The shared_ptr lifetime semantics still apply for the header itself.
    void PtrReset(PtrValue& ptr);

    // Returns true if the pointer is non-null and currently marked alive.
    bool PtrIsValid(const PtrValue& ptr);

    // Return a copy of the payload. Caller is responsible for checking validity
    // and turning invalid access into an appropriate VM error.
    Value PtrGetValue(const PtrValue& ptr);

} // namespace Solstice::Scripting

