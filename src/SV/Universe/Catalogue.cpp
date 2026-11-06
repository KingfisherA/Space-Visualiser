#include "Catalogue.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <numbers>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "SV/Universe/Coordinates.hpp"

namespace SV
{
	namespace
	{
		constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
		constexpr double ARCSEC = 1.0 / 206264.806247; // Radians
		constexpr double ARCMIN = 60.0 * ARCSEC;

		// Absolute magnitudes of the Sun (V and B bands, Willmer 2018) and its luminosity in the 2MASS
		// Ks band (absolute magnitude 3.27)
		constexpr double SUN_ABSOLUTE_V = 4.83;
		constexpr double SUN_ABSOLUTE_B = 5.44;
		constexpr double SUN_ABSOLUTE_K = 3.27;
		// Galaxies' light in B relative to Ks (both in solar units), from their typical B - Ks colours:
		// about 4.1 for ellipticals and 3.4 for spirals
		constexpr double ELLIPTICAL_B_PER_K = 0.12;
		constexpr double SPIRAL_B_PER_K = 0.22;

		// The Hubble constant (km/s/Mpc), for the distances of the 2MRS galaxies from their redshifts.
		// Its nearer galaxies (whose own motions swamp their redshifts) come from the UNGC instead,
		// which has distances measured for them.
		constexpr double HUBBLE_CONSTANT = 70.0;
		constexpr double MIN_REDSHIFT_DISTANCE = 11.0e6; // Parsecs: the UNGC's reach
		// The 2MRS sizes are at the 20 mag/arcsec^2 isophote in Ks, inside galaxies' optical (B band
		// 25 mag/arcsec^2) edges by about this factor
		constexpr double KS_ISOPHOTE_TO_OPTICAL = 1.6;
		// Two galaxies within this angle (radians) of each other in the UNGC and the 2MRS are taken to
		// be the same one
		constexpr double SAME_GALAXY_ANGLE = 1.5 * ARCMIN;

		// The intrinsic thickness (axis ratio seen edge-on) of disc galaxies, for their inclinations
		// from their axis ratios
		constexpr double DISC_THICKNESS = 0.2;

		// Nebulae aren't measured in the catalogues' light, so each kind has a typical surface
		// brightness (solar luminosities per square parsec) from which its luminosity follows
		constexpr float PLANETARY_NEBULA_BRIGHTNESS = 2000.0f;
		constexpr float HII_REGION_BRIGHTNESS = 400.0f;
		constexpr float SUPERNOVA_REMNANT_BRIGHTNESS = 12.0f;
		constexpr float MIN_NEBULA_RADIUS = 0.02f; // Parsecs

		// The Sun's height above the Milky Way's mid-plane (Bennett & Bovy 2019), and the Milky Way's
		// luminosity in B (solar luminosities)
		constexpr double SUN_HEIGHT = 20.8;
		constexpr double MILKY_WAY_LUMINOSITY = 2.5e10;

		// ICRS to galactic coordinates (Hipparcos, ESA 1997)
		constexpr double ICRS_TO_GALACTIC[3][3] = {
			{ -0.0548755604162154, -0.8734370902348850, -0.4838350155487132 },
			{ 0.4941094278755837, -0.4448296299600112, 0.7469822444972189 },
			{ -0.8676661490190047, -0.1980763734312015, 0.4559837761750669 } };

		glm::dvec3 EquatorialToWorld(const glm::dvec3& v)
		{
			return Universe::EclipticToWorld(Universe::EquatorialToEcliptic(v));
		}

		glm::dvec3 GalacticDirectionToWorld(const glm::dvec3& galactic)
		{
			glm::dvec3 equatorial(0.0);
			for (int i = 0; i < 3; i++)
				for (int j = 0; j < 3; j++)
					equatorial[i] += ICRS_TO_GALACTIC[j][i] * galactic[j];
			return EquatorialToWorld(equatorial);
		}

		// ---- Text --------------------------------------------------------------------------------

		std::string_view Trim(std::string_view text)
		{
			while (!text.empty() && std::isspace((unsigned char)text.front()))
				text.remove_prefix(1);
			while (!text.empty() && std::isspace((unsigned char)text.back()))
				text.remove_suffix(1);
			return text;
		}

		double ParseNumber(std::string_view text)
		{
			text = Trim(text);
			if (text.empty())
				return NaN;
			const std::string copy(text);
			char* end = nullptr;
			const double value = std::strtod(copy.c_str(), &end);
			return end == copy.c_str() ? NaN : value;
		}

		bool IsNumber(const double value)
		{
			return !std::isnan(value);
		}

		// Sexagesimal ("hh:mm:ss.s" or "dd mm ss"), in hours or degrees
		double ParseSexagesimal(std::string_view text)
		{
			text = Trim(text);
			if (text.empty())
				return NaN;
			const bool negative = text.front() == '-';
			if (text.front() == '-' || text.front() == '+')
				text.remove_prefix(1);
			double value = 0.0, scale = 1.0;
			size_t start = 0;
			for (int part = 0; part < 3 && start <= text.size(); part++)
			{
				size_t end = text.find_first_of(": ", start);
				if (end == std::string_view::npos)
					end = text.size();
				const double number = ParseNumber(text.substr(start, end - start));
				if (IsNumber(number))
					value += number * scale;
				scale /= 60.0;
				start = end + 1;
			}
			return negative ? -value : value;
		}

