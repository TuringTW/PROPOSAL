
#include <algorithm>
#include <stdexcept>

#include "PROPOSAL/crossection/PhotoIntegral.h"
#include "PROPOSAL/crossection/PhotoInterpolant.h"
#include "PROPOSAL/crossection/factories/PhotonuclearFactory.h"
#include "PROPOSAL/crossection/parametrization/PhotoQ2Integration.h"
#include "PROPOSAL/crossection/parametrization/PhotoRealPhotonAssumption.h"
#include "PROPOSAL/crossection/parametrization/Photonuclear.h"
#include "PROPOSAL/medium/Medium.h"

#include "PROPOSAL/Logging.h"

using namespace PROPOSAL;

PhotonuclearFactory::PhotonuclearFactory()
    : photo_shadow_map_str_()
    , photo_shadow_map_enum_()
    , photo_real_map_str_()
    , photo_real_map_enum_()
    , photo_q2_map_str_()
    , photo_q2_map_enum_()
    , string_enum_()
    , string_shadow_enum_()
{
    // Register all photonuclear parametrizations in lower case!

    RegisterShadowEffect("shadowduttarenosarcevicseckel",
        ShadowDuttaRenoSarcevicSeckel, &ShadowDuttaRenoSarcevicSeckel::create);
    RegisterShadowEffect("shadowbutkevichmikhailov", ShadowButkevichMikhailov,
        &ShadowButkevichMikhailov::create);

    RegisterRealPhoton("photozeus", Zeus, &PhotoZeus::create);
    RegisterRealPhoton(
        "photobezrukovbugaev", BezrukovBugaev, &PhotoBezrukovBugaev::create);
    RegisterRealPhoton("photorhode", Rhode, &PhotoRhode::create);
    RegisterRealPhoton("photokokoulin", Kokoulin, &PhotoKokoulin::create);
    RegisterRealPhoton("none", None, nullptr);

    RegisterQ2("photoabramowiczlevinlevymaor91", AbramowiczLevinLevyMaor91,
        &PhotoAbramowiczLevinLevyMaor91::create);
    RegisterQ2("photoabramowiczlevinlevymaor97", AbramowiczLevinLevyMaor97,
        &PhotoAbramowiczLevinLevyMaor97::create);
    RegisterQ2("photobutkevichmikhailov", ButkevichMikhailov,
        &PhotoButkevichMikhailov::create);
    RegisterQ2("photorenosarcevicsu", RenoSarcevicSu,
        &PhotoRenoSarcevicSu::create);
}

PhotonuclearFactory::~PhotonuclearFactory()
{
    photo_shadow_map_str_.clear();
    photo_shadow_map_enum_.clear();

    photo_real_map_str_.clear();
    photo_real_map_enum_.clear();

    photo_q2_map_str_.clear();
    photo_q2_map_enum_.clear();

    string_enum_.clear();
    string_shadow_enum_.clear();
}

// ------------------------------------------------------------------------- //
void PhotonuclearFactory::RegisterShadowEffect(const std::string& name,
    const Shadow& shadow, RegisterShadowEffectFunction create)
{
    photo_shadow_map_str_[name] = create;
    photo_shadow_map_enum_[shadow] = create;
    string_shadow_enum_.insert(name, shadow);
}

// ------------------------------------------------------------------------- //
void PhotonuclearFactory::RegisterRealPhoton(
    const std::string& name, Enum enum_t, RegisterRealPhotonFunction create)
{
    photo_real_map_str_[name] = create;
    photo_real_map_enum_[enum_t] = create;
    string_enum_.insert(name, enum_t);
}

// ------------------------------------------------------------------------- //
void PhotonuclearFactory::RegisterQ2(const std::string& name, Enum enum_t, RegisterQ2Function create)
{
    photo_q2_map_str_[name] = create;
    photo_q2_map_enum_[enum_t] = create;
    string_enum_.insert(name, enum_t);
}

// ------------------------------------------------------------------------- //
ShadowEffect* PhotonuclearFactory::CreateShadowEffect(const std::string& name)
{
    std::string name_lower = name;
    std::transform(name.begin(), name.end(), name_lower.begin(), ::tolower);

    PhotoShadowEffectMapString::const_iterator it
        = photo_shadow_map_str_.find(name_lower);

    if (it != photo_shadow_map_str_.end()) {
        return it->second();
    } else {
        log_fatal("Photonuclear %s not registered!", name.c_str());
        return NULL; // Just to prevent warnings
    }
}

