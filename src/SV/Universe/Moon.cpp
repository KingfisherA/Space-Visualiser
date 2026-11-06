#include "Moon.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <vector>

#include "Coordinates.hpp"
#include "MoonData.hpp"
#include "PlanetarySystem.hpp"

namespace SV
{
	namespace
	{
		using namespace Universe;

		// Moons with maps (see Tools/Textures/make_moon_maps.py, which also gives the albedo scales: the
		// reflectance of each map's brightest point, for the moon's mean albedo)
		struct MoonMap
		{
			const char* name;
			const char* path;
			float albedoScale;
		};
		const MoonMap MOON_MAPS[] = {
			// LRO WAC global mosaic
			{ "Moon", "Solar System/Earth/Moons/Moon 1536x768.jpg", 0.283f },
			// Viking (NASA 3D Resources)
			{ "Phobos", "Solar System/Mars/Moons/Phobos 1024x512.jpg", 0.160f },
			{ "Deimos", "Solar System/Mars/Moons/Deimos 1024x512.jpg", 0.153f },
			// Galileo and Voyager
			{ "Io", "Solar System/Jupiter/Moons/Io 1440x720.jpg", 1.973f },
			{ "Europa", "Solar System/Jupiter/Moons/Europa 1536x768.jpg", 1.042f },
			{ "Ganymede", "Solar System/Jupiter/Moons/Ganymede 1536x768.jpg", 1.510f },
			{ "Callisto", "Solar System/Jupiter/Moons/Callisto 1536x768.jpg", 0.638f },
			// Cassini ISS
			{ "Mimas", "Solar System/Saturn/Moons/Mimas 1024x512.jpg", 1.236f },
			{ "Enceladus", "Solar System/Saturn/Moons/Enceladus 1024x512.jpg", 1.753f },
			{ "Tethys", "Solar System/Saturn/Moons/Tethys 1024x512.jpg", 1.498f },
			{ "Dione", "Solar System/Saturn/Moons/Dione 1024x512.jpg", 1.240f },
			{ "Rhea", "Solar System/Saturn/Moons/Rhea 1024x512.jpg", 1.157f },
			{ "Titan", "Solar System/Saturn/Moons/Titan 1536x768.jpg", 0.419f },
			{ "Iapetus", "Solar System/Saturn/Moons/Iapetus 1024x512.jpg", 0.600f },
			// Voyager 2, which saw only the southern hemispheres of the Uranian moons and Triton
			{ "Miranda", "Solar System/Uranus/Moons/Miranda 1024x512.jpg", 0.806f },
			{ "Ariel", "Solar System/Uranus/Moons/Ariel 1024x512.jpg", 0.793f },
			{ "Umbriel", "Solar System/Uranus/Moons/Umbriel 1024x512.jpg", 0.310f },
			{ "Titania", "Solar System/Uranus/Moons/Titania 1024x512.jpg", 0.832f },
			{ "Oberon", "Solar System/Uranus/Moons/Oberon 1024x512.jpg", 1.398f },
			{ "Triton", "Solar System/Neptune/Moons/Triton 1536x768.jpg", 2.104f },
		};

		// The colour of a moon without a map (most have only been seen as points of light): the
		// small inner moons of Saturn are clean ice, Amalthea is the reddest body in the solar system,
		// and the captured irregular moons are dark and grey to red
		glm::vec3 SurfaceColour(const MoonData& data)
		{
			const auto Is = [&data](const char* name) { return std::strcmp(data.name, name) == 0; };
			if (Is("Amalthea"))
				return { 1.0f, 0.6f, 0.45f };
			if (Is("Hyperion"))
				return { 1.0f, 0.86f, 0.72f };
			if (Is("Phoebe") || Is("Nereid"))
				return { 1.0f, 0.97f, 0.93f };
			const std::string planet = data.planet;
			if (!data.regular)
				return planet == "Saturn" || planet == "Jupiter" ? glm::vec3(1.0f, 0.9f, 0.8f) : glm::vec3(1.0f, 0.82f, 0.68f);
			if (planet == "Saturn")
				return { 1.0f, 0.98f, 0.95f };
			if (planet == "Jupiter")
				return { 1.0f, 0.82f, 0.68f };
			return { 1.0f, 0.97f, 0.94f };
		}

