/*
 * Test driver program. Right now it only demonstrates
 * use of the Client API.
 */

#include "../clients/logcabin/lg-client.h"
#include <assert.h>
#include <iostream>
#include <unistd.h>

using namespace std;

// Flag for current Raft implementation that's in use.
enum class RaftImplementation
{
  UNKNOWN,
  LOGCABIN
};
RaftImplementation raftImpl = RaftImplementation::UNKNOWN;

// Configuration for Raft cluster.
RaftClusterConfig *clusterConfig = nullptr;

// ... The clients.
vector<RaftClient*> clients;

// --- Functions ---

// Print the help and go away.
void help()
{
  cout << "From the project root directory: " << endl
       << "sudo ./testdriver/testdriver <impl> <nClients>" << endl
       << "\timpl: Raft implementation to use. Values={logcabin}." << endl
       << "\tnClients: Number of clients to start." << endl
       << "TODO: later we'll select a test to run." << endl;
  exit(0);
}

// Select an implementation from command-line argument.
void selectImplementation(const std::string& implStr)
{
  if (implStr == "logcabin")
  {
    raftImpl = RaftImplementation::LOGCABIN;
  }

  if (raftImpl == RaftImplementation::UNKNOWN)
  {
    cerr << "Unrecognized implementation: " << implStr << endl;
    help();
  }
}

// Allocate a new RaftClusterConfig adapted for this
// implementation.
RaftClusterConfig* createClusterConfig()
{
  switch (raftImpl)
  {
  case RaftImplementation::LOGCABIN:
    return new LogCabinRaftClusterConfig();
  default:
    help();
  }
}

// Create requested number of clients.
void createClients(int nClients)
{
  for (int i = 0; i < nClients; i++)
  {
    RaftClient *newClient = nullptr;
    switch (raftImpl)
    {
    case RaftImplementation::LOGCABIN:
      newClient = new LogCabinRaftClient();
      break;
    default:
      help();
    }
    bool yay = newClient->createClient(clusterConfig);
    assert(yay);
    clients.push_back(newClient);
  }
}

int main(const int argc, const char *argv[])
{
  if (argc < 3 || std::string(argv[0]) == "--help")
  {
    help();
  }

  selectImplementation(argv[1]);
  int nClients = std::stoi(argv[2]);

  // Identify and run all tests.
  // Reinitialize the cluster AND the file system
  // between each and every test.
  // TODO: tests could 'build' on each other,
  // but that can come in a later system.
  for (int i = 3; i < argc; i++)
  {
    // Determine what test is being run
    string testName = argv[i];
    cout << "testdriver: running test " << testName << endl;

    // Create cluster configuration.
    clusterConfig = createClusterConfig();
    assert(clusterConfig != nullptr);

    // Launch the cluster.
    clusterConfig->launchCluster(3, 61023);
    cout << "testdriver: started cluster" << endl;
    sleep(1);

    // Configure packet loss on a Raft node.
    // It'll automatically unconfigure itself on
    // destruction.
    // Dropping packets from 2 nodes causes the program
    // to hang with the logcabin implementation, which
    // is good!
    PacketDropConfig packetDropper("192.168.2.1", 1.0);
    //packetDropper.startDropping();
    //PacketDropConfig packetDropper2("192.168.2.3", 1.0);
    //packetDropper2.startDropping();

    // Create at least one client.
    createClients(nClients);
    cout << "created clients" << endl;

    // Connect clients to cluster.
    // TODO: cluster hostname is hardcoded
    for (RaftClient* client : clients)
    {
      string host = string("logcabin:") +
	to_string(clusterConfig->clusterPort);
      client->connectToCluster(host);
    }
    cout << "connected" << endl;
    sleep(1);

    // Start the test by having clients run commands.
    // TODO: identify what test is being run.
    // TODO: this should be done in separate threads.
    int id = 1;
    for(RaftClient* client : clients)
    {
      string testfile = string("/testfile") + to_string(id);
      if (!client->writeFile(testfile, "testvalue"))
      {
	cerr << "Write failed" << endl;
      }
      else
      {
	cout << "Write succeeded!" << endl;
      }

      if (client->readFile(testfile) != "testvalue")
      {
	cerr << "Read failed" << endl;
      }
      else
      {
	cout << "Read succeeded!" << endl;
      }

      if (!client->writeFile(testfile, "erased"))
      {
	cerr << "Erase failed" << endl;
      }
      else
      {
	cerr << "Erase succeeded!" << endl;
      }
      id++;
    }

    // TODO: wait until all clients are finished

    // Destroy clients
    sleep(1);
    cout << "testdriver: destroying clients" << endl;
    for (RaftClient* client : clients)
    {
      client->destroyClient();
      delete client;
    }

    // Stop the cluster
    clusterConfig->stopCluster(3);
    delete clusterConfig;
  }

  return 0;
}
