#ifndef GRAPH_NODE_BITONIC_SORT_LIB
#define GRAPH_NODE_BITONIC_SORT_LIB
#include <vector>
#include "GraphNode.h"
using namespace std;
class GraphNodeBitonicSortLib{
    public:
	block* isAscending;
	NodeComparator* comparator;
    TinyGarblePI_SH* TGPI_SH;

	GraphNodeBitonicSortLib(NodeComparator* comparator, TinyGarblePI_SH* TGPI_SH): comparator(comparator), TGPI_SH(TGPI_SH){}

	void sort(const auto& nodes, auto isAscending) {
		this->isAscending = isAscending;
		bitonicSort(nodes, 0, nodes.size(), isAscending);
	}

	void bitonicSort(const auto& nodes, int start, int n, auto dir) {
		if (n > 1) {
			int m = n / 2;
			bitonicSort(nodes, start, m, isAscending);
            auto rdir = TGPI_SH->TG_int(1);
            TGPI_SH->not_(rdir, dir, 1);
			bitonicSort(nodes, start + m, n - m, rdir);
            TGPI_SH->clear_TG_int(rdir);
			bitonicMerge(nodes, start, n, dir);
//			bitonicSort(a, data, start, m, not(dir));
//			bitonicSort(a, data, start + m, n - m, dir);
//			bitonicMerge(a, data, start, n, dir);
		}
	}

	void bitonicMerge(const auto& nodes, int start, int n, auto dir) {
		if (n > 1) {
			int m = compareAndSwapFirst(nodes, start, n, dir);
			bitonicMerge(nodes, start, m, dir);
			bitonicMerge(nodes, start + m, n - m, dir);
		}
	}

	int compareAndSwapFirst(const auto& nodes, int start, int n, auto dir) {
		int m = greatestPowerOfTwoLessThan(n);
		for (int i = start; i < start + n - m; i++) {
			compareAndSwap(nodes, i, i + m, dir);
		}
		return m;
	}

	void compareAndSwap(const vector<GraphNode*>& nodes, int i, int j, block* dir) {
        // cout << "before swap:" << endl;
		// nodes[i]->printNode(TGPI_SH);
		// nodes[j]->printNode(TGPI_SH);
		
		auto lessthan = TGPI_SH->TG_int(1);
        comparator->lt(lessthan, nodes[i], nodes[j], TGPI_SH);
        auto swap = TGPI_SH->TG_int(1);
        TGPI_SH->xor_(swap, lessthan, dir, 1);
		// cout << "reach cmp&swap" << endl;

		// auto ni_members = nodes[i]->get_member_ptrs(), nj_members = nodes[j]->get_member_ptrs();
		auto node_bit_widths = nodes[i]->get_member_bit_widths();

    	auto ni = nodes[i]->flatten(TGPI_SH);
    	auto nj = nodes[j]->flatten(TGPI_SH);

        auto node_bit_width = nodes[0]->total_bit_width();
        auto s = TGPI_SH->TG_int(node_bit_width);
		int offset = 0;
		for(int i = 0; i < node_bit_widths.size(); i++){
			auto member_bit_width = node_bit_widths[i];
			auto tmp_s = s + offset, tmp_ni = ni + offset, tmp_nj = nj + offset;
			TGPI_SH->ifelse(tmp_s, swap, tmp_ni, tmp_nj, member_bit_width);
			TGPI_SH->xor_(tmp_s, tmp_s, tmp_ni, member_bit_width);

			TGPI_SH->xor_(tmp_nj, tmp_nj, tmp_s, member_bit_width);
			TGPI_SH->xor_(tmp_ni, tmp_ni, tmp_s, member_bit_width);

			offset += member_bit_width;
		}
        // TGPI_SH->ifelse(s, swap, ni, nj, node_bit_width);
        // TGPI_SH->xor_(s, s, ni, node_bit_width);

        // TGPI_SH->xor_(nj, nj, s, node_bit_width);
        // TGPI_SH->xor_(ni, ni, s, node_bit_width);

    	nodes[i]->unflatten(nj, TGPI_SH);
    	nodes[j]->unflatten(ni, TGPI_SH);

		// cout << "after swap:" << endl;
		// nodes[i]->printNode(TGPI_SH);
		// nodes[j]->printNode(TGPI_SH);

        TGPI_SH->clear_TG_int(lessthan);
        TGPI_SH->clear_TG_int(swap);
        delete[] ni;
        delete[] nj;
        TGPI_SH->clear_TG_int(s);
    }

	int greatestPowerOfTwoLessThan(int n) {
        int k = 1;
        while(k < n)
            k = k << 1;
        return k >> 1;
    }
};
#endif
