#include "..\include\csgnode.h"

#include <limits>
#include <fstream>

#include "boost/graph/graphviz.hpp"

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>

#include <igl/copyleft/cgal/mesh_boolean.h>
#include <igl/copyleft/cgal/CSGTree.h>

using namespace lmu;

CSGNodePtr UnionOperation::clone() const
{
	return std::make_shared<UnionOperation>(*this);
}
Eigen::Vector4d UnionOperation::signedDistanceAndGradient(const Eigen::Vector3d& p) const
{
	Eigen::Vector4d res(0, 0, 0, 0);

	res[0] = std::numeric_limits<double>::max();
	for (const auto& child : _childs)
	{
		auto childRes = child.signedDistanceAndGradient(p);
		res = childRes[0] < res[0] ? childRes : res;
	}

	return res;
}
CSGNodeOperationType UnionOperation::operationType() const
{
	return CSGNodeOperationType::Union;
}
std::tuple<int, int> UnionOperation::numAllowedChilds() const
{
	return std::make_tuple(1, std::numeric_limits<int>::max());
}
Mesh lmu::UnionOperation::mesh() const
{
	if (_childs.size() == 0)
		return Mesh(); 
	if (_childs.size() == 1)
		return _childs[0].mesh();

	Mesh res, left, right;
	igl::copyleft::cgal::CSGTree::VectorJ vJ;

	left = _childs[0].mesh();

	for (int i = 1; i < _childs.size();++i)
	{
		right = _childs[i].mesh();

		igl::copyleft::cgal::mesh_boolean(left.vertices, left.indices, right.vertices, right.indices, igl::MESH_BOOLEAN_TYPE_UNION, res.vertices, res.indices, vJ);
		
		left = res;
	}

	return res;	
}

CSGNodePtr IntersectionOperation::clone() const
{
	return std::make_shared<IntersectionOperation>(*this);
}
Eigen::Vector4d IntersectionOperation::signedDistanceAndGradient(const Eigen::Vector3d & p) const
{
	Eigen::Vector4d res(0, 0, 0, 0);

	res[0] = -std::numeric_limits<double>::max();
	for (const auto& child : _childs)
	{
		auto childRes = child.signedDistanceAndGradient(p);
		res = childRes[0] > res[0] ? childRes : res;
	}

	return res;
}
CSGNodeOperationType IntersectionOperation::operationType() const
{
	return CSGNodeOperationType::Intersection;
}
std::tuple<int, int> IntersectionOperation::numAllowedChilds() const
{
	return std::make_tuple(1, std::numeric_limits<int>::max());
}
Mesh lmu::IntersectionOperation::mesh() const
{
	if (_childs.size() == 0)
		return Mesh();
	if (_childs.size() == 1)
		return _childs[0].mesh();

	Mesh res, left, right;
	igl::copyleft::cgal::CSGTree::VectorJ vJ;

	left = _childs[0].mesh();

	for (int i = 1; i < _childs.size(); ++i)
	{
		right = _childs[i].mesh();

		igl::copyleft::cgal::mesh_boolean(left.vertices, left.indices, right.vertices, right.indices, igl::MESH_BOOLEAN_TYPE_INTERSECT, res.vertices, res.indices, vJ);

		left = res;
	}

	return res;
}

