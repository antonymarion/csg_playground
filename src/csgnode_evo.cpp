#include "../include/csgnode_evo.h"
#include "../include/csgnode_helper.h"
#include "../include/dnf.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <boost/dynamic_bitset.hpp>
#include <boost/graph/adjacency_list.hpp>

#include "../include/constants.h"

using namespace lmu;

CSGNode computeForTwoFunctions(const std::vector<ImplicitFunctionPtr>& functions, const ParameterSet& params);
CSGNode computeForTwoFunctions(const std::vector<ImplicitFunctionPtr>& functions, const lmu::CSGNodeRanker& ranker);


lmu::CSGNodeRanker::CSGNodeRanker(double lambda, double epsilon, double alpha, double h, const std::vector<std::shared_ptr<lmu::ImplicitFunction>>& functions, const lmu::Graph& connectionGraph) :
	_lambda(lambda),
	_epsilon(epsilon),
	_alpha(alpha),
	_h(h),
	_functions(functions),
	_earlyOutTest(!connectionGraph.structure.m_vertices.empty()),
	_connectionGraph(connectionGraph),
	_epsilonScale(computeEpsilonScale())
{
}

double lmu::CSGNodeRanker::computeEpsilonScale()
{
	const double minVal = -std::numeric_limits<double>::max(); 
	const double maxVal = std::numeric_limits<double>::max();
	
	Eigen::Vector3d min(maxVal, maxVal, maxVal);
	Eigen::Vector3d max(minVal, minVal, minVal);

	for (const auto& f : _functions)
	{
		auto curMin = f->points().colwise().minCoeff();
		auto curMax = f->points().colwise().maxCoeff();

		min.x() = curMin.x() < min.x() ? curMin.x() : min.x(); 
		min.y() = curMin.y() < min.y() ? curMin.y() : min.y();
		min.z() = curMin.z() < min.z() ? curMin.z() : min.z();

		max.x() = curMax.x() > max.x() ? curMax.x() : max.x();
		max.y() = curMax.y() > max.y() ? curMax.y() : max.y();
		max.z() = curMax.z() > max.z() ? curMax.z() : max.z();
	}

	return (max - min).norm();
}

double lmu::CSGNodeRanker::rank(const lmu::CSGNode& node) const
{	
	return rank(node, _functions);
}

double lmu::CSGNodeRanker::rank(const lmu::CSGNode& node, const std::vector<std::shared_ptr<lmu::ImplicitFunction>>& functions) const
{
	double geometryScore = computeGeometryScore(node, _epsilon * _epsilonScale, _alpha, _h, functions);

	double score = geometryScore - _lambda * numNodes(node);
	
	return score;
}

std::string lmu::CSGNodeRanker::info() const
{
	std::stringstream ss;
	ss << "CSGNode Ranker (lambda: " << _lambda << ", early out test: " << _earlyOutTest << ")";
	return ss.str();
}

boost::dynamic_bitset<> getFunctionConnectionBitfield(const std::shared_ptr<lmu::ImplicitFunction>& func, const lmu::Graph& connectionGraph, const std::unordered_map<std::shared_ptr<lmu::ImplicitFunction>, int>& funcToIdx, int bitfieldSize)
{
	boost::dynamic_bitset<> bf(bitfieldSize);

	//Go through all adjacent nodes of the node holding func and mark them in the bitfield as connected.
	boost::graph_traits<GraphStructure>::adjacency_iterator  neighbour, neighbourEnd;
	for (boost::tie(neighbour, neighbourEnd) = boost::adjacent_vertices(connectionGraph.vertexLookup.at(func), connectionGraph.structure); neighbour != neighbourEnd; ++neighbour)	
		bf.set(funcToIdx.at(connectionGraph.structure[*neighbour]), true);	

	return bf;
}

bool treeIsInvalidRec(const lmu::CSGNode& node, boost::dynamic_bitset<>& bf, const Graph& connectionGraph, const std::unordered_map<std::shared_ptr<lmu::ImplicitFunction>, int>& funcToIdx)
{
	auto numAllowedChilds = node.numAllowedChilds();
	if (node.childs().size() < std::get<0>(numAllowedChilds) || node.childs().size() > std::get<1>(numAllowedChilds))
		return true;

	if (node.type() == CSGNodeType::Operation)
	{	
		boost::dynamic_bitset<> lastBF(bf.size());
		static boost::dynamic_bitset<> emptyBF(bf.size());

		bool firstRun = true; 

		for (const auto& child : node.childsCRef())
		{
			boost::dynamic_bitset<> childBF(bf.size());

			if (treeIsInvalidRec(child, childBF, connectionGraph, funcToIdx))
				return true;
						
			if ((childBF & lastBF) == emptyBF && !firstRun)
				return true; 

			firstRun = false;
			lastBF = childBF;		

			bf |= lastBF;
		}		
	}
	else
	{
		bf = getFunctionConnectionBitfield(node.function(), connectionGraph, funcToIdx, bf.size());
	}

	return false;
}

