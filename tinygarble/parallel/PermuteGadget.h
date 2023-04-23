#pragma GCC diagnostic ignored "-Wignored-attributes"
#ifndef PERMUTE_GADGET
#define PERMUTE_GADGET
#include <chrono>
#include <iostream>
#include <vector>
#include <stdlib.h>
#include "Gadget.h"
#include "GraphNode.h"
#include "Utils.h"
using namespace std;
class PermuteGadget: public Gadget {
    private:
    int switchControlsBase;
    bool isReverse;
    public:
    vector<block*> switchControls;

	PermuteGadget(Machine *machine, TinyGarblePI_SH* TGPI_SH): Gadget(machine, TGPI_SH){}

	PermuteGadget* setInputs(vector<block*> switchControls, bool isReverse=false) {
        this->switchControls = switchControls;
		this->isReverse = isReverse;
		return this;
	}

	virtual void secureCompute() override {}
	virtual void secureCompute(vector<GraphNode*>& nodes) override {
		auto start = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
		this->switchControlsBase = (!isReverse) ? 0 : requiredSwitchControlsLen(nodes.size(), machine);

        int step = 1, totalLength = nodes.size() * machine->totalMachines;
		while(step < totalLength){
			performStep(nodes, step);
			step *= 2;
		}
		step /= 4;
		while(step != 0){
			performStep(nodes, step);
			step /= 2;
		}

		auto totalTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - start;
		cout << "	done secure permute in: " << totalTime << "ms" << endl;
	}

    void condSwap(const vector<GraphNode*>& nodes, int i, int j, block* ifSwap){
        auto node_bit_widths = nodes[i]->get_member_bit_widths();
        auto ni = nodes[i]->flatten(TGPI_SH);
    	auto nj = nodes[j]->flatten(TGPI_SH);

        auto node_bit_width = nodes[0]->total_bit_width();
        auto s = TGPI_SH->TG_int(node_bit_width);
		int offset = 0;
		for(int i = 0; i < node_bit_widths.size(); i++){
			auto member_bit_width = node_bit_widths[i];
			auto tmp_s = s + offset, tmp_ni = ni + offset, tmp_nj = nj + offset;
			TGPI_SH->ifelse(tmp_s, ifSwap, tmp_ni, tmp_nj, member_bit_width);
			TGPI_SH->xor_(tmp_s, tmp_s, tmp_ni, member_bit_width);

			TGPI_SH->xor_(tmp_nj, tmp_nj, tmp_s, member_bit_width);
			TGPI_SH->xor_(tmp_ni, tmp_ni, tmp_s, member_bit_width);

			offset += member_bit_width;
		}

        nodes[i]->unflatten(nj, TGPI_SH);
    	nodes[j]->unflatten(ni, TGPI_SH);

        delete[] ni;
        delete[] nj;
        TGPI_SH->clear_TG_int(s);
    }

    void oneLayerCondSwitch(const vector<GraphNode*>& nodes, int step){
		for(int i = 0; i < nodes.size() / (2 * step); i++){
			for(int j = 0; j < step; j++){
				int base = step * 2 * i + j;
				condSwap(nodes, base, base + step, switchControls[switchControlsBase + i * step + j]);
			}
		}
	}

    void performStep(const vector<GraphNode*>& nodes, int step){
		if(step < nodes.size()){
			// intra-node case
            if(!isReverse){
                oneLayerCondSwitch(nodes, step);
                switchControlsBase += nodes.size() / 2;
            }
            else{
                switchControlsBase -= nodes.size() / 2;
                oneLayerCondSwitch(nodes, step);
            }
			
		}
		else{
			// inter-node case
			int diff = step / nodes.size();
			bool up = ((machine->garblerId / diff) % 2 == 0) ? false : true;
			NetIO *channel;
			int commMachine = (int)log2(diff);
			channel = up ? machine->logPeersUp[commMachine] : machine->logPeersDown[commMachine];
			
			auto receivedNodes = sendReceive(channel, nodes, up, TGPI_SH);

			auto concatenatedNodes = up ? concatenate(receivedNodes, nodes) : concatenate(nodes, receivedNodes);

            if(!isReverse){
                oneLayerCondSwitch(concatenatedNodes, step);
                switchControlsBase += nodes.size();
            }
            else{
                switchControlsBase -= nodes.size();
                oneLayerCondSwitch(concatenatedNodes, step);
            }

			for(int i = 0; i < receivedNodes.size(); i++){
                receivedNodes[i]->free_obj();
			}
		}
	}

    static int requiredSwitchControlsLen(int n, Machine* machine){
		int cnt = 0;
		int step = 1, totalLength = n * machine->totalMachines;
		while(step < totalLength){
			cnt += getPerformStepTotalSwitch(step, n);
			step *= 2;
		}
		step /= 4;
		while(step != 0){
			cnt += getPerformStepTotalSwitch(step, n);
			step /= 2;
		}
		return cnt;
	}

	static int getPerformStepTotalSwitch(int step, int n){
		if (step < n){
			return n / 2;
		}
		else{
			return n;
		}
	}

	static void genRandomShuffle(vector<block*>& switchControls, TinyGarblePI_SH* TGPI_SH){
		for(auto& item: switchControls){
			TGPI_SH->assign(item, rand() % 2, 1);
		}
	}
};

#endif
