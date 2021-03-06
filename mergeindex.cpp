//Author Phinfinity

#define _FILE_OFFSET_BITS 64
#include <glob.h>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <ctime>
#include <string>
#include <vector>
#include <queue>
#include "varbyteencoder.h"
using varbyteencoder::encode;
using varbyteencoder::decode;
using namespace std;
#define WRITE_BUF_SIZE 64*1024*1024
#define READ_BUF_SIZE 1*1024*1024
#define DICT_SEGMENT_SIZE 32*1024*1024

vector<pair<string,off_t> > dictionary;
int dictionary_size = 0;
vector<off_t> dict_offset_list;
double get_time(const timespec &a) {
   return a.tv_sec+double(a.tv_nsec)/1e9;
}
FILE *f;
FILE **tempf;
vector<vector<pair<int,int> > > current_element;
long inread = 0;
//current_element[i][j] is the (docid,freq) pair for the jth document of the current word in file i
string read_element(int i) {
   current_element[i].clear();
   char buf[50];
   assert(strlen(buf)<=22);
   inread+=strlen(buf)+1;
   if(fscanf(tempf[i],"%s%*c",buf)==EOF)
      return "";
   inread += decode(tempf[i],current_element[i]);
   /* USING VARBYTE DECODING
      int c;
      assert(fread(&c,sizeof(int),1,tempf[i]));
      inread+=c*4*2+4;
      while(c--) {
      int doc,freq;
      assert(fread(&doc,sizeof(int),1,tempf[i]));
      assert(fread(&freq,sizeof(int),1,tempf[i]));
      current_element[i].push_back(make_pair(doc,freq));
      }
      */
   return string(buf);
}
vector<string> filenames;
int n;
void get_file_names() {
   glob_t globbuf;
   glob("tempfile-*",0,0,&globbuf);
   for(size_t i = 0; i < globbuf.gl_pathc; i++) {
      filenames.push_back(string(globbuf.gl_pathv[i]));
   }
   n = filenames.size();
   fprintf(stderr,"%lu Files for merging to Index\n",filenames.size());
}
void write_dict_segment() {
   dict_offset_list.push_back(ftello(f));
   fprintf(f,"!dictseg ");
   int c = dictionary.size();
   fwrite(&c,sizeof(int),1,f);
   for(auto it : dictionary) {
      fprintf(f,"%s ",it.first.c_str());
      fwrite(&it.second,sizeof(off_t),1,f);
   }
   dictionary.clear();
   dictionary_size = 0;
}
int main(int argc , char**argv) {
   get_file_names();
   tempf = new FILE*[n];
   char *write_buf = (char*) malloc(WRITE_BUF_SIZE+5);
   char *read_buf = (char*)malloc(READ_BUF_SIZE*n);
   if(argc==2) {
      if(argv[1][0]=='-' && argv[1][1]=='\0')
	 f = stdout;
      else {
	 f = fopen(argv[1],"wb");
	 setvbuf(f,write_buf,_IOFBF,WRITE_BUF_SIZE); // 128MB BUffer
      }
   } else {
      f = fopen("searchindex.dat","wb");
      setvbuf(f,write_buf,_IOFBF,WRITE_BUF_SIZE); // 5MB BUffer
   }
   for(int i = 0; i < n ; i++)
      tempf[i] = fopen(filenames[i].c_str(),"r");
   for(int i = 0; i < n ; i++)
      setvbuf(tempf[i],read_buf+i*READ_BUF_SIZE,_IOFBF,READ_BUF_SIZE); //2MB Buf 
   current_element.resize(n);
   typedef pair<string,int> pq_t;
   priority_queue<pq_t,vector<pq_t>,greater<pq_t> > pq;
   for(int i = 0; i < n ; i++) {
      string s = read_element(i);
      pq.push(make_pair(s,i));
   }
   int wcnt = 0;
   long wlsum = 0;
   timespec ltime,start_time;
   clock_gettime(CLOCK_MONOTONIC,&ltime);
   clock_gettime(CLOCK_MONOTONIC,&start_time);
   long lread = 0;
   while(!pq.empty()) {
      string s = pq.top().first;
      if(dictionary_size >= DICT_SEGMENT_SIZE) {
	 write_dict_segment();
      }
      dictionary.push_back(make_pair(s,ftello(f)));
      dictionary_size += s.size()+9;
      wcnt++;
      wlsum += s.size();
      if(wcnt%100000==0){
	 timespec etime;
	 clock_gettime(CLOCK_MONOTONIC,&etime);
	 float dur = get_time(etime) - get_time(ltime);
	 float rate = float(inread-lread)/(1024*1024);
	 rate/=dur;
	 dur = get_time(etime) - get_time(start_time);
	 ltime = etime;
	 lread = inread;
	 //fprintf(stderr,"%d words output, %lu dict size\n",wcnt,wlsum);
	 fprintf(stderr,"\r%02d:%02d elapsed, %.2f MB read , [%.2f MB/s]",int(dur)/60,int(dur)%60,float(inread)/(1024*1024),rate);
      }
      vector<pair<int,int> > cur_index;
      while(!pq.empty() && pq.top().first==s) {
	 int id = pq.top().second;
	 cur_index.insert(cur_index.end(),current_element[id].begin(),current_element[id].end());
	 pq.pop();
	 string ns = read_element(id);
	 if(ns!="")
	    pq.push(make_pair(ns,id));
	 else {
	    //fprintf(stderr,"tempfile-%d is done!\n",id);
	 }
      }
      fprintf(f,"%s ",s.c_str());
      encode(f,cur_index);
      /* USING VARBYTE ENCODER
	 fwrite(&c, sizeof(int), 1, f);
	 for(auto it : cur_index) {
	 fwrite(&it.first, sizeof(int), 1, f);
	 fwrite(&it.second, sizeof(int), 1, f);
	 }
	 */
   }
   if(dictionary.size()>0)
      write_dict_segment();
   for(int i = 0; i < n; i++)
      fclose(tempf[i]);

   off_t dict_seg_index_offset = ftello(f);
   for(auto it : dict_offset_list)
      fwrite(&it,sizeof(off_t),1,f);
   fwrite(&dict_seg_index_offset,sizeof(off_t),1,f);
   fclose(f);
   fprintf(stderr,"\n");
   return 0;
}
