#include <iostream>
#include <sstream>
#include <fstream>
#include <time.h>

#include "../build/ns3/core-module.h"
#include "../build/ns3/nstime.h"
#include "../build/ns3/simulator.h"
#include "../build/ns3/packet.h"
#include "../build/ns3/output-stream-wrapper.h"
#include "../build/ns3/gnuplot.h"
#include "../build/ns3/trace-source-accessor.h"
#include "../build/ns3/nstime.h"
#include "../build/ns3/simulator.h"
#include "../build/ns3/packet.h"
#include "../build/ns3/output-stream-wrapper.h"
#include "../build/ns3/gnuplot.h"
#include "../build/ns3/trace-source-accessor.h"
#include "../build/ns3/plc.h"
#include "../build/ns3/plc-cable.h"
#include "../build/ns3/plc-noise.h"
#include "../build/ns3/log.h"
#include "../build/ns3/core-module.h"
#include "../build/ns3/applications-module.h"
#include "../build/ns3/network-module.h"
#include "../build/ns3/internet-module.h"
#include "../build/ns3/point-to-point-module.h"
#include "../build/ns3/ipv4-global-routing-helper.h"
#include "../build/ns3/netanim-module.h"
#include "../build/ns3/simple-net-device-helper.h"
#include "../build/ns3/ipv4-static-routing.h"
#include "../build/ns3/global-router-interface.h"
#include "../build/ns3/plc.h"
#include "../src/internet/model/ipv4-global-routing.h"
using namespace ns3;

int main (int argc, char *argv[])
{
        // Define spectrum model
        PLC_SpectrumModelHelper smHelper;
        Ptr<const SpectrumModel> sm, sm1;

        bool narrowBand = false;
        std::string cableStr("NAYY150SE");

        CommandLine cmd;
        cmd.AddValue("nb", "Use narrow-band spectrum spacing?", narrowBand);
        cmd.AddValue("cable", "Cable name. Can be one of the following:\n\t"
                        "AL3x95XLPE, NYCY70SM35, NAYY150SE, NAYY50SE, MV_Overhead", cableStr);
        cmd.Parse (argc, argv);

//                sm = smHelper.GetSpectrumModel(0, 500e3, 5);
        sm = smHelper.GetSpectrumModel(0,10e6,100);

Ptr<PLC_Cable> cable = CreateObject<PLC_NAYY150SE_Cable> (sm);

        // Create nodes
        Ptr<PLC_Node> n1 = CreateObject<PLC_Node> ();
        Ptr<PLC_Node> n2 = CreateObject<PLC_Node> ();
        n1->SetPosition(0,0,0);
        n2->SetPosition(20,0,0);

        PLC_NodeList nodes;
        nodes.push_back(n1);
        nodes.push_back(n2);

        // Link nodes
        CreateObject<PLC_Line> (cable, n1, n2);

        // Set up channel
        PLC_ChannelHelper channelHelper(sm);
        channelHelper.Install(nodes);
        Ptr<PLC_Channel> channel = channelHelper.GetChannel();

        // Create interfaces (usually done by the device helper)
        Ptr<PLC_TxInterface> txIf = CreateObject<PLC_TxInterface> (n1, sm);
        Ptr<PLC_RxInterface> rxIf = CreateObject<PLC_RxInterface> (n2, sm);

        // Add interfaces to the channel (usually done by the device helper)
        channel->AddTxInterface(txIf);
        channel->AddRxInterface(rxIf);

        channel->InitTransmissionChannels();
        channel->CalcTransmissionChannels();

        // Define transmit power spectral density
    Ptr<SpectrumValue> txPsd = Create<SpectrumValue> (sm);
    (*txPsd) = 1e-8; // -50dBm/Hz

    // The receive power spectral density computation is done by the channel
    // transfer implementation from TX interface to RX interface
        Ptr<PLC_ChannelTransferImpl> chImpl = txIf->GetChannelTransferImpl(PeekPointer(rxIf));
        NS_ASSERT(chImpl);
        Ptr<SpectrumValue> rxPsd = chImpl->CalculateRxPowerSpectralDensity(txPsd);
        Ptr<SpectrumValue> absSqrCtf = chImpl->GetAbsSqrCtf(0);

        // SINR is calculated by PLC_Interference (member of PLC_Phy)
        //PLC_NoiseSource N1;
        //N1.SetNoisePsd(sm1);
        PLC_Interference interference;
        //sm1 = smHelper.GetSpectrumModel(0, 10e6, 100);
        //PLC_ColoredNoiseFloor nf;
        //nf.PLC_ColoredNoiseFloor(2,3,4,sm1);
        Ptr<SpectrumValue> noiseFloor= Create<SpectrumValue> (sm);
        (*noiseFloor) = 1e-9;
        interference.SetNoiseFloor(noiseFloor);
        interference.InitializeRx(rxPsd);

        Ptr<const SpectrumValue> sinr = interference.GetSinr();

        NS_LOG_UNCOND("Transmit power spectral density:\n" << *txPsd << "\n");
        NS_LOG_UNCOND("Receive power spectral density:\n" << *rxPsd << "\n");
        NS_LOG_UNCOND("Noise power spectral density:\n" << *noiseFloor << "\n");
        NS_LOG_UNCOND("Signal to interference and noise ratio:\n" << *sinr);
        NS_LOG_UNCOND("Channel Transmit Function CTF (dB):\n" << 10.0 * Log10(*absSqrCtf));


        return EXIT_SUCCESS;
}