bool lmu::CSGNodeRanker::treeIsInvalid(const lmu::CSGNode& node) const
{
	//Check if all functions were used in the tree.
	std::unordered_set<std::shared_ptr<lmu::ImplicitFunction>> usedFuncs; 
	for (auto const& func : allGeometryNodePtrs(node))
		usedFuncs.insert(func->function());

	if (usedFuncs.size() != _functions.size())
		return true;

	//Check if subtrees have overlapping functions sets.
	boost::dynamic_bitset<> bf(_functions.size());
	std::unordered_map<std::shared_ptr<lmu::ImplicitFunction>, int> funcToIdx;
	for (int i = 0; i < _functions.size(); ++i)
		funcToIdx[_functions[i]] = i;

	return treeIsInvalidRec(node, bf, _connectionGraph, funcToIdx);
}

CSGNodeCreator::CSGNodeCreator(const std::vector<std::shared_ptr<ImplicitFunction>>& functions, double createNewRandomProb, double subtreeProb, double simpleCrossoverProb, int maxTreeDepth, double initializeWithUnionOfAllFunctions, const lmu::CSGNodeRanker& ranker, const Graph& connectionGraph) :
	_functions(functions),
	_createNewRandomProb(createNewRandomProb),
	_subtreeProb(subtreeProb),
	_simpleCrossoverProb(simpleCrossoverProb),
	_maxTreeDepth(maxTreeDepth),
	_initializeWithUnionOfAllFunctions(initializeWithUnionOfAllFunctions),
	_ranker(ranker),
	_connectionGraph(connectionGraph)
{
	_rndEngine.seed(_rndDevice());
}

CSGNode CSGNodeCreator::mutate(const CSGNode& node) const
{
	static std::bernoulli_distribution d{};
	using parm_t = decltype(d)::param_type;

	static std::uniform_int_distribution<> du{};
	using parmu_t = decltype(du)::param_type;

	static std::uniform_real_distribution<double> dur(-0.1, 0.1);
	using parmur_t = decltype(dur)::param_type;
	
	//_createNewRandomProb (my_0) 
	if (d(_rndEngine, parm_t{ _createNewRandomProb }))
		return create(_maxTreeDepth);

	int nodeIdx = du(_rndEngine, parmu_t{ 0, numNodes(node) - 1 });

	std::cout << "Mutation at " << nodeIdx << std::endl;

	auto newNode = node;

	CSGNode* subNode = nodePtrAt(newNode, nodeIdx);
	
	create(*subNode, _maxTreeDepth, 0);

	return newNode;
}

std::vector<lmu::CSGNode> lmu::CSGNodeCreator::crossover(const CSGNode& node1, const CSGNode& node2) const
{
	static std::bernoulli_distribution db{};
	using parmb_t = decltype(db)::param_type;

	std::cout << "Crossover" << std::endl;

	if (db(_rndEngine, parmb_t{ _simpleCrossoverProb }))
	{
		return simpleCrossover(node1, node2);
	}
	else
	{
		return sharedPrimitiveCrossover(node1, node2);
	}
}

std::vector<lmu::CSGNode> lmu::CSGNodeCreator::simpleCrossover(const CSGNode & node1, const CSGNode & node2) const
{
	if (!node1.isValid() || !node2.isValid())
		return std::vector<lmu::CSGNode> {node1, node2};

	int numNodes1 = numNodes(node1);
	int numNodes2 = numNodes(node2);

	auto newNode1 = node1;
	auto newNode2 = node2;

	static std::uniform_int_distribution<> du{};
	using parmu_t = decltype(du)::param_type;

	int nodeIdx1 = du(_rndEngine, parmu_t{ 0, numNodes1 - 1 });
	int nodeIdx2 = du(_rndEngine, parmu_t{ 0, numNodes2 - 1 });

	CSGNode* subNode1 = nodePtrAt(newNode1, nodeIdx1);
	CSGNode* subNode2 = nodePtrAt(newNode2, nodeIdx2);

	std::swap(*subNode1, *subNode2);

	return std::vector<lmu::CSGNode>
	{
		newNode1, newNode2
	};
}

