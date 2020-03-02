/*! \file   Scattering.cxx
 *   \brief  Source filefor the Scattering bug routines.
 *
 *   This version has a major bug and produces too small scattering angles.
 *
 *   \date   2013.08.19
 *   \author Tomasz Fuchs
 **/

#include <cassert>
#include <tuple>
#include <cmath>

#include "PROPOSAL/math/RandomGenerator.h"
#include "PROPOSAL/math/Vector3D.h"
#include "PROPOSAL/particle/ParticleDef.h"
#include "PROPOSAL/scattering/Scattering.h"

using namespace PROPOSAL;

/******************************************************************************
 *                                  OStream                                    *
 ******************************************************************************/

namespace PROPOSAL {

std::ostream& operator<<(std::ostream& os, Scattering const& scattering)
{
    std::stringstream ss;
    ss << " Scattering (" << &scattering << ") ";
    os << Helper::Centered(60, ss.str()) << '\n';

    os << scattering.particle_def_ << std::endl;

    scattering.print(os);

    os << Helper::Centered(60, "");

    return os;
}

} // namespace PROPOSAL

/******************************************************************************
 *                                 Scattering                                  *
 ******************************************************************************/

Scattering::Scattering(const ParticleDef& particle_def)
    : particle_def_(particle_def)
{
}

/* Scattering::Scattering(const Scattering& scattering) */
/*     : particle_def_(scattering.particle_def_) */
/* { */
/* } */

Scattering::~Scattering() {}

/* bool Scattering::operator==(const Scattering& scattering) const */
/* { */
/*     if (particle_def_ != scattering.particle_def_) */
/*         return false; */
/*     else */
/*         return this->compare(scattering); */
/* } */

/* bool Scattering::operator!=(const Scattering& scattering) const */
/* { */
/*     return !(*this == scattering); */
/* } */

tuple<Vector3D, Vector3D> Scattering::Scatter(double dr, double ei, double ef,
    const Vector3D& pos, const Vector3D& old_direction, array<double, 4> rnd)
{
    assert(dr <= 0);

    auto random_angles = CalculateRandomAngle(dr, ei, ef, pos, rnd);

    auto sz = std::sqrt(std::max(1.
            - (random_angles[sx] * random_angles[sx]
                  + random_angles[sy] * random_angles[sy]),
        0.));
    auto tz = std::sqrt(std::max(1.
            - (random_angles[tx] * random_angles[tx]
                  + random_angles[ty] * random_angles[ty]),
        0.));

    auto sinth = std::sin(old_direction.GetTheta());
    auto costh = std::cos(old_direction.GetTheta());
    auto sinph = std::sin(old_direction.GetPhi());
    auto cosph = std::cos(old_direction.GetPhi());

    auto rotate_vector_x = Vector3D(costh * cosph, costh * sinph, -sinth);
    auto rotate_vector_y = Vector3D(-sinph, cosph, 0.);

    // Rotation towards all tree axes
    auto averaged_direction = sz * old_direction;
    averaged_direction += random_angles[sx] * rotate_vector_x;
    averaged_direction += random_angles[sy] * rotate_vector_y;

    // Rotation towards all tree axes
    auto final_direction = tz * old_direction;
    final_direction += random_angles[tx] * rotate_vector_x;
    final_direction += random_angles[ty] * rotate_vector_y;
    final_direction.CalculateSphericalCoordinates();

    return std::make_tuple<averaged_direction, final_direction>;
}
