#include "PROPOSAL/Propagator.h"
#include "PROPOSAL/Secondaries.h"
/* #include "PROPOSAL/crosssection/ParticleDefaultCrossSectionList.h" */
/* #include "PROPOSAL/crosssection/factories/AnnihilationFactory.h" */
/* #include "PROPOSAL/crosssection/factories/BremsstrahlungFactory.h" */
/* #include "PROPOSAL/crosssection/factories/ComptonFactory.h" */
/* #include "PROPOSAL/crosssection/factories/EpairProductionFactory.h" */
/* #include "PROPOSAL/crosssection/factories/IonizationFactory.h" */
/* #include "PROPOSAL/crosssection/factories/MupairProductionFactory.h" */
/* #include "PROPOSAL/crosssection/factories/PhotoPairFactory.h" */
/* #include "PROPOSAL/crosssection/factories/PhotonuclearFactory.h" */
/* #include "PROPOSAL/crosssection/factories/WeakInteractionFactory.h" */
#include "PROPOSAL/geometry/GeometryFactory.h"
#include "PROPOSAL/math/RandomGenerator.h"
#include "PROPOSAL/medium/MediumFactory.h"
/* #include "PROPOSAL/scattering/ScatteringFactory.h" */
#include <fstream>

#include <iomanip>

using namespace PROPOSAL;

/* Propagator::Propagator(const ParticleDef& p_def, const std::string&
 * config_file) */
/*     : Propagator(p_def, ParseConfig(config_file)) */
/* { */
/* } */

/* Propagator::Propagator(const ParticleDef& p_def, const nlohmann::json&
 * config) */
/*     : p_def(p_def) */
/* { */
/*     GlobalSettings global; */
/*     if (config.contains("global")) { */
/*         global = GlobalSettings(config["global"]); */
/*     } */
/*     if (config.contains("interpolation")) { */
/*         interpol_def_global */
/*             = std::make_shared<InterpolationDef>(config["interpolation"]); */
/*     } else { */
/*         interpol_def_global = std::make_shared<InterpolationDef>(); */
/*     } */
/*     if (config.contains("sectors")) { */
/*         assert(config["sectors"].is_array()); */
/*         for (const auto& json_sector : config.at("sectors")) { */
/*             InitializeSectorFromJSON(p_def, json_sector, global); */
/*         } */
/*     } else { */
/*         throw std::invalid_argument("No sector array found in json object");
 */
/*     } */
/* } */

Propagator::Propagator(const ParticleDef& p_def, std::vector<Sector> sectors)
    : p_def(p_def)
    , sector_list(sectors)
{
}

std::vector<DynamicData> Propagator::Propagate(
    const DynamicData& initial_particle, double max_distance, double min_energy)
{
    auto track = std::vector<DynamicData>{ initial_particle };
    track.back().SetType(p_def.particle_type);

    auto current_sector = ChooseCurrentSector(
            track.back().GetPosition(), track.back().GetDirection());
    auto rnd
        = std::bind(&RandomGenerator::RandomDouble, &RandomGenerator::Get());

    int advancement_type;
    auto continue_propagation = true;

    // TODO: How to get accurate low information?
    auto InteractionEnergy
        = std::array<double, 3>{std::max(min_energy, p_def.mass), 0., 0.};
    while (continue_propagation) {
        auto& utility = get<UTILITY>(current_sector);
        auto& density = get<DENSITY_DISTR>(current_sector);

        InteractionEnergy[Decay] = utility.EnergyDecay(track.back().GetEnergy(),
                rnd, density->Evaluate(track.back().GetPosition()));
        InteractionEnergy[StochasticELoss]
            = utility.EnergyInteraction(track.back().GetEnergy(), rnd);
        std::cout << " ---------- " << std::endl;
        std::cout << "Decay: " << InteractionEnergy[Decay] << ", " << "Stochastic: " << InteractionEnergy[StochasticELoss] << std::endl;

        auto next_interaction_type = maximize(InteractionEnergy);
        auto& energy_at_next_interaction
            = InteractionEnergy[next_interaction_type];

        track.push_back(track.back());
        advancement_type = AdvanceParticle(track.back(),
                                           energy_at_next_interaction,
                                           max_distance - track.back().GetPropagatedDistance(),
                                           rnd, current_sector);

        switch (advancement_type) {
            case ReachedInteraction:
                switch (next_interaction_type) {
                    case StochasticELoss:
                        track.push_back(track.back());
                        DoStochasticInteraction(track.back(), utility, rnd);
                        if (track.back().GetEnergy() <= InteractionEnergy[MinimalE])
                            continue_propagation = false;
                        break;
                    case Decay:
                        track.push_back(track.back());
                        track.back().SetType(InteractionType::Decay);
                        continue_propagation = false;
                        break;
                    case MinimalE:
                        continue_propagation = false;
                        break;
                }
                break;
            case ReachedMaxDistance:
                continue_propagation = false;
                break;
            case ReachedAdaptiveSteplength:
                break;
            case ReachedBorder:
                track.push_back(track.back());
                current_sector = ChooseCurrentSector(
                        track.back().GetPosition(), track.back().GetDirection());
                break;
        }

    }
    return track;
}