		// One 1x1 texture per surface colour, shared by the moons without maps
		sRef<AsyncTexture> ColourTexture(const glm::vec3& colour)
		{
			static std::map<std::vector<unsigned char>, std::weak_ptr<AsyncTexture>> textures;
			std::vector<unsigned char> texel(4, 255);
			for (int c = 0; c < 3; c++)
				texel[c] = (unsigned char)std::lround(255.0f * std::pow(colour[c], 1.0f / 2.2f)); // As a display map
			std::weak_ptr<AsyncTexture>& cached = textures[texel];
			sRef<AsyncTexture> texture = cached.lock();
			if (texture == nullptr)
			{
				texture = make<AsyncTexture>(DH::Dimension2D(1, 1), texel, DH::Format::RGBA8);
				cached = texture;
			}
			return texture;
		}

		// The reference plane's axes in ecliptic coordinates: x along its ascending node on the ICRF
		// equator, z along its pole
		glm::dmat3 ReferenceFrame(const double poleRightAscension, const double poleDeclination)
		{
			const double ra = poleRightAscension * DEGREES, dec = poleDeclination * DEGREES;
			const glm::dvec3 pole(std::cos(dec) * std::cos(ra), std::cos(dec) * std::sin(ra), std::sin(dec));
			const glm::dvec3 node = glm::normalize(glm::cross(glm::dvec3(0.0, 0.0, 1.0), pole));
			const glm::dvec3 y = glm::cross(pole, node);
			return glm::dmat3(EquatorialToEcliptic(node), EquatorialToEcliptic(y), EquatorialToEcliptic(pole));
		}

		// Kepler's equation, M = E - e sin E
		double EccentricAnomaly(const double meanAnomaly, const double e)
		{
			double E = meanAnomaly + e * std::sin(meanAnomaly);
			for (int i = 0; i < 30; i++)
			{
				const double step = (E - e * std::sin(E) - meanAnomaly) / (1.0 - e * std::cos(E));
				E -= step;
				if (std::abs(step) < 1e-12)
					break;
			}
			return E;
		}