CSGNodePtr DifferenceOperation::clone() const
{
	return std::make_shared<DifferenceOperation>(*this);
}
/*
/*
case OperationType::DifferenceLR:

if (sdsGrads.size() == 2)
{
auto sdGrad1 = sdsGrads[0];
auto sdGrad2 = (-1.0)*sdsGrads[1];

if (sdGrad2[0] > sdGrad1[0])
res = sdGrad2;
else
res = sdGrad1;

//Negate gradient
res[1] = (-1.0)*res[1];
res[2] = (-1.0)*res[2];
res[3] = (-1.0)*res[3];
}
else
{
std::cout << "Warning: Not exactly two operands for difference operation." << std::endl;
res[0] = std::numeric_limits<double>::max();
}

break;
*/
Eigen::Vector4d DifferenceOperation::signedDistanceAndGradient(const Eigen::Vector3d& p) const
{
	auto left = _childs[0].signedDistanceAndGradient(p);		
	auto right = _childs[1].signedDistanceAndGradient(p);

	auto res = left; 
	res[1] = (-1.0)*res[1];
	res[2] = (-1.0)*res[2];
	res[3] = (-1.0)*res[3];

	if ((-1.0)*right[0] > res[0])
	{
		res = (-1.0) * right;		
	}

	return res;
}
CSGNodeOperationType DifferenceOperation::operationType() const
{
	return CSGNodeOperationType::Difference;
}
std::tuple<int, int> DifferenceOperation::numAllowedChilds() const
{
	return std::make_tuple(2, 2);
}
Mesh lmu::DifferenceOperation::mesh() const
{
	if (_childs.size() != 2)
		return Mesh();
	
	Mesh res, left, right;
	igl::copyleft::cgal::CSGTree::VectorJ vJ;

	left = _childs[0].mesh();
	right = _childs[1].mesh();

	igl::copyleft::cgal::mesh_boolean(left.vertices, left.indices, right.vertices, right.indices, igl::MESH_BOOLEAN_TYPE_MINUS, res.vertices, res.indices, vJ);
	
	return res;
}

/*
CSGNodePtr DifferenceRLOperation::clone() const
{
	return std::make_shared<DifferenceRLOperation>(*this);
}
Eigen::Vector4d DifferenceRLOperation::signedDistanceAndGradient(const Eigen::Vector3d & p) const
{
	Eigen::Vector4d res(0, 0, 0, 0);

	auto sdGrad1 = _childs[1].signedDistanceAndGradient(p);
	auto sdGrad2 = (-1.0)*_childs[0].signedDistanceAndGradient(p);

	if (sdGrad2[0] > sdGrad1[0])
		res = sdGrad2;
	else
		res = sdGrad1;

	//Negate gradient
	res[1] = (-1.0)*res[1];
	res[2] = (-1.0)*res[2];
	res[3] = (-1.0)*res[3];

	return res;
}
CSGNodeOperationType DifferenceRLOperation::operationType() const
{
	return CSGNodeOperationType::DifferenceRL;
}
std::tuple<int, int> DifferenceRLOperation::numAllowedChilds() const
{
	return std::make_tuple(2,2);
}
Mesh lmu::DifferenceRLOperation::mesh() const
{
	if (_childs.size() != 2)
		return Mesh();

	Mesh res, left, right;
	igl::copyleft::cgal::CSGTree::VectorJ vJ;

	left = _childs[1].mesh();
	right = _childs[0].mesh();

	igl::copyleft::cgal::mesh_boolean(left.vertices, left.indices, right.vertices, right.indices, igl::MESH_BOOLEAN_TYPE_MINUS, res.vertices, res.indices, vJ);

	return res;
}*/

std::string lmu::operationTypeToString(CSGNodeOperationType type)
{
	switch (type)
	{
	case CSGNodeOperationType::Intersection:
		return "Intersection";
	case CSGNodeOperationType::Difference:
		return "Difference";
	case CSGNodeOperationType::Union:
		return "Union";
	case CSGNodeOperationType::Unknown:
		return "Unknown";
	case CSGNodeOperationType::Complement:
		return "Complement";
	case CSGNodeOperationType::Invalid:
		return "Invalid";
	default:
		return "Undefined Type";
	}
}

std::string lmu::nodeTypeToString(CSGNodeType type)
{
	switch (type)
	{
	case CSGNodeType::Operation:
		return "Operation";
	case CSGNodeType::Geometry:
		return "Geometry";
	default:
		return "Undefined Type";
	}
}

CSGNode lmu::createOperation(CSGNodeOperationType type, const std::string & name, const std::vector<CSGNode>& childs)
{
	switch (type)
	{
	case CSGNodeOperationType::Union:
		return CSGNode(std::make_shared<UnionOperation>(name, childs));
	case CSGNodeOperationType::Intersection:
		return CSGNode(std::make_shared<IntersectionOperation>(name, childs));
	case CSGNodeOperationType::Difference:
		return CSGNode(std::make_shared<DifferenceOperation>(name, childs));
	default:
		throw std::runtime_error("Operation type is not supported");
	}
}

