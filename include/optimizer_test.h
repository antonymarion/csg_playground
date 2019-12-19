#ifndef OPTIMIZER_TEST_H
#define OPTIMIZER_TEST_H

#include "test.h"

#include "csgnode_helper.h"
#include "optimizer_red.h"
#include "optimizer_ga.h"
#include "optimizer_clustering.h"
#include "cit.h"

using namespace lmu;

CSGNode sphere(double x, double y, double z, double r, const std::string& name = "")
{
	return geo<IFSphere>((Eigen::Affine3d)(Eigen::Translation3d(x, y, z)), r, name);
}

OptimizerGAParams get_std_ga_params()
{
	OptimizerGAParams params;

	params.ranker_params.geo_score_weight = 10.0;
	params.ranker_params.size_score_weight = 1.0;
	params.ranker_params.gradient_step_size = 0.0001;
	params.ranker_params.position_tolerance = 0.1;
	params.ranker_params.sampling_params.errorSigma = 0.00000001;
	params.ranker_params.sampling_params.samplingStepSize = 0.1;
	params.ranker_params.sampling_params.maxDistance = 0.1;
	params.ranker_params.max_sampling_points = 500;

	params.creator_params.create_new_prob = 0.3;
	params.creator_params.subtree_prob = 0.3;

	params.ga_params.crossover_rate = 0.4;
	params.ga_params.mutation_rate = 0.3;
	params.ga_params.in_parallel = false;
	params.ga_params.max_iterations = 100;
	params.ga_params.num_best_parents = 2;
	params.ga_params.population_size = 150;
	params.ga_params.tournament_k = 2;
	params.ga_params.use_caching = true;

	return params;
}

TEST(OptimizerRedundancyTest)
{
	const double sampling = 0.01;
	EmptySetLookup esl;

	//s1 does overlap with s2, s3 does neither overlap with s1 nor with s2.
	auto s1 = sphere(0, 0, 0, 1);
	auto s2 = sphere(1, 0, 0, 1, "s2");
	auto s3 = sphere(3, 0, 0, 1);

	ASSERT_TRUE(is_empty_set(opInter({ s1, s3 }), sampling, esl));
	ASSERT_TRUE(!is_empty_set(opInter({ s1, s2 }), sampling, esl));
	
	auto node_with_redun = opUnion({ s2, opInter({ s1, s3 }) });
	auto node_without_redun = remove_redundancies(node_with_redun, sampling);
	ASSERT_TRUE(
		numNodes(node_without_redun) == 1,
		node_without_redun.type() == CSGNodeType::Geometry,
		node_without_redun.name() == "s2"
	);

	ASSERT_TRUE(numNodes(remove_redundancies(opUnion({}), sampling)) == 1);
	ASSERT_TRUE(numNodes(remove_redundancies(opUnion({s1}), sampling)) == 2);
	ASSERT_TRUE(numNodes(remove_redundancies(opInter({}), sampling)) == 1);
	ASSERT_TRUE(numNodes(remove_redundancies(opInter({s1}), sampling)) == 2);
	ASSERT_TRUE(numNodes(remove_redundancies(opDiff({}), sampling)) == 1);
	ASSERT_TRUE(numNodes(remove_redundancies(opDiff({ s1 }), sampling)) == 2);
	ASSERT_TRUE(numNodes(remove_redundancies(opComp({}), sampling)) == 1);
}

TEST(OptimizerPISetTest)
{
	auto s1 = sphere(0, 0, 0, 1, "s1");
	auto s2 = sphere(1, 0, 0, 1, "s2");
	auto s3 = sphere(0.5, 1, 0, 1, "s3");
	auto s4 = sphere(0.5, -1, 0, 1, "s4");
	auto s5 = sphere(2.5, 0, 0, 1, "s5");
	CITSets sets = generate_cit_sets(opUnion({opDiff({ opUnion({ s1, s2 }), opUnion({ s3, s4 }) }), s5 }), 0.05);

	std::cout << sets;
}

TEST(OptimizerGA)
{
	auto s1 = sphere(0, 0, 0, 1, "s1");
	auto s2 = sphere(1, 0, 0, 1, "s2");
	auto s3 = sphere(0.5, 1, 0, 1, "s3");
	auto s4 = sphere(0.5, -1, 0, 1, "s4");
	auto s5 = sphere(2.5, 0, 0, 1, "s5");

	auto node = opUnion({ opDiff({ opInter({opUnion({ s1, s2 }),opUnion({ s1, s2 })}), opUnion({ s3, s4 }) }), s5 });

	OptimizerGAParams params = get_std_ga_params(); 
		
	auto opt_node_ga = optimize_with_ga(node, params, std::cout).node;
	
	auto opt_node_rr = remove_redundancies(node, params.ranker_params.sampling_params.samplingStepSize);

	auto opt_node_sc = optimize_pi_set_cover(node, params.ranker_params.sampling_params.samplingStepSize);

	writeNode(node, "n.gv");
	writeNode(opt_node_ga, "opt_ga.gv");
	writeNode(opt_node_rr, "opt_rr.gv");
	writeNode(opt_node_sc, "opt_sc.gv");
}

TEST(Cluster_Optimizer)
{
	auto s1 = sphere(0, 0, 0, 1, "s1");
	auto s2 = sphere(1, 0, 0, 1, "s2");
	auto s3 = sphere(0.5, 1, 0, 1, "s3");
	auto s4 = sphere(0.5, -1, 0, 1, "s4");
	auto s5 = sphere(2.5, 0, 0, 1, "s5");

	auto node = opUnion({ opDiff({ opInter({ opUnion({ s1, s2 }),opUnion({ s1, s2 }) }), opUnion({ s3, s4 }) }), s5 });

	const double sampling_grid_size = 0.1;

	auto opt_node = apply_per_cluster_optimization
	(
		cluster_union_paths(node), 

		[sampling_grid_size](const CSGNode& n) { return optimize_pi_set_cover(n, sampling_grid_size); },
		/*[](const CSGNode& n) { return optimize_with_ga(n, get_std_ga_params(), std::cout).node; },*/

		union_merge
	);

	writeNode(remove_redundancies(node, sampling_grid_size) , "opt_cluster.gv");

}

#endif