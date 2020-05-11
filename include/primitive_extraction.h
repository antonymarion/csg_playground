#ifndef PRIMITIVE_EXTRACTION_H
#define PRIMITIVE_EXTRACTION_H

#include "primitives.h"
#include "evolution.h"

#include <vector>

typedef struct _object PyObject;

namespace lmu
{
	struct PrimitiveSetRank
	{
		PrimitiveSetRank(double geo, double per_prim_geo_sum, double size, double combined,
			const std::vector<double>& per_primitive_geo_scores = std::vector<double>()) :
			geo(geo),
			per_prim_geo_sum(per_prim_geo_sum),
			size(size),
			combined(combined),
			per_primitive_geo_scores(per_primitive_geo_scores)
		{
		}

		PrimitiveSetRank() :
			PrimitiveSetRank(0.0)
		{
		}

		explicit PrimitiveSetRank(double v) :
			PrimitiveSetRank(v, v, v, v)
		{
		}

		static const PrimitiveSetRank Invalid;

		double geo;
		double per_prim_geo_sum;
		double size;
		double combined;
		std::vector<double> per_primitive_geo_scores;

		operator double() const { return combined; }
		
		friend inline bool operator< (const PrimitiveSetRank& lhs, const PrimitiveSetRank& rhs) { return lhs.combined < rhs.combined; }
		friend inline bool operator> (const PrimitiveSetRank& lhs, const PrimitiveSetRank& rhs) { return rhs < lhs; }
		friend inline bool operator<=(const PrimitiveSetRank& lhs, const PrimitiveSetRank& rhs) { return !(lhs > rhs); }
		friend inline bool operator>=(const PrimitiveSetRank& lhs, const PrimitiveSetRank& rhs) { return !(lhs < rhs); }
		friend inline bool operator==(const PrimitiveSetRank& lhs, const PrimitiveSetRank& rhs) { return lhs.combined == rhs.combined; }
		friend inline bool operator!=(const PrimitiveSetRank& lhs, const PrimitiveSetRank& rhs) { return !(lhs == rhs); }

		PrimitiveSetRank& operator+=(const PrimitiveSetRank& rhs)
		{
			geo += rhs.geo;
			size += rhs.size;
			combined += rhs.combined;

			return *this;
		}

		PrimitiveSetRank& operator-=(const PrimitiveSetRank& rhs)
		{
			geo -= rhs.geo;
			size -= rhs.size;
			combined -= rhs.combined;

			return *this;
		}

		friend PrimitiveSetRank operator+(PrimitiveSetRank lhs, const PrimitiveSetRank& rhs)
		{
			lhs += rhs;
			return lhs;
		}

		friend PrimitiveSetRank operator-(PrimitiveSetRank lhs, const PrimitiveSetRank& rhs)
		{
			lhs -= rhs;
			return lhs;
		}
	};

	std::ostream& operator<<(std::ostream& out, const PrimitiveSetRank& r);

	struct PrimitiveSetCreator
	{
		PrimitiveSetCreator(const ManifoldSet& ms, double intraCrossProb, const std::vector<double>& mutationDistribution,
			int maxMutationIterations, int maxCrossoverIterations, int maxPrimitiveSetSize, double angleEpsilon, 
			double minDistanceBetweenParallelPlanes);

		int getRandomPrimitiveIdx(const PrimitiveSet & ps) const;

		PrimitiveSet mutate(const PrimitiveSet& ps) const;
		std::vector<PrimitiveSet> crossover(const PrimitiveSet& ps1, const PrimitiveSet& ps2) const;
		PrimitiveSet create() const;
		std::string info() const;

	private:

		enum class MutationType
		{
			NEW,
			REPLACE,
			MODIFY,
			REMOVE,
			ADD
		};

		ManifoldPtr getManifold(ManifoldType type, const Eigen::Vector3d& direction, const ManifoldSet& alreadyUsed, 
			double angleEpsilon, bool ignoreDirection = false, 
			const Eigen::Vector3d& point = Eigen::Vector3d(0,0,0), double minimumPointDistance = 0.0) const;

		ManifoldPtr getPerpendicularPlane(const std::vector<ManifoldPtr>& planes, const ManifoldSet& alreadyUsed, double angleEpsilon) const;
		ManifoldPtr getParallelPlane(const ManifoldPtr& plane, const ManifoldSet& alreadyUsed, double angleEpsilon, double minDistanceToPlanePoint) const;

		std::unordered_set<ManifoldType> getAvailableManifoldTypes(const ManifoldSet& ms) const;
		PrimitiveType getRandomPrimitiveType() const;

		Primitive createPrimitive() const;
		Primitive mutatePrimitive(const Primitive& p, double angleEpsilon) const;

		ManifoldSet ms;
		std::unordered_set<ManifoldType> availableManifoldTypes;
		double intraCrossProb;

		std::vector<double> mutationDistribution;