void Propagator::DoStochasticInteraction(DynamicData& p_cond,
    PropagationUtility& utility, std::function<double()> rnd)
{
    InteractionType loss_type;
    const Component* comp;
    double loss_energy;
    std::tie(loss_type, comp, loss_energy)
        = utility.EnergyStochasticloss(p_cond.GetEnergy(), rnd());

    /* auto deflection_angles = stochastic_cross->StochasticDeflection( */
    /*     p_cond.GetEnergy(), loss_energy); // TODO: ugly */
    /* p_cond.DeflectDirection( */
    /*     get<0>(deflection_angles), get<1>(deflection_angles)); */

    p_cond.SetEnergy(p_cond.GetEnergy() - loss_energy);
    p_cond.SetType(loss_type);
}

int Propagator::AdvanceParticle(DynamicData& p_cond, double E_f,
    double max_distance, std::function<double()> rnd, Sector& current_sector)
{
    assert(max_distance > 0);
    assert(E_f >= 0);

    // TODO: For NoScattering, these random numbers are not used
    auto rnd_scattering = std::array<double, 4>{ rnd(), rnd(), rnd(), rnd() };

    auto& utility = get<UTILITY>(current_sector);
    auto& density = get<DENSITY_DISTR>(current_sector);
    auto& geometry = get<GEOMETRY>(current_sector);

    auto AdvanceGrammage = std::array<double, 3>{0., 0., 0.};
    AdvanceGrammage[ReachedInteraction] = utility.LengthContinuous(
            p_cond.GetEnergy(), E_f);
    AdvanceGrammage[ReachedMaxDistance] = density->Calculate(
            p_cond.GetPosition(), p_cond.GetDirection(), max_distance);
    double distance_adaptive = CalculateAdaptiveSteplength(
            p_cond.GetPosition(), max_distance, geometry); //TODO: passing max_distance not optimal (performance)
    AdvanceGrammage[ReachedAdaptiveSteplength] = density->Calculate(
            p_cond.GetPosition(), p_cond.GetDirection(), distance_adaptive);

    auto advancement_type = minimize(AdvanceGrammage);
    std::cout << "Advancement_type: " << advancement_type << std::endl; //DEBUG
    auto& advance_grammage = AdvanceGrammage[advancement_type];

    if (advancement_type != ReachedInteraction) {
        E_f = utility.EnergyDistance(p_cond.GetEnergy(), advance_grammage);
    }

    Vector3D mean_direction, new_direction;
    std::tie(mean_direction, new_direction) = utility.DirectionsScatter(
            advance_grammage, p_cond.GetEnergy(), E_f, p_cond.GetDirection(),
        rnd_scattering);

    double advance_distance;
    if (advancement_type == ReachedInteraction) {
        //TODO: Is max_distance an appropriate upper limit to pass here?
        advance_distance = density->Correct(p_cond.GetPosition(), mean_direction,
                                            advance_grammage, max_distance);
    } else {
        advance_distance = std::min(max_distance, distance_adaptive);
    }

    auto new_position = p_cond.GetPosition() + advance_distance * mean_direction;

    // TODO: We could also compare the utilities here, but this may lead to
    // errors for weird combined geometries
    // TODO: Catch behaviour for no defined sector
    if (geometry
        != get<GEOMETRY>(ChooseCurrentSector(new_position, new_direction))) {
        std::cout << "Sector changed, varying propagation step..." << std::endl;
        std::cout << "Initial distance: " << advance_distance;

        double control_distance;
        advancement_type = ReachedBorder;
        do {
            advance_distance = CalculateDistanceToBorder(p_cond.GetPosition(),
                               mean_direction, *geometry);
            //TODO: Is it correct to calculate the grammage according to the old direction?
            advance_grammage = density->Calculate(p_cond.GetPosition(),
                    p_cond.GetDirection(), advance_distance);
            E_f = utility.EnergyDistance(p_cond.GetEnergy(), advance_grammage);

            std::tie(mean_direction, new_direction) = utility.DirectionsScatter(
                    advance_grammage, p_cond.GetEnergy(), E_f,
                    p_cond.GetDirection(), rnd_scattering);

            control_distance = CalculateDistanceToBorder(p_cond.GetPosition(),
                               mean_direction, *geometry);

            std::cout << "Step: Corrected distance: " << advance_distance << std::endl;
            std::cout << "Control distance: " << control_distance << std::endl;
        } while (std::abs(advance_distance - control_distance)
            > PARTICLE_POSITION_RESOLUTION);
        std::cout << "Sufficient precision reached." << std::endl;
        advance_distance = control_distance; //precision
        new_position = p_cond.GetPosition() + advance_distance * mean_direction;
    }

    p_cond.SetTime(p_cond.GetTime() + utility.TimeElapsed(p_cond.GetEnergy(),
            E_f, advance_distance, density->Evaluate(p_cond.GetPosition())));
    p_cond.SetPosition(new_position);
    p_cond.SetDirection(new_direction);
    p_cond.SetPropagatedDistance(p_cond.GetPropagatedDistance()
        + advance_distance);
    p_cond.SetEnergy(utility.EnergyRandomize(p_cond.GetEnergy(), E_f, rnd));
    p_cond.SetType(InteractionType::ContinuousEnergyLoss);

    return advancement_type;
}