// ------------------------------------------------------------------------- //
ShadowEffect* PhotonuclearFactory::CreateShadowEffect(const Shadow& shadow)
{
    PhotoShadowEffectMapEnum::const_iterator it
        = photo_shadow_map_enum_.find(shadow);

    if (it != photo_shadow_map_enum_.end()) {
        return it->second();
    } else {
        log_fatal("Photonuclear %s not registered!", typeid(shadow).name());
        return NULL; // Just to prevent warnings
    }
}

// ------------------------------------------------------------------------- //
CrossSection* PhotonuclearFactory::CreatePhotonuclear(
    const ParticleDef& particle_def, std::shared_ptr<const Medium> medium,
    std::shared_ptr<const EnergyCutSettings> cuts, const Definition& def,
    std::shared_ptr<const InterpolationDef> interpolation_def) const
{
    PhotoQ2MapEnum::const_iterator it_q2
        = photo_q2_map_enum_.find(def.parametrization);
    if (it_q2 != photo_q2_map_enum_.end()) {
        ShadowEffect* shadow = Get().CreateShadowEffect(def.shadow);
        if (interpolation_def) {
            PhotoInterpolant* photo = new PhotoInterpolant(
                *it_q2->second(particle_def, medium, def.multiplier, *shadow),
                cuts, *interpolation_def);
            delete shadow;
            return photo;
        }
        PhotoIntegral* photo = new PhotoIntegral(
            *it_q2->second(particle_def, medium, def.multiplier, *shadow),
            cuts);
        delete shadow;
        return photo;
    }

    PhotoRealPhotonMapEnum::const_iterator it_photo
        = photo_real_map_enum_.find(def.parametrization);
    if (it_photo != photo_real_map_enum_.end()) {
        if (interpolation_def == nullptr) {
            return new PhotoInterpolant(*it_photo->second(particle_def, medium,
                                            def.multiplier, def.hard_component),
                cuts, *interpolation_def);
        }
        return new PhotoIntegral(*it_photo->second(particle_def, medium,
                                     def.multiplier, def.hard_component),
            cuts);
    }

    std::invalid_argument("Photonuclear not registered!");
}

CrossSection* PhotonuclearFactory::CreatePhotonuclear(
    const Photonuclear& parametrization,
    std::shared_ptr<const EnergyCutSettings> cuts,
    std::shared_ptr<const InterpolationDef> interpolation_def) const
{
    if (interpolation_def) {
        return new PhotoInterpolant(parametrization, cuts, *interpolation_def);
    }
    return new PhotoIntegral(parametrization, cuts);
}

// ------------------------------------------------------------------------- //
PhotonuclearFactory::Enum PhotonuclearFactory::GetEnumFromString(
    const std::string& name)
{
    std::string name_lower = name;
    std::transform(name.begin(), name.end(), name_lower.begin(), ::tolower);

    auto& left = string_enum_.GetLeft();
    auto it = left.find(name_lower);
    if (it != left.end()) {
        return it->second;
    } else {
        log_fatal("Photonuclear %s not registered!", name.c_str());
        return PhotonuclearFactory::Fail; // Just to prevent warnings
    }
}

// ------------------------------------------------------------------------- //
std::string PhotonuclearFactory::GetStringFromEnum(
    const PhotonuclearFactory::Enum& enum_t)
{
    auto& right = string_enum_.GetRight();
    auto it = right.find(enum_t);
    if (it != right.end()) {
        return it->second;
    } else {
        log_fatal("Photonuclear %s not registered!", typeid(enum_t).name());
        return ""; // Just to prevent warnings
    }
}

// ------------------------------------------------------------------------- //
PhotonuclearFactory::Shadow PhotonuclearFactory::GetShadowEnumFromString(
    const std::string& name)
{
    std::string name_lower = name;
    std::transform(name.begin(), name.end(), name_lower.begin(), ::tolower);

    auto& left = string_shadow_enum_.GetLeft();
    auto it = left.find(name_lower);
    if (it != left.end()) {
        return it->second;
    } else {
        log_fatal("Photonuclear %s not registered!", name.c_str());
        return PhotonuclearFactory::ShadowNone; // Just to prevent warnings
    }
}

// ------------------------------------------------------------------------- //
std::string PhotonuclearFactory::GetStringFromShadowEnum(const Shadow& shadow)
{
    auto& right = string_shadow_enum_.GetRight();
    auto it = right.find(shadow);
    if (it != right.end()) {
        return it->second;
    } else {
        log_fatal("Photonuclear %s not registered!", typeid(shadow).name());
        return ""; // Just to prevent warnings
    }
}
