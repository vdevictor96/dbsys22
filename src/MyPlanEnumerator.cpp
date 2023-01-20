#include "MyPlanEnumerator.hpp"


using namespace m;

static std::vector<QueryGraph::Subproblem> connected_subsets(size_t planSize, size_t sourcesSize, const AdjacencyMatrix &M) {
    // if planSize set to 0, then return all the connected subsets
    planSize = planSize == 0 ? sourcesSize : planSize;
    std::vector<QueryGraph::Subproblem> subsets;
    for (size_t bit = 1; bit < pow(2, sourcesSize); bit++) {
        QueryGraph::Subproblem subset(bit);
        if (subset.size() >= planSize && M.is_connected(subset)) {
            subsets.push_back(subset);
        }
    }
    return subsets;
}


static std::vector<QueryGraph::Subproblem> connected_subsetsO(QueryGraph::Subproblem &subset, const AdjacencyMatrix &M) {
    size_t max_value = uint64_t(subset);
    std::vector<QueryGraph::Subproblem> subsetsO;
    // from 1 to max_value, not included
    for (size_t bit = 1; bit < max_value; bit++) {
        // the bits that are 0 in the subset cannot be 1 in the subsetsO
        QueryGraph::Subproblem subsetO(bit);
        if (subsetO.is_subset(subset) && M.is_connected(subsetO)) {
            subsetsO.push_back(subsetO);
        }
    }
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
    size_t n = G.num_sources();
    for(size_t planSize = 2; planSize <= n; planSize++) {
        std::vector<QueryGraph::Subproblem> subsets = connected_subsets(planSize, n, M);
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