double Propagator::CalculateAdaptiveSteplength(const Vector3D &position,
        double steplength, std::shared_ptr<const Geometry> current_geometry) {
    double adaptive_steplength = INF;
    for (auto& sector : sector_list) {
        auto& geometry = get<GEOMETRY>(sector);
        if (geometry != current_geometry and geometry->GetHierarchy() >= current_geometry->GetHierarchy()) {
            double geometry_steplength = geometry->CalculateAdaptiveSteplength(position, steplength);
            if(geometry_steplength > PARTICLE_POSITION_RESOLUTION) {
                //If condition necessary for the case where we just left a geometry
                adaptive_steplength = std::min(adaptive_steplength, geometry_steplength);
            }
        }
    }
    std::cout << "Adpative steplength: " << adaptive_steplength << std::endl; //DEBUG
    return adaptive_steplength;
}

double Propagator::CalculateDistanceToBorder(const Vector3D &position,
       const Vector3D &direction, const Geometry& current_geometry) {
    auto distance_border
        = current_geometry.DistanceToBorder(position, direction).first;
    for (auto& sector : sector_list) {
        auto& geometry = get<GEOMETRY>(sector);
        if (geometry->GetHierarchy() > current_geometry.GetHierarchy())
            distance_border = std::min(distance_border,
                  geometry->DistanceToBorder(position, direction).first);
    }
    return distance_border;
}

int Propagator::maximize(const std::array<double, 3>& InteractionEnergies)
{
    auto max_element_ref = std::max_element(
        InteractionEnergies.begin(), InteractionEnergies.end());
    return std::distance(InteractionEnergies.begin(), max_element_ref);
}

int Propagator::minimize(const std::array<double, 3>& AdvanceGrammage)
{
    auto min_element_ref = std::min_element(
            AdvanceGrammage.begin(), AdvanceGrammage.end());
    return std::distance(AdvanceGrammage.begin(), min_element_ref);
}

