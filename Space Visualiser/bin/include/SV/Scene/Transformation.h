#pragma once
#include<glm.hpp>
#include<vector>

//
#define TRANSFORMATION_DEGREES

namespace SV
{
	//
	class Transformation
	{
	private:
		//
		glm::vec3 translation;
		glm::vec3 scaling;
		std::vector<glm::vec4> rotations;
		//
	public:
		//
		Transformation() : transformationMatrix(1.0f), translation(0.0f), scaling(1.0f) {}
		Transformation(const glm::vec3& posIn) : transformationMatrix(1.0f), translation(posIn), scaling(1.0f) { RecalculateTransformation(); }
		Transformation(const glm::vec3& posIn, const std::vector<glm::vec4>& rotIn) : transformationMatrix(1.0f), translation(posIn), rotations(rotIn), scaling(1.0f) { RecalculateTransformation(); }
		Transformation(const glm::vec3& posIn, const std::vector<glm::vec4>& rotIn, const glm::vec3& scaleIn) : transformationMatrix(1.0f), translation(posIn), rotations(rotIn), scaling(scaleIn) { RecalculateTransformation(); }
		Transformation(const Transformation& t) : translation(t.translation), rotations(t.rotations), scaling(t.scaling) { RecalculateTransformation(); }
		Transformation(const Transformation* t) : translation(t->translation), rotations(t->rotations), scaling(t->scaling) { RecalculateTransformation(); }
		//
		void Translate(const glm::vec3& pos) { translation = pos; }
		void Scale(const glm::vec3& scaleIn) { scaling = scaleIn; }
		//
		void IncreaseTranslation(const glm::vec3& pos) { translation += pos; }
		void IncreaseScale(const glm::vec3& scaleIn) { scaling += scaleIn; }
		//
		void DecreaseTranslation(const glm::vec3& pos) { translation -= pos; }
		void DecreaseScale(const glm::vec3& scaleIn) { scaling -= scaleIn; }
		//
		inline glm::vec3 GetTranslation() { return translation; }
		inline glm::vec3 GetScale() { return scaling; }
		//
		void RemoveRotation(size_t index)
		{
			if (index + 1  > rotations.size()) return;
			rotations.erase(rotations.begin() + index);
		}
		size_t Rotate(glm::vec4& rotation, size_t index = 0)
		{
			if (index + 1 > rotations.size())
			{
				rotations.resize(index + 1);
			}

			rotations[index] = rotation;
			return rotations.size() - 1;
		}
		glm::vec4& GetRotation(size_t index = 0) { return rotations[index]; }
		void IncreaseRotation(float val, size_t index = 0, const glm::vec3& axis = glm::vec3(1.0f, 0.0f, 0.0f))
		{
			if (index + 1 > rotations.size())
			{
				rotations.resize(index + 1);
				rotations[index] = glm::vec4(axis, val);
			}
			else
			{
				rotations[index].w += val;
			}
		}
		//
		const std::vector<glm::vec4>& GetRotations() { return rotations; }
		size_t GetRotationsSize() { return rotations.size(); }
		void ClearRotations() { rotations.clear(); }
		//
		glm::mat4 GetTranslationMatrix() { return glm::translate(translation); }
		glm::mat4 GetRotationMatrix()
		{
			glm::mat4 rotationMatrix(1.0f);
			for (glm::vec4& rot : rotations)
			{
#ifdef TRANSFORMATION_DEGREES
				rotationMatrix *= glm::rotate(glm::radians(rot.w), glm::vec3(rot.x, rot.y, rot.z));
#else
				rotationMatrix *= glm::rotate(rot.w, vec3(rot.x, rot.y, rot.z));
#endif // TRANSFORMATION_DEGREES
			}
			return rotationMatrix;
		}
		glm::mat4 GetScaleMatrix() { return glm::scale(scaling); }
		glm::mat4& RecalculateTransformation() { transformationMatrix = glm::translate(translation) * GetRotationMatrix() * glm::scale(scaling); return transformationMatrix; }
		//
		inline operator glm::mat4() { return transformationMatrix; }
		inline operator glm::mat4& () { return transformationMatrix; }
		//
		glm::mat4 transformationMatrix;
		//
	};
}