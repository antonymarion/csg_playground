#ifndef CSGNODE_EVO_H
#define CSGNODE_EVO_H

#include <vector>
#include <memory>

#include "csgnode.h"
#include "evolution.h"
#include "congraph.h"
#include "params.h"

#include <Eigen/Core>

namespace lmu
{
	struct ImplicitFunction;

	struct CSGNodeRanker
	{
		CSGNodeRanker(double lambda, double epsilon, double alpha, const std::vector<std::shared_ptr<lmu::ImplicitFunction>>& functions, const lmu::Graph& connectionGraph = lmu::Graph());

		double rank(const CSGNode& node) const;
		std::string info() const;

		bool treeIsInvalid(const lmu::CSGNode& node) const;

	private:

		double computeEpsilonScale();

		double _lambda;
		std::vector<std::shared_ptr<lmu::ImplicitFunction>> _functions;
		bool _earlyOutTest;
		lmu::Graph _connectionGraph;
		double _epsilonScale;
		double _epsilon;
		double _alpha;
	};

	using MappingFunction = std::function<double(double)>;

	struct CSGNodeCreator
	{
		CSGNodeCreator(const std::vector<std::shared_ptr<ImplicitFunction>>& functions, double createNewRandomProb, double subtreeProb, double simpleCrossoverProb, int maxTreeDepth, double initializeWithUnionOfAllFunctions, const lmu::CSGNodeRanker& ranker, const lmu::Graph& connectionGraph = lmu::Graph());

		CSGNode mutate(const CSGNode& tree) const;
		std::vector<CSGNode> crossover(const CSGNode& tree1, const CSGNode& tree2) const;
		CSGNode create(bool unions = true) const;
		CSGNode create(int maxDepth) const;
		std::string info() const;

	private:

		std::vector<CSGNode> simpleCrossover(const CSGNode& tree1, const CSGNode& tree2) const;
		std::vector<CSGNode> sharedPrimitiveCrossover(const CSGNode& tree1, const CSGNode& tree2) const;

		void create(CSGNode& node, int maxDepth, int curDepth) const;
		void createUnionTree(CSGNode& node, std::vector<ImplicitFunctionPtr>& funcs) const;

		int getRndFuncIndex(const std::vector<int>& usedFuncIndices) const;

		double _createNewRandomProb;
		double _subtreeProb;
		double _simpleCrossoverProb;
		double _initializeWithUnionOfAllFunctions;

		int _maxTreeDepth;
		std::vector<std::shared_ptr<ImplicitFunction>> _functions;
		mutable std::default_random_engine _rndEngine;
		mutable std::random_device _rndDevice;

		lmu::Graph _connectionGraph;
		lmu::CSGNodeRanker _ranker;
	};


	
	/*struct CSGNodeRankerNew
	{
		CSGNodeRankerNew(const lmu::Graph& graph, double sizePenaltyInfluence, double distAngleDeviationRatio, double maxSize, double maxGeo) :
			_graph(graph), _functions(lmu::getImplicitFunctions(graph), _sizePenaltyInfluence(sizePenaltyInfluence), _distAngleDeviationRatio(distAngleDeviationRatio), _maxSize(maxSize), _maxGeo(maxGeo)
		{
		}

		double rank(const CSGNode& node) const;
		std::string info() const;
		
	private:

		lmu::Graph _graph;
		std::vector<std::shared_ptr<lmu::ImplicitFunction>> _functions;
		double _sizePenaltyInfluence;
		double _distAngleDeviationRatio;
		double _maxSize;
		double _maxGeo;
	};*/

	using CSGNodeTournamentSelector = TournamentSelector<RankedCreature<CSGNode>>;

	using CSGNodeIterationStopCriterion = IterationStopCriterion<RankedCreature<CSGNode>>;
	using CSGNodeNoFitnessIncreaseStopCriterion = NoFitnessIncreaseStopCriterion<RankedCreature<CSGNode>>;

	using CSGNodeGA = GeneticAlgorithm<CSGNode, CSGNodeCreator, CSGNodeRanker, CSGNodeTournamentSelector, CSGNodeNoFitnessIncreaseStopCriterion>;

	CSGNode createCSGNodeWithGA(const std::vector<std::shared_ptr<ImplicitFunction>>& shapes, const lmu::ParameterSet& p, const lmu::Graph& connectionGraph = Graph());

	using GeometryCliqueWithCSGNode = std::tuple<Clique, CSGNode>;

	enum class ParallelismOptions
	{
		NoParallelism = 0,
		PerCliqueParallelism = 1, 
		GAParallelism = 2
	};
	ParallelismOptions operator|(ParallelismOptions lhs, ParallelismOptions rhs);
	ParallelismOptions operator&(ParallelismOptions lhs, ParallelismOptions rhs);

	std::vector<GeometryCliqueWithCSGNode> computeNodesForCliques(const std::vector<Clique>& geometryCliques, const ParameterSet& params, ParallelismOptions po);

	using CSGNodeClique = std::vector<GeometryCliqueWithCSGNode>;

	CSGNode mergeCSGNodeCliqueSimple(CSGNodeClique& clique);
	void optimizeCSGNodeClique(CSGNodeClique& clique, float tolerance);
  
	double lambdaBasedOnPoints(const std::vector<lmu::ImplicitFunctionPtr>& shapes);
	
    CSGNode computeGAWithPartitions(const std::vector<Graph>& partitions, const lmu::ParameterSet& p);
}

#endif