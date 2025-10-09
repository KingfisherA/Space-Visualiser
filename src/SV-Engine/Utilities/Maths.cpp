#include"SV-Engine/Precompiled Global.hpp"
#include"SV-Engine/Utilities/Maths.hpp"

namespace SV::Maths
{
	//
	//
	float Maths::LinearInterpolate(float a, float b, float f)
	{
		return (a * (1.0f - f)) + (b * f);
	}
	//
	float Maths::LinearInterpolateSimplified(float a, float b, float f)
	{
		return a + f * (b - a);
	}
	//
	double Maths::CosineInterpolate(double y1, double y2, double mu)
	{
		double mu2;
		mu2 = (1 - cos(mu * PI)) / 2;
		return(y1 * (1 - mu2) + y2 * mu2);
	}
	//
	glm::vec2 Maths::Rotate2DVector(float angleIn, glm::vec2 vectorIn)
	{
		return glm::vec2(vectorIn.x * cos(angleIn) - vectorIn.y * sin(angleIn), vectorIn.x * sin(angleIn) + vectorIn.y * cos(angleIn));
	}
	//
	float Maths::Remap(float number, float oldRangeMin, float oldRangeMax, float newRangeMin, float newRangeMax)
	{
		return (float)(newRangeMin + ((number - oldRangeMin) * (float)((newRangeMax - newRangeMin) / (oldRangeMax - oldRangeMin))));
	}
	//
	float Maths::GetLinearFadeValue(float distanceIn, float minDistance, float fadeSpeed)
	{
		return glm::clamp(1 / Remap(distanceIn, minDistance, minDistance + fadeSpeed, 0.0f, 1.0f), 0.0f, 1.0f);
	}
	//
	int IntegerPow(int base, int exponent)
	{
		if (exponent == 0) return 1;
		 int result = base;

		for (unsigned int i = 1; i < exponent; i++)
		{
			result *= base;
		}

		return result;
	}
	//
	unsigned int IntegerPow(unsigned int base, unsigned int exponent)
	{
		if (exponent == 0) return 1;
		unsigned int result = base;

		for (unsigned int i = 1; i < exponent; i++)
		{
			result *= base;
		}

		return result;
	}
	//
	long IntegerPow(long base, long exponent)
	{
		if (exponent == 0) return 1;
		long result = base;

		for (unsigned long i = 1; i < exponent; i++)
		{
			result *= base;
		}

		return result;
	}
	//
	unsigned long IntegerPow(unsigned long base, unsigned long exponent)
	{
		if (exponent == 0) return 1;
		unsigned long result = base;

		for (unsigned long i = 1; i < exponent; i++)
		{
			result *= base;
		}

		return result;
	}
	//
	float Maths::GetFadeValue(float distanceIn, float maxDistance, float fadeDensity, float fadeGradient)
	{
		return glm::clamp(exp(-pow((distanceIn - maxDistance) * fadeDensity, fadeGradient)), 0.0f, 1.0f);
	}
	//
	glm::mat4 Maths::SphericalBillboard(const glm::vec3& cameraPosition, const glm::vec3& objectPosition)
	{
		glm::vec3 LookAt, ObjToCameraProj, ObjToCamera, UpDirection;
		glm::mat4 outputMatrix(1.0f);
		float RotationAngle = 0.0f;

		// ObjToCameraProj is the std::vector in world coordinates from the 
		// local origin to the camera projected in the XZ plane
		ObjToCameraProj.x = cameraPosition.x - objectPosition.x;
		ObjToCameraProj.y = 0;
		ObjToCameraProj.z = cameraPosition.z - objectPosition.z;

		// This is the original LookAt std::vector for the object 
		// in world coordinates
		LookAt.x = 0;
		LookAt.y = 0;
		LookAt.z = 1;

		// Normalize both vectors to get the cosine directly afterwards
		ObjToCameraProj = glm::normalize(ObjToCameraProj);

		// Easy fix to determine wether the angle is negative or positive
		// for positive angles UpDirection will be a std::vector pointing in the 
		// positive y direction, otherwise UpDirection will point downwards
		// effectively reversing the rotation.
		UpDirection = glm::cross(LookAt, ObjToCameraProj);

		// Compute the angle
		RotationAngle = glm::dot(LookAt, ObjToCameraProj);

		// Perform the rotation. The if statement is used for stability reasons
		// if the LookAt and ObjToCameraProj vectors are too close together then 
		// |RotationAngle| could be bigger than 1 due to lack of precision
		if ((RotationAngle < 0.99990) && (RotationAngle > -0.9999))
		{
			outputMatrix *= glm::rotate(acos(RotationAngle), UpDirection);
		}

		// The second rotation which tilts the object so that it faces
		// the camera

		// ObjToCamera is the std::vector in world coordinates from 
		// the local origin to the camera
		ObjToCamera.x = cameraPosition.x - objectPosition.x;
		ObjToCamera.y = cameraPosition.y - objectPosition.y;
		ObjToCamera.z = cameraPosition.z - objectPosition.z;

		// Normalize to get the cosine afterwards
		ObjToCamera = glm::normalize(ObjToCamera);

		// Compute the angle between ObjToCameraProj and ObjToCamera, 
		//i.e. compute the required angle for the lookup std::vector
		RotationAngle = glm::dot(ObjToCameraProj, ObjToCamera);

		// Tilt the object. The test is done to prevent instability 
		// when ObjToCamera and ObjToCameraProj have a very small
		// angle between them
		if ((RotationAngle < 0.99990) && (RotationAngle > -0.9999))
		{
			if (ObjToCamera.y < 0)
			{
				outputMatrix *= glm::rotate(acos(RotationAngle), glm::vec3(1, 0, 0));
			}
			else
			{
				outputMatrix *= glm::rotate(acos(RotationAngle), glm::vec3(-1, 0, 0));
			}
		}

		return outputMatrix;
	}
	//
}