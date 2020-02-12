#include <string>
#include <iostream>
#include "red_inserter.h"
#include "optimizer_red.h"
#include "optimizer_ga.h"
#include "csgnode_helper.h"
#include "pointcloud.h"

int main(int argc, char *argv[])
{
	const auto num_inserter_types = 5;

	if (argc < 7) {
		return 1;
	}

	try
	{
		auto inp_file = std::string(argv[1]);
		auto out_file = std::string(argv[2]);
		auto iterations = std::stoi(argv[3]);
		auto check_correctness = (bool)std::stoi(argv[4]);
		auto sampling_grid_size = std::stod(argv[5]);

		std::cout << "Check correctness: " << check_correctness << ". Sampling: " << sampling_grid_size << std::endl;
		std::cout << "Iterations: " << iterations << std::endl;

		std::vector<lmu::Inserter> inserters;
		
		for (int i = 6; i < argc; ++i)
		{
			auto inserter_type_idx = i - 6;
			if (inserter_type_idx >= num_inserter_types)
			{
				std::cerr << "Incorrect inserter type index." << std::endl;
				return 1;
			}

			auto inserter_type = (lmu::InserterType)inserter_type_idx;
			auto prob = std::stod(argv[i]);
			
			inserters.push_back(lmu::inserter(inserter_type, prob));

			std::cout << "Use inserter '" << inserter_type << "' with probability " << prob << "." << std::endl;
		}

		std::cout << "Load tree from json..." << std::endl;
		auto inp = lmu::to_binary_tree(lmu::fromJSONFile(inp_file));
		std::cout << "Done." << std::endl;

		std::cout << "Inflate tree..." << std::endl;
		auto out = lmu::to_binary_tree(lmu::inflate_node(inp, iterations, inserters));
		std::cout << "Done." << std::endl;

		if (check_correctness)
		{ 
			std::cout << "Check for correctness..." << std::endl;
			lmu::EmptySetLookup esLookup;

			auto is_correct = lmu::is_empty_set(lmu::opDiff({ inp,out }), sampling_grid_size, lmu::empty_pc(), esLookup) &&
				lmu::is_empty_set(lmu::opDiff({ out,inp }), sampling_grid_size, lmu::empty_pc(), esLookup);

			std::cout << "Done." << std::endl;
			if (is_correct)
			{
				std::cout << "Tree is correct." << std::endl;
			}
			else
			{
				std::cerr << "Tree is incorrect." << std::endl;
				return 1;
			}
		}

		std::cout << "Old tree size: " << lmu::numNodes(inp) << ". New tree size: " << lmu::numNodes(out) << std::endl;
			 
		std::cout << "Write tree to json..." << std::endl;
		lmu::toJSONFile(out, out_file);
		std::cout << "Done." << std::endl;

		std::cout << "Write tree to gv..." << std::endl;
		lmu::writeNode(out, out_file.substr(0, out_file.find_last_of(".")) + "_graph.gv");
		std::cout << "Done." << std::endl;

		std::cout << "Write info file" << std::endl;
		std::ofstream f(out_file.substr(0, out_file.find_last_of(".")) + "_info.ini");

		lmu::AABB aabb = lmu::aabb_from_primitives(lmu::allDistinctFunctions(inp));

		f << "[Info]" << std::endl;
		f << "OldTreeSize = " << lmu::numNodes(inp) << std::endl;
		f << "OldTreeProx = " << std::setprecision(3) << lmu::compute_local_proximity_score(inp, sampling_grid_size, lmu::empty_pc()) << std::endl;
		f << "OldTreeDims = (" << (aabb.s.x() * 2.0) << ", " << (aabb.s.y() * 2.0) << ", " << (aabb.s.z() * 2.0) << ")" << std::endl;
		f << "NewTreeSize = " << lmu::numNodes(out) << std::endl;
		f << "NewTreeProx = " << std::setprecision(3) << lmu::compute_local_proximity_score(out, sampling_grid_size, lmu::empty_pc()) << std::endl;


		f.close();

		return 0;
	}
	catch (const std::exception& ex)
	{
		std::cerr << "Unable to run inflater. Error: " << ex.what() << std::endl;
		return 1;
	}
}