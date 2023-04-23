#ifndef REVEAL_SORT_LIB
#define REVEAL_SORT_LIB
#include <vector>
#include "GraphNode.h"
using namespace std;
class RevealSortLib{
    public:
	block* isAscending;
	NodeComparator* comparator;
    TinyGarblePI_SH* TGPI_SH;

	RevealSortLib(NodeComparator* comparator, TinyGarblePI_SH* TGPI_SH): comparator(comparator), TGPI_SH(TGPI_SH){}

	void sort(auto& nodes, auto& indexes, auto isAscending) {
		this->isAscending = isAscending;
		bitonicSort(nodes, indexes, 0, nodes.size(), isAscending);
	}

	void bitonicSort(auto& nodes, auto& indexes, int start, int n, auto dir) {
		if (n > 1) {
			int m = n / 2;
			bitonicSort(nodes, indexes, start, m, isAscending);
            auto rdir = TGPI_SH->TG_int(1);
            TGPI_SH->not_(rdir, dir, 1);
			bitonicSort(nodes, indexes, start + m, n - m, rdir);
            TGPI_SH->clear_TG_int(rdir);
			bitonicMerge(nodes, indexes, start, n, dir);
		}
	}

	void bitonicMerge(auto& nodes, auto& indexes, int start, int n, auto dir) {
		if (n > 1) {
			int m = compareAndSwapFirst(nodes, indexes, start, n, dir);
			bitonicMerge(nodes, indexes, start, m, dir);
			bitonicMerge(nodes, indexes, start + m, n - m, dir);
		}
	}

	int compareAndSwapFirst(auto& nodes, auto& indexes, int start, int n, auto dir) {
		int m = greatestPowerOfTwoLessThan(n);
		for (int i = start; i < start + n - m; i++) {
			compareAndSwap(nodes, indexes, i, i + m, dir);
		}
		return m;
	}

	void compareAndSwap(vector<GraphNode*>& nodes, auto& indexes, int i, int j, block* dir) {
		auto lessthan = TGPI_SH->TG_int(1);
        comparator->lt(lessthan, nodes[i], nodes[j], TGPI_SH);
        auto swap = TGPI_SH->TG_int(1);
        TGPI_SH->xor_(swap, lessthan, dir, 1);

		if(TGPI_SH->reveal(swap, 1, false) == 1){
			auto tmpNode = nodes[i];
			nodes[i] = nodes[j];
			nodes[j] = tmpNode;

			auto tmpId = indexes[i];
			indexes[i] = indexes[j];
			indexes[j] = tmpId;

			TGPI_SH->clear_TG_int(lessthan);
			TGPI_SH->clear_TG_int(swap);
			return;
		}
    }

	int greatestPowerOfTwoLessThan(int n) {
        int k = 1;
        while(k < n)
            k = k << 1;
        return k >> 1;
    }
};
#endif
