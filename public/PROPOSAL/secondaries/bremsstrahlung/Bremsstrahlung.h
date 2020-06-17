#pragma once

#include "PROPOSAL/particle/Particle.h"
#include "PROPOSAL/secondaries/Parametrization.h"

namespace PROPOSAL {
namespace secondaries {
    struct Bremsstrahlung : public secondaries::Parametrization {
        Bremsstrahlung() = default;
        virtual ~Bremsstrahlung() = default;

        static constexpr InteractionType type
            = PROPOSAL::InteractionType::Brems;
    };
} // namespace secondaries
} // namespace PROPOSAL