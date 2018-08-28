#include "dnf.h"
#include "csgnode_helper.h"
#include "curvature.h"

#include <algorithm>
#include <unordered_map>
#include <Eigen/Core>

#include <boost/math/special_functions/erf.hpp>

#include "statistics.h"

Eigen::MatrixXd lmu::g_testPoints;
lmu::Clause lmu::g_clause;


lmu::CSGNode lmu::DNFtoCSGNode(const DNF& dnf)
{
	CSGNode node = opUnion();

	for (const auto& clause : dnf.clauses)
		node.addChild(clauseToCSGNode(clause, dnf.functions));

	return node;
}

lmu::CSGNode lmu::clauseToCSGNode(const Clause& clause, const std::vector<ImplicitFunctionPtr>& functions)
{
	CSGNode node = opInter();

	for (int i = 0; i < functions.size(); ++i)
	{
		if (!clause.literals[i])
			continue;

		if (clause.negated[i])
			node.addChild(opComp({ geometry(functions[i]) }));
		else
			node.addChild(geometry(functions[i]));
	}	

	return node.childsCRef().size() == 1 ? node.childsCRef()[0] : node;
}

std::ostream& lmu::operator <<(std::ostream& stream, const lmu::Clause& c)
{
	for (int i = 0; i < c.literals.size(); ++i)
	{
		if (c.negated[i])
			stream << "!";

		stream << c.literals[i];
	}
	return stream;
}

void print (std::ostream& stream, const lmu::Clause& c, const std::vector<lmu::ImplicitFunctionPtr>& functions, bool printNonSetLiterals)
{
	for (int i = 0; i < c.literals.size(); ++i)
	{
		if (c.negated[i])
			stream << "!";

		if(printNonSetLiterals || c.literals[i])
			stream << functions[i]->name();
	}
}

inline double median(std::vector<double> v)
{
	std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
	return v[v.size() / 2];
}

//https://www.mathworks.com/help/matlab/ref/isoutlier.html
double scaledMAD(const lmu::ImplicitFunctionPtr& func)
{	
	lmu::CSGNode node = lmu::geometry(func);

	std::vector<double> values(func->pointsCRef().rows());

	for (int j = 0; j < func->pointsCRef().rows(); ++j)
	{

		Eigen::Matrix<double, 1, 6> pn = func->pointsCRef().row(j);

		Eigen::Vector3d p = pn.leftCols(3);
		Eigen::Vector3d n = pn.rightCols(3);

		double h = 0.01;

		lmu::Curvature c = curvature(p, node, h);

		values[j] = std::sqrt(c.k1 * c.k1 + c.k2 * c.k2);
	}

	double med = median(values);
	std::transform(values.begin(), values.end(), values.begin(), [med](double v) -> double { return std::abs(v - med); });

	const double c = -1.0 / (std::sqrt(2.0)*boost::math::erfc_inv(3.0 / 2.0));
	
	return c * median(values);	
}

std::unordered_map<lmu::ImplicitFunctionPtr, double> lmu::computeOutlierTestValues(const std::vector<lmu::ImplicitFunctionPtr>& functions)
{
	std::unordered_map<lmu::ImplicitFunctionPtr, double> map;

	for (const auto& func : functions)
		map[func] = scaledMAD(func) * 3.0;

	return map;
}


double getInOutThreshold(const std::vector<double>& qualityValues)
{
	const int k = 2;

	auto res = lmu::k_means(qualityValues, k , 300);
	std::cout << "Means: " << std::endl;
	for (auto m : res.means)
		std::cout << m << std::endl;

	std::vector<double> min( k, std::numeric_limits<double>::max());
	size_t i = 0;
	for (auto t : res.assignments)
	{
		min[t] = qualityValues[i] < min[t] ? qualityValues[i] : min[t];
		i++;
	}

	std::sort(min.begin(), min.end());

	for(auto m : min)
		std::cout << "Min: " << m << std::endl;

	
	return min.back();
	/*ValueCountPairContainer sortedUniqueValueCounts;
	GetValueCountPairs(sortedUniqueValueCounts, &qualityValues[0], qualityValues.size());

	std::cout << "Finding Jenks ClassBreaks..." << std::endl;
	LimitsContainer resultingbreaksArray;
	ClassifyJenksFisherFromValueCountPairs(resultingbreaksArray, k, sortedUniqueValueCounts);

	std::cout << "Breaks: " << std::endl;
	for (const auto& b : resultingbreaksArray)
		std::cout << b << std::endl;

	return resultingbreaksArray[1];*/
}