Sector Propagator::ChooseCurrentSector(
    const Vector3D& position, const Vector3D& direction)
{
    auto potential_sec = std::vector<Sector*>{};
    for (auto& sector : sector_list) {
        if (get<GEOMETRY>(sector)->IsInside(position, direction))
            potential_sec.push_back(&sector);
    }

    if (potential_sec.empty())
        throw std::logic_error(
            "Propagator: No sector defined at current particle position.");

    auto highest_sector_iter = std::max_element(
        potential_sec.begin(), potential_sec.end(), [](Sector* a, Sector* b) {
            return get<GEOMETRY>(*a)->GetHierarchy()
                < get<GEOMETRY>(*b)->GetHierarchy();
        });

    return **highest_sector_iter;
}

// Init methods

/* nlohmann::json Propagator::ParseConfig(const std::string& config_file) */
/* { */
/*     nlohmann::json json_config; */
/*     try { */
/*         std::string expanded_config_file_path */
/*             = Helper::ResolvePath(config_file, true); */
/*         std::ifstream infilestream(expanded_config_file_path); */
/*         infilestream >> json_config; */
/*     } catch (const nlohmann::json::parse_error& e) { */
/*         Logging::Get("proposal.propagator")->critical("Unable parse \"%s\" as json file", config_file.c_str()); */
/*     } */
/*     return json_config; */
/* }; */

/* void Propagator::InitializeSectorFromJSON(const ParticleDef& p_def, */
/*     const nlohmann::json& json_sector, GlobalSettings global) */
/* { */
/*     bool do_interpolation */
/*         = json_sector.value("do_interpolation", global.do_interpolation); */
/*     bool do_exact_time = json_sector.value("exact_time",
 * global.do_exact_time); */
/*     std::string scattering = json_sector.value("scattering",
 * global.scattering); */
/*     std::shared_ptr<Medium> medium = global.medium; */
/*     if (json_sector.contains("medium")) { */
/*         medium = CreateMedium(json_sector["medium"]); */
/*     } else if (medium == nullptr) { */
/*         throw std::invalid_argument( */
/*             "Neither a specific Sector medium nor a global medium is
 * defined."); */
/*     } */
/*     std::shared_ptr<EnergyCutSettings> cuts = global.cuts; */
/*     if (json_sector.contains("cuts")) { */
/*         cuts = std::make_shared<EnergyCutSettings>( */
/*             EnergyCutSettings(json_sector["cuts"])); */
/*     } else if (cuts == nullptr) { */
/*         throw std::invalid_argument("Neither a specific Sector EnergyCut nor
 * a " */
/*                                     "global EnergyCut is defined."); */
/*     } */
/*     CrossSectionList crosssections; */
/*     if (json_sector.contains("CrossSections")) { */
/*         crosssections = CreateCrossSectionList( */
/*             json_sector["CrossSections"], medium, cuts, do_interpolation); */
/*     } else if (global.cross != "") { */
/*         crosssections = CreateCrossSectionList( */
/*             global.cross, medium, cuts, do_interpolation); */
/*     } else { */
/*         crosssections = GetStdCrossSections(medium, cuts, p_def); */
/*     } */
/*     // Create Geometry and add them to sector_list with corresponding
 * geometries */
/*     auto utility = CreateUtility(crosssections, medium, cuts->GetContRand(),
 */
/*         do_interpolation, do_exact_time, scattering); */
/*     if (json_sector.contains("geometries")) { */
/*         assert(json_sector["geometries"].is_array()); */
/*         nlohmann::json density_distr_default = { */
/*             { "density_distr_type", "homogeneous" } */
/*         }; // if no density_distr defined, fallback to homogeneous medium */
/*         auto density_distr_sector */
/*             = json_sector.value("density_distr", density_distr_default); */
/*         for (const auto& json_geometry : json_sector.at("geometries")) { */
/*             auto density_distr_json */
/*                 = json_geometry.value("density_distr", density_distr_sector);
 */
/*             sector_list.push_back(std::make_tuple(CreateGeometry(json_geometry),
 */
