#ifndef POINTCLOUD_H
#define POINTCLOUD_H

#include <Eigen/Core>
#include <Eigen/Geometry>

namespace lmu
{
	struct Mesh;

	void writePointCloud(const std::string& file, Eigen::MatrixXd& points);
	Eigen::MatrixXd readPointCloud(const std::string& file, double scaleFactor, bool readHeader);
	Eigen::MatrixXd pointCloudFromMesh(const lmu::Mesh & mesh, double delta, double samplingRate, double errorSigma);

	Eigen::MatrixXd getSIFTKeypoints(Eigen::MatrixXd& points, double minScale, double minContrast, int numOctaves, int numScalesPerOctave, bool normalsAvailable);

	
}

#endif