int lmu::depth(const CSGNode& node, int curDepth)
{
	int maxDepth = curDepth;

	for (const auto& child : node.childs())
	{
		int childDepth = depth(child, curDepth + 1);
		maxDepth = std::max(maxDepth, childDepth);
	}

	return maxDepth;
}

void allGeometryNodePtrsRec(const CSGNode& node, std::vector<CSGNodePtr>& res)
{
	if (node.type() == CSGNodeType::Geometry)
		res.push_back(node.nodePtr());

	for (const auto& child : node.childsCRef())
		allGeometryNodePtrsRec(child, res);
}

std::vector<CSGNodePtr> lmu::allGeometryNodePtrs(const CSGNode& node)
{
	std::vector<CSGNodePtr> res;
	allGeometryNodePtrsRec(node, res);

	return res;
}

double lmu::computeGeometryScore(const CSGNode& node, double epsilon, double alpha, const std::vector<std::shared_ptr<lmu::ImplicitFunction>>& funcs) 
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

			double d = distAndGrad[0] / epsilon;

			Eigen::Vector3d grad = distAndGrad.tail<3>();
			double minusGradientDotN = lmu::clamp(-grad.dot(n), -1.0, 1.0); //clamp is necessary, acos is only defined in [-1,1].
			double theta = std::acos(minusGradientDotN) / alpha;

			double scoreDelta = (std::exp(-(d*d)) + std::exp(-(theta*theta)));

			//if (scoreDelta < 0)
			//	std::cout << "Theta: " << theta << " minusGradientDotN: " << minusGradientDotN << std::endl;

			score += scoreDelta;
		}
	}

	//std::cout << "ScoreGeo: " << score << std::endl;

	return /*1.0 / score*/ score;
}

int lmu::numNodes(const CSGNode & node)
{
	int num = 1;
	for (const auto& child : node.childsCRef())
	{
		num += numNodes(child);
	}

	return num;
}

int lmu::numPoints(const CSGNode& node)
{
	int n = 0;
	
	for (const auto& c : node.childsCRef())
	{
		if(c.function())
			n += c.function()->points().rows();
		else 
			n += numPoints(c);
	}

	return n;
}

CSGNode* nodeRec(CSGNode& node, int idx, int& curIdx)
{
	if (idx == curIdx)
		return &node;

	curIdx++;

	for (auto& child : node.childsRef())
	{
		auto foundTree = nodeRec(child, idx, curIdx);
		if (foundTree)
			return foundTree;
	}

	return nullptr;
}

CSGNode* lmu::nodePtrAt(CSGNode& node, int idx)
{
	int curIdx = 0;
	return nodeRec(node, idx, curIdx);
}

int nodeDepthRec(const CSGNode& node, int idx, int& curIdx, int depth)
{
	if (idx == curIdx)
		return depth;

	curIdx++;

	for (const auto& child : node.childsCRef())
	{
		auto foundDepth = nodeDepthRec(child, idx, curIdx, depth + 1);
		if (foundDepth != -1)
			return foundDepth;
	}

	return -1;
}

int lmu::depthAt(const CSGNode& node, int idx)
{
	int curIdx = 0;
	return nodeDepthRec(node, idx, curIdx, 0);
}

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS, CSGNodePtr> TreeGraph;

void createGraphRec(const CSGNode& node, TreeGraph& graph, size_t parentVertex)
{
	auto v = boost::add_vertex(graph);
	graph[v] = node.nodePtr();
	if (parentVertex < std::numeric_limits<size_t>::max())
		boost::add_edge(parentVertex, v, graph);

	for (auto& child : node.childs())
	{
		createGraphRec(child, graph, v);
	}
}

template <class Name>
class VertexWriter {
public:
	VertexWriter(Name _name) : name(_name) {}
	template <class VertexOrEdge>
	void operator()(std::ostream& out, const VertexOrEdge& v) const
	{
		std::stringstream ss;

		CSGNodeType type = name[v]->type(); 
		if (type == CSGNodeType::Geometry)
		{
			ss << name[v]->name();

		}
		else if (type == CSGNodeType::Operation)
		{
			ss << operationTypeToString(name[v]->operationType()) << std::endl;
		}

		out << "[label=\"" << ss.str() << "\"]";
	}
private:
	Name name;
};