		std::vector<std::string_view> Split(std::string_view line, const char separator)
		{
			std::vector<std::string_view> fields;
			size_t start = 0;
			while (true)
			{
				const size_t end = line.find(separator, start);
				fields.push_back(line.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
				if (end == std::string_view::npos)
					break;
				start = end + 1;
			}
			return fields;
		}

		// A CSV line with quoted fields (as in the HYG database)
		std::vector<std::string> SplitQuoted(std::string_view line)
		{
			std::vector<std::string> fields(1);
			bool quoted = false;
			for (const char c : line)
			{
				if (c == '"')
					quoted = !quoted;
				else if (c == ',' && !quoted)
					fields.emplace_back();
				else if (c != '\r')
					fields.back() += c;
			}
			return fields;
		}

		std::optional<std::string> ReadFile(const std::filesystem::path& path)
		{
			std::ifstream file(path, std::ios::binary);
			if (!file)
				return std::nullopt;
			std::ostringstream text;
			text << file.rdbuf();
			return text.str();
		}

		// Calls `line` for each line of the text
		template <typename Function>
		void ForEachLine(const std::string& text, Function line)
		{
			size_t start = 0;
			while (start < text.size())
			{
				size_t end = text.find('\n', start);
				if (end == std::string::npos)
					end = text.size();
				std::string_view view(text.data() + start, end - start);
				if (!view.empty() && view.back() == '\r')
					view.remove_suffix(1);
				line(view);
				start = end + 1;
			}
		}

		// A VizieR tab-separated table: comments (#), the column names, their units, a line of dashes,
		// then the rows
		struct Table
		{
			std::unordered_map<std::string, size_t> columns;
			std::vector<std::vector<std::string_view>> rows;
			std::string text; // The rows view into this

			[[nodiscard]] std::string_view Get(const std::vector<std::string_view>& row, const std::string& column) const
			{
				const auto found = columns.find(column);
				return found != columns.end() && found->second < row.size() ? Trim(row[found->second]) : std::string_view();
			}
			[[nodiscard]] double Number(const std::vector<std::string_view>& row, const std::string& column) const
			{
				return ParseNumber(Get(row, column));
			}
		};

		std::optional<Table> ReadTable(const std::filesystem::path& path)
		{
			std::optional<std::string> text = ReadFile(path);
			if (!text)
				return std::nullopt;
			Table table;
			table.text = std::move(*text);
			int stage = 0; // 0: before the names, 1: units, 2: dashes, 3: rows
			ForEachLine(table.text, [&](std::string_view line)
				{
					if (line.empty() || line.front() == '#')
					{
						if (stage == 3 && line.empty())
							stage = 4; // The end of the first table
						return;
					}
					if (stage == 0)
					{
						const std::vector<std::string_view> names = Split(line, '\t');
						for (size_t i = 0; i < names.size(); i++)
							table.columns[std::string(Trim(names[i]))] = i;
						stage = 1;
					}
					else if (stage == 1 || stage == 2)
					{
						stage++;
					}
					else if (stage == 3)
					{
						table.rows.push_back(Split(line, '\t'));
					}
				});
			return table;
		}

		// "NGC 0224", "NGC_0224" and "NGC224" alike: upper case, without spaces, underscores or the
		// leading zeros of the number ("MESSIER031" becomes "M31")
		std::string NormaliseId(std::string_view id)
		{
			std::string text;
			for (const char c : Trim(id))
			{
				if (c != ' ' && c != '_')
					text += (char)std::toupper((unsigned char)c);
			}
			if (text.rfind("MESSIER", 0) == 0)
				text = "M" + text.substr(7);
			// Drop leading zeros of the first number after the letters
			size_t letters = 0;
			while (letters < text.size() && std::isalpha((unsigned char)text[letters]))
				letters++;
			size_t zeros = letters;
			while (zeros + 1 < text.size() && text[zeros] == '0' && std::isdigit((unsigned char)text[zeros + 1]))
				zeros++;
			return text.substr(0, letters) + text.substr(zeros);
		}

		// A catalogue designation for display: "NGC 224", "M31", "ESO 349-031"
		std::string PrettyId(std::string_view id)
		{
			const std::string text = NormaliseId(id);
			size_t letters = 0;
			while (letters < text.size() && std::isalpha((unsigned char)text[letters]))
				letters++;
			if (letters <= 1 || letters >= text.size() || !std::isdigit((unsigned char)text[letters]))
				return text;
			return text.substr(0, letters) + " " + text.substr(letters);
		}

		std::string FormatDistance(const double parsecs)
		{
			char text[64];
			if (parsecs < 1000.0)
				std::snprintf(text, sizeof(text), "%.3g pc (%.3g light years)", parsecs, parsecs * 3.26156);
			else if (parsecs < 1.0e6)
				std::snprintf(text, sizeof(text), "%.3g kpc (%.3g thousand light years)", parsecs / 1e3, parsecs * 3.26156e-3);
			else
				std::snprintf(text, sizeof(text), "%.3g Mpc (%.3g million light years)", parsecs / 1e6, parsecs * 3.26156e-6);
			return text;
		}

		// A stable pseudo-random number in [0, 1) for a name
		float Seed(std::string_view text)
		{
			uint32_t hash = 2166136261u;
			for (const char c : text)
				hash = (hash ^ (uint8_t)c) * 16777619u;
			hash ^= hash >> 15;
			hash *= 0x2c1b3c6du;
			hash ^= hash >> 12;
			return (float)(hash & 0xffffff) / 16777216.0f;
		}

		// A star's surface temperature from its B - V colour (Ballesteros 2012)
		float TemperatureFromColour(const double bv)
		{
			const double c = std::clamp(bv, -0.4, 2.0);
			return (float)(4600.0 * (1.0 / (0.92 * c + 1.7) + 1.0 / (0.92 * c + 0.62)));
		}

		// Or from its spectral class, without a colour
		float TemperatureFromSpectrum(std::string_view spectrum)
		{
			switch (spectrum.empty() ? 'G' : std::toupper((unsigned char)spectrum.front()))
			{
			case 'O': return 35000.0f;
			case 'B': return 18000.0f;
			case 'A': return 9000.0f;
			case 'F': return 6800.0f;
			case 'G': return 5600.0f;
			case 'K': return 4500.0f;
			case 'M': return 3300.0f;
			case 'D': return 15000.0f; // White dwarfs
			default: return 5600.0f;
			}
		}

		// ---- OpenNGC: names and position angles -----------------------------------------------

		struct NgcObject
		{
			std::string displayName; // Its common name, Messier number or NGC/IC designation
			std::string designation; // "M31 / NGC 224"
			std::string type;
			std::string hubble;
			glm::dvec3 direction{ 0.0 }; // World axes
			double majorAxis = 0.0;      // Arcminutes
			double minorAxis = 0.0;
			double positionAngle = NaN;  // Degrees
		};

		bool IsNebulaType(const std::string& type)
		{
			return type == "PN" || type == "HII" || type == "EmN" || type == "Neb" || type == "RfN" || type == "SNR" || type == "Cl+N";
		}

		struct NgcIndex
		{
			std::vector<NgcObject> objects;
			std::unordered_map<std::string, size_t> byId; // Normalised designations
			std::vector<size_t> nebulae;
			std::vector<size_t> galaxies;

			[[nodiscard]] const NgcObject* Find(std::string_view id) const
			{
				const auto found = byId.find(NormaliseId(id));
				return found != byId.end() ? &objects[found->second] : nullptr;
			}

			// The nearest of `candidates` to a direction within `radius` (radians), if any
			[[nodiscard]] const NgcObject* Nearest(const std::vector<size_t>& candidates, const glm::dvec3& direction, const double radius) const
			{
				const NgcObject* best = nullptr;
				double bestAngle = std::numeric_limits<double>::max();
				for (const size_t i : candidates)
				{
					const NgcObject& object = objects[i];
					const double limit = radius;
					const double cosine = glm::dot(object.direction, direction);
					if (cosine < std::cos(limit))
						continue;
					const double angle = std::acos(std::min(cosine, 1.0));
					if (angle < bestAngle)
					{
						bestAngle = angle;
						best = &object;
					}
				}
				return best;
			}
		};

		NgcIndex LoadOpenNgc(const std::filesystem::path& path, std::vector<std::string>& report)
		{
			NgcIndex index;
			const std::optional<std::string> text = ReadFile(path);
			if (!text)
			{
				report.push_back("NGC.csv: not found (no common names)");
				return index;
			}
			std::unordered_map<std::string, size_t> columns;
			bool header = true;
			ForEachLine(*text, [&](std::string_view line)
				{
					const std::vector<std::string_view> fields = Split(line, ';');
					if (header)
					{
						for (size_t i = 0; i < fields.size(); i++)
							columns[std::string(Trim(fields[i]))] = i;
						header = false;
						return;
					}
					auto Get = [&](const char* column) -> std::string_view
					{
						const auto found = columns.find(column);
						return found != columns.end() && found->second < fields.size() ? Trim(fields[found->second]) : std::string_view();
					};
					const std::string type(Get("Type"));
					if (type.empty() || type == "Dup" || type == "NonEx")
						return;
					const double ra = ParseSexagesimal(Get("RA")) * 15.0, dec = ParseSexagesimal(Get("Dec"));
					if (!IsNumber(ra) || !IsNumber(dec))
						return;

					NgcObject object;
					object.type = type;
					object.hubble = std::string(Get("Hubble"));
					object.direction = Universe::EquatorialDirectionToWorld(ra, dec);
					const double major = ParseNumber(Get("MajAx"));
					object.majorAxis = IsNumber(major) ? major : 0.0;
					const double minorSize = ParseNumber(Get("MinAx"));
					object.minorAxis = IsNumber(minorSize) ? minorSize : object.majorAxis;
					object.positionAngle = ParseNumber(Get("PosAng"));
					const std::string name = PrettyId(Get("Name"));
					const std::string_view messier = Get("M");
					const std::string messierName = messier.empty() ? std::string() : "M" + std::to_string((int)ParseNumber(messier));
					object.designation = messierName.empty() ? name : messierName + " / " + name;
					// Its first common name ("Orion Nebula" rather than "Great Orion Nebula")
					const std::vector<std::string_view> commonNames = Split(Get("Common names"), ',');
					std::string_view common = Trim(commonNames.front());
					if (common.rfind("Great ", 0) == 0 && commonNames.size() > 1)
						common = Trim(commonNames[1]);
					object.displayName = !common.empty() ? std::string(common) : !messierName.empty() ? messierName : name;

					const size_t i = index.objects.size();
					index.objects.push_back(std::move(object));
					index.byId.emplace(NormaliseId(Get("Name")), i);
					if (!messierName.empty())
						index.byId.emplace(NormaliseId(messierName), i);
					for (const std::string_view id : Split(Get("Identifiers"), ','))
					{
						if (!Trim(id).empty())
							index.byId.emplace(NormaliseId(id), i);
					}
					if (IsNebulaType(type))
						index.nebulae.push_back(i);
					else if (type == "G")
						index.galaxies.push_back(i);
				});
			report.push_back("NGC.csv: " + std::to_string(index.objects.size()) + " NGC and IC objects (for names)");
			return index;
		}

		// ---- Stars ------------------------------------------------------------------------------

		// Stars named as landmarks beyond those with proper names: every star with a Bayer or
		// Flamsteed designation, every star the eye can see (to this magnitude) and the nearby stars
		// of the Gliese catalogue (within this many parsecs)
		constexpr double NAKED_EYE_MAGNITUDE = 6.5;
		constexpr double NEARBY_STAR_DISTANCE = 20.0;

		// A Bayer letter as HYG abbreviates it ("Alp", "Pi-1") spelled out ("Alpha", "Pi1")
		std::string BayerLetter(const std::string_view abbreviation)
		{
			static constexpr std::pair<std::string_view, std::string_view> LETTERS[] = {
				{ "Alp", "Alpha" }, { "Bet", "Beta" }, { "Gam", "Gamma" }, { "Del", "Delta" }, { "Eps", "Epsilon" }, { "Zet", "Zeta" },
				{ "Eta", "Eta" }, { "The", "Theta" }, { "Iot", "Iota" }, { "Kap", "Kappa" }, { "Lam", "Lambda" }, { "Mu", "Mu" },
				{ "Nu", "Nu" }, { "Xi", "Xi" }, { "Omi", "Omicron" }, { "Pi", "Pi" }, { "Rho", "Rho" }, { "Sig", "Sigma" },
				{ "Tau", "Tau" }, { "Ups", "Upsilon" }, { "Phi", "Phi" }, { "Chi", "Chi" }, { "Psi", "Psi" }, { "Ome", "Omega" } };
			const size_t dash = abbreviation.find('-');
			const std::string_view letter = abbreviation.substr(0, dash);
			std::string name(letter);
			for (const auto& [abbreviated, full] : LETTERS)
			{
				if (abbreviated == letter)
					name = full;
			}
			if (dash != std::string_view::npos)
				name += abbreviation.substr(dash + 1);
			return name;
		}

		// The genitive of a constellation's IAU abbreviation ("Ori" -> "Orionis"), as in a star's
		// Bayer or Flamsteed designation
		std::string_view ConstellationGenitive(const std::string_view abbreviation)
		{
			static constexpr std::pair<std::string_view, std::string_view> GENITIVES[] = {
				{ "And", "Andromedae" }, { "Ant", "Antliae" }, { "Aps", "Apodis" }, { "Aqr", "Aquarii" }, { "Aql", "Aquilae" },
				{ "Ara", "Arae" }, { "Ari", "Arietis" }, { "Aur", "Aurigae" }, { "Boo", "Bootis" }, { "Cae", "Caeli" },
				{ "Cam", "Camelopardalis" }, { "Cnc", "Cancri" }, { "CVn", "Canum Venaticorum" }, { "CMa", "Canis Majoris" },
				{ "CMi", "Canis Minoris" }, { "Cap", "Capricorni" }, { "Car", "Carinae" }, { "Cas", "Cassiopeiae" },
				{ "Cen", "Centauri" }, { "Cep", "Cephei" }, { "Cet", "Ceti" }, { "Cha", "Chamaeleontis" }, { "Cir", "Circini" },
				{ "Col", "Columbae" }, { "Com", "Comae Berenices" }, { "CrA", "Coronae Australis" }, { "CrB", "Coronae Borealis" },
				{ "Crv", "Corvi" }, { "Crt", "Crateris" }, { "Cru", "Crucis" }, { "Cyg", "Cygni" }, { "Del", "Delphini" },
				{ "Dor", "Doradus" }, { "Dra", "Draconis" }, { "Equ", "Equulei" }, { "Eri", "Eridani" }, { "For", "Fornacis" },
				{ "Gem", "Geminorum" }, { "Gru", "Gruis" }, { "Her", "Herculis" }, { "Hor", "Horologii" }, { "Hya", "Hydrae" },
				{ "Hyi", "Hydri" }, { "Ind", "Indi" }, { "Lac", "Lacertae" }, { "Leo", "Leonis" }, { "LMi", "Leonis Minoris" },
				{ "Lep", "Leporis" }, { "Lib", "Librae" }, { "Lup", "Lupi" }, { "Lyn", "Lyncis" }, { "Lyr", "Lyrae" },
				{ "Men", "Mensae" }, { "Mic", "Microscopii" }, { "Mon", "Monocerotis" }, { "Mus", "Muscae" }, { "Nor", "Normae" },
				{ "Oct", "Octantis" }, { "Oph", "Ophiuchi" }, { "Ori", "Orionis" }, { "Pav", "Pavonis" }, { "Peg", "Pegasi" },
				{ "Per", "Persei" }, { "Phe", "Phoenicis" }, { "Pic", "Pictoris" }, { "Psc", "Piscium" }, { "PsA", "Piscis Austrini" },
				{ "Pup", "Puppis" }, { "Pyx", "Pyxidis" }, { "Ret", "Reticuli" }, { "Sge", "Sagittae" }, { "Sgr", "Sagittarii" },
				{ "Sco", "Scorpii" }, { "Scl", "Sculptoris" }, { "Sct", "Scuti" }, { "Ser", "Serpentis" }, { "Sex", "Sextantis" },
				{ "Tau", "Tauri" }, { "Tel", "Telescopii" }, { "Tri", "Trianguli" }, { "TrA", "Trianguli Australis" },
				{ "Tuc", "Tucanae" }, { "UMa", "Ursae Majoris" }, { "UMi", "Ursae Minoris" }, { "Vel", "Velorum" },
				{ "Vir", "Virginis" }, { "Vol", "Volantis" }, { "Vul", "Vulpeculae" } };
			for (const auto& [abbreviated, genitive] : GENITIVES)
			{
				if (abbreviated == abbreviation)
					return genitive;
			}
			return abbreviation;
		}

		// A star's catalogue designations, best known first: its Bayer designation ("Alpha
		// Orionis"), Flamsteed's ("58 Orionis"), Gliese's ("GJ 551"), then its Henry Draper and
		// Hipparcos numbers
		std::vector<std::string> StarDesignations(const std::string_view bayer, const std::string_view flamsteed, const std::string_view constellation,
												  const std::string_view gliese, const std::string_view hd, const std::string_view hip)
		{
			std::vector<std::string> names;
			const std::string genitive(ConstellationGenitive(constellation));
			if (!bayer.empty() && !constellation.empty())
				names.push_back(BayerLetter(bayer) + " " + genitive);
			if (!flamsteed.empty() && !constellation.empty())
				names.push_back(std::string(flamsteed) + " " + genitive);
			if (!gliese.empty())
				names.emplace_back(gliese);
			if (!hd.empty())
				names.push_back("HD " + std::string(hd));
			if (!hip.empty())
				names.push_back("HIP " + std::string(hip));
			return names;
		}

		void LoadStars(const std::filesystem::path& path, Catalogue& catalogue)
		{
			const std::optional<std::string> text = ReadFile(path);
			if (!text)
			{
				catalogue.report.push_back("hygdata_v41.csv: not found");
				return;
			}
			std::unordered_map<std::string, size_t> columns;
			bool header = true;
			ForEachLine(*text, [&](std::string_view line)
				{
					if (line.empty())
						return;
					const std::vector<std::string> fields = SplitQuoted(line);
					if (header)
					{
						for (size_t i = 0; i < fields.size(); i++)
							columns[fields[i]] = i;
						header = false;
						return;
					}
					auto Get = [&](const char* column) -> std::string_view
					{
						const auto found = columns.find(column);
						return found != columns.end() && found->second < fields.size() ? std::string_view(fields[found->second]) : std::string_view();
					};
					// Without the Sun (drawn by the solar system), or stars of unknown distance
					const double distance = ParseNumber(Get("dist"));
					if (!IsNumber(distance) || distance <= 0.0 || distance >= 100000.0)
						return;
					const double absoluteMagnitude = ParseNumber(Get("absmag"));
					if (!IsNumber(absoluteMagnitude))
						return;

					CatalogueStar star;
					star.position = EquatorialToWorld({ ParseNumber(Get("x")), ParseNumber(Get("y")), ParseNumber(Get("z")) });
					star.luminosity = (float)std::pow(10.0, -0.4 * (absoluteMagnitude - SUN_ABSOLUTE_V));
					const double colour = ParseNumber(Get("ci"));
					const std::string_view spectrum = Trim(Get("spect"));
					star.temperature = IsNumber(colour) ? TemperatureFromColour(colour) : TemperatureFromSpectrum(spectrum);
					catalogue.stars.push_back(star);

					// Landmarks: the stars with proper names, then those with designations people know them by
					const std::string_view proper = Trim(Get("proper"));
					const std::string_view bayer = Trim(Get("bayer")), flamsteed = Trim(Get("flam"));
					const std::string_view gliese = Trim(Get("gl"));
					const double magnitude = ParseNumber(Get("mag"));
					const bool nakedEye = IsNumber(magnitude) && magnitude < NAKED_EYE_MAGNITUDE;
					const bool nearby = !gliese.empty() && distance < NEARBY_STAR_DISTANCE;
					if (!proper.empty() || !bayer.empty() || !flamsteed.empty() || nakedEye || nearby)
					{
						std::vector<std::string> designations = StarDesignations(bayer, flamsteed, Trim(Get("con")), gliese, Trim(Get("hd")), Trim(Get("hip")));
						if (designations.empty())
							return;
						const std::string name = !proper.empty() ? std::string(proper) : designations.front();
						std::string also;
						for (const std::string& designation : designations)
						{
							if (designation != name)
								also += (also.empty() ? "" : ", ") + designation;
						}
						char description[320];
						std::snprintf(description, sizeof(description), "Star%s%s, %s\nApparent magnitude %.2f, %.3g times the Sun's luminosity%s%s",
									  spectrum.empty() ? "" : " (", spectrum.empty() ? "" : (std::string(spectrum) + ")").c_str(),
									  FormatDistance(distance).c_str(), magnitude, star.luminosity, also.empty() ? "" : "\nAlso ", also.c_str());
						// Those with proper names are labelled first
						const float priority = (float)std::log10(std::max(star.luminosity, 1e-4f)) - (proper.empty() ? 2.0f : 0.0f);
						catalogue.landmarks.push_back({ name, description, CatalogueKind::Star, star.position, 0.0f, star.luminosity, priority });
					}
				});
			catalogue.report.push_back("hygdata_v41.csv: " + std::to_string(catalogue.stars.size()) + " stars (HYG 4.1)");
		}

		// ---- Pulsars, neutron stars and black holes ---------------------------------------------

		void LoadPulsars(const std::filesystem::path& path, Catalogue& catalogue)
		{
			const std::optional<std::string> text = ReadFile(path);
			if (!text)
			{
				catalogue.report.push_back("psrcat.db: not found");
				return;
			}
			std::unordered_map<std::string, std::string> fields;
			size_t count = 0;
			// Each pulsar's lines, up to a line of "@"
			auto Flush = [&]()
			{
				if (fields.empty())
					return;
				auto Get = [&](const char* key) -> std::string
				{
					const auto found = fields.find(key);
					return found != fields.end() ? found->second : std::string();
				};
				double distance = ParseNumber(Get("DIST_A"));
				const bool measured = IsNumber(distance) && distance > 0.0;
				if (!measured)
					distance = ParseNumber(Get("DIST_DM"));
				glm::dvec3 direction(0.0);
				const double ra = ParseSexagesimal(Get("RAJ")), dec = ParseSexagesimal(Get("DECJ"));
				if (IsNumber(ra) && IsNumber(dec))
				{
					direction = Universe::EquatorialDirectionToWorld(ra * 15.0, dec);
				}
				else
				{
					const double longitude = ParseNumber(Get("ELONG")) * Universe::DEGREES, latitude = ParseNumber(Get("ELAT")) * Universe::DEGREES;
					if (IsNumber(longitude) && IsNumber(latitude))
						direction = Universe::EclipticToWorld({ std::cos(latitude) * std::cos(longitude), std::cos(latitude) * std::sin(longitude), std::sin(latitude) });
				}
				const std::string type = Get("TYPE");
				const std::string association = Get("ASSOC");
				const std::string jName = Get("PSRJ"), bName = Get("PSRB");
				double period = ParseNumber(Get("P0"));
				const double frequency = ParseNumber(Get("F0"));
				if (!IsNumber(period) && IsNumber(frequency) && frequency > 0.0)
					period = 1.0 / frequency;
				const bool binary = !Get("BINARY").empty() || !Get("PB").empty();
				fields.clear();
				if (!IsNumber(distance) || distance <= 0.0 || glm::length(direction) < 0.5)
					return;

				CompactSource source;
				source.position = direction * distance * 1000.0;
				source.period = IsNumber(period) ? (float)period : 0.0f;
				if (type.find("AXP") != std::string::npos)
					source.kind = CatalogueKind::Magnetar;
				else if (type.find("XINS") != std::string::npos)
					source.kind = CatalogueKind::NeutronStar;
				else if (IsNumber(period) && period < 0.03)
					source.kind = CatalogueKind::MillisecondPulsar;
				else
					source.kind = CatalogueKind::Pulsar;
				source.name = "PSR " + (bName.empty() ? jName : bName);

				// Its associations: "SNR:Crab[ref],GC:47Tuc[ref]"
				std::string where;
				std::string remnant;
				for (std::string_view entry : Split(association, ','))
				{
					entry = Trim(entry.substr(0, entry.find('[')));
					const size_t colon = entry.find(':');
					if (colon == std::string_view::npos)
						continue;
					const std::string_view kind = entry.substr(0, colon), name = entry.substr(colon + 1);
					if (kind == "SNR" && where.empty())
					{
						where = "\nIn the supernova remnant " + std::string(name);
						if (!name.empty() && std::isalpha((unsigned char)name.front()))
							remnant = std::string(name);
					}
					else if (kind == "GC" && where.empty())
					{
						where = "\nIn the globular cluster " + std::string(name);
					}
					else if (kind == "LMC" || kind == "SMC")
					{
						where = kind == "LMC" ? "\nIn the Large Magellanic Cloud" : "\nIn the Small Magellanic Cloud";
					}
				}
				char description[256];
				std::snprintf(description, sizeof(description), "%s%s, %s%s%s", GetCatalogueKindName(source.kind), binary ? " (in a binary)" : "",
							  FormatDistance(distance * 1000.0).c_str(),
							  source.period > 0.0f ? ("\nSpins every " + std::string(source.period < 1.0f ? std::to_string((int)std::round(source.period * 1000.0f)) + " ms" : std::to_string(source.period).substr(0, 5) + " s")).c_str() : "",
							  where.c_str());
				source.description = description;
				source.inRemnant = where.find("supernova") != std::string::npos;
				source.measuredDistance = measured;
				if (!remnant.empty())
					source.name += " (" + remnant + ")";
				catalogue.landmarks.push_back({ source.name, source.description, source.kind, source.position, 0.0f, 0.0f,
												!bName.empty() || !remnant.empty() ? -1.0f : -3.0f });
				catalogue.compactSources.push_back(std::move(source));
				count++;
			};

			ForEachLine(*text, [&](std::string_view line)
				{
					if (line.empty() || line.front() == '#')
						return;
					if (line.front() == '@')
					{
						Flush();
						return;
					}
					const size_t keyEnd = line.find_first_of(" \t");
					if (keyEnd == std::string_view::npos)
						return;
					const std::string key(line.substr(0, keyEnd));
					std::string_view rest = Trim(line.substr(keyEnd));
					const size_t valueEnd = rest.find_first_of(" \t");
					// The first value of each key (later ones are other measurements)
					fields.emplace(key, std::string(rest.substr(0, valueEnd)));
				});
			Flush();
			catalogue.report.push_back("psrcat.db: " + std::to_string(count) + " pulsars, magnetars and neutron stars (ATNF)");
		}

		void LoadXrayBinaries(const std::filesystem::path& directory, Catalogue& catalogue)
		{
			size_t count = 0, blackHoles = 0;
			auto Add = [&](const glm::dvec3& direction, const double distance, std::string name, const bool blackHole, const double period,
						   const std::string& type)
			{
				if (!IsNumber(distance) || distance <= 0.0 || name.empty())
					return;
				CompactSource source;
				source.position = direction * distance;
				source.kind = blackHole ? CatalogueKind::BlackHole : CatalogueKind::XrayBinary;
				source.period = IsNumber(period) ? (float)period : 0.0f;
				source.name = std::move(name);
				source.description = std::string(blackHole ? "Stellar black hole feeding on a companion star (" : "Neutron star feeding on a companion star (") +
									 type + "), " + FormatDistance(distance);
				catalogue.landmarks.push_back({ source.name, source.description, source.kind, source.position, 0.0f, 0.0f, blackHole ? -1.5f : -2.5f });
				catalogue.compactSources.push_back(std::move(source));
				count++;
				blackHoles += blackHole ? 1 : 0;
			};

			if (const std::optional<Table> table = ReadTable(directory / "hmxb.tsv"))
			{
				for (const auto& row : table->rows)
				{
					const double mass = table->Number(row, "Mx");
					const std::string type = "high-mass X-ray binary";
					Add(Universe::EquatorialDirectionToWorld(table->Number(row, "_RAJ2000"), table->Number(row, "_DEJ2000")), table->Number(row, "Dist"),
						std::string(table->Get(row, "ID")), IsNumber(mass) && mass > 3.0, table->Number(row, "PerSpin"), type);
				}
			}
			else
			{
				catalogue.report.push_back("hmxb.tsv: not found");
			}
			if (const std::optional<Table> table = ReadTable(directory / "lmxb.tsv"))
			{
				for (const auto& row : table->rows)
				{
					const std::string_view types = table->Get(row, "XrayType");
					const bool blackHole = types.find("BH") != std::string_view::npos;
					Add(Universe::EquatorialDirectionToWorld(table->Number(row, "_RAJ2000"), table->Number(row, "_DEJ2000")), table->Number(row, "MeanDist"),
						std::string(table->Get(row, "Name")), blackHole, table->Number(row, "Ppulse"), "low-mass X-ray binary");
				}
			}
			else
			{
				catalogue.report.push_back("lmxb.tsv: not found");
			}
			catalogue.report.push_back("hmxb.tsv, lmxb.tsv: " + std::to_string(count) + " X-ray binaries (" + std::to_string(blackHoles) + " with black holes)");
		}

		// ---- Nebulae ----------------------------------------------------------------------------

		// A nebula as it is parsed, before it is named
		struct NebulaEntry
		{
			ExtendedObject nebula;
			std::string designation; // In its catalogue
			std::string region;      // H II regions: the region its source is part of
			const NgcObject* known = nullptr; // The OpenNGC object it was matched to
			double matchScore = 0.0;
		};

		std::optional<ExtendedObject> MakeNebula(const glm::dvec3& direction, const double distance, double radius, const CatalogueKind kind,
												 const SpriteShape shape, const std::string& designation, const double axisRatio)
		{
			if (!IsNumber(distance) || distance <= 0.0 || !IsNumber(radius))
				return std::nullopt;
			radius = std::max(radius, (double)MIN_NEBULA_RADIUS);
			ExtendedObject nebula;
			nebula.position = direction * distance;
			nebula.kind = kind;
			nebula.shape = shape;
			nebula.radius = (float)radius;
			nebula.axisRatio = (float)std::clamp(IsNumber(axisRatio) ? axisRatio : 1.0, 0.3, 1.0);
			nebula.seed = Seed(designation);
			// Turned about the line of sight at random
			const glm::dvec3 north = Universe::EquatorialDirectionToWorld(0.0, 90.0);
			const glm::dvec3 up = glm::normalize(north - glm::dot(north, direction) * direction);
			const glm::dvec3 east = glm::cross(up, direction);
			const double angle = nebula.seed * 2.0 * std::numbers::pi;
			nebula.majorAxis = glm::vec3(std::cos(angle) * up + std::sin(angle) * east);
			nebula.normal = glm::vec3(-direction);
			nebula.distance = (float)distance;
			nebula.name = designation;
			return nebula;
		}

		float NebulaSurfaceBrightness(const CatalogueKind kind)
		{
			return kind == CatalogueKind::PlanetaryNebula ? PLANETARY_NEBULA_BRIGHTNESS
				   : kind == CatalogueKind::HIIRegion	 ? HII_REGION_BRIGHTNESS
														 : SUPERNOVA_REMNANT_BRIGHTNESS;
		}

		// Whether an OpenNGC object's type can be a nebula of this kind
		bool IsCompatible(const std::string& type, const CatalogueKind kind)
		{
			switch (kind)
			{
			case CatalogueKind::PlanetaryNebula: return type == "PN";
			case CatalogueKind::SupernovaRemnant: return type == "SNR";
			default: return type == "HII" || type == "EmN" || type == "Neb" || type == "Cl+N" || type == "RfN";
			}
		}

		CatalogueKind KindOfNgcType(const std::string& type)
		{
			return type == "PN" ? CatalogueKind::PlanetaryNebula : type == "SNR" ? CatalogueKind::SupernovaRemnant : CatalogueKind::HIIRegion;
		}

		// An open cluster with a Gaia distance, for the nebulae it lights
		struct Cluster
		{
			glm::dvec3 direction{ 0.0 };
			double radius = 0.0;   // Tidal radius (radians)
			double distance = 0.0; // Parsecs
			std::string name;
		};

		std::vector<Cluster> LoadClusters(const std::filesystem::path& path, std::vector<std::string>& report)
		{
			std::vector<Cluster> clusters;
			const std::optional<Table> table = ReadTable(path);
			if (!table)
			{
				report.push_back("clusters.tsv: not found");
				return clusters;
			}
			for (const auto& row : table->rows)
			{
				const double distance = table->Number(row, "dist50");
				if (table->Get(row, "Type") != "o" || !IsNumber(distance))
					continue;
				Cluster cluster;
				cluster.direction = Universe::EquatorialDirectionToWorld(table->Number(row, "_RAJ2000"), table->Number(row, "_DEJ2000"));
				const double radius = table->Number(row, "rt");
				cluster.radius = (IsNumber(radius) ? radius : 0.1) * Universe::DEGREES;
				cluster.distance = distance;
				std::string name(table->Get(row, "Name"));
				std::replace(name.begin(), name.end(), '_', ' ');
				cluster.name = name;
				clusters.push_back(std::move(cluster));
			}
			report.push_back("clusters.tsv: " + std::to_string(clusters.size()) + " open clusters (Hunt & Reffert 2023, for nebulae's distances)");
			return clusters;
		}

		void LoadNebulae(const std::filesystem::path& directory, const NgcIndex& ngc, Catalogue& catalogue)
		{
			std::vector<NebulaEntry> entries;
			auto Add = [&](std::optional<ExtendedObject> nebula, const std::string& designation, std::string region = {})
			{
				if (nebula)
					entries.push_back({ std::move(*nebula), designation, std::move(region) });
			};

			// Planetary nebulae, at their central stars' Gaia distances
			size_t planetaryCount = 0;
			if (const std::optional<Table> table = ReadTable(directory / "pne.tsv"))
			{
				for (const auto& row : table->rows)
				{
					const std::string_view status = table->Get(row, "pnStat");
					const double reliability = table->Number(row, "Rel");
					if ((status != "T" && status != "L") || !(reliability >= 0.5))
						continue;
					const double distance = table->Number(row, "rcomb");
					const std::string name(table->Get(row, "Name"));
					Add(MakeNebula(Universe::EquatorialDirectionToWorld(table->Number(row, "pnRAdeg"), table->Number(row, "pnDEdeg")), distance,
								   table->Number(row, "pnRad") * ARCSEC * distance, CatalogueKind::PlanetaryNebula, SpriteShape::PlanetaryNebula,
								   name, 0.6 + 0.4 * Seed(name + "q")), name);
				}
				planetaryCount = entries.size();
				catalogue.report.push_back("pne.tsv: " + std::to_string(planetaryCount) + " planetary nebulae (Chornay & Walton 2021)");
			}
			else
			{
				catalogue.report.push_back("pne.tsv: not found");
			}

			// H II regions with distances: a region (such as Orion A) can be several WISE sources
			if (const std::optional<Table> table = ReadTable(directory / "hii.tsv"))
			{
				const size_t before = entries.size();
				for (const auto& row : table->rows)
				{
					const double distance = table->Number(row, "Dist") * 1000.0;
					const std::string region(table->Get(row, "HIIName"));
					const size_t semicolon = region.find(';');
					const std::string wise = "WISE " + std::string(table->Get(row, "WISE"));
					const std::string name = semicolon != std::string::npos ? std::string(Trim(std::string_view(region).substr(semicolon + 1))) : wise;
					std::optional<ExtendedObject> nebula = MakeNebula(
						Universe::EquatorialDirectionToWorld(table->Number(row, "_RAJ2000"), table->Number(row, "_DEJ2000")), distance,
						table->Number(row, "Rad") * ARCSEC * distance, CatalogueKind::HIIRegion, SpriteShape::HIIRegion, wise, 0.7 + 0.3 * Seed(wise + "q"));
					if (nebula)
						nebula->name = name;
					Add(std::move(nebula), wise, region.empty() ? wise : region);
				}
				catalogue.report.push_back("hii.tsv: " + std::to_string(entries.size() - before) + " H II regions with distances (WISE, Anderson et al. 2014)");
			}
			else
			{
				catalogue.report.push_back("hii.tsv: not found");
			}

			// Supernova remnants with distances
			if (const std::optional<Table> table = ReadTable(directory / "snr.tsv"))
			{
				const size_t before = entries.size();
				for (const auto& row : table->rows)
				{
					const double distance = table->Number(row, "X") * 1000.0;
					const double major = table->Number(row, "Size1"), minorSize = table->Number(row, "Size2");
					const double size = IsNumber(minorSize) ? std::sqrt(major * minorSize) : major;
					const std::string designation = "SNR " + std::string(table->Get(row, "SNR"));
					Add(MakeNebula(Universe::EquatorialDirectionToWorld(table->Number(row, "_RAJ2000"), table->Number(row, "_DEJ2000")), distance,
								   0.5 * size * ARCMIN * distance, CatalogueKind::SupernovaRemnant,
								   table->Get(row, "Type") == "F" ? SpriteShape::FilledRemnant : SpriteShape::ShellRemnant, designation,
								   major > 0.0 ? minorSize / major : 1.0), designation);
				}
				catalogue.report.push_back("snr.tsv: " + std::to_string(entries.size() - before) + " supernova remnants (Ranasinghe & Leahy 2023)");
			}
			else
			{
				catalogue.report.push_back("snr.tsv: not found");
			}

			// Each well known nebula of OpenNGC (with a common name or a Messier number) names the
			// catalogue nebula that best matches it: of a compatible kind, over it and of a similar
			// size. Those not in the catalogues (without distances there) are added at the distance
			// of the open cluster lighting them, if there is one.
			// The largest first, and each name once (IC 4703 is the Eagle Nebula's core, NGC 6611 all of it)
			const std::vector<Cluster> clusters = LoadClusters(directory / "clusters.tsv", catalogue.report);
			std::vector<size_t> order = ngc.nebulae;
			std::stable_sort(order.begin(), order.end(), [&](const size_t a, const size_t b) { return ngc.objects[a].majorAxis > ngc.objects[b].majorAxis; });
			std::unordered_map<std::string, bool> usedNames;
			size_t clusterCount = 0;
			for (const size_t n : order)
			{
				const NgcObject& known = ngc.objects[n];
				if (known.displayName == known.designation && known.designation.rfind('M', 0) != 0)
					continue; // Only an NGC or IC number
				if (!usedNames.emplace(known.displayName, true).second)
					continue;
				const double knownRadius = 0.5 * std::sqrt(known.majorAxis * known.minorAxis) * ARCMIN;
				NebulaEntry* best = nullptr;
				double bestScore = 1.5;
				for (NebulaEntry& entry : entries)
				{
					if (!IsCompatible(known.type, entry.nebula.kind))
						continue;
					const double distance = entry.nebula.distance;
					const glm::dvec3 direction = entry.nebula.position / (double)distance;
					const double angularRadius = entry.nebula.radius / distance;
					const double reach = knownRadius + angularRadius + 2.0 * ARCMIN;
					const double cosine = glm::dot(direction, known.direction);
					if (cosine < std::cos(reach))
						continue;
					const double angle = std::acos(std::min(cosine, 1.0));
					const double score = angle / reach + 0.5 * std::abs(std::log(std::max(angularRadius, 0.5 * ARCMIN) / std::max(knownRadius, 0.5 * ARCMIN)));
					if (score < bestScore && (entry.known == nullptr || score < entry.matchScore))
					{
						bestScore = score;
						best = &entry;
					}
				}
				if (best != nullptr)
				{
					best->known = &known;
					best->matchScore = bestScore;
					continue;
				}

				const Cluster* lighting = nullptr;
				// (Within the nebula)
				double nearest = std::max(knownRadius, 5.0 * ARCMIN);
				for (const Cluster& cluster : clusters)
				{
					const double angle = std::acos(std::min(glm::dot(cluster.direction, known.direction), 1.0));
					if (angle < nearest)
					{
						nearest = angle;
						lighting = &cluster;
					}
				}
				if (lighting == nullptr || knownRadius <= 0.0)
					continue;
				const CatalogueKind kind = KindOfNgcType(known.type);
				std::optional<ExtendedObject> nebula = MakeNebula(known.direction, lighting->distance, knownRadius * lighting->distance, kind,
																  kind == CatalogueKind::PlanetaryNebula ? SpriteShape::PlanetaryNebula : SpriteShape::HIIRegion,
																  known.designation, 0.8);
				if (!nebula)
					continue;
				entries.push_back({ std::move(*nebula), "lit by the cluster " + lighting->name, {}, &known, 0.0 });
				clusterCount++;
			}
			if (clusterCount > 0)
				catalogue.report.push_back("NGC.csv: " + std::to_string(clusterCount) + " more nebulae at their clusters' distances");

			// Supernova remnants with a pulsar in them whose distance is measured better (by parallax
			// or otherwise, as the Crab's) are moved to it, keeping their size on the sky
			for (NebulaEntry& entry : entries)
			{
				ExtendedObject& nebula = entry.nebula;
				if (nebula.kind != CatalogueKind::SupernovaRemnant)
					continue;
				const glm::dvec3 direction = nebula.position / (double)nebula.distance;
				const double angularRadius = nebula.radius / nebula.distance;
				for (const CompactSource& pulsar : catalogue.compactSources)
				{
					if (!pulsar.inRemnant || !pulsar.measuredDistance)
						continue;
					const double distance = glm::length(pulsar.position);
					if (glm::dot(pulsar.position / distance, direction) < std::cos(std::max(angularRadius, 3.0 * ARCMIN)))
						continue;
					nebula.position = direction * distance;
					nebula.radius = (float)(angularRadius * distance);
					nebula.distance = (float)distance;
					entry.designation += ", at the distance of PSR " + pulsar.name.substr(4);
					break;
				}
			}

			// Finished: named, sized to at least their optical extent, described and labelled (each
			// named nebula, every planetary nebula and remnant, and each H II region once)
			std::unordered_map<std::string, std::pair<double, size_t>> regions; // Region: radius, nebula
			for (NebulaEntry& entry : entries)
			{
				ExtendedObject& nebula = entry.nebula;
				std::string designations = entry.designation;
				if (entry.known != nullptr)
				{
					nebula.name = entry.known->displayName;
					nebula.radius = std::max(nebula.radius, (float)(0.5 * std::sqrt(entry.known->majorAxis * entry.known->minorAxis) * ARCMIN * nebula.distance));
					const bool repeated = entry.known->designation.find(entry.designation) != std::string::npos;
					designations = entry.known->designation + (repeated ? "" : ", " + entry.designation);
				}
				nebula.luminosity = NebulaSurfaceBrightness(nebula.kind) * (float)std::numbers::pi * nebula.radius * nebula.radius;
				char description[320];
				std::snprintf(description, sizeof(description), "%s, %s\n%.3g pc across\n%s", GetCatalogueKindName(nebula.kind),
							  FormatDistance(nebula.distance).c_str(), 2.0 * nebula.radius, designations.c_str());
				nebula.description = description;
				nebula.designation = designations;

				const size_t index = catalogue.nebulae.size();
				if (entry.known != nullptr || nebula.kind != CatalogueKind::HIIRegion)
				{
					const float priority = (entry.known != nullptr ? 3.0f : 0.0f) + std::log10(nebula.radius);
					catalogue.landmarks.push_back({ nebula.name, nebula.description, nebula.kind, nebula.position, nebula.radius, 0.0f, priority });
				}
				else
				{
					auto [region, added] = regions.emplace(entry.region, std::pair{ (double)nebula.radius, index });
					if (!added && nebula.radius > region->second.first)
						region->second = { nebula.radius, index };
				}
				catalogue.nebulae.push_back(std::move(nebula));
			}
			for (const auto& [region, largest] : regions)
			{
				const ExtendedObject& nebula = catalogue.nebulae[largest.second];
				catalogue.landmarks.push_back({ nebula.name, nebula.description, nebula.kind, nebula.position, nebula.radius, 0.0f,
												std::log10(nebula.radius) });
			}
		}

		// ---- Galaxies ---------------------------------------------------------------------------

		// The disc's orientation: its major axis at a position angle (degrees, from north through
		// east) and its normal tilted from the line of sight by the inclination
		void Orient(ExtendedObject& galaxy, const glm::dvec3& direction, const double positionAngle, const double inclination)
		{
			const glm::dvec3 north = Universe::EquatorialDirectionToWorld(0.0, 90.0);
			const glm::dvec3 up = glm::normalize(north - glm::dot(north, direction) * direction);
			const glm::dvec3 east = glm::cross(up, direction);
			const double pa = positionAngle * Universe::DEGREES, i = inclination * Universe::DEGREES;
			const glm::dvec3 major = std::cos(pa) * up + std::sin(pa) * east;
			const glm::dvec3 minorAxis = -std::sin(pa) * up + std::cos(pa) * east;
			// Which side is nearer isn't known: chosen at random
			const double side = galaxy.seed < 0.5 ? 1.0 : -1.0;
			galaxy.majorAxis = glm::vec3(major);
			galaxy.normal = glm::vec3(glm::normalize(-std::cos(i) * direction + side * std::sin(i) * minorAxis));
		}

		double InclinationFromAxisRatio(const double axisRatio)
		{
			const double q = std::clamp(axisRatio, DISC_THICKNESS, 1.0);
			return std::acos(std::sqrt((q * q - DISC_THICKNESS * DISC_THICKNESS) / (1.0 - DISC_THICKNESS * DISC_THICKNESS))) / Universe::DEGREES;
		}

		// The colour of a galaxy's stars (K) by its type: redder for ellipticals, bluer for late
		// spirals and irregulars
		float GalaxyTemperature(const double typeCode, const SpriteShape shape)
		{
			if (shape == SpriteShape::EllipticalGalaxy)
				return typeCode <= -3.0 ? 4300.0f : 4700.0f;
			if (shape == SpriteShape::IrregularGalaxy)
				return 8000.0f;
			return (float)std::clamp(4700.0 + 300.0 * typeCode, 4700.0, 7500.0);
		}

		const char* GalaxyKindName(const SpriteShape shape, const double typeCode)
		{
			switch (shape)
			{
			case SpriteShape::EllipticalGalaxy:
				return typeCode <= -3.0 ? "Elliptical galaxy" : typeCode < 0.0 ? "Lenticular galaxy" : "Dwarf spheroidal galaxy";
			case SpriteShape::IrregularGalaxy:
				return "Irregular galaxy";
			case SpriteShape::BarredSpiralGalaxy:
				return "Barred spiral galaxy";
			default:
				return typeCode < 0.5 ? "Lenticular galaxy" : "Spiral galaxy";
			}
		}

		void FinishGalaxy(Catalogue& catalogue, ExtendedObject galaxy, const NgcObject* match, const std::string& designation, const bool label,
						  std::unordered_map<const NgcObject*, bool>& named)
		{
			// Each OpenNGC object names one galaxy
			if (match != nullptr && !named.emplace(match, true).second)
				match = nullptr;
			std::string known = designation;
			if (match != nullptr)
			{
				galaxy.name = match->displayName;
				known = match->designation;
			}
			char description[256];
			std::snprintf(description, sizeof(description), "%s, %s\n%.3g kpc across, %.2g times the Sun's luminosity (B)\n%s",
						  GalaxyKindName(galaxy.shape, galaxy.typeCode), FormatDistance(galaxy.distance).c_str(), 2.0 * galaxy.radius / 1000.0,
						  galaxy.luminosity, known.c_str());
			galaxy.description = description;
			galaxy.designation = known;
			if (label && !galaxy.name.empty())
			{
				const float priority = (match != nullptr && match->displayName != match->designation ? 3.0f : 0.0f) + (float)std::log10(galaxy.luminosity) - 6.0f;
				catalogue.landmarks.push_back({ galaxy.name, galaxy.description, CatalogueKind::Galaxy, galaxy.position, galaxy.radius, galaxy.luminosity, priority });
			}
			catalogue.galaxies.push_back(std::move(galaxy));
		}

		void LoadGalaxies(const std::filesystem::path& directory, const NgcIndex& ngc, Catalogue& catalogue)
		{
			std::vector<glm::dvec3> nearby; // Directions of the UNGC's galaxies
			std::unordered_map<std::string, bool> nearbyNames;
			std::unordered_map<const NgcObject*, bool> named;

			// The Local Volume, with measured distances
			if (const std::optional<Table> table = ReadTable(directory / "ungc.tsv"))
			{
				const size_t before = catalogue.galaxies.size();
				for (const auto& row : table->rows)
				{
					const double distance = table->Number(row, "Dist") * 1.0e6;
					// (Not the Milky Way itself)
					if (!IsNumber(distance) || distance < 2.0e4)
						continue;
					const glm::dvec3 direction = Universe::EquatorialDirectionToWorld(table->Number(row, "_RAJ2000"), table->Number(row, "_DEJ2000"));
					const std::string id(table->Get(row, "Name"));
					const std::string_view dwarf = table->Get(row, "Mcl");
					const double typeCode = table->Number(row, "TT");
					const NgcObject* match = ngc.Find(id);
					if (match == nullptr)
						match = ngc.Nearest(ngc.galaxies, direction, SAME_GALAXY_ANGLE);

					ExtendedObject galaxy;
					galaxy.position = direction * distance;
					galaxy.distance = (float)distance;
					galaxy.seed = Seed(id);
					galaxy.typeCode = IsNumber(typeCode) ? (float)typeCode : 9.0f;
					if (dwarf == "Sph" || dwarf == "dE" || dwarf == "dEem" || dwarf == "dS0em" || dwarf == "Tr" || galaxy.typeCode <= -3.0f)
						galaxy.shape = SpriteShape::EllipticalGalaxy;
					else if (dwarf == "Ir" || dwarf == "Im" || dwarf == "BCD" || dwarf == "HIcld" || galaxy.typeCode >= 9.0f)
						galaxy.shape = SpriteShape::IrregularGalaxy;
					else if (match != nullptr && match->hubble.find("SB") != std::string::npos)
						galaxy.shape = SpriteShape::BarredSpiralGalaxy;
					else
						galaxy.shape = SpriteShape::SpiralGalaxy;

					const double linear = table->Number(row, "A26") * 1000.0, angular = table->Number(row, "a26") * ARCMIN * distance;
					galaxy.radius = (float)(0.5 * (IsNumber(linear) ? linear : IsNumber(angular) ? angular : 1000.0));
					const double axisRatio = table->Number(row, "b/a");
					galaxy.axisRatio = IsNumber(axisRatio) ? (float)std::clamp(axisRatio, 0.1, 1.0) : 0.8f;
					double absolute = table->Number(row, "BMag");
					if (!IsNumber(absolute))
						absolute = table->Number(row, "Bmag") - 5.0 * std::log10(distance / 10.0);
					galaxy.luminosity = IsNumber(absolute) ? (float)std::pow(10.0, -0.4 * (absolute - SUN_ABSOLUTE_B)) : 1.0e7f;
					galaxy.temperature = GalaxyTemperature(galaxy.typeCode, galaxy.shape);
					double inclination = table->Number(row, "i");
					if (!IsNumber(inclination))
						inclination = InclinationFromAxisRatio(galaxy.axisRatio);
					const double positionAngle = match != nullptr && IsNumber(match->positionAngle) ? match->positionAngle : 180.0 * Seed(id + "pa");
					Orient(galaxy, direction, positionAngle, inclination);
					// (The UNGC abbreviates the Magellanic Clouds, which aren't NGC objects)
					galaxy.name = id == "LMC" ? "Large Magellanic Cloud" : id == "SMC" ? "Small Magellanic Cloud" : PrettyId(id);
					nearby.push_back(direction);
					nearbyNames[NormaliseId(id)] = true;
					const std::string designation = PrettyId(id);
					FinishGalaxy(catalogue, std::move(galaxy), match, designation, true, named);
				}
				catalogue.report.push_back("ungc.tsv: " + std::to_string(catalogue.galaxies.size() - before) + " galaxies of the Local Volume (UNGC, Karachentsev et al. 2013)");
			}
			else
			{
				catalogue.report.push_back("ungc.tsv: not found");
			}

			// Further out, at distances from their redshifts
			if (const std::optional<Table> table = ReadTable(directory / "2mrs.tsv"))
			{
				const size_t before = catalogue.galaxies.size();
				const double sameCosine = std::cos(SAME_GALAXY_ANGLE);
				for (const auto& row : table->rows)
				{
					const double distance = table->Number(row, "cz") / HUBBLE_CONSTANT * 1.0e6;
					if (!IsNumber(distance) || distance < MIN_REDSHIFT_DISTANCE)
						continue;
					const std::string id(table->Get(row, "CAT"));
					if (nearbyNames.contains(NormaliseId(id)))
						continue;
					const glm::dvec3 direction = Universe::EquatorialDirectionToWorld(table->Number(row, "_RAJ2000"), table->Number(row, "_DEJ2000"));
					bool duplicate = false;
					for (const glm::dvec3& other : nearby)
					{
						if (glm::dot(other, direction) > sameCosine)
						{
							duplicate = true;
							break;
						}
					}
					if (duplicate)
						continue;

					// The ZCAT type: the T-type, then A (unbarred), X (weakly) or B (barred)
					const std::string_view type = table->Get(row, "type");
					const double typeCode = ParseNumber(type.substr(0, type.find_first_not_of("-0123456789")));
					const bool barred = type.find('B') != std::string_view::npos;
					const NgcObject* match = ngc.Find(id);

					ExtendedObject galaxy;
					galaxy.position = direction * distance;
					galaxy.distance = (float)distance;
					galaxy.seed = Seed(std::string(table->Get(row, "ID")));
					galaxy.typeCode = IsNumber(typeCode) ? (float)typeCode : 3.0f;
					galaxy.shape = galaxy.typeCode <= -2.0f ? SpriteShape::EllipticalGalaxy
								 : galaxy.typeCode >= 9.0f ? SpriteShape::IrregularGalaxy
								 : barred ? SpriteShape::BarredSpiralGalaxy
										  : SpriteShape::SpiralGalaxy;
					const double isophote = table->Number(row, "Riso");
					galaxy.radius = (float)(KS_ISOPHOTE_TO_OPTICAL * (IsNumber(isophote) ? std::pow(10.0, isophote) : 20.0) * ARCSEC * distance);
					const double axisRatio = table->Number(row, "b/a");
					galaxy.axisRatio = IsNumber(axisRatio) ? (float)std::clamp(axisRatio, 0.1, 1.0) : 0.8f;
					const double absoluteK = table->Number(row, "Ktmag") - 5.0 * std::log10(distance / 10.0);
					const double luminosityK = std::pow(10.0, -0.4 * (absoluteK - SUN_ABSOLUTE_K));
					galaxy.luminosity = (float)(luminosityK * (galaxy.shape == SpriteShape::EllipticalGalaxy ? ELLIPTICAL_B_PER_K : SPIRAL_B_PER_K));
					galaxy.temperature = GalaxyTemperature(galaxy.typeCode, galaxy.shape);
					const double positionAngle = match != nullptr && IsNumber(match->positionAngle) ? match->positionAngle : 180.0 * galaxy.seed;
					Orient(galaxy, direction, positionAngle, InclinationFromAxisRatio(galaxy.axisRatio));
					// Only galaxies of the NGC, IC, Messier and UGC catalogues are named
					const std::string normalised = NormaliseId(id);
					const bool isNamed = normalised.rfind("NGC", 0) == 0 || normalised.rfind("IC", 0) == 0 || normalised.rfind("UGC", 0) == 0 ||
									   (normalised.size() > 1 && normalised[0] == 'M' && std::isdigit((unsigned char)normalised[1]));
					galaxy.name = isNamed ? PrettyId(id) : std::string();
					FinishGalaxy(catalogue, std::move(galaxy), match, isNamed ? PrettyId(id) : "2MASX J" + std::string(table->Get(row, "ID")), isNamed, named);
				}
				catalogue.report.push_back("2mrs.tsv: " + std::to_string(catalogue.galaxies.size() - before) + " more distant galaxies (2MRS, Huchra et al. 2012)");
			}
			else
			{
				catalogue.report.push_back("2mrs.tsv: not found");
			}
		}

		std::filesystem::path& CatalogueDirectory()
		{
			static std::filesystem::path directory = SV_DEFAULT_CATALOGUE_DIR;
			return directory;
		}
	}

