#ifndef MACHINE_H
#define MACHINE_H
#include <emp-tool/emp-tool.h>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include "tinygarble/program_interface.h"
#include "tinygarble/program_interface_sh.h"
#include "tinygarble/TinyGarble_config.h"

using namespace std;
namespace po = boost::program_options;

class Machine {
    // static bool DEBUG = false;
    public:
	int garblerId;
	int peerPort;
	int totalMachines;
	int logMachines;
	bool isGen;
    int inputLength;
    bool localMode;
	// private Gadget gadget;
	// private CompEnv env;
    // TinyGarblePI* TGPI = nullptr;
    TinyGarblePI_SH* TGPI_SH = nullptr;
    NetIO* partyIO = nullptr;
	NetIO** peersUp = nullptr;
	NetIO** peersDown = nullptr;

    
	int numberOfIncomingConnections;
	int numberOfOutgoingConnections;

    Machine(int garblerId,
            int peerPort,
			int totalMachines,
			bool isGen,
			int inputLength,
            bool localMode
            // TinyGarblePI* TGPI
            ) : garblerId(garblerId), totalMachines(totalMachines), isGen(isGen), inputLength(inputLength), peerPort(peerPort), localMode(localMode){
        this->logMachines = (int)log2(totalMachines);
        this->numberOfIncomingConnections = getNumberOfIncomingConnections(garblerId);
        this->numberOfOutgoingConnections = getNumberOfIncomingConnections(totalMachines - garblerId - 1);
    
        // set default peer port
        if (this->peerPort == -1) this->peerPort = isGen ? 50000 : 55000; 
        string party_ip = getIPByID(garblerId, !isGen);

        cout << isGen << " " << party_ip.c_str() << " " << 35000 + garblerId << endl;
        this->partyIO = new NetIO(isGen ? nullptr : party_ip.c_str(), 35000 + garblerId, true);
        partyIO->set_nodelay();
        
        cout << "testing program interface in semi-honest setting" << endl;
        this->TGPI_SH = new TinyGarblePI_SH(partyIO, isGen ? ALICE : BOB);
        partyIO->flush();

        if(logMachines > 0){
            // peersDown/Up newed, init at length numberOfIncoming/OutGoingConnections
            this->peersDown = new NetIO*[numberOfIncomingConnections];
            this->peersUp = new NetIO*[numberOfOutgoingConnections];
            // connect to peers
            connect();
        }
        }

    Machine(int argc, char **argv){
        int party, partyPort;
        string netlist_address;
        string server_ip;

        int garblerId;
        int peerPort;
        int totalMachines;
        int inputLength;
        bool localMode;
        
        po::options_description desc{"Yao's Millionair's Problem \nAllowed options"};
        desc.add_options()  //
        ("help,h", "produce help message")  //
        ("party,k", po::value<int>(&party)->default_value(1), "party id: 1 for garbler, 2 for evaluator")  //
        // ("partyPort,p", po::value<int>(&partyPort)->default_value(35000), "party port(base)")  //
        // ("server_ip,s", po::value<string>(&server_ip)->default_value("127.0.0.1"), "server's IP.") //
        ("garblerId", po::value<int>(&garblerId)->default_value(0), "garbler ID") //
        ("peerPort", po::value<int>(&peerPort)->default_value(-1), "peer port(base)") //
        ("totalMachines", po::value<int>(&totalMachines)->default_value(1), "total machines for each party") //
        ("inputLength", po::value<int>(&inputLength)->default_value(0), "length of input") //
        ("isLocal", po::value<bool>(&localMode)->default_value(true), "run in local mode") //
        ("sh", "semi-honest setting (default is semi)");
        
        po::variables_map vm;
        try {
            po::parsed_options parsed = po::command_line_parser(argc, argv).options(desc).allow_unregistered().run();
            po::store(parsed, vm);
            if (vm.count("help")) {
                cout << desc << endl;
                exit(0);
            }
            po::notify(vm);
        }catch (po::error& e) {
            cout << "ERROR: " << e.what() << endl << endl;
            cout << desc << endl;
            exit(-1);
        }

        // placement_new
        new (this)Machine(garblerId, peerPort, totalMachines, (party==ALICE), inputLength, localMode);
        cout << "machine initialized." << endl;
    }

