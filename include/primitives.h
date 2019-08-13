#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <memory>

#include <Eigen/Core>
#include "pointcloud.h"
#include "mesh.h"

namespace lmu
{
	//Manifolds without border
	enum class ManifoldType
	{
		None = 0,		
		Cylinder, 
		Sphere, 
		Plane,
		Cone
	};
	std::string manifoldTypeToString(ManifoldType type);
	ManifoldType manifoldTypeFromString(std::string type);

	struct Manifold 
	{
		Manifold(ManifoldType type, const Eigen::Vector3d& p, const Eigen::Vector3d& n, const Eigen::Vector3d& r, const PointCloud& pc) :
			type(type),
			p(p),
			n(n),
			r(r),
			pc(pc)
		{
		}

		Manifold(ManifoldType type, const Eigen::Vector3d& p, const Eigen::Vector3d& n, const Eigen::Vector3d& r) :
			type(type),
			p(p),
			n(n),
			r(r)
		{
		}
		
		Manifold(ManifoldType type, const Eigen::Vector3d& p, const Eigen::Vector3d& n) :
			Manifold(type, p, n, Eigen::Vector3d())
		{
		}

		Manifold(ManifoldType type) : 
			Manifold(type, Eigen::Vector3d(), Eigen::Vector3d(), Eigen::Vector3d())
		{
		}

		Manifold(ManifoldType type, const Eigen::Vector3d& p) :
			Manifold(type, p, Eigen::Vector3d(), Eigen::Vector3d())
		{
		}

		Manifold(const Manifold& m) :
			p(m.p),
			n(m.n),
			r(m.r),
			type(m.type),
			pc(m.pc)
		{
			//std::cout << "copy" << std::endl;
		}

		void projectPointsOnSurface() {
			switch (type)
			{
			case ManifoldType::Plane:
				projectPointCloudOnPlane(pc, p, n);
				break;
			case ManifoldType::Cylinder:
				break;
			case ManifoldType::Sphere:
				projectPointCloudOnSphere(pc, p, r.x());
				break;
			case ManifoldType::Cone:
				break;
			}			
		}

		double signedDistance(const Eigen::Vector3d& p) const
		{
			switch (type)
			{
				case ManifoldType::Plane:
					return 0.0;
				case ManifoldType::Cylinder:
					return 0.0;
				case ManifoldType::Sphere:
					return 0.0;
				case ManifoldType::Cone:
					return 0.0;
			}
		}
		
		Eigen::Vector3d p; 
		Eigen::Vector3d n; 
		Eigen::Vector3d r; 
		ManifoldType type; 
		PointCloud pc;

		friend std::ostream& operator<<(std::ostream& os, const Manifold& m);
	};
	std::ostream& operator<<(std::ostream& os, const Manifold& m);

	using ManifoldPtr = std::shared_ptr<Manifold>;
	using ManifoldSet = std::vector<ManifoldPtr>;

	const int numPrimitiveTypes = 5;
	enum class PrimitiveType
	{
		None = 0,
		Cylinder, 
		Sphere,
		Cone,
		Box
	};

	ManifoldType fromPrimitiveType(PrimitiveType pt);
	ManifoldType fromPredictedTypeType(int pt);

	std::string primitiveTypeToString(PrimitiveType type);
	PrimitiveType primitiveTypeFromString(std::string type);
	
	struct Primitive
	{
		Primitive(const ImplicitFunctionPtr& imFunc, const ManifoldSet& ms, PrimitiveType type) :
			imFunc(imFunc), ms(ms), type(type)
		{
		}
				
		bool isNone() const
		{
			return type == PrimitiveType::None;
		}

		static Primitive None()
		{
			return Primitive();
		}

		ImplicitFunctionPtr imFunc; 
		ManifoldSet ms;
		PrimitiveType type;

		friend std::ostream& operator<<(std::ostream& os, const Primitive& p);

	private:
		Primitive() :
			Primitive(nullptr, ManifoldSet(), PrimitiveType::None)
		{
		}
	};
	std::ostream& operator<<(std::ostream& os, const Primitive& np);

	struct PrimitiveSet : std::vector<Primitive>
	{
		size_t hash(size_t seed) const
		{
			return 0; //TODO
		}
	};

	struct RansacParams 
	{
		RansacParams() : 
			probability(0.01),
			min_points((std::numeric_limits<size_t>::max)()),
			epsilon(-1),
			normal_threshold(0.9),
			cluster_epsilon(-1)
		{
		}

		double probability;         // Probability to control search endurance. %Default value: 5%.
		size_t min_points;			// Minimum number of points of a shape. %Default value: 1% of total number of input points.
		double epsilon;             // Maximum tolerance Euclidian distance from a point and a shape. %Default value: 1% of bounding box diagonal.
		double normal_threshold;	// Maximum tolerance normal deviation from a point's normal to the normal on shape at projected point. %Default value: 0.9 (around 25 degrees).
		double cluster_epsilon;	    // Maximum distance between points to be considered connected. %Default value: 1% of bounding box diagonal.
		std::set<ManifoldType> types;
	};

	struct RansacResult
	{
		ManifoldSet manifolds;
		PointCloud pc;
	};

	RansacResult mergeRansacResults(const std::vector<RansacResult>& results);

	struct RansacMergeParams
	{
		RansacMergeParams(double dist_threshold, double dot_threshold, double angle_threshold) :
			dist_threshold(dist_threshold),
			dot_threshold(dot_threshold),
			angle_threshold(angle_threshold)
		{
		}

		RansacMergeParams() :
			dist_threshold(0.0),
			dot_threshold(0.0),
			angle_threshold(0.0)
		{
		}

		double dist_threshold;
		double dot_threshold;
		double angle_threshold;
	};

	RansacResult extractManifoldsWithCGALRansac(const PointCloud& pc, const RansacParams& params, 
		bool projectPointsOnSurface = false);
	RansacResult extractManifoldsWithOrigRansac(const PointCloud& pc, const RansacParams& params, 
		bool projectPointsOnSurface = false, int ransacIterations = 1, const RansacMergeParams& rmParams = RansacMergeParams());

	void writeToFile(const std::string& file, const RansacResult& res);
	RansacResult readFromFile(const std::string& file);
}

#endif 