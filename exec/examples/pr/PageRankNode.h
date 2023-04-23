#ifndef PAGE_RANK_NODE
#define PAGE_RANK_NODE
#include <iostream>
#include "tinygarble/parallel/GraphNode.h"
using namespace std;
class PageRankNode: public GraphNode {
	public:
	block* pr;
	block* l;
	const static int OFFSET = 20;
	const static int WIDTH = 40;

	PageRankNode(){}
	PageRankNode(block* u, block* v, block* isVertex, block* pr, block* l): GraphNode(u, v, isVertex), pr(pr), l(l) {}
	// PageRankNode(block* u, block* v, block* isVertex, TinyGarblePI_SH* TGPI_SH): GraphNode(u, v, isVertex) {
	// 	this->pr = TGPI_SH->TG_int(PageRankNode::WIDTH);
	// 	this->l = TGPI_SH->TG_int(PageRankNode::WIDTH);
	// }

    // ~PageRankNode(){
    //     delete[] pr;
    //     delete[] l;
    // }

	virtual block* flatten(TinyGarblePI_SH* TGPI_SH) const override { // alloc, copy and return
		auto ret = TGPI_SH->TG_int(total_bit_width());
		int offset = 0;

		copy_from_member(ret, offset, GraphNode::VERTEX_LEN, u, TGPI_SH);
		copy_from_member(ret, offset, GraphNode::VERTEX_LEN, v, TGPI_SH);
		copy_from_member(ret, offset, 1, isVertex, TGPI_SH);
		copy_from_member(ret, offset, WIDTH, pr, TGPI_SH);
		copy_from_member(ret, offset, WIDTH, l, TGPI_SH);
        return ret;
	}

    virtual void unflatten(block* flat, TinyGarblePI_SH* TGPI_SH) const override { // copy from flat to self
		int offset = 0;

		copy_to_member(u, flat, GraphNode::VERTEX_LEN, offset, TGPI_SH);
		copy_to_member(v, flat, GraphNode::VERTEX_LEN, offset, TGPI_SH);
		copy_to_member(isVertex, flat, 1, offset, TGPI_SH);
		copy_to_member(pr, flat, WIDTH, offset, TGPI_SH);
		copy_to_member(l, flat,WIDTH, offset, TGPI_SH);
    }

	virtual size_t flattened_size() const override {
		return sizeof(block) * total_bit_width();
	}

	virtual uint64_t total_bit_width() const override {
		return (2 * GraphNode::VERTEX_LEN + 1) + WIDTH * 2;
	}

	virtual GraphNode* alloc_obj() const override {
		auto data = new block[this->total_bit_width()];
        int offset = 0;

        auto new_u = get_pointer(data, offset, GraphNode::VERTEX_LEN);
		auto new_v = get_pointer(data, offset, GraphNode::VERTEX_LEN);
		auto new_isVertex = get_pointer(data, offset, 1);
        auto new_pr = get_pointer(data, offset, WIDTH);
        auto new_l = get_pointer(data, offset, WIDTH);
		// cout << "alloc: " << new_u << " " << new_v << " " << new_isVertex << " " << new_pr << " " << new_l << endl;
		return new PageRankNode(new_u, new_v, new_isVertex, new_pr, new_l);
	}

	virtual GraphNode* set_zero(TinyGarblePI_SH* TGPI_SH) override{
		TGPI_SH->assign(u, (int64_t)0, VERTEX_LEN);
		TGPI_SH->assign(v, (int64_t)0, VERTEX_LEN);
		TGPI_SH->assign(isVertex, (int64_t)0, 1);
		// TODO: assign 0 correctly
		TGPI_SH->assign(pr, (int64_t)0, WIDTH);
		TGPI_SH->assign(l, (int64_t)0, WIDTH);
		return this;
	}

	virtual void free_obj() override {
		// cout << "start free obj in prnode" << endl;
		// cout << "u: " << TGPI_SH->reveal(u, GraphNode::VERTEX_LEN) << endl;
		delete[] u;
		// delete[] v;
		// delete[] isVertex;
		// cout << "done free u v isV" << endl;
        // delete[] pr;
        // delete[] l;
		// cout << "done free pr l" << endl;
		delete this;
		// cout << "done free this" << endl;
	}

	// virtual vector<block*> get_member_ptrs() const override {
	// 	return vector<block*>{u, v, isVertex, pr, l};
	// }

	virtual vector<int64_t> get_member_bit_widths() const override {
		return vector<int64_t>{GraphNode::VERTEX_LEN, GraphNode::VERTEX_LEN, 1, WIDTH, WIDTH};
	}

	virtual void printNode(TinyGarblePI_SH* TGPI_SH) const override{
		cout << "u: " << TGPI_SH->reveal(u, GraphNode::VERTEX_LEN) << ", v: " << TGPI_SH->reveal(v, GraphNode::VERTEX_LEN) << ", isV: " << TGPI_SH->reveal(isVertex, 1, false) << ", pr: " << TGPI_SH->reveal(pr, PageRankNode::WIDTH) << ", l: " << TGPI_SH->reveal(l, PageRankNode::WIDTH) << endl;
	}
};
#endif
