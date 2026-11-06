#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace SV
{
	// World units (Earth radii) per parsec (as Universe::AU_IN_UNITS, in SolarSystem.hpp, per AU)
	constexpr double PARSEC_IN_UNITS = 206264.806247 * 149597870.7 / 6371.0;

	// Sagittarius A*, the black hole at the centre of the Milky Way: its position (Reid & Brunthaler
	// 2004), distance and mass (GRAVITY Collaboration 2022)
	constexpr double SGR_A_RIGHT_ASCENSION = 266.416837; // Degrees (J2000)
	constexpr double SGR_A_DECLINATION = -29.007811;
	constexpr double SGR_A_DISTANCE = 8277.0; // Parsecs
	constexpr double SGR_A_MASS = 4.297e6;    // Solar masses

	// What a catalogue object is
	enum class CatalogueKind : uint8_t
	{
		Star,
		Pulsar,
		MillisecondPulsar,
		Magnetar,
		NeutronStar,   // Isolated (seen by its heat), not pulsing in radio
		XrayBinary,    // A neutron star feeding on a companion star
		BlackHole,     // Sagittarius A*, or a stellar black hole feeding on a companion star
		PlanetaryNebula,
		HIIRegion,
		SupernovaRemnant,
		Galaxy,
		Count
	};

	[[nodiscard]] const char* GetCatalogueKindName(CatalogueKind kind);

	// Catalogue positions are in parsecs from the Sun, along the world's axes (the ecliptic, see
	// Universe::EclipticToWorld)

	// A star of the HYG database
	struct CatalogueStar
	{
		glm::dvec3 position{ 0.0 };
		float luminosity = 1.0f;     // In the V band, in solar luminosities
		float temperature = 5772.0f; // Of its surface (K), from its colour
	};

	// A pulsar, neutron star or black hole (as a point)
	struct CompactSource
	{
		glm::dvec3 position{ 0.0 };
		CatalogueKind kind = CatalogueKind::Pulsar;
		float period = 0.0f; // Spin period (s, 0 if unknown)
		std::string name;
		std::string description;
		// Pulsars: in a supernova remnant, at a distance measured other than by its dispersion
		// (which its remnant is moved to)
		bool inRemnant = false;
		bool measuredDistance = false;
	};

	// The shapes of nebulae and galaxies drawn
	enum class SpriteShape : uint8_t
	{
		SpiralGalaxy,
		BarredSpiralGalaxy,
		EllipticalGalaxy, // Also lenticulars and dwarf spheroidals
		IrregularGalaxy,
		PlanetaryNebula,
		HIIRegion,
		ShellRemnant,     // A supernova remnant's expanding shell
		FilledRemnant,    // A pulsar wind nebula, such as the Crab
	};

	// A nebula or galaxy: drawn as a sprite (an ellipse, or for disc galaxies a disc in 3D)
	struct ExtendedObject
	{
		glm::dvec3 position{ 0.0 };
		CatalogueKind kind = CatalogueKind::Galaxy;
		SpriteShape shape = SpriteShape::SpiralGalaxy;
		float radius = 1.0f;     // Parsecs
		float axisRatio = 1.0f;  // Minor to major axis
		// The major axis and, for discs, the normal to the disc (world axes, unit)
		glm::vec3 majorAxis{ 1.0f, 0.0f, 0.0f };
		glm::vec3 normal{ 0.0f, 1.0f, 0.0f };
		float luminosity = 1.0f; // Solar luminosities (galaxies: B band)
		float temperature = 6000.0f; // Colour of its stars (galaxies)
		float typeCode = 0.0f;   // Galaxies: the de Vaucouleurs T-type
		float seed = 0.0f;       // [0, 1): varies its pattern
		std::string name;        // Empty for none (unnamed galaxies of the 2MRS)
		std::string description;
		std::string designation; // In its catalogues, such as "M31 / NGC 224"
		float distance = 0.0f;   // From the Sun (pc), for its description
	};

	// A labelled object that can be travelled to: a named star, nebula, galaxy or compact source
	struct CatalogueLandmark
	{
		std::string name;
		std::string description;
		CatalogueKind kind = CatalogueKind::Star;
		glm::dvec3 position{ 0.0 };
		float radius = 0.0f;      // Parsecs (0 for points)
		float luminosity = 0.0f;  // Stars: solar luminosities (V band)
		// Which landmarks are labelled first where they would overlap (higher first)
		float priority = 0.0f;
	};

	// Everything parsed from the catalogue files (see Tools/Catalogues/download_catalogues.py)
	struct Catalogue
	{
		std::vector<CatalogueStar> stars;
		std::vector<CompactSource> compactSources;
		std::vector<ExtendedObject> nebulae;
		std::vector<ExtendedObject> galaxies;
		std::vector<CatalogueLandmark> landmarks;
		// What was loaded (or couldn't be), one line per file
		std::vector<std::string> report;
	};

	// Directory the catalogue files are in (SV_DEFAULT_CATALOGUE_DIR unless overridden)
	void SetCatalogueDirectory(const std::filesystem::path& directory);
	[[nodiscard]] const std::filesystem::path& GetCatalogueDirectory();

	// Parses every catalogue file in the directory (missing files are skipped and reported). Slow
	// (about a second): run it on a worker thread.
	[[nodiscard]] Catalogue LoadCatalogue(const std::filesystem::path& directory);

	// The rotation from the Milky Way model's axes (Galaxy.hpp: the centre at the origin, the disc
	// in the XZ plane, the north galactic pole along +y and the Sun towards +x) to world axes
	[[nodiscard]] glm::dmat3 GetGalacticModelToWorld();
	// Where Sagittarius A* is (pc from the Sun, world axes)
	[[nodiscard]] glm::dvec3 GetGalacticCentre();
}
