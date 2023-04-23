#ifndef GRAPH_NODE
#define GRAPH_NODE
#include <emp-tool/emp-tool.h>
#include <vector>
#include "tinygarble/program_interface_sh.h"
#include "NodeComparator.h"
using namespace std;
class GraphNode{
    public:
	const static int VERTEX_LEN = 16;

	block *u;
	block *v;
	block *isVertex;

	GraphNode(){}
	GraphNode(block* u, block* v, block* isVertex) {
		this->u = u;
		this->v = v;
		this->isVertex = isVertex;
	}

	protected:
    inline auto get_pointer(const auto base, int& offset, const int& inc) const {
        auto res = base + offset;
        offset += inc;
        return res;
    }

	inline void copy_from_member(block* dst, int& offset, const int& bit_width, block* src, TinyGarblePI_SH* TGPI_SH) const {
		auto tmp = get_pointer(dst, offset, bit_width);
		TGPI_SH->assign(tmp, src, bit_width);
	}

	inline void copy_to_member(block* dst, block* src, const int& bit_width, int& offset, TinyGarblePI_SH* TGPI_SH) const {
		TGPI_SH->assign(dst, src + offset, bit_width);
		offset += bit_width;
	}

	public:
	void send(NetIO* io, TinyGarblePI_SH* TGPI_SH){
		block* flattened = this->flatten(TGPI_SH);
		io->send_data(flattened, this->flattened_size());
		delete[] flattened;
	}

	void read(NetIO* io, TinyGarblePI_SH* TGPI_SH){
		block* flattened = new block[this->total_bit_width()];
		io->recv_data(flattened, this->flattened_size());
		this->unflatten(flattened, TGPI_SH);
		delete[] flattened;
	}

	void mux(GraphNode* ifFalse, GraphNode* ifTrue, block* condition, TinyGarblePI_SH* TGPI_SH) { // this = condition ? ifTrue : ifFalse;
		block* ifFalse1 = ifFalse->flatten(TGPI_SH);
		block* ifTrue1 = ifTrue->flatten(TGPI_SH);
		auto node_bit_widths = this->get_member_bit_widths();
		int offset = 0;
		for(int i = 0; i < node_bit_widths.size(); i++){
			auto member_bit_width = node_bit_widths[i];
			auto tmp_true = ifTrue1 + offset, tmp_false = ifFalse1 + offset;
			TGPI_SH->ifelse(tmp_true, condition, tmp_true, tmp_false, member_bit_width);
			offset += member_bit_width;
		}
		// TGPI_SH->ifelse(ifTrue1, condition, ifTrue1, ifFalse1, this->total_bit_width());
		this->unflatten(ifTrue1, TGPI_SH);
		delete[] ifFalse1;
		delete[] ifTrue1;
	}

	void swapEdgeDirections() {
		auto temp = this->u;
		this->u = this->v;
		this->v = temp;
	}

	virtual block* flatten(TinyGarblePI_SH* TGPI_SH) const{ // alloc, copy and return
		auto ret = TGPI_SH->TG_int(total_bit_width());
		int offset = 0;

		copy_from_member(ret, offset, VERTEX_LEN, u, TGPI_SH);
		copy_from_member(ret, offset, VERTEX_LEN, v, TGPI_SH);
		copy_from_member(ret, offset, 1, isVertex, TGPI_SH);
        return ret;
	}

	virtual void unflatten(block* flat, TinyGarblePI_SH* TGPI_SH) const{ // copy from flat to self
		int offset = 0;

		copy_to_member(u, flat, VERTEX_LEN, offset, TGPI_SH);
		copy_to_member(v, flat, VERTEX_LEN, offset, TGPI_SH);
		copy_to_member(isVertex, flat, 1, offset, TGPI_SH);
	}

	virtual size_t flattened_size() const {
		return 528; // sizeof(block) * (len(u) + len(v) + len(isVertex)) = 16 * (16 + 16 + 1) = 528
	}

	virtual uint64_t total_bit_width() const {
		return 33; // 2 * 16 + 1
	}

	virtual GraphNode* alloc_obj() const {
		block *data = new block[this->total_bit_width()];
		auto new_u = data;
		auto new_v = data + 16;
		auto new_isVertex = data + 32;
		return new GraphNode(new_u, new_v, new_isVertex);
	}