/*                 utility, CreateDensityDistribution(density_distr_json))); */
/*         } */
/*     } else { */
/*         throw std::invalid_argument( */
/*             "At least one geometry must be defined for each sector"); */
/*     } */
/* } */

/* PropagationUtility Propagator::CreateUtility(CrossSectionList crosssections,
 */
/*     std::shared_ptr<Medium> medium, bool do_cont_rand, bool do_interpolation,
 */
/*     bool do_exact_time, std::string scattering) */
/* { */
/*     PropagationUtility::Collection def; */
/*     if (do_interpolation) { */
/*         def.interaction_calc */
/*             = std::make_shared<InteractionBuilder<UtilityInterpolant>>( */
/*                 crosssections); */
/*     } else { */
/*         def.interaction_calc */
/*             = std::make_shared<InteractionBuilder<UtilityIntegral>>( */
/*                 crosssections); */
/*     } */
/*     if (do_interpolation) { */
/*         def.displacement_calc */
/*             = std::make_shared<DisplacementBuilder<UtilityInterpolant>>( */
/*                 crosssections); */
/*     } else { */
/*         def.displacement_calc */
/*             = std::make_shared<DisplacementBuilder<UtilityIntegral>>( */
/*                 crosssections); */
/*     } */
/*     if (do_exact_time) { */
/*         if (do_interpolation) { */
/*             def.time_calc */
/*                 = std::make_shared<ExactTimeBuilder<UtilityInterpolant>>( */
/*                     crosssections, p_def); */
/*         } else { */
/*             def.time_calc =
 * std::make_shared<ExactTimeBuilder<UtilityIntegral>>( */
/*                 crosssections, p_def); */
/*         } */
/*     } else { */
/*         def.time_calc = std::make_shared<ApproximateTimeBuilder>(); */
/*     } */
/*     if (scattering != "" and scattering != "NoScattering") { */
/*         if (do_interpolation) { */
/*             // TODO: Making a shared_ptr out of a raw pointer is bad-practise
 */
/*             // (and looks ugly), the underlying factory should be changed
 * (jm) */
/*             auto aux = ScatteringFactory::Get().CreateScattering(scattering,
 */
/*                 p_def, medium, interpol_def_global, */
/*                 std::unique_ptr<CrossSectionList>( */
/*                     new CrossSectionList(crosssections))); */
/*             def.scattering = std::shared_ptr<Scattering>(aux); */
/*         } else { */
/*             auto aux = ScatteringFactory::Get().CreateScattering(scattering,
 */
/*                 p_def, medium, */
/*                 std::unique_ptr<CrossSectionList>( */
/*                     new CrossSectionList(crosssections))); */
/*             def.scattering = std::shared_ptr<Scattering>(aux); */
/*         } */
/*     } */
/*     if (isfinite(p_def.lifetime)) { */
/*         if (do_interpolation) { */
/*             def.decay_calc =
 * std::make_shared<DecayBuilder<UtilityInterpolant>>( */
/*                 crosssections, p_def); */
/*         } else { */
/*             def.decay_calc = std::make_shared<DecayBuilder<UtilityIntegral>>(
 */
/*                 crosssections, p_def); */
/*         } */
/*     } */
/*     if (do_cont_rand) { */
/*         if (do_interpolation) { */
/*             def.cont_rand */
/*                 = std::make_shared<ContRandBuilder<UtilityInterpolant>>( */
/*                     crosssections); */
/*         } else { */
/*             def.cont_rand =
 * std::make_shared<ContRandBuilder<UtilityIntegral>>( */
/*                 crosssections); */
/*         } */
/*     } */
/*     return def; */
/* } */