std::vector<lmu::CSGNode> lmu::CSGNodeCreator::sharedPrimitiveCrossover(const CSGNode& node1, const CSGNode& node2) const
{
	if (!node1.isValid() || !node2.isValid())
		return std::vector<lmu::CSGNode> {node1, node2};

	auto newNode1 = node1;
	auto newNode2 = node2;

	static std::uniform_int_distribution<> du{};
	using parmu_t = decltype(du)::param_type;

	int nodeIdx1 = du(_rndEngine, parmu_t{ 0, numNodes(newNode1) - 1 });
	
	CSGNode* subNode1 = nodePtrAt(newNode1, nodeIdx1);	

	auto allDistinctFunctions = lmu::allDistinctFunctions(*subNode1);

	CSGNode* subNode2 = findSmallestSubgraphWithImplicitFunctions(newNode2, allDistinctFunctions);

	if (!subNode2)
		return std::vector<lmu::CSGNode> {newNode1, newNode2};

	double score1 = _ranker.rank(*subNode1, allDistinctFunctions);
	double score2 = _ranker.rank(*subNode2, allDistinctFunctions);

	std::cout << "CROSSOVER" << std::endl;
	//std::cout << serializeNode(*subNode1) << "     " << serializeNode(*subNode2) << std::endl;
	std::cout << score1 << "     " << score2 << std::endl;
	
	if (score1 > score2)
		*subNode2 = *subNode1;
	else if (score1 < score2)
		*subNode1 = *subNode2;
	else //If both scores are equal, do normal crossover.
	{
		std::cout << "NORMAL" << std::endl;
		int nodeIdx2 = du(_rndEngine, parmu_t{ 0, numNodes(newNode2) - 1 });
		subNode2 = nodePtrAt(newNode2, nodeIdx2);
		std::swap(*subNode1, *subNode2);
	}

	return std::vector<lmu::CSGNode>{ newNode1, newNode2};
}

lmu::CSGNode lmu::CSGNodeCreator::create(bool unions) const
{
	static std::bernoulli_distribution db{};
	using parmb_t = decltype(db)::param_type;

	if (!unions || !db(_rndEngine, parmb_t{ _initializeWithUnionOfAllFunctions }))
	{
		return create(_maxTreeDepth);
	}
	else
	{
		auto node = opUnion();
		auto funcs = _functions;//lmu::getImplicitFunctions(_connectionGraph);
		createUnionTree(node, funcs);
		return node;
	}
}

void lmu::CSGNodeCreator::createUnionTree(CSGNode& node, std::vector<ImplicitFunctionPtr>& funcs) const
{
	static std::uniform_int_distribution<> du{};
	using parmu_t = decltype(du)::param_type;

	if (funcs.size() == 0)
	{
		node = CSGNode::invalidNode;
	}
	else if (funcs.size() == 1)
	{	
		node = lmu::geometry(funcs[0]);
		funcs.clear();
	}
	else
	{
		int funcIdx = du(_rndEngine, parmu_t{ 0, static_cast<int>(funcs.size() - 1) });
		node.addChild(lmu::geometry(funcs[funcIdx]));
		
		funcs.erase(funcs.begin() + funcIdx);

		CSGNode child = opUnion();
		createUnionTree(child, funcs);
		if (child.isValid())
		{
			node.addChild(child);
		}
		else
		{
			node = node.childsCRef()[0];
		}
	}
}

lmu::CSGNode lmu::CSGNodeCreator::create(int maxDepth) const
{
	auto node = CSGNode::invalidNode;
	create(node, maxDepth, 0);
	return node;
}

void lmu::CSGNodeCreator::create(lmu::CSGNode& node, int maxDepth, int curDepth) const
{
	static std::bernoulli_distribution db{};
	using parmb_t = decltype(db)::param_type;

	static std::uniform_int_distribution<> du{};
	using parmu_t = decltype(du)::param_type;

	static std::uniform_real_distribution<double> dur(0, 1);
	using parmur_t = decltype(dur)::param_type;

	if (curDepth >= maxDepth)
	{		
		int funcIdx = du(_rndEngine, parmu_t{ 0, static_cast<int>(_functions.size() - 1) });
		node = geometry(_functions[funcIdx]);				
	}
	else
	{
		if (db(_rndEngine, parmb_t{ _subtreeProb }))
		{
			std::discrete_distribution<> d({ 1, 1, 1 });
			int op = d(_rndEngine) + 1; //0 is OperationType::Unknown, 6 is OperationType::Invalid.

			node = createOperation(static_cast<CSGNodeOperationType>(op));

			auto numAllowedChilds = node.numAllowedChilds();
			int numChilds = clamp(std::get<1>(numAllowedChilds), std::get<0>(numAllowedChilds), 2); //2 is the maximum number of childs allowed for create

			for (int i = 0; i < numChilds; ++i)
			{
				auto child = CSGNode::invalidNode;
				create(child, maxDepth, curDepth + 1);
				node.addChild(child);
			}
		}
		else 
		{
			int funcIdx = du(_rndEngine, parmu_t{ 0, static_cast<int>(_functions.size() - 1) });
			node = geometry(_functions[funcIdx]);
		}
	}
}

std::string lmu::CSGNodeCreator::info() const
{
	std::stringstream ss;
	ss << "CSGTree Creator (create new random prob: " << _createNewRandomProb << ", sub tree prob: " << _subtreeProb << ", max tree depth: " << _maxTreeDepth << ")";
	return ss.str();
}


size_t functionHash(const std::vector<lmu::ImplicitFunctionPtr>& funcs)
{
	size_t seed = 0; 
	for(const auto& func : funcs)
		boost::hash_combine(seed, func);
	return seed;
}

