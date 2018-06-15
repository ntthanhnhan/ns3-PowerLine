#include<iostream>
#include<arpa/inet.h>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
using namespace std;

void error( char *msg)
{
 perror(msg);
 exit(EXIT_FAILURE);
}
int main()
{
 int sockfd;
 struct sockaddr_in serv, client;
 sockfd = socket(AF_INET,SOCK_DGRAM,0); //sockfd duoc dinh nghia la mot so nguyen va tra ve -1 neu no gap loi
                                        //neu no tra ve sockfd>0 lenh duoc thuc hien va socket duoc thiet lap voi cac tham so da cho
                                        //AF_INET la de chi ra rang no su dung giao thuc IPv4
                                        //SOCK_DRAM chi ra rang no ho tro datagram va no khong ket noi duoc

 bzero(&serv,sizeof(serv));
 serv.sin_family = AF_INET;
 serv.sin_addr.s_addr = htonl(INADDR_ANY);
 serv.sin_port = htons(32000); //dat cong cua may chu socket
// serv.sin_addr.s_addr = inet_addr("127.0.0.1");
 bind(sockfd,(struct sockaddr *)&serv,sizeof(serv));//lien ket voi socket

 char buffer[256];
 socklen_t l = sizeof(client);
 //socklen_t m = client;
// cout<<"\ngoing to recv\n";
 int rc= recvfrom(sockfd,buffer,sizeof(buffer),0,(struct sockaddr *)&client,&l);
 if(rc<0)
 {
 cout<<"ERROR READING FROM SOCKET";
 }
 cout<<"\n the message received is : "<<buffer<<endl;
 int rp= sendto(sockfd,"ack",2,0,(struct sockaddr *)&client,l);

 if(rp<0)
 {
 cout<<"ERROR writing to SOCKET";
 }

 /*****/ //Viec nhan va gui lai duoc thuc hien
//for (;;)
//{
    // char ack[3] = "ACK";    //message gui cho client duoc thiet lap
// socklen_t l = sizeof(client);    //chieu dai cua dia chi client duoc luu trong mot so nguyen l
// recvfrom(sockfd, mesg, 1490, 0, (struct sockaddr *)&client,&l);    //mesg: bo dem trong do thong bao duoc luu, bo dem giong voi do dai goi la 1498 byte trong truong hop nay
// sendto(sockfd, ack, 3, 0, (struct sockaddr *),&client, sizeof(client));
//}
 /*****/

 /*CLIENT*/
// bzero(&serv,sizeof(serv));
// int sockfd;
// sockfd = socket(AF_INET,SOCK_DGRAM,0);
// struct sockaddr_in serv,client;

// serv.sin_family = AF_INET;
 // serv.sin_addr.s_addr = inet_addr(serv);
// serv.sin_port = htons(32000);
//gettimeofday(&tv1, NULL); //thoi gian hien tai duoc luu trong struct co ten la tv1
 //sendto(socketfd,payload,strlen(payload),0,(struct sockaddr *),&seraddr,sizeof(seraddr))  //ham sendto duoc goi va goi tin duoc gui toi may chu

// char buffer[256];
// socklen_t l = sizeof(client);
 socklen_t m = sizeof(serv);
 //socklen_t m = client;
 cout<<"\ngoing to send\n";
 cout<<"\npls enter the mssg to be sent\n";
 fgets(buffer,256,stdin);
 sendto(sockfd,buffer,sizeof(buffer),0,(struct sockaddr *)&serv,m);
 recvfrom(sockfd,buffer,256,0,(struct sockaddr *)&client,&l);

}
