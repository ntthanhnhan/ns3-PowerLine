#include <iostream>
#include <fstream>
#include <string>

#include "ns3/wifi-net-device.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-phy.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/error-rate-model.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/ptr.h"
#include "ns3/mobility-model.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/vector.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include "ns3/command-line.h"
#include "ns3/flow-id-tag.h"
#include "ns3/wifi-tx-vector.h"
#define SIGNALSPREAD 28000000
#define SINRFILE "Test/Sinr250M.txt"
#define RESULTFILE "Test/Result250M.txt"

using namespace ns3;
double SinrArray[1147], ber[1147], pms[1147];
int i, packetsize = 1024;
int dFree = 10, adFree = 11;

double Factorial (double k)
{
  double fact = 1;
  while (k > 0)
    {
      fact *= k;
      k--;
    }
  return fact;
}


double Binomial (double k, double p, double n)
{
  double retval = Factorial (n) / (Factorial (k) * Factorial (n - k)) * std::pow (p, static_cast<double> (k)) * std::pow (1 - p, static_cast<double> (n - k));
  return retval;
}


double CalculatePdOdd (double ber, unsigned int d)
{
        NS_ASSERT ((d % 2) == 1);
        unsigned int dstart = (d + 1) / 2;
        unsigned int dend = d;
        double pd = 0;

        for (unsigned int i = dstart; i < dend; i++)
        {
                pd += Binomial (i, ber, d);
        }
        return pd;
}

double Log2 (double val)
{
  return std::log (val) / std::log (2.0);
}

double CalculatePdEven (double ber, unsigned int d)
{
        NS_ASSERT ((d % 2) == 0);
        unsigned int dstart = d / 2 + 1;
        unsigned int dend = d;
        double pd = 0;

        for (unsigned int i = dstart; i < dend; i++)
        {
                pd +=  Binomial (i, ber, d);
        }
        pd += 0.5 * Binomial (d / 2, ber, d);

        return pd;
}

double CalculatePd (double ber, unsigned int d)
{
  double pd;
  if ((d % 2) == 0)
    {
      pd = CalculatePdEven (ber, d);
    }
  else
    {
      pd = CalculatePdOdd (ber, d);
    }
  return pd;
}

int main(int argc, char *argv[])
{

        /*
        ** Catching Signal to interference and noise ratio input from our "small-test"
        */
        std::string Line;
        std::ifstream SinrFile(SINRFILE);
        if (!SinrFile.is_open())
        {
                std::cout << "Unable to open file\n";
        }
        else
        {

                Line.assign( (std::istreambuf_iterator<char>(SinrFile) ),
                                         (std::istreambuf_iterator<char>()    ) );
                SinrFile.close();
                std::stringstream StringArray(Line);
                double PhyRate = 0;
                for(i = 0; i <= 1146; i++)
                {
                        StringArray >> SinrArray[i];
                        PhyRate += log2(1+SinrArray[i]);
                }
                PhyRate *= (SIGNALSPREAD / 1146);

                std::ofstream SignalToNoise;
                SignalToNoise.open(RESULTFILE);
                SignalToNoise << "Ber\tWith FEC\n";

                for(i = 0; i <= 1146; i++)
                {
                        double EbNo = SinrArray[i] * SIGNALSPREAD / PhyRate;
                        double z = std::sqrt (EbNo);
                        ber[i] = 0.5 * erfc (z);
                        double pd = CalculatePd (ber[i], dFree);
                        double pmu = adFree * pd;
                        pmu = std::min (pmu, 1.0);
                        pms[i] = std::pow (1 - pmu, packetsize);
                        SignalToNoise << ber[i] << "\t" << pms[i] << "\n";
                }

                double BerSum = 0;
                double PmsSum = 0;
                for(i = 0; i <= 1146; i++)
                {
                        BerSum += ber[i];
                        PmsSum += pms[i];
                }
                BerSum /= 1146;
                PmsSum /= 1146;
                SignalToNoise << "Capasity: "<< PhyRate << "\n";
                SignalToNoise << "Ber Average: " << BerSum << "\n";
                SignalToNoise << "FEC Average: " << PmsSum << "\n";
                SignalToNoise.close();



        }
}