lmu::CSGNodeOptimization lmu::optimizationTypeFromString(std::string type)
{
	std::transform(type.begin(), type.end(), type.begin(), ::tolower);

	if (type == "random")
		return CSGNodeOptimization::RANDOM;
	else if (type == "traverse")
		return CSGNodeOptimization::TRAVERSE;

	return CSGNodeOptimization::TRAVERSE;
}

lmu::CSGNodePopMan::CSGNodePopMan(double optimizationProb, double preOptimizationProb, int maxFunctions, int nodeSelectionTries, int randomIterations, CSGNodeOptimization type, const lmu::CSGNodeRanker& ranker, const lmu::Graph& connectionGraph) :
	_optimizationProb(optimizationProb),
	_preOptimizationProb(preOptimizationProb),
	_maxFunctions(maxFunctions),
	_nodeSelectionTries(nodeSelectionTries),
	_randomIterations(randomIterations),
	_type(type),
	_ranker(ranker),
	_connectionGraph(connectionGraph)
{
	_rndEngine.seed(_rndDevice());
}

CSGNode lmu::CSGNodePopMan::getOptimizedTree(std::vector<ImplicitFunctionPtr> funcs) const
{
	std::sort(funcs.begin(), funcs.end(), [](ImplicitFunctionPtr f1, ImplicitFunctionPtr f2) { return std::less<>()(f1.get(), f2.get()); });

	size_t hash = 0;
	for (const auto& func : funcs)
		boost::hash_combine(hash, func);
	
	auto cachedNode = _nodeLookup.find(hash);
	if (cachedNode == _nodeLookup.end())
	{
		auto node = CSGNode::invalidNode;

		if (funcs.size() == 1)
		{
			node = geometry(funcs[0]);		
		}
		else if (funcs.size() == 2)
		{
			node = computeForTwoFunctions(funcs, _ranker);
		}
		else if(funcs.size() > 2)
		{
			auto dnf = lmu::computeShapiro(funcs, true, _connectionGraph, { 0.001 });
			node = lmu::DNFtoCSGNode(dnf);
			convertToTreeWithMaxNChilds(node, 2);
		}
		
		_nodeLookup.insert(std::make_pair(hash, node));

		return node;
	}
	else
	{
		return cachedNode->second;
	}
}

std::vector<ImplicitFunctionPtr> lmu::CSGNodePopMan::getSuitableFunctions(const std::vector<ImplicitFunctionPtr>& funcs) const
{
	static std::uniform_int_distribution<> du{};
	using parmu_t = decltype(du)::param_type;
	
	if (funcs.size() == 2)
	{
		//if functions are not connected, search for a connected one.
		if (!lmu::areConnected(_connectionGraph, funcs[0], funcs[1]))
		{
			int funcIdx = du(_rndEngine, parmu_t{ 0, 1 });

			auto neighbors = lmu::getConnectedImplicitFunctions(_connectionGraph, funcs[funcIdx]);

			int newFuncIdx = du(_rndEngine, parmu_t{ 0, (int)neighbors.size() - 1 });

			auto res = funcs;
			res[funcIdx == 1 ? 0 : 1] = neighbors[newFuncIdx];
	
			return res;
		}
	}

	return funcs;
}

void lmu::CSGNodePopMan::manipulateAfterRanking(std::vector<RankedCreature<CSGNode>>& population) const
{
}

void lmu::CSGNodePopMan::manipulateBeforeRanking(std::vector<RankedCreature<CSGNode>>& population) const
{
	static std::bernoulli_distribution db{};
	using parmb_t = decltype(db)::param_type;

	static std::uniform_int_distribution<> du{};
	using parmu_t = decltype(du)::param_type;
		
	for (int i = 0; i < population.size(); ++i)
	{
		if (db(_rndEngine, parmb_t{_preOptimizationProb }))
		{
			auto& node = population[i].creature;
			int numOptimizations = optimizeCSGNodeStructure(node);
		}
		
		if (db(_rndEngine, parmb_t{ _optimizationProb }))
		{
					
			auto& node = population[i].creature;

			switch (_type)
			{
			case CSGNodeOptimization::TRAVERSE:

				lmu::visit(node, [this](CSGNode& n)
				{
					auto& childs = n.childsRef();
					if (childs.size() == 2 && childs[0].type() == CSGNodeType::Geometry && childs[1].type() == CSGNodeType::Geometry)
					{
						std::vector<ImplicitFunctionPtr> funcs = getSuitableFunctions({ childs[0].function(), childs[1].function() });
						n = getOptimizedTree(funcs);
					}
				});

				break;

			case CSGNodeOptimization::RANDOM:

				for (int iter = 0; iter < _randomIterations; ++iter)
				{
					for (int tries = 0; tries < _nodeSelectionTries; ++tries)
					{
						int subNodeIdx = du(_rndEngine, parmu_t{ 0, numNodes(node) - 1 });
						CSGNode* subNode = nodePtrAt(node, subNodeIdx);
						auto funcs = getSuitableFunctions(lmu::allDistinctFunctions(*subNode));

						if (funcs.size() < _maxFunctions)
						{
							*subNode = getOptimizedTree(funcs);
							break;
						}
					}
				}

				break;
			}
		}
	}
}