void lmu::writeNode(const CSGNode& node, const std::string & file)
{
	TreeGraph graph;
	createGraphRec(node, graph, std::numeric_limits<size_t>::max());

	std::ofstream f(file);
	boost::write_graphviz(f, graph, VertexWriter<TreeGraph>(graph));
	f.close();
}

void serializeNodeRec(CSGNode& node, SerializedCSGNode& res)
{
	if (node.childsCRef().size() == 2)
	{		
		res.push_back(NodePart(NodePartType::LeftBracket, nullptr));
		serializeNodeRec(node.childsRef()[0], res);
		res.push_back(NodePart(NodePartType::RightBracket, nullptr));

		res.push_back(NodePart(NodePartType::Node, &node));

		res.push_back(NodePart(NodePartType::LeftBracket, nullptr));
		serializeNodeRec(node.childsRef()[1], res);
		res.push_back(NodePart(NodePartType::RightBracket, nullptr));

	}
	else if (node.childsCRef().size() == 0)
	{
		res.push_back(NodePart(NodePartType::Node, &node));
	}
}

SerializedCSGNode lmu::serializeNode(CSGNode& node)
{
	SerializedCSGNode res;
	serializeNodeRec(node, res);
	return res;
}

CSGNode* getRoot(const SerializedCSGNode& n, int start, int end)
{
	//Note: We assume that n is representing a correct serialization of a tree.

	//end index is exclusive
	int size = end - start;

	if (size == 1)
		return n[start].node;

	int counter = 0;
	for (int i = start; i < end; ++i)
	{
		NodePart np = n[i];

		if (np.type == NodePartType::LeftBracket)
		{
			counter++;
		}
		else if (np.type == NodePartType::RightBracket)
		{
			counter--;
		}

		if (counter == 0)
			return n[i + 1].node;
	}

	return nullptr; 
}

//Code from https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Longest_common_substring#C++_2
/*LargestCommonSubgraph lmu::findLargestCommonSubgraph(const SerializedCSGNode& n1, const SerializedCSGNode& n2)
{
	int startN1 = 0;
	int startN2 = 0;
	int max = 0;
	for (int i = 0; i < n1.size(); i++)
	{
		for (int j = 0; j < n2.size(); j++)
		{
			int x = 0;
			while (n1[i + x] == n2[j + x])
			{
				x++;
				if (((i + x) >= n1.size()) || ((j + x) >= n2.size())) break;
			}
			if (x > max)
			{
				max = x;
				startN1 = i;
				startN2 = j;
			}
		}
	}

	//return S1.substring(Start, (Start + Max));

	return LargestCommonSubgraph(
		getRoot(n1, startN1, startN1 + max),
		getRoot(n2, startN2, startN2 + max), max);
}*/

using SubgraphMap = std::unordered_map<std::string, std::vector<CSGNode*>>;

void getSubgraphsRec(CSGNode& node, SubgraphMap& res)
{
	std::stringstream ss;
	ss << serializeNode(node);
	
	res[ss.str()].push_back(&node);

	for (auto& child : node.childsRef())
		getSubgraphsRec(child, res);
}

void printSubgraphMap(const SubgraphMap& map)
{
	std::cout << "SubgraphMap:" << std::endl;
	for (const auto& item : map)
	{
		std::cout << item.first << ": " << item.second.size() << std::endl;
	}
}

LargestCommonSubgraph lmu::findLargestCommonSubgraph(CSGNode& n1, CSGNode& n2)
{
	SubgraphMap n1Subgraphs, n2Subgraphs;
	getSubgraphsRec(n1, n1Subgraphs);
	getSubgraphsRec(n2, n2Subgraphs);

	printSubgraphMap(n1Subgraphs);
	printSubgraphMap(n2Subgraphs);
	
	LargestCommonSubgraph lgs(&n1, &n2, {}, {}, 0);

	for (auto serN1 : n1Subgraphs)
	{
		auto it = n2Subgraphs.find(serN1.first);
		if (it != n2Subgraphs.end())
		{			
			int sgSize = numNodes(*it->second[0]);
			if (lgs.size < sgSize)
			{	
				lgs = LargestCommonSubgraph(&n1, &n2, serN1.second, it->second, sgSize);
			}			
		}
	}

	return lgs;
}