	const char* GetCatalogueKindName(const CatalogueKind kind)
	{
		switch (kind)
		{
		case CatalogueKind::Star: return "Star";
		case CatalogueKind::Pulsar: return "Pulsar";
		case CatalogueKind::MillisecondPulsar: return "Millisecond pulsar";
		case CatalogueKind::Magnetar: return "Magnetar";
		case CatalogueKind::NeutronStar: return "Isolated neutron star";
		case CatalogueKind::XrayBinary: return "X-ray binary";
		case CatalogueKind::BlackHole: return "Black hole";
		case CatalogueKind::PlanetaryNebula: return "Planetary nebula";
		case CatalogueKind::HIIRegion: return "H II region (star-forming nebula)";
		case CatalogueKind::SupernovaRemnant: return "Supernova remnant";
		case CatalogueKind::Galaxy: return "Galaxy";
		default: return "";
		}
	}

	void SetCatalogueDirectory(const std::filesystem::path& directory)
	{
		CatalogueDirectory() = directory;
	}

	const std::filesystem::path& GetCatalogueDirectory()
	{
		return CatalogueDirectory();
	}

	glm::dvec3 GetGalacticCentre()
	{
		return Universe::EquatorialDirectionToWorld(SGR_A_RIGHT_ASCENSION, SGR_A_DECLINATION) * SGR_A_DISTANCE;
	}