std::string lmu::CSGNodePopMan::info() const
{
	return "Standard Manipulator";
}

double lmu::lambdaBasedOnPoints(const std::vector<lmu::ImplicitFunctionPtr>& shapes)
{
int numPoints = 0;
for (const auto& shape : shapes)
numPoints += shape->points().rows();

return std::log(numPoints);
}

long long binom(int n, int k)
{
	long long ans = 1;
	k = k > n - k ? n - k : k;
	int j = 1;
	for (; j <= k; j++, n--)
	{
		if (n%j == 0)
		{
			ans *= n / j;
		}
		else
			if (ans%j == 0)
			{
				ans = ans / j*n;
			}
			else
			{
				ans = (ans*n) / j;
			}
	}
	return ans;
}

lmu::CSGNode lmu::createCSGNodeWithGA(const std::vector<std::shared_ptr<ImplicitFunction>>& shapes, const ParameterSet& p, const lmu::Graph& connectionGraph)
{
	bool inParallel = p.getBool("GA", "InParallel", false);
	bool useCaching = p.getBool("GA", "UseCaching", false);

	int popSize = p.getInt("GA", "PopulationSize", 150);
	int numBestParents = p.getInt("GA", "NumBestParents", 2);
	double mutation = p.getDouble("GA", "MutationRate", 0.3);
	double crossover = p.getDouble("GA", "CrossoverRate", 0.4);
	double simpleCrossoverProb = p.getDouble("GA", "SimpleCrossoverRate", 1.0);
	bool initializeWithUnionOfAllFunctions = p.getBool("GA", "InitializeWithUnionOfAllFunctions", false);
	ScheduleType crossScheduleType = scheduleTypeFromString(p.getStr("GA", "CrossoverScheduleType", "identity"));
	ScheduleType mutationScheduleType = scheduleTypeFromString(p.getStr("GA", "MutationScheduleType", "identity"));

	int k = p.getInt("Selection", "TournamentK", 2);
	
	int maxIter = p.getInt("StopCriterion", "MaxIterations", 500);
	int maxIterWithoutChange = p.getInt("StopCriterion", "MaxIterationsWithoutChange", 200);
	double changeDelta = p.getDouble("StopCriterion", "ChangeDelta", 0.01);

	std::string statsFile = p.getStr("Statistics", "File", "stats.dat");

	int maxTreeDepth = p.getInt("Creation", "MaxTreeDepth", 10);
	double createNewRandomProb = p.getDouble("Creation", "CreateNewRandomProb", 0.5);
	double subtreeProb = p.getDouble("Creation", "SubtreeProb", 0.7);

	double alpha = p.getDouble("Ranking", "Alpha", (M_PI / 180.0) * 35.0);
	double epsilon = p.getDouble("Ranking", "Epsilon", 0.01);

	int nodeSelectionTries = p.getInt("Optimization", "NodeSelectionTries", 10); 
	int maxFunctions = p.getInt("Optimization", "MaxFunctions", 4);
	double optimizationProb = p.getDouble("Optimization", "OptimizationProb", 0.0);
	double preOptimizationProb = p.getDouble("Optimization", "PreOptimizationProb", 0.0);
	CSGNodeOptimization optimizationType = optimizationTypeFromString(p.getStr("Optimization", "OptimizationType", "traverse"));
	int randomIterations = p.getInt("Optimization", "RandomIterations", 1);

	double gradientStepSize = p.getDouble("Sampling", "GradientStepSize", 0.001);

	if (shapes.size() == 1)
		return lmu::geometry(shapes[0]);

	lmu::CSGNodeGA ga;
	lmu::CSGNodeGA::Parameters params(popSize, numBestParents, mutation, crossover, inParallel, Schedule(crossScheduleType), Schedule(mutationScheduleType), useCaching);

	lmu::CSGNodeTournamentSelector s(k, true);
	
	lmu::CSGNodeNoFitnessIncreaseStopCriterion isc(maxIterWithoutChange, changeDelta, maxIter);

	double lambda = lambdaBasedOnPoints(shapes);
	std::cout << "lambda: " << lambda << std::endl;

	lmu::CSGNodeRanker r(lambda, epsilon, alpha, gradientStepSize, shapes, connectionGraph);

	lmu::CSGNodeCreator c(shapes, createNewRandomProb, subtreeProb, simpleCrossoverProb, maxTreeDepth, initializeWithUnionOfAllFunctions, r, connectionGraph);

	lmu::CSGNodePopMan popMan(optimizationProb, preOptimizationProb, maxFunctions, nodeSelectionTries, randomIterations, optimizationType, r, connectionGraph);

	auto task = ga.runAsync(params, s, c, r, isc, popMan);

	int i;
	std::cout << "Press a Key and Enter to break." << std::endl;
	std::cin >>  i;

	ga.stop();

	auto res = task.get(); // ga.run(params, s, c, r, isc);// task.get();

	res.statistics.save(statsFile, &res.population[0].creature);
	return res.population[0].creature;
}

