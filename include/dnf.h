#ifndef DNF_H
#define DNF_H

#include <vector>
//#include <boost/dynamic_bitset.hpp>
#include <boost/container/vector.hpp>

#include "csgnode.h"

namespace lmu
{
	struct Clause
	{
		Clause(int size) : 
			literals(size, false),
			negated(size, false)
		{
		}

		void clearAll()
		{
			literals = boost::container::vector<bool>(literals.size(), false);
			negated = boost::container::vector<bool>(negated.size(), false);
		}

		boost::container::vector<bool> literals; 
		boost::container::vector<bool> negated;
	};

	struct DNF
	{
		std::vector<Clause> clauses; 		
		std::vector<ImplicitFunctionPtr> functions;
	};

	struct SampleParams
	{
		double maxDistDelta;
		double requiredCorrectSamples;
		double requiredConsideredSamples;
	};

	std::ostream& operator <<(std::ostream& stream, const Clause& c);


	
	CSGNode DNFtoCSGNode(const DNF& dnf);
	CSGNode clauseToCSGNode(const Clause& clause, const std::vector<ImplicitFunctionPtr>& functions);
	
	bool isIn(const Clause& clause, const std::vector<ImplicitFunctionPtr>& functions, const std::unordered_map<lmu::ImplicitFunctionPtr, double> outlierTestValues, const SampleParams& params);
	bool isPrime(const ImplicitFunctionPtr& func, const std::vector<ImplicitFunctionPtr>& functions, const std::unordered_map<lmu::ImplicitFunctionPtr, double> outlierTestValues, const SampleParams& params);

	std::unordered_map<lmu::ImplicitFunctionPtr, double> computeOutlierTestValues(const std::vector<lmu::ImplicitFunctionPtr>& functions);


	DNF computeShapiro(const std::vector<ImplicitFunctionPtr>& functions, bool usePrimeImplicantOptimization, const SampleParams& params);
	DNF mergeDNFs(const std::vector<DNF>& dnfs);
}

#endif