	glm::dmat3 GetGalacticModelToWorld()
	{
		// +x from the centre to the Sun, +y to the north galactic pole (square to it); then turned
		// about z so the Sun is SUN_HEIGHT above the mid-plane
		const glm::dvec3 toSun = -glm::normalize(GetGalacticCentre());
		const glm::dvec3 pole = GalacticDirectionToWorld({ 0.0, 0.0, 1.0 });
		glm::dvec3 x = toSun;
		glm::dvec3 y = glm::normalize(pole - glm::dot(pole, x) * x);
		const glm::dvec3 z = glm::cross(x, y);
		const double tilt = std::asin(SUN_HEIGHT / SGR_A_DISTANCE);
		const glm::dvec3 tiltedX = std::cos(tilt) * x - std::sin(tilt) * y;
		y = std::sin(tilt) * x + std::cos(tilt) * y;
		x = tiltedX;
		return glm::dmat3(x, y, z);
	}

	Catalogue LoadCatalogue(const std::filesystem::path& directory)
	{
		const auto start = std::chrono::steady_clock::now();
		Catalogue catalogue;
		if (!std::filesystem::is_directory(directory))
		{
			catalogue.report.push_back("No catalogues in \"" + directory.string() + "\": run Tools/Catalogues/download_catalogues.py");
			return catalogue;
		}
		const NgcIndex ngc = LoadOpenNgc(directory / "NGC.csv", catalogue.report);
		LoadStars(directory / "hygdata_v41.csv", catalogue);
		LoadPulsars(directory / "psrcat.db", catalogue);
		LoadXrayBinaries(directory, catalogue);
		LoadNebulae(directory, ngc, catalogue);
		LoadGalaxies(directory, ngc, catalogue);

		// The Milky Way itself (seen from afar as a galaxy among the others): a barred spiral, 30 kpc
		// across, in the plane of the galaxy model around Sagittarius A*
		{
			const glm::dmat3 model = GetGalacticModelToWorld();
			ExtendedObject milkyWay;
			milkyWay.position = GetGalacticCentre();
			milkyWay.kind = CatalogueKind::Galaxy;
			milkyWay.shape = SpriteShape::BarredSpiralGalaxy;
			milkyWay.radius = 15000.0f;
			milkyWay.majorAxis = glm::vec3(model[0]);
			milkyWay.normal = glm::vec3(model[1]);
			milkyWay.luminosity = MILKY_WAY_LUMINOSITY;
			milkyWay.temperature = GalaxyTemperature(4.0, milkyWay.shape);
			milkyWay.typeCode = 4.0f;
			milkyWay.seed = 0.3f;
			milkyWay.name = "Milky Way";
			milkyWay.designation = "Milky Way";
			milkyWay.distance = (float)SGR_A_DISTANCE;
			milkyWay.description = "Our galaxy: a barred spiral 30 kpc across, of a few hundred billion stars\nThe Sun is " + FormatDistance(SGR_A_DISTANCE) + " from its centre";
			catalogue.landmarks.push_back({ milkyWay.name, milkyWay.description, CatalogueKind::Galaxy, milkyWay.position, milkyWay.radius,
											milkyWay.luminosity, 3.0f + (float)std::log10(MILKY_WAY_LUMINOSITY) - 6.0f });
			catalogue.galaxies.push_back(std::move(milkyWay));
		}

		// Sagittarius A*
		CompactSource sgrA;
		sgrA.position = GetGalacticCentre();
		sgrA.kind = CatalogueKind::BlackHole;
		sgrA.name = "Sagittarius A*";
		sgrA.description = "Supermassive black hole at the centre of the Milky Way, " + FormatDistance(SGR_A_DISTANCE) + "\n4.3 million solar masses";
		catalogue.landmarks.push_back({ sgrA.name, sgrA.description, CatalogueKind::BlackHole, sgrA.position, 0.0f, 0.0f, 20.0f });
		catalogue.compactSources.push_back(std::move(sgrA));

		const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
		catalogue.report.push_back("Parsed in " + std::to_string(seconds).substr(0, 4) + " s");
		return catalogue;
	}
}
