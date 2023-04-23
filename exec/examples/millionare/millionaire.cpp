// #include <emp-tool/emp-tool.h>
// #include <boost/program_options.hpp>
// #include <boost/format.hpp>
// #include "tinygarble/program_interface.h"
// #include "tinygarble/program_interface_sh.h"
// #include "tinygarble/TinyGarble_config.h"
#include <unistd.h>
#include "tinygarble/parallel/Machine.h"
#include "tinygarble/parallel/GraphNode.h"
#include "tinygarble/parallel/GatherFromEdges.h"
#include "tinygarble/parallel/ScatterToEdges.h"
#include "tinygarble/parallel/SortGadget.h"
using namespace std;
// namespace po = boost::program_options;

void sendRecieve(NetIO* io, auto dst, auto src, int len, bool up, int party=0, int gId=0){
	if(up){
		io->send_data(src, len);
		io->flush();
		io->recv_data(dst, len);
	}
	else{
		// sleep(2);
		io->recv_data(dst, len);
		io->send_data(src, len);
		io->flush();
	}

	// io->send_data(src, len);
	// io->flush();
	// cout << party << gId << "done send" << endl;
	// io->recv_data(dst, len);
	// cout << party << gId << "received" << endl;
}

void millionaire(auto machine){	
	auto TGPI = machine->TGPI_SH;
	uint64_t bit_width = 64;
    int64_t a1 = 12, a2 = 7;
	int64_t b1 = 17, b2 = 1;

	// string inputFile = TGPI->party == ALICE ? "./bin/in/garb.in" : "./bin/in/eval.in";
	// ifstream fin(inputFile.c_str(), std::ios::in);
	if (TGPI->party == ALICE){
        // fin >> a;
        cout << "(Bank" << machine->garblerId << ") ALICE's wealth: " << ((machine->garblerId == 0) ? a1 : a2) << "$" << endl;
    }
    else if(TGPI->party == BOB){
        // fin >> b;
        cout << "(Bank" << machine->garblerId << ") BOB's wealth: " << ((machine->garblerId == 0) ? b1 : b2) << "$" << endl;
    }

	// cout << "Input wealth: ";
    // if (TGPI->party == ALICE){
    //     cin >> a;
    //     cout << "ALICE's wealth: $" << a << endl;
    // }
    // else{
    //     cin >> b;
    //     cout << "BOB's wealth: $" << b << endl;
    // }
    
    auto a1_x = TGPI->TG_int_init(ALICE, bit_width, a1);
	auto a2_x = TGPI->TG_int_init(ALICE, bit_width, a2);
    auto b1_x = TGPI->TG_int_init(BOB, bit_width, b1);
	auto b2_x = TGPI->TG_int_init(BOB, bit_width, b2);

    TGPI->gen_input_labels();

    TGPI->retrieve_input_labels(a1_x, ALICE, bit_width);
	TGPI->retrieve_input_labels(a2_x, ALICE, bit_width);
    TGPI->retrieve_input_labels(b1_x, BOB, bit_width);
    TGPI->retrieve_input_labels(b2_x, BOB, bit_width);

	// auto other_a = TGPI->TG_int(bit_width), other_b = TGPI->TG_int(bit_width);
	// bool up = machine->garblerId != 0;
	// NetIO *io = !up ? machine->peersDown[0] : machine->peersUp[0];
	// sendRecieve(io, other_a, a_x, sizeof(block) * bit_width, up);
	// sendRecieve(io, other_b, b_x, sizeof(block) * bit_width, up);
	// cout << "after sr" << endl;

	auto sum_a = TGPI->TG_int(bit_width), sum_b = TGPI->TG_int(bit_width);
	TGPI->add(sum_a, a1_x, a2_x, bit_width);
	TGPI->add(sum_b, b1_x, b2_x, bit_width);
	// cout << TGPI->party << " " << machine->garblerId << " sum_a: " << TGPI->reveal(sum_a, bit_width) << endl;
	// cout << TGPI->party << " " << machine->garblerId << " sum_b: " << TGPI->reveal(sum_b, bit_width) << endl;

    auto res_x = TGPI->TG_int(1);
    TGPI->lt(res_x, sum_a, sum_b, bit_width);
	// TGPI->lt(res_x, other_a, other_b, bit_width);
	// cout << "after lt" << endl;
    int64_t res = TGPI->reveal(res_x, 1, false);

#if !SEC_SH
    if (TGPI->party == BOB) //authenticated garbling allows only evaluator to compute the result
#endif
        if (res == 1) cout << "BOB is richer" << endl;
        else cout << "ALICE is richer" << endl;

	TGPI->clear_TG_int(a1_x);
	TGPI->clear_TG_int(b1_x);
	TGPI->clear_TG_int(a2_x);
	TGPI->clear_TG_int(b2_x);
	TGPI->clear_TG_int(sum_a);
	TGPI->clear_TG_int(sum_b);
	TGPI->clear_TG_int(res_x);
	
	delete TGPI;
}