/* CrossSectionList Propagator::CreateCrossSectionList( */
/*     const nlohmann::json& config_cross, std::shared_ptr<Medium> medium, */
/*     std::shared_ptr<EnergyCutSettings> cuts, bool do_interpolation) */
/* { */
/*     CrossSectionList crosssections; */
/*     std::shared_ptr<InterpolationDef> interpol_def = nullptr; */
/*     if (do_interpolation) { */
/*         interpol_def = interpol_def_global; */
/*     } */
/*     if (config_cross.contains("annihilation")) { */
/*         AnnihilationFactory::Definition annihilation_def( */
/*             config_cross["annihilation"]); */
/*         crosssections.emplace_back( */
/*             AnnihilationFactory::Get().CreateAnnihilation( */
/*                 p_def, medium, annihilation_def, interpol_def)); */
/*     } */
/*     if (config_cross.contains("brems")) { */
/*         BremsstrahlungFactory::Definition brems_def(config_cross["brems"]);
 */
/*         crosssections.emplace_back( */
/*             BremsstrahlungFactory::Get().CreateBremsstrahlung( */
/*                 p_def, medium, cuts, brems_def, interpol_def)); */
/*     } */
/*     if (config_cross.contains("compton")) { */
/*         ComptonFactory::Definition compton_def(config_cross["compton"]); */
/*         crosssections.emplace_back(ComptonFactory::Get().CreateCompton( */
/*             p_def, medium, cuts, compton_def, interpol_def)); */
/*     } */
/*     if (config_cross.contains("epair")) { */
/*         EpairProductionFactory::Definition epair_def(config_cross["epair"]);
 */
/*         crosssections.emplace_back( */
/*             EpairProductionFactory::Get().CreateEpairProduction( */
/*                 p_def, medium, cuts, epair_def, interpol_def)); */
/*     } */
/*     if (config_cross.contains("ioniz")) { */
/*         IonizationFactory::Definition ioniz_def(config_cross["ioniz"]); */
/*         crosssections.emplace_back(IonizationFactory::Get().CreateIonization(
 */
/*             p_def, medium, cuts, ioniz_def, interpol_def)); */
/*     } */
/*     if (config_cross.contains("mupair")) { */
/*         MupairProductionFactory::Definition
 * mupair_def(config_cross["mupair"]); */
/*         crosssections.emplace_back( */
/*             MupairProductionFactory::Get().CreateMupairProduction( */
/*                 p_def, medium, cuts, mupair_def, interpol_def)); */
/*     } */
/*     if (config_cross.contains("photonuclear")) { */
/*         PhotonuclearFactory::Definition photonuclear_def( */
/*             config_cross["photonuclear"]); */
/*         crosssections.emplace_back( */
/*             PhotonuclearFactory::Get().CreatePhotonuclear( */
/*                 p_def, medium, cuts, photonuclear_def, interpol_def)); */
/*     } */
/*     if (config_cross.contains("photopair")) { */
/*         PhotoPairFactory::Definition
 * photopair_def(config_cross["photopair"]); */
/*         crosssections.emplace_back(PhotoPairFactory::Get().CreatePhotoPair(
 */
/*             p_def, medium, photopair_def, interpol_def)); */
/*     } */
/*     if (config_cross.contains("weak")) { */
/*         WeakInteractionFactory::Definition weak_def(config_cross["weak"]); */
/*         crosssections.emplace_back( */
/*             WeakInteractionFactory::Get().CreateWeakInteraction( */
/*                 p_def, medium, weak_def, interpol_def)); */
/*     } */
/*     if (crosssections.size() == 0) { */
/*         throw std::invalid_argument( */
/*             "No crosssections could be initialized for sector"); */
/*     } */
/*     return crosssections; */
/* } */

Propagator::GlobalSettings::GlobalSettings(const nlohmann::json& config_global)
{
    if (config_global.contains("medium")) {
        medium = CreateMedium(config_global);
    }
    if (config_global.contains("CrossSections")) {
        cross = config_global["CrossSections"];
    } else {
        cross = "";
    }
    if (config_global.contains("cuts")) {
        cuts = std::make_shared<EnergyCutSettings>(config_global["cuts"]);
    }
    // Read global propagation settings
    do_exact_time = config_global.value("exact_time", true);
    do_interpolation = config_global.value("do_interpolation", true);
    scattering = config_global.value("scattering", "");
}

Propagator::GlobalSettings::GlobalSettings()
{
    medium = nullptr;
    cross = nullptr;
    do_exact_time = true;
    do_interpolation = true;
    scattering = "";
}
