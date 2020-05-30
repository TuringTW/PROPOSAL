#include "PROPOSAL/EnergyCutSettings.h"
#include "PROPOSAL/Logging.h"
#include "PROPOSAL/methods.h"

#include <sstream>
#include <stdexcept>

using namespace PROPOSAL;
using std::max;
using std::min;
using std::get;

//----------------------------------------------------------------------------//
//-------------------------public member functions----------------------------//
//----------------------------------------------------------------------------//

EnergyCutSettings::EnergyCutSettings(
    double ecut, double vcut, bool continuous_randomization)
    : ecut_(ecut)
    , vcut_(vcut)
    , continuous_randomization_(continuous_randomization)
{
    if (ecut_ < 0)
        throw std::invalid_argument(
            "Absolut EnergyCut (ecut) must be larger > 0! \nINFO: If a ecut = "
            "std::numeric_limits<double>::infinity() is set the particle will "
            "only propagated continously. ");

    if (vcut_ <= 0 || vcut_ > 1)
        throw std::invalid_argument(
            "Relative EnergyCut (vcut) must be between [0, 1]. \nINF0: If a vc"
            "ut = 1 is set, the particle is only propagate continous.");

    log_debug(
        "EnergyCut set to ecut(%f), vcut(%f), continuous_randomization(%b)",
        ecut_, vcut_, continuous_randomization_);
}

EnergyCutSettings::EnergyCutSettings(const nlohmann::json& config)
{
    if (!config.contains("e_cut"))
        throw std::invalid_argument("A e_cut must be defined. e_cut = [0-inf]");
    if (!config.contains("v_cut"))
        throw std::invalid_argument("A v_cut must be defined. v_cut = [0-1]");
    if (!config.contains("cont_rand"))
        throw std::invalid_argument(
            "cont_rand must be activated/deactivated. cont_rand = true/false");

    double ecut, vcut;
    bool cont_rand;

    config.at("e_cut").get_to(ecut);
    config.at("v_cut").get_to(vcut);
    config.at("cont_rand").get_to(cont_rand);

    *this = EnergyCutSettings(ecut, vcut, cont_rand);
}

//----------------------------------------------------------------------------//
//----------------------------------------------------------------------------//

bool EnergyCutSettings::operator==(const EnergyCutSettings& cut) const noexcept
{
    if (ecut_ != cut.ecut_)
        return false;
    if (vcut_ != cut.vcut_)
        return false;
    if (continuous_randomization_ != cut.continuous_randomization_)
        return false;

    return true;
}

inline double EnergyCutSettings::GetCut(double energy) const noexcept
{
    assert(energy > 0);
    return std::min(ecut_ / energy, vcut_);
}

inline auto EnergyCutSettings::GetCut(
    tuple<double, double>& lim, double energy) const noexcept -> double
{
    return min(max(get<Parametrization::V_MIN>(lim), GetCut(energy)),
        get<Parametrization::V_MAX>(lim));
}

size_t EnergyCutSettings::GetHash() const noexcept
{
    size_t hash_digest = 0;
    hash_combine(hash_digest, ecut_, vcut_, continuous_randomization_);
    return hash_digest;
}

//----------------------------------------------------------------------------//
//----------------------------------------------------------------------------//

std::ostream& PROPOSAL::operator<<(
    std::ostream& os, PROPOSAL::EnergyCutSettings const& cut_settings)
{
    std::stringstream ss;
    ss << " EnergyCutSettings (" << &cut_settings << ") ";
    os << Helper::Centered(60, ss.str()) << '\n';

    os << "Ecut: " << cut_settings.GetEcut() << std::endl;
    os << "Vcut: " << cut_settings.GetVcut() << std::endl;
    os << "Continuous randomization: " << cut_settings.GetContRand()
       << std::endl;

    os << Helper::Centered(60, "");
    return os;
}