bool isValidMergeNode(const CSGNode& node, const CSGNode& searchNode)
{
	if (&node == &searchNode)
		return true;
	 
	if (node.type() == CSGNodeType::Operation)
	{
		if (node.operationType() == CSGNodeOperationType::Difference)
		{
			return isValidMergeNode(node.childsCRef()[0], searchNode);
		}
		else if (node.operationType() == CSGNodeOperationType::Union)
		{
			for (const auto& child : node.childsCRef())
			{
				if (isValidMergeNode(child, searchNode))
					return true;
			}
		}
		else if (node.operationType() == CSGNodeOperationType::Intersection)
		{
			return false;
		}
	}
		
	return false;
}

CSGNode* getValidMergeNode(const CSGNode& root, const std::vector<CSGNode*>& candidateNodes)
{
	for (const auto& candidateNode : candidateNodes)
	{
		if (isValidMergeNode(root, *candidateNode))
			return candidateNode;
	}

	return nullptr;
}

void mergeNode(CSGNode* dest, const CSGNode& source)
{
	*dest = source;
}

MergeResult lmu::mergeNodes(const LargestCommonSubgraph& lcs)
{
	if (lcs.isEmptyOrInvalid())
		return MergeResult::None;

	CSGNode* validMergeNodeInN1 = getValidMergeNode(*lcs.n1Root, lcs.n1Appearances);
	CSGNode* validMergeNodeInN2 = getValidMergeNode(*lcs.n2Root, lcs.n2Appearances);

	if (validMergeNodeInN1 && validMergeNodeInN2)
	{
		if (numNodes(*lcs.n1Root) >= numNodes(*lcs.n2Root))
		{
			mergeNode(validMergeNodeInN1, *lcs.n2Root);
			return MergeResult::First;
		}
		else
		{
			mergeNode(validMergeNodeInN2, *lcs.n1Root);
			return MergeResult::Second;
		}
	}
	else if (validMergeNodeInN1)
	{
		mergeNode(validMergeNodeInN1, *lcs.n2Root);
		return MergeResult::First;
	}
	else if (validMergeNodeInN2)
	{
		mergeNode(validMergeNodeInN2, *lcs.n1Root);
		return MergeResult::Second;
	}
	else
	{
		throw MergeResult::None;
	}
}

std::ostream& lmu::operator<<(std::ostream& os, const SerializedCSGNode& v)
{
	for (const auto& np : v)
		os << np;

	return os;
}

bool lmu::operator==(const NodePart& lhs, const NodePart& rhs)
{
	if (lhs.type == NodePartType::Node && rhs.type == NodePartType::Node)
	{
		if (lhs.node->type() != rhs.node->type())
			return false;

		if (lhs.node->type() == CSGNodeType::Operation)
			return lhs.node->operationType() == rhs.node->operationType();
		else if (lhs.node->type() == CSGNodeType::Geometry)
			return lhs.node->function() == rhs.node->function();

		return false;
	}

	return lhs.type == rhs.type;
}

bool lmu::operator!=(const NodePart& lhs, const NodePart& rhs)
{
	return !(lhs == rhs);
}

std::ostream& lmu::operator<<(std::ostream& os, const NodePart& np)
{
	switch (np.type)
	{
	case NodePartType::LeftBracket:
		os << "(";
		break;
	case NodePartType::RightBracket:
		os << ")";
		break;
	case NodePartType::Node:
		switch (np.node->type())
		{
		case CSGNodeType::Operation:
			os << operationTypeToString(np.node->operationType());
			break;
		case CSGNodeType::Geometry:
			os << np.node->function()->name();
			break;
		}
		break;
	}
	return os;
}