CSGNode computeForTwoFunctions(const std::vector<ImplicitFunctionPtr>& functions, const lmu::CSGNodeRanker& ranker)
{
	std::vector<CSGNode> candidates;

	CSGNode un(std::make_shared<UnionOperation>("un"));
	un.addChild(CSGNode(std::make_shared<CSGNodeGeometry>(functions[0])));
	un.addChild(CSGNode(std::make_shared<CSGNodeGeometry>(functions[1])));
	candidates.push_back(un);

	CSGNode inter(std::make_shared<IntersectionOperation>("inter"));
	inter.addChild(CSGNode(std::make_shared<CSGNodeGeometry>(functions[0])));
	inter.addChild(CSGNode(std::make_shared<CSGNodeGeometry>(functions[1])));
	candidates.push_back(inter);

	CSGNode lr(std::make_shared<DifferenceOperation>("lr"));
	lr.addChild(CSGNode(std::make_shared<CSGNodeGeometry>(functions[0])));
	lr.addChild(CSGNode(std::make_shared<CSGNodeGeometry>(functions[1])));
	candidates.push_back(lr);

	CSGNode rl(std::make_shared<DifferenceOperation>("rl"));
	rl.addChild(CSGNode(std::make_shared<CSGNodeGeometry>(functions[1])));
	rl.addChild(CSGNode(std::make_shared<CSGNodeGeometry>(functions[0])));
	candidates.push_back(rl);

	double maxScore = -std::numeric_limits<double>::max();
	const CSGNode* bestCandidate = nullptr;
	for (const auto& candidate : candidates)
	{
		double curScore = ranker.rank(candidate);

		if (maxScore < curScore)
		{
			maxScore = curScore;
			bestCandidate = &candidate;
		}
	}

	return *bestCandidate;
}

CSGNode computeForTwoFunctions(const std::vector<ImplicitFunctionPtr>& functions, const ParameterSet& params)
{
	double alpha = params.getDouble("Ranking", "Alpha", (M_PI / 180.0) * 35.0);
	double epsilon = params.getDouble("Ranking", "Epsilon", 0.01);
	double gradientStepSize = params.getDouble("Sampling", "GradientStepSize", 0.001);

	lmu::CSGNodeRanker ranker(lambdaBasedOnPoints(functions), epsilon, alpha, gradientStepSize, functions);

	return computeForTwoFunctions(functions, ranker);
}

// Mimic computeShapiroWithPartitions in dnf.cpp
// Apply a GA to each group of intersecting shapes
lmu::CSGNode
lmu::computeGAWithPartitions
(const std::vector<Graph>& partitions, const lmu::ParameterSet& params)
{
	lmu::CSGNode res = lmu::op<Union>();

	//for (const auto& pi: get<1>(partition)) {
	//  res.addChild(lmu::geometry(pi));
	//}

	for (const auto& p : partitions)
	{
		std::vector<std::shared_ptr<ImplicitFunction>> shapes = lmu::getImplicitFunctions(p);

		lmu::CSGNode partRes(nullptr);
		if (shapes.size() == 1)
		{
			partRes = geometry(shapes.front());
		}
		else if (shapes.size() == 2)
		{
			partRes = computeForTwoFunctions(shapes, params);
		}
		else
		{
			partRes = lmu::createCSGNodeWithGA(shapes, params, p);
		}
    
		if (partitions.size() == 1)
			return partRes;

		res.addChild(partRes);
  }

  return res;
}