void compute(auto machine){
	auto TGPI = machine->TGPI_SH;
	uint64_t bit_width = 4;
    int64_t a1 = 2, a2 = 3;
	int64_t b1 = 7, b2 = 3;
	int64_t a, b;
	NetIO* io;
	bool up;
	if(machine->garblerId % 2 == 0){
		a = a1;
		b = b1;
		io = machine->logPeersDown[0];
		up = false;
	}
	else{
		a = a2;
		b = b2;
		io = machine->logPeersUp[0];
		up = true;
	}
	if(TGPI->party == ALICE){
		b1 = 4;
		b2 = 5;
	}
	auto a_x = TGPI->TG_int_init(ALICE, bit_width, a);
	auto b_x = TGPI->TG_int_init(BOB, bit_width, b);
	TGPI->gen_input_labels();
	TGPI->retrieve_input_labels(a_x, ALICE, bit_width);
	TGPI->retrieve_input_labels(b_x, BOB, bit_width);
	auto recv_a = TGPI->TG_int(bit_width), recv_b = TGPI->TG_int(bit_width);
	sendRecieve(io, recv_a, a_x, sizeof(block) * bit_width, up, TGPI->party, machine->garblerId);
	// io->flush();
	sendRecieve(io, recv_b, b_x, sizeof(block) * bit_width, up, TGPI->party, machine->garblerId);
	// io->flush();
	cout << TGPI->party << " " << machine->garblerId << ", send_a: " << TGPI->reveal(a_x, bit_width) << ", send_b: " << TGPI->reveal(b_x, bit_width) << endl;
	cout << TGPI->party << " " << machine->garblerId << ", recv_a: " << TGPI->reveal(recv_a, bit_width) << ", recv_b: " << TGPI->reveal(recv_b, bit_width) << endl;

	// auto a_x = TGPI->TG_int(2 * bit_width), b_x = TGPI->TG_int(2 * bit_width);
	// memcpy(a_x, a1_x, sizeof(block) * bit_width);
	// memcpy(a_x + bit_width, a2_x, sizeof(block) * bit_width);
	// memcpy(b_x, b1_x, sizeof(block) * bit_width);
	// memcpy(b_x + bit_width, b2_x, sizeof(block) * bit_width);

	// auto res1 = TGPI->TG_int(1), res2 = TGPI->TG_int(1);
	// TGPI->eq(res1, a1_x, b1_x, bit_width);
	// TGPI->eq(res2, a2_x, b2_x, bit_width);
	// cout << TGPI->party << " " << machine->garblerId << ", res1: " << TGPI->reveal(res1, 1, false) << ", res2: " << TGPI->reveal(res2, 1, false) << endl;
}



int main(int argc, char** argv) {
	// cout << sizeof(block) << " " << sizeof(int) << endl;
	// int party = 1, port = 1234;
	// string netlist_address;
	// string server_ip;
	
	// po::options_description desc{"Yao's Millionair's Problem \nAllowed options"};
	// desc.add_options()  //
	// ("help,h", "produce help message")  //
	// ("party,k", po::value<int>(&party)->default_value(1), "party id: 1 for garbler, 2 for evaluator")  //
	// ("port,p", po::value<int>(&port)->default_value(1234), "socket port")  //
	// ("server_ip,s", po::value<string>(&server_ip)->default_value("127.0.0.1"), "server's IP.")
	// ("sh", "semi-honest setting (default is malicious)");
	
	// po::variables_map vm;
	// try {
	// 	po::parsed_options parsed = po::command_line_parser(argc, argv).options(desc).allow_unregistered().run();
	// 	po::store(parsed, vm);
	// 	if (vm.count("help")) {
	// 		cout << desc << endl;
	// 		return 0;
	// 	}
	// 	po::notify(vm);
	// }catch (po::error& e) {
	// 	cout << "ERROR: " << e.what() << endl << endl;
	// 	cout << desc << endl;
	// 	return -1;
	// }
		
	// NetIO* io = new NetIO(party==ALICE ? nullptr:server_ip.c_str(), port, true);
	// io->set_nodelay();
	
	// TinyGarblePI_SH* TGPI_SH;
	// TinyGarblePI* TGPI; 
	
	// if (vm.count("sh")){
	// 	cout << "testing program interface in semi-honest setting" << endl;
	// 	TGPI_SH = new TinyGarblePI_SH(io, party);
	// 	io->flush();
	// 	millionaire(TGPI_SH);		
	// }
	// else {
	// 	cout << "Millionair's Problem in malicious setting" << endl;
	// 	TGPI = new TinyGarblePI(io, party, 192, 64);
	// 	io->flush();
	// 	millionaire(TGPI);
	// }
	
	// delete io;
	Machine* machine = new Machine(argc, argv);
	// GatherFromEdges(machine->TGPI_SH, machine, true, nullptr, nullptr, nullptr);
	// ScatterToEdges(machine->TGPI_SH, machine, true, nullptr);
	millionaire(machine);
	// compute(machine);
	return 0;
}
