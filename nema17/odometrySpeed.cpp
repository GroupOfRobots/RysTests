#include <iostream>
#include <unistd.h>
#include <csignal>
#include <cmath>
#include "MotorsController.hpp"

#include <kdl/frames.hpp>

bool exitFlag = false;

void sigintHandler(int signum) {
	if (signum == SIGINT) {
		exitFlag = true;
	}
}

int main(int argc, char * argv[]) {
	if (argc < 6) {
		std::cerr << "Not enough arguments. Usage:" << std::endl;
		std::cerr << "\t" << argv[0] << " <time> <microstep> <speed> <wheelRadius> <baseWidth>" << std::endl;
		return 1;
	}

	const float time = atof(argv[1]);
	const int microstep = atoi(argv[2]);
	float speed = atof(argv[3]);
	const float wheelRadius = atof(argv[4]);
	const float baseWidth = atof(argv[5]);

	signal(SIGINT, sigintHandler);

	std::cout << "[ODOM] Initializing motors..." << std::endl;
	MotorsController * motorsController = new MotorsController();
	motorsController->setInvertSpeed(true, false);
	motorsController->setMotorsSwapped(true);
	std::cout << "[ODOM] Enabling motors..." << std::endl;
	motorsController->enableMotors();
	std::cout << "[ODOM] Running..." << std::endl;

	KDL::Frame currentOdometryFrame;

	auto startTime = std::chrono::high_resolution_clock::now();
	auto previousTimestamp = startTime;
	float timeElapsed = 0.0f;

	while (!exitFlag) {
		// Save current speeds in units suitable for odometry (m/s)
		float leftSpeed = motorsController->getMotorSpeedLeft() * wheelRadius * 2 * M_PI;
		float rightSpeed = motorsController->getMotorSpeedRight() * wheelRadius * 2 * M_PI;

		motorsController->setMotorSpeeds(speed, speed, microstep, false);

		auto now = std::chrono::high_resolution_clock::now();
		auto loopTimeSpan = std::chrono::duration_cast<std::chrono::duration<float>>(now - previousTimestamp);
		float loopTime = loopTimeSpan.count();
		previousTimestamp = now;

		KDL::Frame odometryUpdateFrame;
		if (leftSpeed == rightSpeed) {
			// Only linear movement
			odometryUpdateFrame = KDL::Frame(KDL::Vector(0, leftSpeed * loopTime, 0));
		} else {
			float linearVelocity = (rightSpeed + leftSpeed) / 2;
			float angularVelocity = (rightSpeed - leftSpeed) / baseWidth;
			float rotationPointDistance = linearVelocity / angularVelocity;
			float rotationAngle = angularVelocity * loopTime;
			// Mobile robots traditionally are Y-forward-oriented
			float deltaX = rotationPointDistance * (1.0 - std::cos(rotationAngle));
			float deltaY = rotationPointDistance * std::sin(rotationAngle);
			odometryUpdateFrame = KDL::Frame(KDL::Rotation::RotZ(rotationAngle), KDL::Vector(deltaX, deltaY, 0));
		}

		// Third, update the odometry frame and put it into the message
		currentOdometryFrame = currentOdometryFrame * odometryUpdateFrame;

		auto totalTimeSpan = std::chrono::duration_cast<std::chrono::duration<float>>(now - startTime);
		timeElapsed = totalTimeSpan.count();
		if (timeElapsed > time) {
			speed = 0;
		}
		if (timeElapsed > time && leftSpeed == 0 && rightSpeed == 0) {
			exitFlag = true;
			break;
		}
		usleep(10 * 1000);
	}

	std::cout << "[ODOM] disabling motors" << std::endl;
	motorsController->disableMotors();
	delete motorsController;

	std::cout << "Time elapsed:\n\tT: " << timeElapsed << std::endl;
	std::cout << "Current position:\n\tX: " << currentOdometryFrame.p.x() << "\n\tY: " << currentOdometryFrame.p.y() << std::endl;
	double roll, pitch, yaw;
	currentOdometryFrame.M.GetRPY(roll, pitch, yaw);
	std::cout << "Current heading:\n\tYaw: " << yaw << std::endl;

	return 0;
}