std::tuple<long long, double> computeNodesForClique(const Clique& clique, const ParameterSet& params, std::vector<GeometryCliqueWithCSGNode>& res)
{
	TimeTicker ticker; 
	double score = 0.0;

	double alpha = params.getDouble("Ranking", "Alpha", (M_PI / 180.0) * 35.0);
	double epsilon = params.getDouble("Ranking", "Epsilon", 0.01);
	double gradientStepSize = params.getDouble("Sampling", "GradientStepSize", 0.001);

	if (clique.functions.empty())
	{
		return std::make_tuple(0, 0.0);
	}
	else if (clique.functions.size() == 1)
	{
		res.push_back(std::make_tuple(clique, CSGNode(std::make_shared<CSGNodeGeometry>(clique.functions[0]))));		
	}
	else if (clique.functions.size() == 2)
	{
		lmu::CSGNodeRanker ranker(lambdaBasedOnPoints(clique.functions), epsilon, alpha, gradientStepSize, clique.functions);
		
		std::vector<CSGNode> candidates;

		CSGNode un(std::make_shared<UnionOperation>("un"));
		un.addChild(CSGNode(std::make_shared<CSGNodeGeometry>(clique.functions[0])));
		un.addChild(CSGNode(std::make_shared<CSGNodeGeometry>(clique.functions[1])));
		candidates.push_back(un);

		CSGNode inter(std::make_shared<IntersectionOperation>("inter"));
		inter.addChild(CSGNode(std::make_shared<CSGNodeGeometry>(clique.functions[0])));
		inter.addChild(CSGNode(std::make_shared<CSGNodeGeometry>(clique.functions[1])));
		candidates.push_back(inter);

		CSGNode lr(std::make_shared<DifferenceOperation>("lr"));
		lr.addChild(CSGNode(std::make_shared<CSGNodeGeometry>(clique.functions[0])));
		lr.addChild(CSGNode(std::make_shared<CSGNodeGeometry>(clique.functions[1])));
		candidates.push_back(lr);

		CSGNode rl(std::make_shared<DifferenceOperation>("rl"));
		rl.addChild(CSGNode(std::make_shared<CSGNodeGeometry>(clique.functions[1])));
		rl.addChild(CSGNode(std::make_shared<CSGNodeGeometry>(clique.functions[0])));
		candidates.push_back(rl);

		double maxScore = -std::numeric_limits<double>::max();
		const CSGNode* bestCandidate = nullptr;
		for (const auto& candidate : candidates)
		{
			double curScore = ranker.rank(candidate);
			std::cout << candidate.name() << " for " << clique.functions[0]->name() << " " << clique.functions[1]->name() << " rank: " << curScore /*<< " tree rank: " << ranker2.rank( trees[i++]) */ << std::endl;

			if (maxScore < curScore)
			{
				maxScore = curScore;
				bestCandidate = &candidate;
			}
		}

		score = maxScore;
		res.push_back(std::make_tuple(clique, *bestCandidate));
	}
	else
	{
		std::stringstream ss; 
		ss << clique << ".dat";
		score = 666.0;

		res.push_back(std::make_tuple(clique, createCSGNodeWithGA(clique.functions, params, Graph())));
	}

	return std::make_tuple(ticker.tick(), score);
}