	virtual GraphNode* set_zero(TinyGarblePI_SH* TGPI_SH) {
		TGPI_SH->assign(u, (int64_t)0, VERTEX_LEN);
		TGPI_SH->assign(v, (int64_t)0, VERTEX_LEN);
		TGPI_SH->assign(isVertex, (int64_t)0, 1);
		return this;
	}

	virtual void free_obj(){
		delete[] u;
		delete[] v;
		delete[] isVertex;
		delete this;
	}

	// virtual vector<block*> get_member_ptrs() const {
	// 	return vector<block*>{u, v, isVertex};
	// }

	virtual vector<int64_t> get_member_bit_widths() const {
		return vector<int64_t>{GraphNode::VERTEX_LEN, GraphNode::VERTEX_LEN, 1};
	}

	virtual void printNode(TinyGarblePI_SH* TGPI_SH) const {
		cout << "u: " << TGPI_SH->reveal(u, GraphNode::VERTEX_LEN) << ", v: " << TGPI_SH->reveal(v, GraphNode::VERTEX_LEN) << ", isV: " << TGPI_SH->reveal(isVertex, 1, false) << endl;
	}

	GraphNode* getCopy(TinyGarblePI_SH* TGPI_SH) {
		block* flattened = this->flatten(TGPI_SH);
		GraphNode* ret = this->alloc_obj();
		ret->unflatten(flattened, TGPI_SH);
		delete[] flattened;
		return ret;
	}

	// sort on u and have the vertex last/first
	static NodeComparator* getComparator(TinyGarblePI_SH* TGPI_SH, bool isVertexLast);

	static NodeComparator* allVerticesFirst(TinyGarblePI_SH* TGPI_SH);
};

class SortUComparator : public NodeComparator{
	public:
	bool isVertexLast;
	SortUComparator(bool b):isVertexLast(b){}
	virtual void lt(block* dst, GraphNode* n1, GraphNode* n2, TinyGarblePI_SH* TGPI_SH) override{
		auto ai = TGPI_SH->TG_int(17), aj = TGPI_SH->TG_int(17);

		auto tmp = ai + 1;
		TGPI_SH->assign(tmp, n1->u, 16);
		tmp = aj + 1;
		TGPI_SH->assign(tmp, n2->u, 16);

		if(isVertexLast){
			TGPI_SH->assign(ai, n1->isVertex, 1);
			TGPI_SH->assign(aj, n2->isVertex, 1);
		}
		else{
			TGPI_SH->not_(ai, n1->isVertex, 1);
			TGPI_SH->not_(aj, n2->isVertex, 1);
		}

		TGPI_SH->lt(dst, ai, aj, 17);

		TGPI_SH->clear_TG_int(ai);
		TGPI_SH->clear_TG_int(aj);
	}
};

NodeComparator* GraphNode::getComparator(TinyGarblePI_SH* TGPI_SH, bool isVertexLast) {
	return new SortUComparator(isVertexLast);
}

class AllVerticesFirst : public NodeComparator{
	public:
	virtual void lt(block* dst, GraphNode* n1, GraphNode* n2, TinyGarblePI_SH* TGPI_SH) override{
		auto ai = TGPI_SH->TG_int(18), aj = TGPI_SH->TG_int(18);

		auto tmp = ai + 17;
		TGPI_SH->assign(tmp, (int64_t)0, 1);
		tmp = aj + 17;
		TGPI_SH->assign(tmp, (int64_t)0, 1);

		tmp = ai + 16;
		TGPI_SH->not_(tmp, n1->isVertex, 1);
		tmp = aj + 16;
		TGPI_SH->not_(tmp, n2->isVertex, 1);
		
		TGPI_SH->assign(ai, n1->u, 16);
		TGPI_SH->assign(aj, n2->u, 16);
		
		TGPI_SH->lt(dst, ai, aj, 18);

		TGPI_SH->clear_TG_int(ai);
		TGPI_SH->clear_TG_int(aj);
	}
};

NodeComparator* GraphNode::allVerticesFirst(TinyGarblePI_SH* TGPI_SH) {
	return new AllVerticesFirst();
}

#endif