    int getNumberOfIncomingConnections(int machineId){
        int k = 0;
        while(true){
            if (machineId >= totalMachines - (1 << k)) return k;
            k++;
        }
    }

    void connect(){
		// TODO(kartiknayak): This may necessitate 2^x input length
        listenFromPeer();
        connectToPeers();

        // if (garblerId == 0) {
        // 	setRInFile();
        // }
        // if (garblerId > 0) {
        // 	GCGenComp.R = GCSignal.receive(peersUp[0]);
        // 	setRInFile();
        // }
        // if (garblerId < totalMachines - 1) {
        // 	GCGenComp.R.send(peersDown[0]);
        // 	peersDown[0].flush();
        // }
//        debug(" " + env.getParty() + " connected?");
	}

    void listenFromPeer(){
		// Socket clientSock = null;
		// ServerSocket serverSocket = new ServerSocket(port);
        for (int i = 0; i < numberOfIncomingConnections; i++) {
            int port = peerPort + garblerId * logMachines + i;
            cout << "listening on port: " << port << endl;
            NetIO* io = new NetIO(nullptr, port);
            io->set_nodelay();
            peersDown[i] = io;
            // cout << "connceted" << endl;
            // io->send_data(&garblerId, sizeof(int));
            // io->flush();
            // int id;
            // io->recv_data(&id, sizeof(int));
            // cout << "id received: " << id << endl;
            // int index = (int)log2(id - garblerId);
            // peersDown[index] = io;
        	// clientSock = serverSocket.accept();
        	// OutputStream os = new BufferedOutputStream(clientSock.getOutputStream());
        	// InputStream     is = new BufferedInputStream(clientSock.getInputStream());
        	// Network channel = new Network(is, os, clientSock);
        	// int id = channel.readInt();
			// int index = Utils.log2(id - garblerId);
			// debug("Accepted a connection from " + id + ". Stored at index " + index);
			// peersDown[index] = channel;
			// debug(id + " peerIsDown " + peersDown[index].hashCode());
        }
        // serverSocket.close();
	}

	void connectToPeers(){
		for (int i = 0; i < numberOfOutgoingConnections; i++) {
            // TODO: use correct IP
            int peerId = garblerId - (1 << i), dstPort = peerPort + peerId * logMachines + i;
            string peer_ip = getIPByID(peerId, isGen);
            cout << "connecting to " << peer_ip << " at port: " << dstPort << endl;
            NetIO* io = new NetIO(peer_ip.c_str(), dstPort);
            io->set_nodelay();
            // cout << "connected" << endl;
            // int id;
            // io->recv_data(&id, sizeof(int));
            // cout << "id received: " << id << endl;
            // assert(peerId == id);
            // io->send_data(&garblerId, sizeof(int));
            // io->flush();
            // cout << "id sent: " << garblerId << endl;
            peersUp[i] = io;
			// debug("I'm trying to connect to " + (garblerId - (1 << i)) + " at " + (peerPort + garblerId - (1 << i)) + ". Storing connection at " + i);
			// String peerIp = null;
			// peerIp = isGen ? IPManager.gIp[(garblerId - (1 << i))] : IPManager.eIp[(garblerId - (1 << i))];
			// peersUp[i] = new Client();
			// ((Client) peersUp[i]).connect(peerIp, peerPort + garblerId - (1 << i));
			// peersUp[i].writeInt(garblerId);
			// peersUp[i].flush();
			// debug((garblerId - (1 << i)) + "peerOsUp " + peersUp[i].hashCode());
		}
		// debug("I'm done connecting ");
	}

    ~Machine(){
        cout << "machine deconstructing..." << endl;
        delete TGPI_SH;
        // delete TGPI;
        delete partyIO;
        for(int i = 0; i < numberOfIncomingConnections; i++){
            delete peersDown[i];
        }
        for(int i = 0; i < numberOfOutgoingConnections; i++){
            delete peersUp[i];
        }
        delete[] peersUp;
        delete[] peersDown;
    }

    string getIPByID(int gId, bool isGen){
        if (localMode) return "127.0.0.1";
        else return "10.0.0." + to_string((isGen ? (gId + 1) : (gId + totalMachines + 1)));
    }
};

#endif
