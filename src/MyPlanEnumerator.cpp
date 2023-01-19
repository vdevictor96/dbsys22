#include "MyPlanEnumerator.hpp"


using namespace m;


void print_subsets(std::vector<QueryGraph::Subproblem> &subsets) {
    std::cout << "there are n connected subsets: " << subsets.size() << '\n';
    for (size_t i = 0; i < subsets.size(); i++) {
        // std::cout << "subset " << i << '\n';
        // for (int j = 0; j < subsets[i].size(); j++) {
        //     std::cout << subsets[i].at(j) << ' ';
        // }
        subsets[i].dump();
        std::cout << '\n';
    }
}


static std::vector<QueryGraph::Subproblem> connected_subsets(std::vector<size_t> sources, size_t planSize, const AdjacencyMatrix &M) {
    // if planSize set to 0, then return all the connected subsets
    if (planSize == 0) {
        planSize = sources.size();
    }
    std::vector<QueryGraph::Subproblem> subsets;
    for (size_t bit = 1; bit < pow(2, sources.size()); bit++) {
        QueryGraph::Subproblem subset(bit);
        // std::cout << "size: " << subset.size();
        // subset.dump();
        if (subset.size() >= planSize && M.is_connected(subset)) {
            subsets.push_back(subset);
        }
        // std::cout << '\n';
    }
    // print_subsets(subsets);
    return subsets;
}


static std::vector<QueryGraph::Subproblem> connected_subsetsO(QueryGraph::Subproblem &subset, const AdjacencyMatrix &M) {
    // if planSize set to 0, then return all the connected subsets
    // if (planSize == 0) {
    //     planSize = sources.size();
    // }
    size_t max_value = uint64_t(subset);

    // std::cout << subset.dump() << "max_value: " << max_value << '\n';
    std::vector<QueryGraph::Subproblem> subsetsO;
    // from 1 to max_value, not included
    for (size_t bit = 1; bit < max_value; bit++) {
        // the bits that are 0 in the subset cannot be 1 in the subsetsO
        QueryGraph::Subproblem subsetO(bit);
        // std::cout << "size: " << subset.size();
        // subset.dump();
        if (subsetO.is_subset(subset) && M.is_connected(subsetO)) {
            subsetsO.push_back(subsetO);
        }
        // std::cout << '\n';
    }
    // print_subsets(subsetsO);
    return subsetsO;
}

template<typename PlanTable>
void MyPlanEnumerator::operator()(enumerate_tag, PlanTable &PT, const QueryGraph &G, const CostFunction &CF) const
{
    const AdjacencyMatrix &M = G.adjacency_matrix();
    auto &CE = Catalog::Get().get_database_in_use().cardinality_estimator();
    cnf::CNF condition; // Use this as join condition for PT.update(); we have fake cardinalities, so the condition
                        // doesn't matter.

    // TODO 3: Implement algorithm for plan enumeration (join ordering).
    std::vector<size_t> sourceIds;
    for (const auto &ds : G.sources()) {
        sourceIds.push_back(ds->id());
    }
    // sourceIds.push_back(0);
    // sourceIds.push_back(1);
    // sourceIds.push_back(2);
    // std::cout << "sourceIds " << sourceIds.size() << '\n';   

    for(size_t planSize = 2; planSize <= sourceIds.size(); planSize++) {
        std::vector<QueryGraph::Subproblem> subsets = connected_subsets(sourceIds, planSize, M);
        for(auto subset: subsets) {
            std::vector<QueryGraph::Subproblem> subsetsO = connected_subsetsO(subset, M);
            std::vector<QueryGraph::Subproblem> computed;
            for(auto left : subsetsO) {
                // check for symmetric cases and do not calculate them
                if (std::find(computed.begin(), computed.end(), left) == computed.end()) {
                    QueryGraph::Subproblem right(subset & ~left);
                    if(M.is_connected(right)) {
                        computed.push_back(right);
                        PT.update(G, CE, CF, left, right, condition);
                    }
                }
                
            }
        }
    }
}

template void MyPlanEnumerator::operator()<PlanTableSmallOrDense&>(enumerate_tag, PlanTableSmallOrDense&, const QueryGraph&, const CostFunction&) const;
template void MyPlanEnumerator::operator()<PlanTableLargeAndSparse&>(enumerate_tag, PlanTableLargeAndSparse&, const QueryGraph&, const CostFunction&) const;


