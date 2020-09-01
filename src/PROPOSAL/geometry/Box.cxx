
#include <cmath>
#include <vector>

#include "PROPOSAL/Constants.h"
#include "PROPOSAL/Logging.h"
#include "PROPOSAL/geometry/Box.h"

using namespace PROPOSAL;


Box::Box(const Vector3D position, double x, double y, double z)
    : Geometry("Box", position)
    , x_(x)
    , y_(y)
    , z_(z)
    , space_diagonal_(std::sqrt(x * x + y * y + z * z))
{
}

Box::Box(const nlohmann::json& config)
    : Geometry(config)
{
    if(not config.at("length").is_number())
        throw std::invalid_argument("Length is not a number.");
    if(not config.at("width").is_number())
        throw std::invalid_argument("Width is not a number.");
    if(not config.at("height").is_number())
        throw std::invalid_argument("Height is not a number.");

    x_ = config["length"].get<double>();
    y_ = config["width"].get<double>();
    z_ = config["height"].get<double>();

    if(x_ < 0) throw std::logic_error("lenght must be > 0");
    if(y_ < 0) throw std::logic_error("width must be > 0");
    if(z_ < 0) throw std::logic_error("height must be > 0");
}

bool Box::compare(const Geometry& geometry) const
{
    const Box* box = dynamic_cast<const Box*>(&geometry);

    if (!box)
        return false;
    else if (x_ != box->x_)
        return false;
    else if (y_ != box->y_)
        return false;
    else if (z_ != box->z_)
        return false;
    else if (space_diagonal_ != box->space_diagonal_)
        return false;
    else
        return true;
}

// ------------------------------------------------------------------------- //
void Box::print(std::ostream& os) const
{
    os << "Width_x: " << x_ << "\tWidth_y " << y_ << "\tHeight: " << z_ << '\n';
}