		int maxMutationIterations;
		int maxCrossoverIterations;
		int maxPrimitiveSetSize;
		double angleEpsilon;
		double minDistanceBetweenParallelPlanes;

		mutable std::default_random_engine rndEngine;
		mutable std::random_device rndDevice;
	};
	
	struct PrimitiveSetRanker;
	struct GAResult
	{
		PrimitiveSet primitives; 
		ManifoldSet manifolds; 
		std::shared_ptr<PrimitiveSetRanker> ranker;
	};

	struct SDFValue
	{
		static const float max_distance;

		SDFValue();
		SDFValue(float v, float w);

		float v;
		float w;
	};


	struct ModelSDF
	{
		ModelSDF(const PointCloud& pc, double voxel_size, double block_radius, double sigma_sq);

		~ModelSDF();

		double distance(const Eigen::Vector3d& p) const;
		SDFValue sdf_value(const Eigen::Vector3d& p) const;

		Mesh to_mesh() const;
		PointCloud to_pc() const;

		Eigen::Vector3i grid_size;
		Eigen::Vector3d origin;
		double voxel_size;

	private: 

		void fill_block(const Eigen::Vector3d& p, const Eigen::Vector3d& n, int block_size, float& min_w, float& max_w);

		SDFValue* data;
		Eigen::Vector3d size;

		double sigma_sq;
		int n;
	};

	struct PrimitiveSetRanker
	{
		PrimitiveSetRanker(const PointCloud& pc, const ManifoldSet& ms, const PrimitiveSet& staticPrims,
			double distanceEpsilon, int maxPrimitiveSetSize, double cell_size, const std::shared_ptr<ModelSDF>& model_sdf);

		PrimitiveSetRank rank(const PrimitiveSet& ps, bool debug = false) const;

		std::vector<double> get_per_prim_geo_score(const PrimitiveSet& ps, std::vector<Eigen::Matrix<double, 1, 6>>& points, bool debug = false) const;

		std::string info() const;

		std::shared_ptr<ModelSDF> model_sdf;
		
	private:

		double get_geo_score(const PrimitiveSet& ps) const;
	
		PrimitiveSet staticPrimitives;
		PointCloud pc;
		ManifoldSet ms;
		double distanceEpsilon;
		double cell_size;
		int maxPrimitiveSetSize;
	};

	struct PrimitiveSetPopMan
	{
		PrimitiveSetPopMan(const PrimitiveSetRanker& ranker, int maxPrimitiveSetSize,
			double geoWeight, double perPrimGeoWeight, double sizeWeight,
			bool do_elite_optimization);

		void manipulateBeforeRanking(std::vector<RankedCreature<PrimitiveSet, PrimitiveSetRank>>& population) const;
		void manipulateAfterRanking(std::vector<RankedCreature<PrimitiveSet, PrimitiveSetRank>>& population) const;
		std::string info() const;

		double geoWeight;
		double perPrimGeoWeight;
		double sizeWeight;
		bool do_elite_optimization;
		int maxPrimitiveSetSize;

		const PrimitiveSetRanker* ranker;
	};

	using PrimitiveSetTournamentSelector = TournamentSelector<RankedCreature<PrimitiveSet, PrimitiveSetRank>>;
	using PrimitiveSetIterationStopCriterion = NoFitnessIncreaseStopCriterion<RankedCreature<PrimitiveSet, PrimitiveSetRank>, PrimitiveSetRank>;
	//IterationStopCriterion<RankedCreature<PrimitiveSet, PrimitiveSetRank>>;
	using PrimitiveSetGA = GeneticAlgorithm<PrimitiveSet, PrimitiveSetCreator, PrimitiveSetRanker, PrimitiveSetRank,
		PrimitiveSetTournamentSelector, PrimitiveSetIterationStopCriterion, PrimitiveSetPopMan>;


	GAResult extractPrimitivesWithGA(const RansacResult& ransacResult, const PointCloud& full_pc);

	Primitive createBoxPrimitive(const ManifoldSet& planes);
	lmu::Primitive createSpherePrimitive(const ManifoldPtr& m);
	Primitive createCylinderPrimitive(const ManifoldPtr& m, ManifoldSet& planes);
	
	double estimateCylinderHeightFromPointCloud(const Manifold& m);
	ManifoldPtr estimateSecondCylinderPlaneFromPointCloud(const Manifold& m, const Manifold& firstPlane);

	struct OutlierDetector
	{
		OutlierDetector(const std::string& python_module_path);
		~OutlierDetector();

		PrimitiveSet remove_outliers(const PrimitiveSet& ps, const PrimitiveSetRank& psr) const;

	private:
		PyObject *od_method_name, *od_module, *od_dict, *od_method;		
	};

	CSGNode generate_tree(const GAResult& res, const lmu::PointCloud& inp_pc, double cutout_threshold, double sampling_grid_size);

}

#endif 