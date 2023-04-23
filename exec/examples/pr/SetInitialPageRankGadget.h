#ifndef SET_INIT_PR
#define SET_INIT_PR
#include <vector>
#include "tinygarble/parallel/Gadget.h"
#include "PageRankNode.h"
using namespace std;
class SetInitialPageRankGadget: public Gadget {
	vector<GraphNode*> prNodes;
    public:
	SetInitialPageRankGadget(TinyGarblePI_SH* TGPI_SH, Machine* machine): Gadget(machine, TGPI_SH) {}

	SetInitialPageRankGadget* setInputs(vector<GraphNode*> prNodes) {
		this->prNodes = prNodes;
		return this;
	}

	virtual void secureCompute() override {
		// T[] one = env.inputOfAlice(Utils.fromFixPoint(1, PageRankNode.WIDTH, PageRankNode.OFFSET));
		// T[] zero = env.inputOfAlice(Utils.fromFixPoint(0, PageRankNode.WIDTH, PageRankNode.OFFSET));
        auto one = TGPI_SH->TG_int(PageRankNode::WIDTH);
        auto zero = TGPI_SH->TG_int(PageRankNode::WIDTH);
        // TODO: init one and zero correctly
		TGPI_SH->assign(one, (int64_t)1, PageRankNode::WIDTH);
		TGPI_SH->assign(zero, (int64_t)0, PageRankNode::WIDTH);
		for (auto node: prNodes) {
			auto prNode = dynamic_cast<PageRankNode*>(node);
            TGPI_SH->ifelse(prNode->pr, prNode->isVertex, one, zero, PageRankNode::WIDTH);
            TGPI_SH->ifelse(prNode->l, prNode->isVertex, zero, one, PageRankNode::WIDTH);
		}
        TGPI_SH->clear_TG_int(one);
        TGPI_SH->clear_TG_int(zero);
	}

};
#endif
