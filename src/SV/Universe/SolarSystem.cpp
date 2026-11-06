#include "SolarSystem.hpp"

#include <chrono>
#include <cmath>
#include <functional>

#include <glm/glm.hpp>

#include "Dahlia/Maths/Maths.h"
#include "SV/Atmosphere/Atmosphere.hpp"
#include "Coordinates.hpp"
#include "MoonData.hpp"
#include "PlanetarySystem.hpp"
#include "Planet.hpp"
#include "Star.hpp"

namespace SV::Universe
{
	namespace
	{
		constexpr double METRES_PER_AU = 149597870700.0;
		constexpr double SECONDS_PER_DAY = 86400.0;
		// J2000.0 as a Unix time (2000-01-01 12:00:00 UTC)
		constexpr double J2000_UNIX_SECONDS = 946728000.0;

		constexpr double SUN_MASS = 1.98847E30;
		constexpr double SUN_RADIUS_KM = 695700.0;

		// Keplerian elements of a planet's orbit, relative to the mean ecliptic and equinox of J2000,
		// with their rates of change per Julian century. From E. M. Standish, "Keplerian Elements for
		// Approximate Positions of the Major Planets" (JPL), table 1 (valid 1800 to 2050).
		struct OrbitalElements
		{
			double semiMajorAxis, semiMajorAxisRate;         // AU
			double eccentricity, eccentricityRate;
			double inclination, inclinationRate;             // Degrees
			double meanLongitude, meanLongitudeRate;         // Degrees
			double perihelionLongitude, perihelionLongitudeRate; // Degrees
			double ascendingNode, ascendingNodeRate;         // Degrees
		};

		struct PlanetDescription
		{
			const char* name;
			std::function<sRef<Planet>()> create;
			double equatorialRadiusKm;
			OrbitalElements orbit;
			// Direction of the north pole (IAU WGCCRE 2015), equatorial J2000
			double poleRightAscension, poleDeclination; // Degrees
		};

		// Turns the body so its local +Y (its rotation axis) points along `pole`
		void PointAxis(CelestrialBody& body, const glm::dvec3& pole)
		{
			const glm::dvec3 up(0.0, 1.0, 0.0);
			const glm::dvec3 axis = glm::cross(up, pole);
			const double length = glm::length(axis);
			if (length < 1e-9)
				return;
			const double angle = std::atan2(length, glm::dot(up, pole));
			body.Rotate(glm::vec4(glm::vec3(axis / length), (float)(angle / DEGREES)));
		}

		// Heliocentric position (world units) and velocity (m/s) of an orbit at a time
		void OrbitalState(const OrbitalElements& elements, const double days, const double mu,
						  glm::dvec3& position, glm::dvec3& velocity)
		{
			const double centuries = days / 36525.0;
			const double a = (elements.semiMajorAxis + elements.semiMajorAxisRate * centuries) * METRES_PER_AU;
			const double e = elements.eccentricity + elements.eccentricityRate * centuries;
			const double inclination = (elements.inclination + elements.inclinationRate * centuries) * DEGREES;
			const double meanLongitude = elements.meanLongitude + elements.meanLongitudeRate * centuries;
			const double perihelion = elements.perihelionLongitude + elements.perihelionLongitudeRate * centuries;
			const double node = (elements.ascendingNode + elements.ascendingNodeRate * centuries) * DEGREES;
			const double argumentOfPerihelion = perihelion * DEGREES - node;
			const double meanAnomaly = std::remainder(meanLongitude - perihelion, 360.0) * DEGREES;

			// Kepler's equation, M = E - e sin E
			double E = meanAnomaly + e * std::sin(meanAnomaly);
			for (int i = 0; i < 10; i++)
				E -= (E - e * std::sin(E) - meanAnomaly) / (1.0 - e * std::cos(E));

			// In the orbit's plane, x towards perihelion
			const double b = a * std::sqrt(1.0 - e * e);
			const double rate = std::sqrt(mu / (a * a * a)) / (1.0 - e * std::cos(E)); // dE/dt
			const glm::dvec2 planePosition(a * (std::cos(E) - e), b * std::sin(E));
			const glm::dvec2 planeVelocity(-a * std::sin(E) * rate, b * std::cos(E) * rate);

			// Rotate by the argument of perihelion, the inclination and the ascending node
			const double cw = std::cos(argumentOfPerihelion), sw = std::sin(argumentOfPerihelion);
			const double cn = std::cos(node), sn = std::sin(node);
			const double ci = std::cos(inclination), si = std::sin(inclination);
			auto ToEcliptic = [&](const glm::dvec2& v)
			{
				return glm::dvec3((cw * cn - sw * sn * ci) * v.x + (-sw * cn - cw * sn * ci) * v.y,
								  (cw * sn + sw * cn * ci) * v.x + (-sw * sn + cw * cn * ci) * v.y,
								  (sw * si) * v.x + (cw * si) * v.y);
			};
			position = EclipticToWorld(ToEcliptic(planePosition)) / METRES_PER_UNIT;
			velocity = EclipticToWorld(ToEcliptic(planeVelocity));
		}
	}

	double DaysSinceJ2000Now()
	{
		const double unixSeconds = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
		return (unixSeconds - J2000_UNIX_SECONDS) / SECONDS_PER_DAY;
	}

