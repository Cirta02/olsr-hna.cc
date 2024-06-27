#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/udp-client-server-helper.h"

#include <iostream>
#include <fstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OlsrCsmaThroughput");

// Déclaration des variables globales pour les statistiques
uint32_t packetsSent = 0;
uint32_t packetsReceived = 0;
double totalThroughput = 0.0;

// Fonction de réception de paquets
void ReceivePacket(Ptr<Socket> socket)
{
    NS_LOG_UNCOND("Received one packet!");
    packetsReceived++;
}

// Fonction de génération de trafic
static void GenerateTraffic(Ptr<Socket> socket, uint32_t pktSize, uint32_t pktCount, Time pktInterval)
{
    if (pktCount > 0)
    {
        socket->Send(Create<Packet>(pktSize));
        packetsSent++;
        Simulator::Schedule(pktInterval,
                            &GenerateTraffic,
                            socket,
                            pktSize,
                            pktCount - 1,
                            pktInterval);
    }
    else
    {
        socket->Close();
    }
}

// Fonction pour calculer le throughput à la fin de la simulation
void CalculateThroughput()
{
    totalThroughput = (packetsReceived * 967 * 8.0) / (2.0 * 1000000.0); // Exemple : Utilisation directe des valeurs pour la démonstration
    std::cout << "Total Throughput: " << totalThroughput << " Mbps" << std::endl;
}

int main(int argc, char *argv[])
{
    // Paramètres de simulation
    std::string phyMode("DsssRate1Mbps");
    double rss = -67;           // dBm
    uint32_t packetSize = 967;  // bytes
    uint32_t numPackets = 2;
    double interval = 2.0;      // seconds
    uint32_t numOlsrNodes = 5;  // Nombre de nœuds OLSR

    // Paramètres de ligne de commande
    CommandLine cmd;
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("rss", "received signal strength", rss);
    cmd.AddValue("packetSize", "size of application packet sent", packetSize);
    cmd.AddValue("numPackets", "number of packets generated", numPackets);
    cmd.AddValue("interval", "interval (seconds) between packets", interval);
    cmd.AddValue("numOlsrNodes", "Number of OLSR nodes", numOlsrNodes);
    cmd.Parse(argc, argv);

    // Convertir l'intervalle en objet Time
    Time interPacketInterval = Seconds(interval);

    // Initialisation du réseau NS-3
    NodeContainer olsrNodes;
    olsrNodes.Create(numOlsrNodes);

    NodeContainer csmaNodes;
    csmaNodes.Create(2); // Deux nœuds CSMA pour chaque simulation

    // Initialisation des helpers pour le réseau Wifi et CSMA
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy;
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(rss));
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer wifiDevices = wifi.Install(wifiPhy, wifiMac, olsrNodes);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(olsrNodes);

    // Configuration du protocole OLSR
    OlsrHelper olsr;
    Ipv4StaticRoutingHelper staticRouting;

    Ipv4ListRoutingHelper list;
    list.Add(staticRouting, 0);
    list.Add(olsr, 10);

    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(olsrNodes);
    internet.Install(csmaNodes);

    // Assignation des adresses IP
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces;
    wifiInterfaces = address.Assign(wifiDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces;
    csmaInterfaces = address.Assign(csmaDevices);

    // Configuration des sockets pour la communication
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> recvSink = Socket::CreateSocket(csmaNodes.Get(0), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 80);
    recvSink->Bind(local);
    recvSink->SetRecvCallback(MakeCallback(&ReceivePacket));

    Ptr<Socket> source = Socket::CreateSocket(olsrNodes.Get(0), tid);
    InetSocketAddress remote = InetSocketAddress(csmaInterfaces.GetAddress(0, 1), 80);
    source->Connect(remote);

    // Planification de l'envoi du trafic
    Simulator::ScheduleWithContext(source->GetNode()->GetId(),
                                   Seconds(15.0),
                                   &GenerateTraffic,
                                   source,
                                   packetSize,
                                   numPackets,
                                   interPacketInterval);

    // Arrêt de la simulation et destruction
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Calcul du throughput
    CalculateThroughput();

    Simulator::Destroy();

    return 0;
}

