#include "../build/ns3/core-module.h"
#include <string>
#include <fstream>

int main (int argc, char *argv[])
{
    //.waf --run 180313_Nhan_MoHinhMang1 --seed_Number=Start_s --errorRate=
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

    double start_distance = 10;
    double end_distance = 20;
    double step_distance = 10;
    int count_distance = (end_distance - start_distance)/step_distance;

    for (double j=0 ; j<=count_distance ; j++)

    {

        char temp_s[200] = "./waf --run \"plc-udp-narrow --distance=";
                char *temp_s1 = new char[200];
                strcpy(temp_s1,temp_s);

                char temp_s2[200]="";
                sprintf(temp_s2,"%f",start_distance);
                strcat(temp_s1,temp_s2);


        temp_s1 = strcat(temp_s1,"\"");

        system(temp_s1);

start_distance += step_distance;

    }

     return 0;
}

//tạo 1 cái file



//system("mnt/c/ns-3-1/ns-allinone-3.28/ns-3.28 a=2");

//system("mnt/c/ns-3-1/ns-allinone-3.28/ns-3.28 a=3");
//system("mnt/c/ns-3-1/ns-allinone-3.28/ns-3.28 a=4");
//system("mnt/c/ns-3-1/ns-allinone-3.28/ns-3.28");