	std::vector<SolarSystemBody> CreateSolarSystem(PlanetarySystem& system, const double daysSinceJ2000)
	{
		std::vector<SolarSystemBody> bodies;

		const sRef<PresetStars::Sun> sun = make<PresetStars::Sun>();
		sun->SetPosition(glm::dvec3(0.0));
		sun->Scale(glm::vec3((float)(SUN_RADIUS_KM / KM_PER_UNIT)));
		PointAxis(*sun, EquatorialDirectionToWorld(286.13, 63.87));
		system.Add(sun);
		bodies.push_back({ "Sun", sun, "" });

		// Earth's elements are those of the Earth-Moon barycentre
		const PlanetDescription planets[] = {
			{ "Mercury", [] { return make<Mercury>(); }, 2440.53,
			  { 0.38709927, 0.00000037, 0.20563593, 0.00001906, 7.00497902, -0.00594749,
				252.25032350, 149472.67411175, 77.45779628, 0.16047689, 48.33076593, -0.12534081 },
			  281.0103, 61.4155 },
			{ "Venus", [] { return make<Venus>(); }, 6051.8,
			  { 0.72333566, 0.00000390, 0.00677672, -0.00004107, 3.39467605, -0.00078890,
				181.97909950, 58517.81538729, 131.60246718, 0.00268329, 76.67984255, -0.27769418 },
			  272.76, 67.16 },
			{ "Earth", [] { return make<Earth>(); }, 6378.137,
			  { 1.00000261, 0.00000562, 0.01671123, -0.00004392, -0.00001531, -0.01294668,
				100.46457166, 35999.37244981, 102.93768193, 0.32327364, 0.0, 0.0 },
			  0.0, 90.0 },
			{ "Mars", [] { return make<Mars>(); }, 3396.19,
			  { 1.52371034, 0.00001847, 0.09339410, 0.00007882, 1.84969142, -0.00813131,
				-4.55343205, 19140.30268499, -23.94362959, 0.44441088, 49.55953891, -0.29257343 },
			  317.68143, 52.88650 },
			{ "Jupiter", [] { return make<Jupiter>(); }, 71492.0,
			  { 5.20288700, -0.00011607, 0.04838624, -0.00013253, 1.30439695, -0.00183714,
				34.39644051, 3034.74612775, 14.72847983, 0.21252668, 100.47390909, 0.20469106 },
			  268.056595, 64.495303 },
			{ "Saturn", [] { return make<Saturn>(); }, 60268.0,
			  { 9.53667594, -0.00125060, 0.05386179, -0.00050991, 2.48599187, 0.00193609,
				49.95424423, 1222.49362201, 92.59887831, -0.41897216, 113.66242448, -0.28867794 },
			  40.589, 83.537 },
			{ "Uranus", [] { return make<Uranus>(); }, 25559.0,
			  { 19.18916464, -0.00196176, 0.04725744, -0.00004397, 0.77263783, -0.00242939,
				313.23810451, 428.48202785, 170.95427630, 0.40805281, 74.01692503, 0.04240589 },
			  257.311, -15.175 },
			{ "Neptune", [] { return make<Neptune>(); }, 24764.0,
			  { 30.06992276, 0.00026291, 0.00859048, 0.00005105, 1.77004347, 0.00035372,
				-55.12002969, 218.45945325, 44.96476227, -0.32241464, 131.78422574, -0.00508664 },
			  299.36, 43.46 },
		};

		system.SetTime(daysSinceJ2000);
		for (const PlanetDescription& description : planets)
		{
			const sRef<Planet> planet = description.create();
			planet->Scale(glm::vec3((float)(description.equatorialRadiusKm / KM_PER_UNIT)));
			PointAxis(*planet, EquatorialDirectionToWorld(description.poleRightAscension, description.poleDeclination));

			// The elements are of the planet's system's barycentre (for Earth, the Earth-Moon
			// barycentre): the moons, added below, offset the planet from it
			std::vector<const MoonData*> moonData;
			double moonsMass = 0.0;
			for (const MoonData& data : MOON_DATA)
			{
				if (description.name == std::string(data.planet))
				{
					moonData.push_back(&data);
					moonsMass += data.massKg;
				}
			}

			glm::dvec3 position, velocity;
			const double mu = SV::Maths::Gravity::G * (SUN_MASS + planet->GetMass() + moonsMass);
			OrbitalState(description.orbit, daysSinceJ2000, mu, position, velocity);
			planet->SetPosition(position);
			planet->SetVelocity(velocity);

			system.Add(planet);
			bodies.push_back({ description.name, planet, "" });

			// The airless moons share one atmosphere (a vacuum, lit for the planet's distance)
			const double distance = description.orbit.semiMajorAxis;
			sRef<SV::Atmosphere::IAtmosphere> airless;
			for (const MoonData* data : moonData)
			{
				sRef<SV::Atmosphere::IAtmosphere> atmosphere;
				if (data->naifId == 606)
					atmosphere = make<SV::Atmosphere::TitanAtmosphere>();
				else if (data->naifId == 801)
					atmosphere = make<SV::Atmosphere::TritonAtmosphere>();
				else
				{
					if (airless == nullptr)
						airless = make<SV::Atmosphere::AirlessAtmosphere>(distance);
					atmosphere = airless;
				}
				const sRef<Moon> moon = make<Moon>(*data, atmosphere);
				system.AddMoon(moon, planet);
				bodies.push_back({ moon->GetName(), moon, description.name });
			}
		}

		// Keep the system's centre of mass still
		system.ZeroMomentum(sun);
		return bodies;
	}
}
