#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <regex.h>
#include "rapidxml/rapidxml.hpp"
using rapidxml::xml_document;
using rapidxml::xml_node;
int main() {

   regex_t page,pageclose;
   regmatch_t match1,match2;
   regcomp(&page,"<page>",0);
   regcomp(&pageclose,"</page>",0);

   int bufsz = 102400;
   char *buf = new char[bufsz+5];
   int s = fread(buf,1,bufsz,stdin);
   buf[s]=0;
   int b = 0;
   int cnt = 0;
   while(true) {
      if(!regexec(&pageclose,buf+b,1,&match1,0)) {
	 regexec(&page,buf+b,1,&match2,0);
	 cnt++;
	 buf[b+match1.rm_eo]=0;
	 xml_document<char> doc;
	 doc.parse<0>(buf+b+match2.rm_so);
	 xml_node<char> *root = doc.first_node();
	 xml_node<char> *title,*id,*text;
	 title = root->first_node("title");
	 id = root->first_node("id");
	 text = root->first_node("revision")->first_node("text");
	 printf("%lu %s\n",title->value_size(), title->value());
	 printf("%lu %s\n",id->value_size(), id->value());
	 printf("%lu %s\n",text->value_size(), text->value());
	 b += match1.rm_eo+1;
      } else {
	 if(b*2 < bufsz) {
	    bufsz *= 2;
	    buf = (char*) realloc(buf,bufsz+5);
	 } 

	 for(int i = b; i < s; i++)
	    buf[i-b] = buf[i];
	 s-=b;
	 b=0;

	 int r = fread(buf+s,1,bufsz-s,stdin);
	 if(r==0)
	    return 0;
	 s += r;
	 buf[s]=0;
      }
   }
}