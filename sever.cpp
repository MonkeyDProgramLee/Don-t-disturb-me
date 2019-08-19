#include "httplib.h"
#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>
#define SHARED_DIR "shared"
using std::string;
using namespace httplib;
namespace bf = boost::filesystem;
class Sever{
  private:
    //附近主机配对请求
    static void PairHandle(const httplib::Request& req, httplib::Response& rep)
    {
      rep.status = 200;
    }
    //文件列表请求处理
    static void ListHandle(const httplib::Request& req, httplib::Response& rep)
    {
      bf::directory_iterator item_begin(SHARED_DIR);
      bf::directory_iterator item_end;
      //string body
      while(item_begin != item_end)
      {
        if (bf::is_directory(item_begin->status())) {
          continue;
        }
        string name = item_begin->path().filename().string();
        std::cerr<<name<<std::endl;
        rep.body += name + '\n';
        item_begin++;
      }
        rep.status = 200;
    }
    static bool RangeHangle(string& Range,int64_t& start,int64_t& len)
    {
      size_t pos1 = Range.find("=");
      size_t pos2 = Range.find("-");
      if(pos1 == string::npos ||pos2 == string::npos)
      {
        return false;
      }
      else
      {
        string r_start;
        string r_end;
        int64_t end;
        r_start  = Range.substr(pos1+1,pos2-pos1-1);
        r_end = Range.substr(pos2+1,Range.size()-pos2-1);
        std::stringstream temp;
        temp<<r_start;
        temp>>start;
        temp.clear();
        temp<<r_end;
        temp>>end;
        len = end - start + 1;
        return  true;
      }
    }
    //文件下载请求处理
    static void DownloadHandle(const httplib::Request& req, httplib::Response& rep)
    {
      bf::path path(req.path);
      string name = SHARED_DIR;
      name = name + "/" + path.filename().string(); 
      std::cout<<name<<std::endl;
      if(!bf::exists(name))
      {
        std::cerr<<"open file:"<< name <<"failed"<<std::endl;
        rep.status = 404;
        return;
      }
      if(bf::is_directory(name))
      {
        rep.status = 403;
        return;
      }
      int64_t filesize = bf::file_size(name);
      if(req.method == "HEAD")
      {
        rep.status = 200;
        rep.set_header("Content-Length",std::to_string(filesize).c_str());
        return;
      } 
      else
      {
         if(!req.has_header("Range"))
         {
          rep.status = 400;
          return;
         }
         string range_val = req.get_header_value("Range");
         int64_t start,len;
         bool ret = RangeHangle(range_val,start,len);
         if(!ret)
         {
           rep.status = 400;
           return;
         }
         std::ifstream file(name,std::ios::binary);
         if(!file.is_open())
         {
           std::cerr<<"open   "<<name<<"  failed"<<std::endl;
           rep.status = 404;
           return;
         }
         rep.body.resize(len);
         file.seekg(start,std::ios::beg);
         file.read(&rep.body[0],len);
         if(!file.good())
         {
           std::cerr<<"read file"<<name<<"body errot"<<std::endl;
           rep.status = 500;
           return;
         }
         file.close();
         rep.status = 206;
         rep.set_header("Content-Type","application/octet-stream");
      }
    }
  public:
    Sever()
    {
      //创建共享目录
      if(!bf::exists(SHARED_DIR))
      {
         bf::create_directory(SHARED_DIR);
      }
    }
    bool Start(uint16_t port){
      _svr.Get("/hostpair",PairHandle);
      _svr.Get("/list",ListHandle);
      _svr.Get("/list/(.*)",DownloadHandle);
      _svr.listen("0.0.0.0",port);
    }
private:
  Server _svr;
};

int main()
{
  Sever srv;
  srv.Start(9000);
  return 0;
}