		// The Moon's geocentric position (km, ecliptic J2000) from the main terms of ELP-2000/82, as
		// abridged in J. Meeus, "Astronomical Algorithms" (2nd ed.), chapter 47: within 0.01 degrees
		// of JPL's DE440 (the elements' fit is within 2.7, as the Sun perturbs the Moon strongly)
		glm::dvec3 LunarPosition(const double daysTT)
		{
			struct Term
			{
				signed char d, m, mp, f;
				int longitude; // 1e-6 degrees
				int distance;  // Metres
			};
			static const Term LONGITUDE_DISTANCE[] = {
				{ 0, 0, 1, 0, 6288774, -20905355 }, { 2, 0, -1, 0, 1274027, -3699111 }, { 2, 0, 0, 0, 658314, -2955968 },
				{ 0, 0, 2, 0, 213618, -569925 }, { 0, 1, 0, 0, -185116, 48888 }, { 0, 0, 0, 2, -114332, -3149 },
				{ 2, 0, -2, 0, 58793, 246158 }, { 2, -1, -1, 0, 57066, -152138 }, { 2, 0, 1, 0, 53322, -170733 },
				{ 2, -1, 0, 0, 45758, -204586 }, { 0, 1, -1, 0, -40923, -129620 }, { 1, 0, 0, 0, -34720, 108743 },
				{ 0, 1, 1, 0, -30383, 104755 }, { 2, 0, 0, -2, 15327, 10321 }, { 0, 0, 1, 2, -12528, 0 },
				{ 0, 0, 1, -2, 10980, 79661 }, { 4, 0, -1, 0, 10675, -34782 }, { 0, 0, 3, 0, 10034, -23210 },
				{ 4, 0, -2, 0, 8548, -21636 }, { 2, 1, -1, 0, -7888, 24208 }, { 2, 1, 0, 0, -6766, 30824 },
				{ 1, 0, -1, 0, -5163, -8379 }, { 1, 1, 0, 0, 4987, -16675 }, { 2, -1, 1, 0, 4036, -12831 },
				{ 2, 0, 2, 0, 3994, -10445 }, { 4, 0, 0, 0, 3861, -11650 }, { 2, 0, -3, 0, 3665, 14403 },
				{ 0, 1, -2, 0, -2689, -7003 }, { 2, 0, -1, 2, -2602, 0 }, { 2, -1, -2, 0, 2390, 10056 },
				{ 1, 0, 1, 0, -2348, 6322 }, { 2, -2, 0, 0, 2236, -9884 }, { 0, 1, 2, 0, -2120, 5751 },
				{ 0, 2, 0, 0, -2069, 0 }, { 2, -2, -1, 0, 2048, -4950 }, { 2, 0, 1, -2, -1773, 4130 },
				{ 2, 0, 0, 2, -1595, 0 }, { 4, -1, -1, 0, 1215, -3958 }, { 0, 0, 2, 2, -1110, 0 },
				{ 3, 0, -1, 0, -892, 3258 }, { 2, 1, 1, 0, -810, 2616 }, { 4, -1, -2, 0, 759, -1897 },
				{ 0, 2, -1, 0, -713, -2117 }, { 2, 2, -1, 0, -700, 2354 }, { 2, 1, -2, 0, 691, 0 },
				{ 2, -1, 0, -2, 596, 0 }, { 4, 0, 1, 0, 549, -1423 }, { 0, 0, 4, 0, 537, -1117 },
				{ 4, -1, 0, 0, 520, -1571 }, { 1, 0, -2, 0, -487, -1739 }, { 2, 1, 0, -2, -399, 0 },
				{ 0, 0, 2, -2, -381, -4421 }, { 1, 1, 1, 0, 351, 0 }, { 3, 0, -2, 0, -340, 0 },
				{ 4, 0, -3, 0, 330, 0 }, { 2, -1, 2, 0, 327, 0 }, { 0, 2, 1, 0, -323, 1165 },
				{ 1, 1, -1, 0, 299, 0 }, { 2, 0, 3, 0, 294, 0 }, { 2, 0, -1, -2, 0, 8752 },
			};
			struct LatitudeTerm
			{
				signed char d, m, mp, f;
				int latitude; // 1e-6 degrees
			};
			static const LatitudeTerm LATITUDE[] = {
				{ 0, 0, 0, 1, 5128122 }, { 0, 0, 1, 1, 280602 }, { 0, 0, 1, -1, 277693 }, { 2, 0, 0, -1, 173237 },
				{ 2, 0, -1, 1, 55413 }, { 2, 0, -1, -1, 46271 }, { 2, 0, 0, 1, 32573 }, { 0, 0, 2, 1, 17198 },
				{ 2, 0, 1, -1, 9266 }, { 0, 0, 2, -1, 8822 }, { 2, -1, 0, -1, 8216 }, { 2, 0, -2, -1, 4324 },
				{ 2, 0, 1, 1, 4200 }, { 2, 1, 0, -1, -3359 }, { 2, -1, -1, 1, 2463 }, { 2, -1, 0, 1, 2211 },
				{ 2, -1, -1, -1, 2065 }, { 0, 1, -1, -1, -1870 }, { 4, 0, -1, -1, 1828 }, { 0, 1, 0, 1, -1794 },
				{ 0, 0, 0, 3, -1749 }, { 0, 1, -1, 1, -1565 }, { 1, 0, 0, 1, -1491 }, { 0, 1, 1, 1, -1475 },
				{ 0, 1, 1, -1, -1410 }, { 0, 1, 0, -1, -1344 }, { 1, 0, 0, -1, -1335 }, { 0, 0, 3, 1, 1107 },
				{ 4, 0, 0, -1, 1021 }, { 4, 0, -1, 1, 833 },
			};

			const double T = daysTT / 36525.0;
			const double T2 = T * T, T3 = T2 * T, T4 = T3 * T;
			// Mean longitude, mean elongation, the Sun's mean anomaly, the Moon's mean anomaly and its
			// argument of latitude (degrees)
			const double L = 218.3164477 + 481267.88123421 * T - 0.0015786 * T2 + T3 / 538841.0 - T4 / 65194000.0;
			const double D = 297.8501921 + 445267.1114034 * T - 0.0018819 * T2 + T3 / 545868.0 - T4 / 113065000.0;
			const double M = 357.5291092 + 35999.0502909 * T - 0.0001536 * T2 + T3 / 24490000.0;
			const double Mp = 134.9633964 + 477198.8675055 * T + 0.0087414 * T2 + T3 / 69699.0 - T4 / 14712000.0;
			const double F = 93.2720950 + 483202.0175233 * T - 0.0036539 * T2 - T3 / 3526000.0 + T4 / 863310000.0;
			const double A1 = 119.75 + 131.849 * T, A2 = 53.09 + 479264.290 * T, A3 = 313.45 + 481266.484 * T;
			// The decreasing eccentricity of Earth's orbit
			const double E = 1.0 - 0.002516 * T - 0.0000074 * T2;

			const auto Sin = [](const double degrees) { return std::sin(degrees * DEGREES); };
			const auto Cos = [](const double degrees) { return std::cos(degrees * DEGREES); };
			double longitude = 0.0, distance = 0.0, latitude = 0.0;
			for (const Term& term : LONGITUDE_DISTANCE)
			{
				const double argument = term.d * D + term.m * M + term.mp * Mp + term.f * F;
				const double scale = std::pow(E, std::abs(term.m));
				longitude += term.longitude * scale * Sin(argument);
				distance += term.distance * scale * Cos(argument);
			}
			for (const LatitudeTerm& term : LATITUDE)
				latitude += term.latitude * std::pow(E, std::abs(term.m)) * Sin(term.d * D + term.m * M + term.mp * Mp + term.f * F);
			// Venus, Jupiter and the Earth's flattening
			longitude += 3958.0 * Sin(A1) + 1962.0 * Sin(L - F) + 318.0 * Sin(A2);
			latitude += -2235.0 * Sin(L) + 382.0 * Sin(A3) + 175.0 * Sin(A1 - F) + 175.0 * Sin(A1 + F) + 127.0 * Sin(L - Mp) -
						115.0 * Sin(L + Mp);

			// From the mean equinox of date to J2000 (the general precession in longitude)
			const double lambda = (L + longitude * 1e-6 - 1.3969713 * T) * DEGREES;
			const double beta = latitude * 1e-6 * DEGREES;
			const double km = 385000.56 + distance * 1e-3;
			return km * glm::dvec3(std::cos(beta) * std::cos(lambda), std::cos(beta) * std::sin(lambda), std::sin(beta));
		}