std::vector<std::tuple<lmu::Clause, size_t>> getValidClauses(const std::vector<std::tuple<lmu::Clause, double>>& clauseQualityPairs)
{
	std::vector<double> qualityValues; 
	std::transform(clauseQualityPairs.begin(), clauseQualityPairs.end(), std::back_inserter(qualityValues), 
		[](auto p) { return std::get<1>(p); });

	double t = getInOutThreshold(qualityValues);

	std::vector<std::tuple<lmu::Clause, size_t>> validClauses;

	for (size_t i = 0; i < clauseQualityPairs.size(); ++i)
	{
		if (std::get<1>(clauseQualityPairs[i]) >= t)		
			validClauses.push_back(std::make_tuple(std::get<0>(clauseQualityPairs[i]), i));
	}

	return validClauses; 
}

std::tuple<lmu::Clause, double> lmu::scoreClause(const Clause& clause, const std::vector<ImplicitFunctionPtr>& functions, const std::unordered_map<lmu::ImplicitFunctionPtr, double> outlierTestValues, const SampleParams& params)
{
	lmu::CSGNode node = clauseToCSGNode(clause, functions);

	int numCorrectSamples = 0;
	int numTotalSamples = 0;
	int numConsideredSamples = 0;

	double h = 0.001;
	
	std::vector<Eigen::Matrix<double, 1, 2>> consideredPoints;

	for (int i = 0; i < functions.size(); ++i)
	{	
		//if (!clause.literals[i])
		//	continue;

		lmu::ImplicitFunctionPtr func = functions[i];

		//if (i == 0)
		//	continue;

		double outlierTestValue = outlierTestValues.at(func);

		for (int j = 0; j < func->pointsCRef().rows(); ++j)
		{
			numTotalSamples++;

			Eigen::Matrix<double, 1,6> pn = func->pointsCRef().row(j);

			Eigen::Vector3d sampleP = pn.leftCols(3);
			Eigen::Vector3d sampleN = pn.rightCols(3);

			Eigen::Vector4d sampleDistGradFunction = func->signedDistanceAndGradient(sampleP);
			double sampleDistFunction = sampleDistGradFunction[0];
			Eigen::Vector3d sampleGradFunction = sampleDistGradFunction.bottomRows(3);			
			
			//Move sample position back on the function's implied surface.			
			//sampleP = sampleP - sampleGradFunction.cwiseProduct(Eigen::Vector3d(sampleDistFunction, sampleDistFunction, sampleDistFunction));

			Eigen::Vector4d sampleDistGradNode = node.signedDistanceAndGradient(sampleP, h);
			double sampleDistNode = sampleDistGradNode[0];
			Eigen::Vector3d sampleGradNode = sampleDistGradNode.bottomRows(3);
			
			//if(abs(func->signedDistanceAndGradient(sampleP)[0]) > h*h / 2.0)
			//	std::cout << sampleDistFunction << " " << func->signedDistanceAndGradient(sampleP)[0] << std::endl;

			const double smallestDelta = 0.000000001;

			if (sampleDistNode - sampleDistFunction > smallestDelta)
			{			
			
				continue;
			}
			else
			{
			}
						
			//Curvature c = curvature(sampleP, geometry(func), h);
			//double deviationFromFlatness = std::sqrt(c.k1 * c.k1 + c.k2 * c.k2);
			//if (deviationFromFlatness > outlierTestValue)
			//{				
			//	continue;
			//}
		
			numConsideredSamples++;

			if (sampleDistNode - sampleDistFunction < -smallestDelta)
			{
				//std::cout << "D: " << (sampleDistNode - sampleDistFunction) << std::endl;

				//Eigen::Matrix<double, 1, 6> m;
				//m << sampleP.transpose(), Eigen::Vector3d(1, 0, 0).transpose();
				//consideredPoints.push_back(m);
			
				continue;
			}

			if (sampleGradNode.dot(sampleN) <= 0.0)
			{
				//Eigen::Matrix<double, 1, 6> m;
				//m << sampleP.transpose(), Eigen::Vector3d(0, 1, 1).transpose();
				//consideredPoints.push_back(m);

				continue;
			}
			
			numCorrectSamples++;
		}
	}

	
	
	/*g_testPoints = Eigen::MatrixXd(consideredPoints.size(), 6);
	int i = 0;
	for (const auto& p : consideredPoints)
		g_testPoints.row(i++) = p;
	g_clause = clause;
	*/

	//if (numSameDir + numOtherDir == 0)
	//	return false;

	double consideredSamples = (double)(numConsideredSamples) / (double)numTotalSamples;
	double correctSamples = numConsideredSamples == 0.0 ? 0.0 : (double)numCorrectSamples / (double)(numConsideredSamples);

	std::cout << "Clause: ";
	print(std::cout, clause, functions, false);
	std::cout << std::endl;
	std::cout << "Considered Samples: " << consideredSamples << std::endl;
	std::cout << "Correct Samples: " << correctSamples << std::endl;

	Eigen::Matrix<double, 1, 2> m;
	m << consideredSamples, correctSamples;
    g_testPoints.conservativeResize(g_testPoints.rows()+1,2);
    g_testPoints.row(g_testPoints.rows() - 1) = m;

	//std::cout << g_testPoints << std::endl;
	
	//return correctSamples >= params.requiredCorrectSamples &&
	//	consideredSamples >= params.requiredConsideredSamples;

	return std::make_tuple(clause, correctSamples);	
}

