/*
 * Implementation of LogCabinRaftClient.
 */

#include "../env.h"
#include "lg-client.h"
#include "Client/Client.h" // LogCabin RAFT
#include <assert.h>
#include <fstream>
#include <iostream>
#include <unistd.h>

using LogCabin::Client::Cluster;
using LogCabin::Client::Tree;

std::string LogCabinRaftClusterConfig::getHost(int nodeNumber)
{
  return std::string("192.168.2.") + std::to_string(nodeNumber);
}

std::string LogCabinRaftClusterConfig::getHostPort(int nodeNumber)
{
  return std::string("192.168.2.") + std::to_string(nodeNumber) + std::string(":") + std::to_string(clusterPort);
}

bool LogCabinRaftClusterConfig::writeConfFile(const std::string& storageDir)
{
  std::ofstream ofs(confFile.c_str());
  if (!ofs.good())
  {
    std::cout << "error: Failed to open: " << confFile << std::endl;
    return false;
  }

  ofs << "storageModule = filesystem" << std::endl
      << "storagePath = " << storageDir << std::endl;
  ofs << "servers = ";
  bool first = true;
  for (int id = firstNodeId(); id <= lastNodeId(); id++)
  {
    if (!first)
    {
      ofs << ";";
    }
    first = false;
    ofs << getHostPort(id);
  }
  ofs << std::endl;

  ofs.close();
  return true;
}

void LogCabinRaftClusterConfig::launchCluster(int nNodes, int port)
{
  id2pid.clear();

  clusterPort = port;
  numNodes = nNodes;
  assert(numNodes >= 1);

  // Clear out test storage directory.
  std::string storageDir = RaftEnv::rootDir +
    std::string("/teststorage/logcabin");
  executeCommand((std::string("rm -rf ") + storageDir).c_str());

  // Write out logcabin.conf figuration file.
  // Put it into teststorage-- it's autogenerated.
  confFile = RaftEnv::rootDir + std::string("/teststorage/logcabin.conf");
  if (!writeConfFile(storageDir))
  {
    std::cout << "error: failed to write conf file" << std::endl;
    exit(1);
  }

  // Do bootstrap, in case it hasn't been done before.
  executeCommand((RaftEnv::rootDir + std::string("/logcabin/build/LogCabin --bootstrap --id 1 --config ") + confFile).c_str());

  // Start individual Raft nodes.
  // TODO: hmm... wait, we don't need to start the
  // first one again...
  for (int id = firstNodeId(); id <= lastNodeId(); id++)
  {
    if (!launchNode(id))
    {
      std::cout << "error: Failed to launch cluster; exiting" << std::endl;
      exit(1);
    }
  }

  // reconfigure once all nodes have joined
  sleep(1);
  std::string cmdStr = RaftEnv::rootDir + std::string("/logcabin/build/Examples/Reconfigure");
  for (int id = firstNodeId(); id <= lastNodeId(); id++)
  {
    cmdStr += std::string(" ") + getHostPort(id);
  }
  executeCommand(cmdStr.c_str());
}

void LogCabinRaftClusterConfig::stopCluster()
{
  for (int id = firstNodeId(); id <= lastNodeId(); id++)
  {
    if (!killNode(id))
    {
      std::cout << "error: Failed to kill node: " << id << std::endl;
    }
  }
}

bool LogCabinRaftClusterConfig::launchNode(int id)
{
  const std::string program = RaftEnv::rootDir + "/logcabin/build/LogCabin";
  const std::string idStr = std::to_string(id);

  // sigh... need char*, not const char*.
  char* idCopy = copyStr(idStr.c_str());
  char* confCopy = copyStr(confFile.c_str());
  char* progCopy = copyStr(program.c_str());

  char* const args[] = {progCopy, "--id", idCopy, "--config", confCopy, nullptr};
  pid_t pid = createProcess(program.c_str(), args);

  delete[] idCopy;
  delete[] confCopy;
  delete[] progCopy;
  if (pid == -1)
  {
    std::cout << "error: failed to launch subprocess" << std::endl;
    return false;
  }

  id2pid[id] = pid;

  return true;
}

bool LogCabinRaftClusterConfig::killNode(int id)
{
  if (id2pid.find(id) == id2pid.end())
  {
    return false;
  }
  pid_t pid = id2pid[id];
  id2pid.erase(id);
  return stopProcess(pid);
}

LogCabinRaftClient::LogCabinRaftClient(int id):
  RaftClient(id)
{
}

bool LogCabinRaftClient::createClient(RaftClusterConfig *config)
{
  cluster = nullptr;
  clusterConfig = config;
  return true;
}

void LogCabinRaftClient::destroyClient()
{
  delete cluster;
}

bool LogCabinRaftClient::connectToCluster(const std::string& hosts)
{
  cluster = new Cluster(hosts);

  return true;
}

bool LogCabinRaftClient::writeFile(const std::string& path,
				   const std::string& contents)
{
  LogCabin::Client::Result result;

  Tree tree = cluster->getTree();
  result = tree.write(path, contents);

  return (result.status == LogCabin::Client::Status::OK);
}

std::string LogCabinRaftClient::readFile(const std::string& path)
{
  std::string content;
  Tree tree = cluster->getTree();
  tree.read(path, content);
  return content;
}
