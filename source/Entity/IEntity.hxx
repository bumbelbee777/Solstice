#pragma once

#include "Kind.hxx"

namespace Solstice::ECS {
// Legacy compatibility type. Prefer pure ECS components on Registry entities.
struct IEntity {
    virtual ~IEntity() = default;
};

}
