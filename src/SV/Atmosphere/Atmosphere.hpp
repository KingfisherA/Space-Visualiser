#pragma once
#include "Dahlia/Utilities/Memory.h"
#include "AtmospherePrecomputer.hpp"

namespace SV::Atmosphere
{
	// A planet's atmosphere: its scattering parameters and precomputed textures. The textures are
	// precomputed on the GPU the first time the atmosphere is rendered.
	class IAtmosphere
	{
	public:
		virtual ~IAtmosphere() = default;

		[[nodiscard]] const sRef<AtmospherePrecomputer>& GetPrecomputer() const { return atmoPrecomputer; }
		[[nodiscard]] const ProcessedAtmoData& GetAtmosphericData() const { return atmoPrecomputer->GetProcessedAtmoData(); }
		// How strongly the atmosphere's colour is added over the planet's surface
		[[nodiscard]] float GetSurfaceAtmoIntensity() const { return surfaceAtmoIntensity; }

	protected:
		sRef<AtmospherePrecomputer> atmoPrecomputer = nullptr;
		float surfaceAtmoIntensity = 1.0f;
	};

	class EarthAtmosphere : public IAtmosphere
	{
	public:
		EarthAtmosphere();
	};

	// Mercury has only a tenuous exosphere: the atmosphere is a vacuum
	class MercuryAtmosphere : public IAtmosphere
	{
	public:
		MercuryAtmosphere();
	};

	// Venus' atmosphere above its cloud tops (the planet's visible surface)
	class VenusAtmosphere : public IAtmosphere
	{
	public:
		VenusAtmosphere();
	};

	class MarsAtmosphere : public IAtmosphere
	{
	public:
		MarsAtmosphere();
	};

	// The giant planets: hydrogen and helium over a cloud deck veiled by haze. The bottom of each is
	// the planet's 1 bar level.
	class JupiterAtmosphere : public IAtmosphere
	{
	public:
		JupiterAtmosphere();
	};

	class SaturnAtmosphere : public IAtmosphere
	{
	public:
		SaturnAtmosphere();
	};

	class UranusAtmosphere : public IAtmosphere
	{
	public:
		UranusAtmosphere();
	};

	class NeptuneAtmosphere : public IAtmosphere
	{
	public:
		NeptuneAtmosphere();
	};

	class TitanAtmosphere : public IAtmosphere
	{
	public:
		TitanAtmosphere();
	};
}
