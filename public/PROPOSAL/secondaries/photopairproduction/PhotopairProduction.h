#pragma once

#include "PROPOSAL/medium/Components.h"
/* #include "PROPOSAL/particle/Particle.h" */
#include "PROPOSAL/secondaries/Parametrization.h"

#include <array>
#include <vector>

using PROPOSAL::Components::Component;
using std::array;
using std::tuple;

namespace PROPOSAL {
namespace secondaries {
    struct PhotopairProduction : public secondaries::Parametrization {
        PhotopairProduction() = default;
        virtual ~PhotopairProduction() = default;

        static constexpr InteractionType type
            = PROPOSAL::InteractionType::Photopair;

        virtual double CalculateRho(double, double) = 0;
        virtual tuple<Vector3D, Vector3D> CalculateDirections(
            Vector3D, double, double, const Component&, vector<double>)
            = 0;
        virtual tuple<double, double> CalculateEnergy(double, double, double) = 0;
    };
} // namespace secondaries
} // namespace PROPOSAL