ParallelismOptions lmu::operator|(ParallelismOptions lhs, ParallelismOptions rhs)
{
	return static_cast<ParallelismOptions>(static_cast<int>(lhs) | static_cast<int>(rhs));
}
ParallelismOptions lmu::operator&(ParallelismOptions lhs, ParallelismOptions rhs)
{
	return static_cast<ParallelismOptions>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

std::vector<GeometryCliqueWithCSGNode> lmu::computeNodesForCliques(const std::vector<Clique>& geometryCliques, const ParameterSet& params, ParallelismOptions po)
{
	std::vector<GeometryCliqueWithCSGNode> res;

	bool cliquesParallel = (po & ParallelismOptions::PerCliqueParallelism) == ParallelismOptions::PerCliqueParallelism;

	//NOTE: this has no effect anymore since param delivers ga parallism yes/no
	bool gAParallel = (po & ParallelismOptions::GAParallelism) == ParallelismOptions::GAParallelism;

	std::ofstream f("clique_info.dat");
	
	if (cliquesParallel)
	{
#ifndef _OPENMP 
		throw std::runtime_error("Cliques should run in parallel but OpenMP is not available.");
#endif

#pragma omp parallel
		{
#pragma omp master  
			{
				f << "OpenMP is running with " << omp_get_num_threads() << " threads." << std::endl;
			}
#pragma omp for
			for (int i = 0; i < geometryCliques.size(); ++i)
			{
				f << "Clique " << (i + 1) << " of " << geometryCliques.size() << " is started" << geometryCliques[i] << std::endl;
							
				auto stats = computeNodesForClique(geometryCliques[i], params, res);
				f << "Timing: " << std::get<0>(stats) << " Score: " << std::get<1>(stats) << std::endl;

				f << geometryCliques[i] << " done." << std::endl;
			}
		}
	}
	else
	{
		int i = 1;
		for (const auto& clique : geometryCliques)
		{
			f << "Clique " << (i++) << " of " << geometryCliques.size() << " is started: " << clique << std::endl;

			auto stats = computeNodesForClique(clique, params, res);
			f << "Timing: " << std::get<0>(stats) << " Score: " << std::get<1>(stats) << std::endl;

			f << clique << " done." << std::endl;
		}
	}

	f.close();

	return res;
}

size_t getHash(const CSGNode* n1, const CSGNode* n2)
{
	std::size_t seed = 0;
	boost::hash_combine(seed, reinterpret_cast<std::uintptr_t>(n1));
	boost::hash_combine(seed, reinterpret_cast<std::uintptr_t>(n2));
	
	return seed;
}

void lmu::optimizeCSGNodeClique(CSGNodeClique& clique, float tolerance)
{
	for (auto& item : clique)
	{
		optimizeCSGNodeStructure(std::get<1>(item));// , tolerance);
	}
}

CSGNode lmu::mergeCSGNodeCliqueSimple(CSGNodeClique& clique)
{
	if (clique.empty())
		throw std::runtime_error("Cannot merge empty clique.");

	if (clique.size() == 1)
		return std::get<1>(clique.front());

	//Fill candidate list. 
	std::list<CSGNode*> candidateList;	
	for (auto& item : clique)	
		candidateList.push_back(&std::get<1>(item));	

	bool allowIntersections = false;

	while (candidateList.front() != candidateList.back())
	{
		auto n1 = candidateList.front(); 
		candidateList.pop_front();

		auto n2 = candidateList.front();
		candidateList.pop_front();

		std::cout << "Took two new nodes" << std::endl;

		auto firstN2 = n2;
		while (true)
		{
			std::cout << "Find css" << std::endl;

			auto css = findCommonSubgraphs(*n1, *n2);

			std::cout << "Found css" << std::endl;

			CSGNode* mergedNode = nullptr;
			for (const auto& cs : css)
			{
				std::cout << "Write nodes" << std::endl;

				writeNode(*n1, "n1.dot");
				writeNode(*n2, "n2.dot");

				std::cout << "Wrote nodes" << std::endl;

				std::cout << "Merge nodes" << std::endl;

				switch (mergeNodes(cs, allowIntersections))
				{
				case MergeResult::First:
					std::cout << "Merged with n1" << std::endl;
					mergedNode = n1;
					break;
				case MergeResult::Second:
					std::cout << "Merged with n2" << std::endl;
					mergedNode = n2;
					break;
				case MergeResult::None:
					std::cout << "Not merged" << std::endl;			
					break;
				}
				
				if (mergedNode)
					break;
			}

			std::cout << "Merged nodes" << std::endl;

			if (mergedNode)
			{
				std::cout << "Merge node available" << std::endl;

				candidateList.push_front(mergedNode);
				allowIntersections = false;
				break;
			}
			else
			{
				std::cout << "Merge node not available" << std::endl;

				candidateList.push_back(n2);
				auto n2 = candidateList.front();
				candidateList.pop_front();

				if (n2 == firstN2)
				{
					if (allowIntersections)
					{
						std::cout << "could not merge n1 with any other node - n1 is ignored now." << std::endl;
						break;
					}
					else
					{
						std::cout << "Try to merge now with intersections allowed." << std::endl;
						allowIntersections = true;
					}					
				}
			}
		}
	}

	std::cout << "Candidate list: " << candidateList.size() << std::endl;

	return *candidateList.front();
}

double computeGeometryScore(const CSGNode& node, double distAngleDeviationRatio, double maxDistance, const std::vector<std::shared_ptr<lmu::ImplicitFunction>>& funcs)
{
	//std::cout << "Compute Geometry Score" << std::endl;

	double score = 0.0;
	for (const auto& func : funcs)
	{
		for (int i = 0; i < func->points().rows(); ++i)
		{
			auto row = func->points().row(i);

			Eigen::Vector3d p = row.head<3>();
			Eigen::Vector3d n = row.tail<3>();

			Eigen::Vector4d distAndGrad = node.signedDistanceAndGradient(p);

			double distance = lmu::clamp(distAndGrad[0] / maxDistance, 0.0, 1.0); //distance in [0,1]

			Eigen::Vector3d grad = distAndGrad.tail<3>();
			double gradientDotN = lmu::clamp(/*-*/grad.dot(n), -1.0, 1.0); //clamp is necessary, acos is only defined in [-1,1].			

			double theta = std::acos(gradientDotN) / M_PI; //theta in [0,1]

			//double scoreDelta = (std::exp(-(d*d)) + std::exp(-(theta*theta)));

			//if (scoreDelta < 0)
			//	std::cout << "Theta: " << theta << " minusGradientDotN: " << minusGradientDotN << std::endl;
			

			score += (1.0 - distAngleDeviationRatio) * distance + distAngleDeviationRatio * theta;
		}
	}

	//std::cout << "ScoreGeo: " << score << std::endl;

	return /*1.0 / score*/ score;
}


/*double lmu::CSGNodeRankerNew::rank(const CSGNode& node) const
{
	double geo = computeGeometryScore(node, _distAngleDeviationRatio, _maxDistance, _functions);

	double normalizedGeo = lmu::clamp(geo / _maxGeo, 0.0, 1.0);

	double normalizedSize = lmu::numNodes(node) / _maxSize <= 1.0 ? 0.0 : 1.0; //TODO

	return lmu::clamp(normalizedGeo - normalizedSize, 0.0, 1.0);
}

std::string lmu::CSGNodeRankerNew::info() const
{
	return std::string();
}*/