std::vector<std::tuple<lmu::Clause, double>>  permutateAllPossibleFPs(lmu::Clause clause /*copy necessary*/, lmu::DNF& dnf, const std::unordered_map<lmu::ImplicitFunctionPtr, double> outlierTestValues, const lmu::SampleParams& params, int& iterationCounter)
{	
	std::vector<std::tuple<lmu::Clause, double>> clauses;

	std::sort(clause.negated.begin(), clause.negated.end());
	do {							
		clauses.push_back(lmu::scoreClause(clause, dnf.functions, outlierTestValues, params));

		iterationCounter++;		
		std::cout << "Ready: " << (double)iterationCounter / std::pow(2, clause.negated.size()) * 100.0 << "%" << std::endl;

	} while (std::next_permutation(clause.negated.begin(), clause.negated.end()));

	return clauses;
}

std::tuple<lmu::DNF, std::vector<lmu::ImplicitFunctionPtr>> identifyPrimeImplicants(const std::vector<lmu::ImplicitFunctionPtr>& functions, const std::unordered_map<lmu::ImplicitFunctionPtr, double> outlierTestValues, const lmu::SampleParams& params)
{
	//Check which primitive is completely inside the geometry.
	std::vector<std::tuple<lmu::Clause, double>> clauses;
	for (int i = 0; i < functions.size(); ++i)
	{
		//Defining a clause representing a single primitive.
		lmu::Clause clause(functions.size());
		clause.literals[i] = true;

		print(std::cout, clause, functions, false);
		
		auto c = lmu::scoreClause(clause, functions, outlierTestValues, params);	
			clauses.push_back(c);			
	}

	//Create a DNF that contains a clause for each primitive.
	lmu::DNF dnf; 
	std::vector<int> functionDeleteMarker(functions.size(),0);
	auto validClauses = getValidClauses(clauses);
	int i = 0; //New index.
	for (const auto& validClause :validClauses)
	{
		lmu::Clause clause(validClauses.size());
			
		clause.literals[i] = true;

		//Index in vector that contains all clauses (valid and non-valid).
		auto idx = std::get<1>(validClause);
		dnf.functions.push_back(functions[idx]); //Add function to the dnf's functions
		
		functionDeleteMarker[idx] = 1;
	
		dnf.clauses.push_back(clause);

		i++;
	}

	//Create set of non PI functions.
	std::vector<lmu::ImplicitFunctionPtr> nonPIs;
	for (int i = 0; i < functionDeleteMarker.size(); ++i)
	{
		if (functionDeleteMarker[i] != 1)
			nonPIs.push_back(functions[i]);
	}
		
	return std::make_tuple(dnf, nonPIs);
}

