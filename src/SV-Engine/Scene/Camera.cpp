#include"SV-Engine/Precompiled Global.hpp"
#include"Camera.hpp"
#include"SV-Engine/Utilities/String.hpp"
#include<GLFW/glfw3.h>


namespace SV
{
	//
	//
	constexpr float POS_MAX_ACCELERATION = 100.0f;
	constexpr float POS_ACCELERATION_FACTOR = 0.08f;
	//
	constexpr float ANGLE_AROUND_MAX_ACCELERATION = 2.0f;
	constexpr float ANGLE_AROUND_ACCELERATION_FACTOR = 0.8f;
	//
	constexpr float PITCH_MAX_ACCELERATION = 10.0f;
	constexpr float PITCH_ACCELERATION_FACTOR = 0.6f;
	//
	constexpr float ZOOM_MAX_ACCELERATION = 100.0f;
	constexpr float ZOOM_ACCELERATION_FACTOR = 2.0f;
	//
	Camera::Camera() : viewMatrix(1.0f), centrePosition(0.0f)
	{
		RecalculateActualPos();
	}
	//
	void Camera::UpdateKeyboard(int key)
	{
		if (key != GLFW_KEY_W &&
			key != GLFW_KEY_S &&
			key != GLFW_KEY_A &&
			key != GLFW_KEY_D)
		{
			posAcceleration *= 0.8f;
			return;
		}

		if (isFollowingPath) StopPath();
		glm::vec3 vecToCam = normalize(centrePosition - actualPosition);

		glm::vec3 deltaVec;
		posAcceleration += POS_ACCELERATION_FACTOR;

		if (key == GLFW_KEY_W)
		{
			deltaVec = vecToCam;
		}
		if (key == GLFW_KEY_S)
		{
			deltaVec = -vecToCam;
		}
		if (key == GLFW_KEY_A)
		{
			deltaVec = -glm::cross(vecToCam, glm::vec3(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]));
		}
		if (key == GLFW_KEY_D)
		{
			deltaVec = glm::cross(vecToCam, glm::vec3(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]) /*The camera's up vector in world space*/);
		}

		deltaVec *= posAcceleration;

		posVelocity += deltaVec;

		posAcceleration = min(posAcceleration, POS_MAX_ACCELERATION);
	}
	void Camera::UpdatePitch(float mouseDY, int buttonClicked)
	{
		if (buttonClicked == GLFW_MOUSE_BUTTON_RIGHT)
		{
			pitchAcceleration += PITCH_ACCELERATION_FACTOR;
			pitchAcceleration = glm::clamp(pitchAcceleration, -PITCH_MAX_ACCELERATION, PITCH_MAX_ACCELERATION);
			pitchVelocity += -mouseDY * pitchAcceleration;
		}
	}
	void Camera::UpdateYaw(float mouseDX, int buttonClicked)
	{
		if (buttonClicked == GLFW_MOUSE_BUTTON_LEFT)
		{
			yawAcceleration += ANGLE_AROUND_ACCELERATION_FACTOR;
			yawAcceleration = glm::clamp(yawAcceleration, -ANGLE_AROUND_MAX_ACCELERATION, ANGLE_AROUND_MAX_ACCELERATION);
			yawVelocity += mouseDX * yawAcceleration;
		}
	}
	void Camera::UpdateZoom(double mouseWheelD)
	{
		if (isFollowingPath) StopPath();
		zoomAcceleration += ZOOM_ACCELERATION_FACTOR;
		zoomAcceleration = glm::clamp(zoomAcceleration, -ZOOM_MAX_ACCELERATION, ZOOM_MAX_ACCELERATION);
		zoomVelocity -= mouseWheelD * zoomAcceleration;
	}
	void Camera::MoveTo(const glm::vec3& newPosition, const float targetZoomIn)
	{
		targetZoom = targetZoomIn;
		targetPosition = newPosition;
		isFollowingPath = true;
		posVelocity = glm::vec3(0.11f, 0.0f, 0.0f);
	}
	void Camera::UpdateCamera(double deltaTime, int currentKey)
	{
		if (isFollowingPath)
		{
			constexpr float thresholdDistance = 0.001f;
			if (glm::distance2(centrePosition, targetPosition) < thresholdDistance * thresholdDistance &&
				glm::abs(targetZoom - zoom) < thresholdDistance)
			{
				isFollowingPath = false;
				posVelocity = glm::vec3(0.0f);
				centrePosition = targetPosition;
				zoom = targetZoom;
			}
			else
			{
				centrePosition = glm::mix(centrePosition, targetPosition, posVelocity.x);
				zoom = glm::mix(zoom, targetZoom, posVelocity.x);
			}
		}
		else
		{
			centrePosition += posVelocity * (float)deltaTime;
			posVelocity *= 0.9;

			zoomAcceleration *= 0.92f;
			zoomVelocity *= 0.94f;
		}

		yaw += yawVelocity * deltaTime;
		pitch += pitchVelocity * deltaTime;
		zoom += zoomVelocity * deltaTime;

		pitchVelocity *= 0.95f;
		pitchAcceleration *= 0.8f;

		yawAcceleration *= 0.99f;
		yawVelocity *= 0.9f;

		this->RecalculateActualPos();
		this->RecalculateViewMatrix();

		UpdateKeyboard(currentKey);
	}
	//
	void Camera::RecalculateActualPos()
	{
		float horiDistanceFromCentre = zoom * cos(glm::radians(pitch));
		float vertiDistanceFromCentre = zoom * sin(glm::radians(pitch));

		actualPosition = centrePosition;
		actualPosition.x -= horiDistanceFromCentre * sin(glm::radians(180.0f - yaw));
		actualPosition.y += vertiDistanceFromCentre;
		actualPosition.z -= horiDistanceFromCentre * cos(glm::radians(180.0f - yaw));
	}
	void Camera::RecalculateViewMatrix()
	{
		rotationsMatrix = glm::rotate(glm::radians(pitch), glm::vec3(1, 0, 0)) * glm::rotate(glm::radians(yaw), glm::vec3(0, 1, 0)) * glm::rotate(glm::radians(roll), glm::vec3(0, 0, 1));
		viewMatrix = rotationsMatrix * glm::translate(-actualPosition);
		invViewMatrix = glm::inverse(viewMatrix);
	}
	//
}