// ------------------------------------------------------------------------- //
std::pair<double, double> Box::DistanceToBorder(const Vector3D& position, const Vector3D& direction) const
{
    // Calculate intersection of particle trajectory and the box
    // Surface of the box is defined by six planes:
    // E1: x1   =   position.GetX() + 0.5*x
    // E2: x1   =   position.GetX() - 0.5*x
    // E3: x2   =   position.GetY() + 0.5*y
    // E4: x2   =   position.GetY() - 0.5*y
    // E5: x3   =   position.GetZ() + 0.5*z
    // E6: x3   =   position.GetZ() - 0.5*z
    // straight line (particle trajectory) g = vec(x,y,z) + t * dir_vec( cosph
    // *sinth, sinph *sinth , costh)
    // We are only interested in postive values of t
    // ( we want to find the intersection in direction of the particle
    // trajectory)

    double dir_vec_x = direction.GetX();
    double dir_vec_y = direction.GetY();
    double dir_vec_z = direction.GetZ();

    std::pair<double, double> distance;
    double t;
    double intersection_x;
    double intersection_y;
    double intersection_z;

    std::vector<double> dist;

    double x_calc_pos = position_.GetX() + 0.5 * x_;
    double x_calc_neg = position_.GetX() - 0.5 * x_;
    double y_calc_pos = position_.GetY() + 0.5 * y_;
    double y_calc_neg = position_.GetY() - 0.5 * y_;
    double z_calc_pos = position_.GetZ() + 0.5 * z_;
    double z_calc_neg = position_.GetZ() - 0.5 * z_;

    // intersection with E1
    if (dir_vec_x != 0) // if dir_vec == 0 particle trajectory is parallel to E1
    {
        t = (x_calc_pos - position.GetX()) / dir_vec_x;

        // Computer precision controll
        if (t > 0 && t < GEOMETRY_PRECISION)
            t = 0;

        if (t > 0) // Interection is in particle trajectory direction
        {
            // Check if intersection is inside the box borders
            intersection_y = position.GetY() + t * dir_vec_y;
            intersection_z = position.GetZ() + t * dir_vec_z;
            if (intersection_y >= y_calc_neg && intersection_y <= y_calc_pos && intersection_z >= z_calc_neg &&
                intersection_z <= z_calc_pos)
            {
                dist.push_back(t);
            }
        }
    }

    // intersection with E2
    if (dir_vec_x != 0) // if dir_vec == 0 particle trajectory is parallel to E2
    {
        t = (x_calc_neg - position.GetX()) / dir_vec_x;

        // Computer precision controll
        if (t > 0 && t < GEOMETRY_PRECISION)
            t = 0;

        if (t > 0) // Interection is in particle trajectory direction
        {
            // Check if intersection is inside the box borders
            intersection_y = position.GetY() + t * dir_vec_y;
            intersection_z = position.GetZ() + t * dir_vec_z;
            if (intersection_y >= y_calc_neg && intersection_y <= y_calc_pos && intersection_z >= z_calc_neg &&
                intersection_z <= z_calc_pos)
            {
                dist.push_back(t);
            }
        }
    }

    // intersection with E3
    if (dir_vec_y != 0) // if dir_vec == 0 particle trajectory is parallel to E3
    {
        t = (y_calc_pos - position.GetY()) / dir_vec_y;

        // Computer precision controll
        if (t > 0 && t < GEOMETRY_PRECISION)
            t = 0;

        if (t > 0) // Interection is in particle trajectory direction
        {
            // Check if intersection is inside the box borders
            intersection_x = position.GetX() + t * dir_vec_x;
            intersection_z = position.GetZ() + t * dir_vec_z;
            if (intersection_x >= x_calc_neg && intersection_x <= x_calc_pos && intersection_z >= z_calc_neg &&
                intersection_z <= z_calc_pos)
            {
                dist.push_back(t);
            }
        }
    }

    // intersection with E4
    if (dir_vec_y != 0) // if dir_vec == 0 particle trajectory is parallel to E4
    {
        t = (y_calc_neg - position.GetY()) / dir_vec_y;

        // Computer precision controll
        if (t > 0 && t < GEOMETRY_PRECISION)
            t = 0;

        if (t > 0) // Interection is in particle trajectory direction
        {
            // Check if intersection is inside the box borders
            intersection_x = position.GetX() + t * dir_vec_x;
            intersection_z = position.GetZ() + t * dir_vec_z;
            if (intersection_x >= x_calc_neg && intersection_x <= x_calc_pos && intersection_z >= z_calc_neg &&
                intersection_z <= z_calc_pos)
            {
                dist.push_back(t);
            }
        }
    }

    // intersection with E5
    if (dir_vec_z != 0) // if dir_vec == 0 particle trajectory is parallel to E5
    {
        t = (z_calc_pos - position.GetZ()) / dir_vec_z;

        // Computer precision controll
        if (t > 0 && t < GEOMETRY_PRECISION)
            t = 0;

        if (t > 0) // Interection is in particle trajectory direction
        {
            // Check if intersection is inside the box borders
            intersection_x = position.GetX() + t * dir_vec_x;
            intersection_y = position.GetY() + t * dir_vec_y;
            if (intersection_x >= x_calc_neg && intersection_x <= x_calc_pos && intersection_y >= y_calc_neg &&
                intersection_y <= y_calc_pos)
            {
                dist.push_back(t);
            }
        }
    }

    // intersection with E6
    if (dir_vec_z != 0) // if dir_vec == 0 particle trajectory is parallel to E6
    {
        t = (z_calc_neg - position.GetZ()) / dir_vec_z;

        // Computer precision controll
        if (t > 0 && t < GEOMETRY_PRECISION)
            t = 0;

        if (t > 0) // Interection is in particle trajectory direction
        {
            // Check if intersection is inside the box borders
            intersection_x = position.GetX() + t * dir_vec_x;
            intersection_y = position.GetY() + t * dir_vec_y;
            if (intersection_x >= x_calc_neg && intersection_x <= x_calc_pos && intersection_y >= y_calc_neg &&
                intersection_y <= y_calc_pos)
            {
                dist.push_back(t);
            }
        }
    }

    if (dist.size() < 1) // No intersection with the box
    {
        distance.first  = -1;
        distance.second = -1;
    } else if (dist.size() == 1) // Particle is inside the box and we have one
                                 // intersection in direction of the particle
                                 // trajectory
    {
        distance.first  = dist.at(0);
        distance.second = -1;
    } else if (dist.size() == 2) // Particle is outside and the box is infront
                                 // of the particle trajectory ( two
                                 // intersections).
    {
        distance.first  = dist.at(0);
        distance.second = dist.at(1);
        if (distance.second < distance.first)
        {
            std::swap(distance.first, distance.second);
        }

    } else
    {
        Logging::Get("proposal.geometry")->error("This point should nerver be reached... (-1/-1) is returned");

        distance.first  = -1;
        distance.second = -1;
    }

    // Make a computer precision controll!
    // This is necessary cause due to numerical effects it meight be happen
    // that a particle which is located on a gemoetry border is treated as
    // inside
    // or outside
    if (distance.first < GEOMETRY_PRECISION)
        distance.first = -1;
    if (distance.second < GEOMETRY_PRECISION)
        distance.second = -1;
    if (distance.first < 0)
        std::swap(distance.first, distance.second);

    return distance;
}

// ------------------------------------------------------------------------- //
double Box::CalculateAdaptiveSteplength(const Vector3D& position, double steplength) const
{
    double distance_from_center = (position - position_).magnitude();

    if(steplength <= distance_from_center - 0.5 * space_diagonal_)
        return steplength;

    return DistanceToPoint(position) + PARTICLE_POSITION_RESOLUTION;
    /* TODO: The added distance is a tolerance that says how far we can propagate
     * into the box. If the tolerance is zero, we would never reach the box
     * (or at least need a large amount of small steps). If the tolerance is too
     * large, we risk skipping the box. PARTICLE_POSITION_RESOLUTION is probably
     * too small, this needs to be discussed and tested.
     */
}

// ------------------------------------------------------------------------- //
double Box::DistanceToPoint(const Vector3D& position) const
{
    double x_max = position_.GetX() + 0.5 * x_;
    double x_min = position_.GetX() - 0.5 * x_;
    double y_max = position_.GetY() + 0.5 * y_;
    double y_min = position_.GetY() - 0.5 * y_;
    double z_max = position_.GetZ() + 0.5 * z_;
    double z_min = position_.GetZ() - 0.5 * z_;

    double dx = std::max({x_min - position.GetX(), 0., position.GetX() - x_max});
    double dy = std::max({y_min - position.GetY(), 0., position.GetY() - y_max});
    double dz = std::max({z_min - position.GetZ(), 0., position.GetZ() - z_max});

    return std::sqrt(dx * dx + dy * dy + dz * dz);
}