		// Position from the mean elements (km, ecliptic J2000)
		glm::dvec3 KeplerPosition(const MoonData& m, const glm::dmat3& frame, const double daysTT)
		{
			const double dt = daysTT - MOON_ELEMENT_EPOCH;
			const double w = (m.argumentOfPeriapsis + m.argumentOfPeriapsisRate * dt) * DEGREES;
			const double node = (m.ascendingNode + m.ascendingNodeRate * dt) * DEGREES;
			const double meanAnomaly = std::remainder((m.meanLongitude + m.meanLongitudeRate * dt) * DEGREES - w - node, 2.0 * glm::pi<double>());
			const double e = m.eccentricity;
			const double E = EccentricAnomaly(meanAnomaly, e);
			const double x = m.semiMajorAxisKm * (std::cos(E) - e);
			const double y = m.semiMajorAxisKm * std::sqrt(1.0 - e * e) * std::sin(E);

			const double cw = std::cos(w), sw = std::sin(w), cn = std::cos(node), sn = std::sin(node);
			const double ci = std::cos(m.inclination * DEGREES), si = std::sin(m.inclination * DEGREES);
			const glm::dvec3 inPlane((cw * cn - sw * sn * ci) * x + (-sw * cn - cw * sn * ci) * y,
									 (cw * sn + sw * cn * ci) * x + (-sw * sn + cw * cn * ci) * y,
									 (sw * si) * x + (cw * si) * y);
			return frame * inPlane;
		}
	}

