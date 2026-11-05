#pragma once
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

namespace SV
{
	// Translation, rotations and scale of an object, combined into transformationMatrix by
	// RecalculateTransformation. Rotations are (axis.xyz, angle in degrees), applied in order.
	class Transformation
	{
	public:
		virtual ~Transformation() = default;

		void Translate(const glm::vec3& pos) { translation = pos; }
		void Scale(const glm::vec3& scaleIn) { scaling = scaleIn; }

		[[nodiscard]] const glm::vec3& GetTranslation() const { return translation; }
		[[nodiscard]] const glm::vec3& GetScale() const { return scaling; }

		void Rotate(const glm::vec4& rotation) { rotations.push_back(rotation); }

		[[nodiscard]] glm::mat4 GetRotationMatrix() const
		{
			glm::mat4 rotationMatrix(1.0f);
			for (const glm::vec4& rot : rotations)
				rotationMatrix *= glm::rotate(glm::radians(rot.w), glm::vec3(rot));
			return rotationMatrix;
		}

		const glm::mat4& RecalculateTransformation()
		{
			transformationMatrix = glm::translate(translation) * GetRotationMatrix() * glm::scale(scaling);
			return transformationMatrix;
		}

		glm::mat4 transformationMatrix{ 1.0f };

	protected:
		glm::vec3 translation{ 0.0f };
		glm::vec3 scaling{ 1.0f };
		std::vector<glm::vec4> rotations;
	};
}
