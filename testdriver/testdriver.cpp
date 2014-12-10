/*
 * Test driver program. Right now it only demonstrates
 * use of the Client API.
 */

#include "../clients/logcabin/lg-client.h"
#include <assert.h>
#include <iostream>
#include <unistd.h>

int main(const int argc, const char *argv[])
{
  // Create cluster configuration.
  RaftClusterConfig *clusterConfig = new LogCabinRaftClusterConfig();
  clusterConfig->launchCluster(3, 61023);

  std::cout << "started cluster" << std::endl;

  // Configure packet loss on a Raft node.
  // It'll automatically unconfigure itself on
  // destruction.
  // Dropping packets from 2 nodes causes the program
  // to hang with the logcabin implementation, which
  // is good!
  PacketDropConfig packetDropper("192.168.2.1", 1.0);
  packetDropper.startDropping();
  //PacketDropConfig packetDropper2("192.168.2.3", 1.0);
  //packetDropper2.startDropping();

  // Create at least one client.
  RaftClient *client = new LogCabinRaftClient();
  bool yay = client->createClient(clusterConfig);
  assert(yay);

  std::cout << "created client" << std::endl;

  // TODO: how to spec multiple hosts?
  client->connectToCluster("logcabin:61023");

  //sleep(10);

  std::cout << "connected" << std::endl;
  
  if (!client->writeFile("/testfile", "testvalue"))
  {
    std::cerr << "Write failed" << std::endl;
  }
  std::cout << "write succeeded!" << std::endl;
  if (client->readFile("/testfile") != "testvalue") {
    std::cerr << "Read failed" << std::endl;
  }
  std::cout << "read succeeded!" << std::endl;

  //sleep(10); // Here to check if packetDropper works

  // Destroy client
  client->destroyClient();
  delete client;

  // Stop the cluster
  clusterConfig->stopCluster(3);
  delete clusterConfig;

  return 0;
}
