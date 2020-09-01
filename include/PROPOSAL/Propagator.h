#pragma once

#include <unordered_map>

#include "PROPOSAL/crosssection/CrossSection.h"
#include "PROPOSAL/density_distr/density_distr.h"
#include "PROPOSAL/propagation_utility/PropagationUtility.h"
#include "PROPOSAL/json.hpp"
#include "PROPOSAL/geometry/Geometry.h"

using std::get;
namespace PROPOSAL {

using Sector = std::tuple<std::shared_ptr<const Geometry>, PropagationUtility,
    std::shared_ptr<const Density_distr>>;

class Propagator
{
public:
    /* Propagator(const ParticleDef&, const nlohmann::json&); */
    /* Propagator(const ParticleDef&, const std::string& config_file); */
    Propagator(const ParticleDef&, std::vector<Sector> sectors);

    std::vector<DynamicData> Propagate(const DynamicData& initial_particle,
            double max_distance = 1e20, double min_energy = 0.);
private:
    void DoStochasticInteraction(DynamicData&, PropagationUtility&,
            std::function<double()>);
    int AdvanceParticle(DynamicData& p_cond, double E_f, double max_distance,
                         std::function<double()> rnd, Sector& current_sector);
    double CalculateAdaptiveSteplength(const Vector3D& position,
            double max_steplength, std::shared_ptr<const Geometry> current_geometry);
    double CalculateDistanceToBorder(const Vector3D& position,
                                     const Vector3D& direction,
                                     const Geometry& current_geometry);
    int maximize(const std::array<double, 3>& InteractionEnergies);
    int minimize(const std::array<double, 3>& AdvanceGrammage);
    Sector ChooseCurrentSector(const Vector3D& particle_position,
            const Vector3D& particle_direction);

    //Global settings
    struct GlobalSettings{
        GlobalSettings();
        GlobalSettings(const nlohmann::json& config_global);
        nlohmann::json cross;
        std::string scattering;
        std::shared_ptr<EnergyCutSettings> cuts = nullptr;
        std::shared_ptr<Medium> medium = nullptr;
        bool do_exact_time;
        bool do_interpolation;
    };

    //Initializing methods
    /* static nlohmann::json ParseConfig(const std::string& config_file); */
    /* void InitializeSectorFromJSON(const ParticleDef&, const nlohmann::json&, GlobalSettings); */
    /* PropagationUtility CreateUtility(CrossSectionList, std::shared_ptr<Medium>, bool, bool, bool, std::string); */
    /* CrossSectionList CreateCrossSectionList(const nlohmann::json&, std::shared_ptr<Medium>, std::shared_ptr<EnergyCutSettings>, bool); */

    ParticleDef p_def;
    std::shared_ptr<InterpolationDef> interpol_def_global = nullptr;
    enum {GEOMETRY, UTILITY, DENSITY_DISTR};
    enum Type : int {
        MinimalE = 0,
        Decay = 1,
        StochasticELoss = 2
    };
    enum AdvancementType : int {
        ReachedInteraction = 0,
        ReachedMaxDistance = 1,
        ReachedAdaptiveSteplength = 2,
        ReachedBorder = 3
    };

    std::vector<Sector> sector_list;

};

} // namespace PROPOSAL