	Moon::Moon(const Universe::MoonData& dataIn, const sRef<SV::Atmosphere::IAtmosphere>& atmosphere) :
		Planet(dataIn.massKg, atmosphere), data(dataIn), name(dataIn.name)
	{
		mytype = CelestrialBodyType::Moon;
		scaling = glm::vec3((float)(data.radiusKm / KM_PER_UNIT));
		referenceFrame = ReferenceFrame(data.poleRightAscension, data.poleDeclination);

		// Moons turn with their orbits (see Orient), not with the planets' animated spin
		rotationSpeed = 0.0f;
		litSurface = true;

		const MoonMap* map = nullptr;
		for (const MoonMap& candidate : MOON_MAPS)
		{
			if (name == candidate.name)
				map = &candidate;
		}
		if (map != nullptr)
		{
			diffuseMap = make<AsyncTexture>(map->path);
			surfaceAlbedo = map->albedoScale;
		}
		else
		{
			diffuseMap = ColourTexture(SurfaceColour(data));
			// A Lambertian sphere's geometric albedo is 2/3 of its reflectance; dark, rough surfaces
			// (most of these moons) are closer to their geometric albedo, as they brighten sharply
			// towards full phase
			surfaceAlbedo = (float)std::min(1.25 * data.albedo, 1.0);
		}
	}

	glm::dvec3 Moon::GetOrbitOffset(const double days) const
	{
		const double daysTT = days + TT_MINUS_UTC_DAYS;
		const glm::dvec3 km = data.naifId == 301 ? LunarPosition(daysTT) : KeplerPosition(data, referenceFrame, daysTT);
		return EclipticToWorld(km) / KM_PER_UNIT;
	}

	void Moon::Orient(const glm::dvec3& offset, const glm::dvec3& orbitPole)
	{
		// North is the pole on the north side of the ecliptic (as the IAU defines it for moons), so
		// moons on retrograde orbits (Triton, and Uranus' tipped over system) turn backwards
		const glm::dvec3 north = orbitPole.y >= 0.0 ? orbitPole : -orbitPole;
		glm::dvec3 forward;
		if (data.synchronous || !hasOrientation)
		{
			// The prime meridian (+Z in the moon's space) faces the planet
			forward = -offset - north * glm::dot(-offset, north);
			if (glm::dot(forward, forward) < 1e-30)
				return;
			forward = glm::normalize(forward);
		}
		else
		{
			// Non-synchronous moons keep their prime meridian where it was, around the moving pole
			const glm::dvec3 previous(GetRotationMatrix()[2]);
			forward = glm::normalize(previous - north * glm::dot(previous, north));
		}
		const glm::dvec3 east = glm::cross(north, forward);
		SetOrientation(glm::mat3(glm::vec3(east), glm::vec3(north), glm::vec3(forward)));
		hasOrientation = true;
	}
}