lmu::DNF lmu::computeShapiro(const std::vector<ImplicitFunctionPtr>& functions, bool usePrimeImplicantOptimization, const SampleParams& params)
{
	DNF primeImplicantsDNF;
	DNF dnf;

	auto outlierTestValues = computeOutlierTestValues(functions);
	
	if (usePrimeImplicantOptimization)
	{
		auto res = identifyPrimeImplicants(functions, outlierTestValues, params);
		primeImplicantsDNF = std::get<0>(res);
		dnf.functions = std::get<1>(res);		
	}
	else
	{
		dnf.functions = functions;
	}

	Clause clause(dnf.functions.size());
	std::fill(clause.literals.begin(), clause.literals.end(), true);
		
	std::cout << "Do Shapiro..." << std::endl;
	//return primeImplicantsDNF;
	
	int iterationCounter = 0;

	std::vector<std::tuple<lmu::Clause, double>> clauses;

	for (int i = 0; i <= dnf.functions.size(); i++)
	{	
		auto newClauses = permutateAllPossibleFPs(clause, dnf, outlierTestValues, params, iterationCounter);
		clauses.insert(clauses.end(), newClauses.begin(), newClauses.end());
				
		if(i < dnf.functions.size())
			clause.negated[i] = true;
	}

	//Check for validity of all found clauses
	for (const auto& validClause : getValidClauses(clauses))
		dnf.clauses.push_back(std::get<0>(validClause));

	std::cout << "Done Shapiro." << std::endl;

	return lmu::mergeDNFs({ primeImplicantsDNF, dnf });
}

lmu::DNF lmu::mergeDNFs(const std::vector<DNF>& dnfs)
{
	lmu::DNF mergedDNF;
	for (const auto& dnf : dnfs)
	{
		int oldClauseSize = mergedDNF.functions.size();
		int newClauseSize = oldClauseSize + dnf.functions.size();

		if (oldClauseSize == newClauseSize)
			continue;

		//Resize existing clauses.
		for (auto& clause : mergedDNF.clauses)
		{
			clause.literals.resize(newClauseSize, false);
			clause.negated.resize(newClauseSize, false);
		}

		//Add modified new clauses.
		for (auto& clause : dnf.clauses)
		{
			lmu::Clause newClause(oldClauseSize);			
			newClause.literals.insert(newClause.literals.end(), clause.literals.begin(), clause.literals.end());
			newClause.negated.insert(newClause.negated.end(), clause.negated.begin(), clause.negated.end());

			mergedDNF.clauses.push_back(newClause);
		}
		
		mergedDNF.functions.insert(mergedDNF.functions.end(), dnf.functions.begin(), dnf.functions.end());
	}
		
	return mergedDNF;
}

std::string lmu::espressoExpression(const DNF& dnf)
{
	std::stringstream ss;

	std::stringstream literalsS;
	for (const auto& func : dnf.functions)
	{
		literalsS << func->name() << ",";
	}
	std::string literals = literalsS.str();
	literals = literals.substr(0, literals.size() - 1);

	ss << literals << "= map(exprvar, '" << literals << "'.split(','))" << std::endl;

	ss << "expr = ";

	bool firstClause = true;

	for (const auto& clause : dnf.clauses)
	{
		
		if (!firstClause)
		{
			ss << "| ";
		}
		else
		{
			firstClause = false;
		}

		bool firstLiteral = true;

		for (int i = 0; i < clause.size(); ++i)
		{

			if (clause.literals[i])
			{
				if (!firstLiteral)
				{
					ss << "& ";
				}
				else
				{
					firstLiteral = false;
				}

				if (clause.negated[i])
					ss << "~";

				ss << dnf.functions[i]->name() << " ";
			}
		}
	}

	ss << std::endl;
	ss << "dnf = expr.to_dnf()";
	
	return ss.str();
}
