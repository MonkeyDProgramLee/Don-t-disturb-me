#include "httplib.h"
#include <iostream>
#include <fstream>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <mutex>
#define PART_SIZE 10240
using std::string;
using std::vector;
using std::cin;
using std::cout;
using std::endl;
using namespace httplib;
namespace  bf = boost::filesystem;

class MyClient{
  private:
    int _host_idx;
    uint16_t port = 9000;
    vector<string> _online_list;
    vector<string> _file_list;
    std::mutex _mutex;
  private:
    //找到所有的主机
    bool GetALLhost(vector<string>& list){
      struct ifaddrs* addrs = NULL;
      struct sockaddr_in* ip = NULL;
      struct sockaddr_in* mask = NULL;
      getifaddrs(&addrs);
      while(addrs != NULL){
      ip = (struct sockaddr_in*)addrs->ifa_addr;
      mask = (struct sockaddr_in*)addrs->ifa_netmask;
      if(ip->sin_family != AF_INET) {
        addrs = addrs->ifa_next;
        continue;
      }
      if(ip->sin_addr.s_addr == inet_addr("127.0.0.1")){
        addrs = addrs->ifa_next;
        continue;
      }
      uint32_t net,host;
      net = ntohl(ip->sin_addr.s_addr&mask->sin_addr.s_addr);
      host = ntohl(~mask->sin_addr.s_addr);
      for(uint64_t i = 2 ;i < host - 1;++i){
        struct in_addr ip;
        ip.s_addr = htonl(net+i);
        list.push_back(inet_ntoa(ip));
      }
        addrs = addrs->ifa_next;
      }
      freeifaddrs(addrs);
      return true;
    }
    void GetPair(string& ip)
    {
      Client client(&ip[0],port);
      auto rep = client.Get("/hostpair");
      if(rep&&rep->status == 200)
      {
          std::cerr<<"host"<< ip <<" pair success"<<endl;
           std::lock_guard<std::mutex> Lock(_mutex);
          _online_list.push_back(ip);
      }
      else 
        std::cerr<<"host   "<<ip<<"    pair failed"<<endl;
    }
    bool GetOnlineHost(vector<string>&list){
      vector<std::thread> thread_pair(list.size());
      for(size_t i = 0;i < list.size();i++)
      {
        std::thread thr(&MyClient::GetPair,this,std::ref(list[i]));
        thread_pair[i] = std::move(thr);
        //cout<<i<<endl;
      }
      for(size_t i = 0;i < thread_pair.size();++i)
      {
        thread_pair[i].join();
      }
      return  true;
    }
    bool ShowOnlineHost()
    {
      int index = 0;
      //打印当前在线主机
      for(auto &i:_online_list)
      {
        cout<<"["<<index++<<"]"<<"   "<<i<<endl;
      } 
      cout<<"please choose"<<endl;
      fflush(stdout);
      //input your choose
      cin>>_host_idx;
      if(_host_idx < 0||_host_idx > _online_list.size())
      {
          _host_idx = -1;
          std::cerr<<"choose error"<<endl;
          return false;
      }
      return true;
    }
    bool GetFileList()
    {
      Client client(_online_list[_host_idx].c_str(),port);
      auto rsp =  client.Get("/list");
      if(rsp->status == 200)
      {
        boost::split(_file_list,rsp->body,boost::is_any_of("\n"));
      }
      else 
      {
        std::cerr<<"failed"<<endl;
      }
      return true;
    }
    bool ShowFileList(string& filename){
      int index = 0;
      for(auto &it:_file_list)
      {
        cout<<++index<<"."<<it<<endl;
      }
      cout<<"please chose:";
      fflush(stdout);
      int file_index;
      cin>>file_index;
      if(file_index < 0||file_index > _file_list.size())
      {
        std::cerr<<"choose error"<<endl;
        return false;
      }
      filename = _file_list[file_index];
      return  true;
      }
    int64_t GetFileSize(string& host,string& name)
    {
       int64_t ret = -1;
      Client client(host.c_str(),port);
      auto rep =  client.Head(name.c_str());
      if(rep->status == 200)
      {
        if(!rep->has_header("Content-Length"))
        {
             return -1;
        }
       string temp = rep->get_header_value("Content-Length");
       std::stringstream cur;
       cur<<temp;
       cur>>ret;
      }
      return  ret;
    }
    void  LoadCount(int64_t start,int64_t end,string& host,string& name,int& flag)
    {
      flag = 1;
      string uri = "/list/" + name;
      std::stringstream range_val;
      range_val<<"bytes="<<start<<"-"<<end;
      Client client(host.c_str(),port);
      Headers header;
      header.insert(std::make_pair("Range",range_val.str().c_str()));
      auto rsp = client.Get(uri.c_str(),header);
      cout<<rsp->status<<endl;
      if(rsp&&rsp->status == 206)
      {
        string realpath = "Download/"+name;
        if(!bf::exists("Download"))
        {
          bf::create_directory("Download");
        }
        std::ofstream file(realpath,std::ios::binary|std::ios::app|std::ios::out);
        if(!file.is_open())
        {
          std::cerr<<"file"<<""<<realpath<<" "<<"open failed"<<endl;
          flag = 0;
          return ;
        }
        file.seekp(start,std::ios::beg);
        file.write(&rsp->body[0],rsp->body.size());
        cout<<"myname is"<<flag<<endl;
        if(!file.good())
        {
          std::cerr<<"file"<<realpath<<"write body error"<<endl;
          file.close();
          flag = 0;
          return ;
        }
        else
        {
          cout<<start<<"--"<<end<<"successed"<<endl;
          file.close();
          flag = 1;
          return;
        }
      }
      cout<<"stauts != 206"<<endl;
    }
    bool DownloadFile(string &name)
    {
      string host = _online_list[_host_idx];
      string uri = "/list/" + name;
      Client client(host.c_str(),port);
      int64_t filesize = GetFileSize(host,uri);
      if(filesize < 0)
      {
        std::cerr<<"DownloadFile   "<<name<<"   failed"<<endl;
      }
      int part_count = filesize / PART_SIZE;
      cout<<part_count<<endl;
      vector<boost::thread>thread_arr(part_count+1);
      vector<int>flag(part_count+1);
      for(int i = 0;i <= part_count;++i)
      {
        int64_t start,end;
        start = (int64_t)i*PART_SIZE;
        end = (int64_t)(i+1)*PART_SIZE-1;
        if(i == part_count)
        {
          if(filesize % (PART_SIZE) == 0)
          {
            break;
          }
            end = filesize - 1;
        }
    boost::thread thr(&MyClient::LoadCount,this,start,end,host,name,std::ref(flag[i]));
        thread_arr[i] = move(thr);
      }
      bool ret = true;
     for(int i = 0;i <= part_count; ++i)
     {
       if(i == part_count
           &&filesize%(PART_SIZE) == 0)
         break;
       cout<<"flag["<<i<<"]="<<flag[i]<<endl;
       if(flag[i] == 0)
       {
         ret = false;
       cout<<"task  failed"<<endl;
       }
       thread_arr[i].join();
     }
     if(ret == false)
     {
       cout<<"file download failed"<<endl;
       return ret;
     }
     cout<<"file download success"<<endl;
     return ret;
    }
    int  Menu(){
      cout<<"1. find host"<<endl;
      cout<<"2. show onlinehost"<<endl;
      cout<<"3. show file list"<<endl;
      int choose;
      std::cin>>choose;
      return choose;
    }
  public:
    MyClient(int16_t svr_port = 9000)
      :port(svr_port)
    {}
    bool Start()
    {
      while(1)
      {
        int choose = Menu();
        vector<string> list;
        string filename;
        switch(choose)
        {
          case 0:
            exit(0);
          case 1:
            GetALLhost(list);
            GetOnlineHost(list);
            break;
          case 2:
            if(ShowOnlineHost() == false)
            {
              break;
            }
            GetFileList();
            break;
          case 3:
            ShowFileList(filename);
            DownloadFile(filename);
            break;
        }
      }
    }
};
int main()
{
  MyClient cli;
  cli.Start();
  return 0;
}
