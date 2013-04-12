#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string.h>
#include <string>
#include <map>
#include <set>

#include <math.h>

#include "ssw_cpp.h"

using namespace std;
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define UNMAPPED        0x4
#define M_UNMAPPED      0x8
#define REVERSE         0x10
#define M_REVERSE       0x20

#define WEIRD_STDDEV    4
#define MAX_STDDEV      15

#define READ_SIZE	100000
#define NEAR	1000

#define SHARP_BP	20

class pos {
	public:
		bool marked;
		bool sharp;
		int chr;
		unsigned int coord;
		bool strand; // true is positive
		pos(int chr, unsigned int coord,bool strand);
		pos();
		bool operator<(const pos &other) const;
		bool operator>(const pos &other) const;
		bool operator==(const pos &other) const;
		bool operator!=(const pos &other) const;
		string str();
		unsigned int operator-(const pos &other) const;
};

class cluster {
	public:
		pos b1;
		pos b2;

		int original_support;

		multiset<pos> b1p;
		multiset<pos> b1pc;
		multiset<pos> b2p;
		multiset<pos> b2pc;

		multiset<pos> b1pairs;
		multiset<pos> b2pairs;
	
		pos b1paired;
		pos b2paired;
		pos b1snapped;
		pos b2snapped;
	
		cluster(pos,pos,int);
};




class cread {
	public:
		string name;
		int id; //into vector
		pos inside;
		int cid;
		cread(string name, int id, pos inside, int cid);

};


double mean, stddev;
char * ref[25];
unsigned int ref_sizes[25];

vector<cluster> clusters;
map<pos, set<  int > > cluster_pos;
map<string, vector< cread > > reads;

pos::pos(int chr, unsigned int coord, bool strand) {
	this->chr=chr;
	this->coord=coord;
	this->strand=strand;
	this->marked=false;
	this->sharp=false;
}


pos::pos() {
	chr=-1;
	coord=-1;
	strand=false;
	marked=false;
}

cluster::cluster(pos b1, pos b2, int support) {
	this->b1 = b1;
	this->b2 = b2;
	this->original_support = support;
}
	
		
cread::cread(string name , int id, pos inside, int cid) {
	this->name=name;
	this->id=id;
	this->inside=inside;
	this->cid=cid;
}


string pos::str() {
	char buffer[5000];
	char cchr[10];
	if (chr==23) {
		cchr[0]='X'; cchr[1]='\0';
	} else if (chr==24) {
		cchr[0]='Y'; cchr[1]='\0';
	} else {
		sprintf(cchr,"%d",chr);
	}
	sprintf(buffer,"chr%s\t%u\t%c" , cchr,coord, strand ? '+' : '-');
	return string(buffer);
}
bool pos::operator<(const pos &other) const {
	if (chr<other.chr) {
		return true;
	} else if (other.chr==chr) {
		return coord<other.coord;
	} else {
		return false;
	}
}

bool pos::operator>(const pos &other) const {
	if (chr>other.chr) {
		return true;
	} else if (other.chr==chr) {
		return coord>other.coord;
	} else {
		return false;
	}
}

unsigned int pos::operator-(const pos &other) const {
	if (chr!=other.chr) {
		return -1;
	}
	if (coord>other.coord) {
		return coord-other.coord;
	}
	return other.coord-coord;
}

bool pos::operator==(const pos &other) const {
	if (chr!=other.chr || coord!=other.coord || strand!=other.strand) {
		return false;
	} 
	return true;

}


bool pos::operator!=(const pos &other) const {
	if (*this==other) {
		return false;
	}
	return true;

}

//take a chr name and give back the int
int to_chr(const char * s) {
	char buff[1024]="";
	unsigned int i=0;
	for (; i<strlen(s); i++) {
		buff[i]=tolower(s[i]);
	}
	buff[i]='\0';
	char * p = buff;
	if (i>3 && buff[0]=='c' && buff[1]=='h' && buff[2]=='r') {
		p=buff+3;
	}
	if (p[0]=='x') {
		return 23;
	}
	if (p[0]=='y') {
		return 24;
	}
	if (p[0]=='m') {
		return 25;
	}
	return atoi(p);
}

unsigned int cigar_len(const char * s, bool * sharp) {
	unsigned int len=0;
	unsigned int xlen=0;
	for (int i=0; i<strlen(s); i++) {
		if (isdigit(s[i])) {
			//keep going
			xlen=xlen*10+(s[i]-48);
		} else {
			//ok lets process the op
			switch(s[i]) {
				case 'M':
				case 'D':
					len+=xlen;
					break;
				case 'S':
					if (xlen>15) {
						*sharp=true;
					}
				case 'I':
					break;
				default:
					cerr << "Failed to handle cigar op " << s[i] << endl;
					exit(1);		
			}
			xlen=0;
		}
	}
	//cerr << "CIGAR " << s << " " << len << endl;
	return len;
}


set<int> find_clusters_for_pos(pos & a ) {
	pos left_bound = pos(a.chr,MAX(stddev*MAX_STDDEV,a.coord)-stddev*MAX_STDDEV,true);
	pos right_bound = pos(a.chr,a.coord+stddev*MAX_STDDEV,true);

	map<pos, set<int> >::iterator left_it = cluster_pos.lower_bound(left_bound);	
	map<pos, set<int> >::iterator right_it = cluster_pos.upper_bound(right_bound);	

	set<int> found;

	//keep going!
	while (left_it!=right_it) {
		pos p = left_it->first;
		//if ( p.chr==a.chr && ( (p.strand && a.coord < p.coord) || (!p.strand && a.coord > p.coord) ) ) {
		if ( p.chr==a.chr ) {
			if ( (a.coord+stddev*MAX_STDDEV > p.coord) || (p.coord+stddev*MAX_STDDEV > a.coord) ) {
				set<int> & s = left_it->second;
				for (set<int>::iterator sit=s.begin(); sit!=s.end(); sit++) {
					found.insert(*sit);	
				}
			}
		}
		left_it++;
	}

	return found;

}


pos set_median(multiset<pos> s,double f) {
	map<unsigned int, int> m;
	unsigned int x=0;
	unsigned int n=-1;
	
	int chr=-1;

	int total=0;
	for (multiset<pos>::iterator sit = s.begin(); sit!=s.end(); sit++) {
		m[sit->coord]++;
		if (sit->coord<n) {
			n=sit->coord;	
		}
		if (sit->coord>x) {
			x=sit->coord;
		}
		chr=sit->chr;
		total++;
	}


	int z=0;
	for (int i=n; i<=x; i++) {
		z+=m[i];
		if (z>=(floor(total*f))) {
			return pos(chr,i,true);
		}
	}
	cerr << "BAD ERRRO" << endl;
	exit(1);
}

pos set_max(multiset<pos> s) {
	if (s.size()==0) {
		cerr << " INVALID SET " << endl;
		exit(1);
	}
	multiset<pos>::iterator it = s.begin();
	pos x = *it;
	it++;
	while (it!=s.end()) {
		if ((*it)>x) {
			x=*it;
		}
		it++;
	}
	return x;
}

pos set_min(multiset<pos> s) {
	if (s.size()==0) {
		cerr << " INVALID SET " << endl;
		exit(1);
	}
	multiset<pos>::iterator it = s.begin();
	pos x = *it;
	it++;
	while (it!=s.end()) {
		if ((*it)<x) {
			x=*it;
		}
		it++;
	}
	return x;
}

pos snap_pos(pos p, multiset<pos> ps) {
	map<unsigned int, int> clips;
	for (multiset<pos>::iterator sit=ps.begin(); sit!=ps.end(); sit++) {
		pos x = *sit;
		clips[x.coord]++;
	}

	int total_clips=0;
	
	for (int i=-SHARP_BP; i<=SHARP_BP; i++ ) {
		total_clips+=clips[p.coord+i];
	}


	if (total_clips>10) {
		int median=total_clips/2;
		int z=0;
		for (int i=-SHARP_BP; i<=SHARP_BP; i++ ) {
			z+=clips[p.coord+i];
			if (z>=median) {
				pos r = pos(p.chr,p.coord+i,p.strand);
				r.sharp=true;
				return r;
			}
		}
	}
	p.sharp=false;
	return p;
}

void estimate_breakpoint(cluster & c ) {
	if (c.b1pairs.size()==0 || c.b2pairs.size()==0) {
		return;
	}
	if (c.b1.strand) {
		c.b1paired=set_max(c.b1pairs);
		//c.b1paired=set_median(c.b1pairs,0.98);
	} else {
		c.b1paired=set_min(c.b1pairs);
		//c.b1paired=set_median(c.b1pairs,0.02);
	}
	//c.b1paired.strand=!c.b1paired.strand; //read strand to cluster strand
	
	if (!c.b2.strand) {
		c.b2paired=set_max(c.b2pairs);
		//c.b2paired=set_median(c.b2pairs,0.98);
	} else {
		c.b2paired=set_min(c.b2pairs);
		//c.b2paired=set_median(c.b2pairs,0.02);
	}
	c.b2paired.strand=!c.b2paired.strand; //read strand to cluster strand

	//lets try to snap this thing
	c.b1snapped=snap_pos(c.b1paired,c.b1pc);
	c.b2snapped=snap_pos(c.b2paired,c.b2pc);
}

int find_cluster(pos  a, pos  b) {

	if (a>b) {
		pos t = a;
		a=b;
		b=t;
	}

	if (a>b) {
		cerr << " Failed pre condition " << endl;
		exit(1);
	}

	set<int> foota = find_clusters_for_pos(a);
	set<int> footb = find_clusters_for_pos(b);
	
	//find how big the intersection is
	set<int> intersection;
	for (set<int>::iterator sit=foota.begin(); sit!=foota.end(); sit++) {
		for (set<int>::iterator ssit=footb.begin(); ssit!=footb.end(); ssit++) {
			if (*sit==*ssit) {
				intersection.insert(*sit);
			}
		}
	}


	//TODO FIX
	//try to find best cluster to pair with
	unsigned int d=-1;
	int cid=-1;
	//cerr << a.str() << "\t" << b.str() << endl;
	for (set<int>::iterator sit=intersection.begin(); sit!=intersection.end(); sit++) {
		cluster & c = clusters[*sit];
		//if the pair spans then check strands
		bool a_b1 = (c.b1-a)<(c.b2-a);
		bool b_b1 = (c.b1-b)<(c.b2-b);	

		if (a_b1!=b_b1) {
			//spans, check strands
			if (a_b1) {
				if (a.strand!=c.b1.strand || b.strand==c.b2.strand) {
					continue;
				}
			} else {
				if (b.strand!=c.b1.strand || a.strand==c.b2.strand) {
					continue;
				}
			}	
		}	

		unsigned int cd = MIN(a-c.b1,a-c.b2)+MIN(b-c.b1,b-c.b2);
		if (cd<d) {
			d=cd;
			cid=*sit;
		}
		//cerr << c.b1.str() << " " << c.b2.str() << endl;
	}	

	/*if (cid!=-1) {
		cluster & c = clusters[cid];
		cerr << "X" << c.b1.str() << " " << c.b2.str() << endl;
	}*/
	//return pair<pos,pos>(a,b);	
	return cid;

}



void read_ref(char * filename) {
	

	size_t length=0;
	char * buffer = (char*)malloc(1024*1024*1024);
	if (buffer==NULL) {
		cerr << "ERROR IN AMLLOC" << endl;
		exit(1);
	}


	FILE * fptr = fopen(filename,"r");
	if (fptr==NULL) {
		cerr << "Failed to read reference " << endl;
		exit(1);
	}
	char line_buffer[1024*5];
	string name="";
	while(fgets(line_buffer,1024*5,fptr)!=NULL) {
		line_buffer[strlen(line_buffer)-1]='\0';
		if (line_buffer[0]=='>') {
			//dump the last contig
			if (name.length()>0) {
				int chr= to_chr(name.c_str()+1);
				if (chr==0) {
					cerr << "Failed to read fasta" << endl;
					exit(1);
				}
				char * ref_chr = (char*)malloc(length);
				if (ref_chr==NULL) {
					cerr << "Failed to amlloc" << endl;
					exit(1);
				}
				memcpy(ref_chr,buffer,length);
				ref[chr-1]=ref_chr;
				ref_sizes[chr-1]=length;
				//cerr << chr << endl;
			}
			//setup the new
			length=0;	
			name=string(line_buffer);
		} else {
			//add to current contig
			const char * s = line_buffer;
			unsigned int l = strlen(line_buffer);
			char buffer2[l+1];
			for (unsigned int i=0; i<l; i++) {
				buffer2[i]=tolower(s[i]);
			}
			buffer2[l]='\0'; //just in case make sure its legit
			for (unsigned int i=0; i<l; i++) {
				buffer[length+i]=toupper(buffer2[i]);
			}
			//memcpy(buffer+length,buffer2,l);
			length+=l;
		}
	}
	if (name.length()>0) {
		int chr= to_chr(name.c_str()+1);
		if (chr==0) {
			cerr << "Failed to read fasta" << endl;
			exit(1);
		}
		char * ref_chr = (char*)malloc(length);
		if (ref_chr==NULL) {
			cerr << "Failed to amlloc" << endl;
			exit(1);
		}
		memcpy(ref_chr,buffer,length);
		ref[chr-1]=ref_chr;
		ref_sizes[chr-1]=length;
		//cerr << chr << endl;
	}



}


string reverse_comp(string s) {
	char buffer[s.length()+1];
	const char * c = s.c_str();
	unsigned int l = s.length();
	for (int i=0; i<l; i++) {
		switch (c[i]) {
			case 'A':
				buffer[l-1-i]='T';
				break;
			case 'T':
				buffer[l-1-i]='A';
				break;
			case 'G':
				buffer[l-1-i]='C';
				break;
			case 'C':
				buffer[l-1-i]='G';
				break;
			case 'N':
				buffer[l-1-i]='N';
				break;
			default:
				cerr << "FAILDE REV COMP " << endl;
				exit(1);
		}
	}
	buffer[l]='\0';
	return string(buffer);
}

StripedSmithWaterman::Alignment align(pos & align_near , string & squery) {
	//cout << "ALIGN NEAR POS " << align_near.chr << ":" << align_near.coord << endl;
	pos start = pos(align_near.chr,MAX(NEAR,align_near.coord)-NEAR,true);
	pos end = pos(align_near.chr,MIN(ref_sizes[align_near.chr-1],align_near.coord+NEAR),true);


	if (end.coord<start.coord) {
		cerr << "Error " << endl;
		exit(1);
	}
	char cref[3000];
	memcpy(cref,ref[start.chr-1]+start.coord-1,end.coord-start.coord+1);
	cref[end.coord-start.coord+1]='\0';

	string sref=string(cref);

	StripedSmithWaterman::Aligner aligner(2,10,58,1);
	StripedSmithWaterman::Filter filter;
	StripedSmithWaterman::Alignment swalignment;
	//cerr << "Ref: " << sref << endl;
	//cerr << "Qeury: " << squery << endl;
	aligner.Align(squery.c_str(), sref.c_str(), sref.size(), filter, &swalignment);
	swalignment.ref_begin+=start.coord;
	swalignment.ref_end+=start.coord;
	return swalignment;
/*cout << "===== SSW result =====" << endl;
//cout << "REF: " << squery << endl;
//cout << "Q: " << sref << endl;
cout << "Best Smith-Waterman score:\t" << swalignment.sw_score << endl
//<< "Next-best Smith-Waterman score:\t" << swalignment.sw_score_next_best << endl
//<< "Reference start:\t" << swalignment.ref_begin << endl
//<< "Reference end:\t" << swalignment.ref_end << endl
<< "Query start:\t" << swalignment.query_begin << endl
<< "Query end:\t" << swalignment.query_end << endl
//<< "Next-best reference end:\t" << swalignment.ref_end_next_best << endl
<< "Number of mismatches:\t" << swalignment.mismatches << endl
<< "Cigar: " << swalignment.cigar_string << endl;
cout << "======================" << endl;
	return swalignment.sw_score;*/

}

void process_mapped_read(vector<string> v_row) {
	int flags = atoi(v_row[1].c_str());
	string qname = v_row[0];
	int my_chr = to_chr(v_row[2].c_str());
	unsigned long my_pos = atol(v_row[3].c_str());
	bool my_strand = ((flags & REVERSE)==0);

	pos my =  pos(my_chr,my_pos,my_strand);

	my.marked=true;
	string my_cigar = v_row[5];
	unsigned int c_len=cigar_len(my_cigar.c_str(),&(my.sharp));

	if ((flags & M_UNMAPPED)==0) {

		int mate_chr = my_chr;
		if (v_row[6].c_str()[0]!='=') {
			mate_chr = to_chr(v_row[6].c_str());	
		}
		unsigned long mate_pos = atol(v_row[7].c_str());
		bool mate_strand = ((flags & M_REVERSE)==0);
	
		double isize=1000*(stddev+mean+10); //TODO: HARD THRESHOLD
		if (mate_chr==my_chr) {
			isize=atof(v_row[8].c_str());
			if (isize<0) {
				isize=-isize;
			}
		}

		pos mate = pos(mate_chr,mate_pos,mate_strand);


		

		if (isize<(WEIRD_STDDEV*stddev+mean)) {
			//this is kinda normal
			if (my.sharp) {
				if (!my.strand) {
					my.coord+=c_len;
				}
				//cerr << my.str() << "SHARP" << endl;
			}
			int cid = find_cluster(my,mate);
			if (cid!=-1) {
				//cerr << cid << endl;
				int id = reads[qname].size();
				cread r = cread(qname,id,my,cid);
				reads[qname].push_back(r);
			}
			
			return;
		}


		if (my.strand) {
			my.coord+=c_len;
		}

		//cerr << my_chr << ":" << my_pos << " " << mate_chr << ":" << mate_pos << endl;

				
		int cid = find_cluster(my,mate);
		if (cid!=-1) {
			//cerr << cid << endl;
			cluster & c = clusters[cid];
			int id = reads[qname].size();
			cread r = cread(qname,id,my,cid);
			reads[qname].push_back(r);
		}
	} else {
		if (my.strand) {
			my.coord+=c_len;
		}
		//this is mapped but mate is not
		int cid = -1; //dont know which cluster we blong to :(
		int id = reads[qname].size();
		cread r = cread(qname,id,my,cid);
		reads[qname].push_back(r);
		//cerr << "Added half mapped " << qname << endl; 
	}	

}

void process_unmapped_read(vector<string> v_row) {
	int flags = atoi(v_row[1].c_str());
	string qname = v_row[0];
	int my_chr = to_chr(v_row[2].c_str());

	int mate_chr = my_chr;
	if (v_row[6].c_str()[0]!='=') {
		mate_chr = to_chr(v_row[6].c_str());	
	}
	unsigned long mate_pos = atol(v_row[7].c_str());
	bool mate_strand = ((flags & M_REVERSE)==0);

	//lets find out what clusters we can go to 
	pos mate = pos(mate_chr,mate_pos,mate_strand);
	set<int> cids = find_clusters_for_pos(mate);



	pos my = pos(0,0,true);

	bool skip=false;

	for (set<int>::iterator sit=cids.begin(); sit!=cids.end(); sit++) {
		//need to find out which b we hit, or just map to both for all
		cluster & c = clusters[*sit];
	
		unsigned int d1 = mate-c.b1;
		unsigned int d2 = mate-c.b2;
		if (d1==-1 && d2==-1) {
			cerr << "both wrong " << endl;
			continue;
		}
		if (d1==d2) {
			cerr << "unclear " << endl;
			continue;
		}
	
		int cid = *sit;

		//cerr << *sit << endl;
		string squery = string(v_row[9]);	
		string rsquery = reverse_comp(squery);
		StripedSmithWaterman::Alignment sw1s = align(c.b1,squery);
		StripedSmithWaterman::Alignment sw2s = align(c.b2,squery);
		StripedSmithWaterman::Alignment sw1rs = align(c.b1,rsquery);
		StripedSmithWaterman::Alignment sw2rs = align(c.b2,rsquery);

		//cerr << "ALIGN:" << sw1s.sw_score << " " << sw1rs.sw_score << " " << sw2s.sw_score << " " << sw2rs.sw_score << endl;

		//alignment score threshold // TODO: hand set!
		int max_forward=MAX(sw1s.sw_score,sw2s.sw_score);
		int max_reverse=MAX(sw1rs.sw_score,sw2rs.sw_score);
		if (sw1s.sw_score>80 && sw2s.sw_score>80) {
			continue;
		}
		if (sw1rs.sw_score>80 && sw2rs.sw_score>80) {
			continue;
		}
		if (max_forward>80 && max_reverse>80) {
			continue;
		}
		if ( MAX(max_forward,max_reverse) >80 ) {
			pos inside;
			bool clipped=false; 
			//use the alignment!
			if (max_forward>80) {
				if (sw1s.sw_score >80) {
					inside = pos(c.b1.chr,sw1s.ref_begin+cigar_len(sw1s.cigar_string.c_str(),&clipped),true);
				} else {
					inside = pos(c.b2.chr,sw2s.ref_begin+cigar_len(sw2s.cigar_string.c_str(),&clipped),true);
					//sw2s.sw_score>80
				}
			}

			if (max_reverse>80) {
				if (sw1rs.sw_score >80) {
					cigar_len(sw1rs.cigar_string.c_str(),&clipped);
					inside = pos(c.b1.chr,sw1rs.ref_begin,false);
				} else {
					//sw2rs.sw_score>80
					cigar_len(sw2rs.cigar_string.c_str(),&clipped);
					inside = pos(c.b2.chr,sw2rs.ref_begin,false);
				}
			}
			if (my.coord==0) {
				my=inside;
				my.sharp=clipped;
			} else {
				skip=true;
			}
		} 
		
		/*int max=MAX(MAX(b1s,b2s),MAX(b1rs,b2rs));
		if (max>80) {
			cout << qname << "support" << endl;
			cout << c.b1.chr << ":" << c.b1.coord << " " << c.b2.chr << ":" << c.b2.coord << endl;
			cout << mate-c.b1 << " " << mate-c.b2 << endl;
			
		}*/
	}

	if (!skip && my.coord!=0) {
		//should add the alignment
		//cerr << "TRYING TO ADD " <<  a.inside.chr << ":" << a.inside.coord << " " << mate.chr << ":" << mate.coord << endl;
		int cid = find_cluster(my,mate);
		if (cid!=-1) {
			//check if this is even close to possible
			cluster & c  = clusters[cid];
			bool my_b1 = (c.b1-my)<(c.b2-my) ;
			if (my_b1 && my.strand!=c.b1.strand) {

			} else if (!my_b1 && my.strand==c.b2.strand) {

			} else if (MIN(my-c.b1,my-c.b2)>=4*stddev) {
				//skip it
				
			} else {
				
				int id = reads[qname].size();
				cread r = cread(qname,id,my,cid);
				reads[qname].push_back(r);
			}
			//cerr << cid << " " << qname << " " << a.inside.chr << " " << a.inside.coord << endl;			
		}
	}

}

void process_read(vector<string> v_row) {
	//lets try to make an alignment and find its cluster
	//ok now we have a line lets check it out
	if (v_row[0].c_str()[0]=='@') {
		//its header
		//cerr << row << endl;
	} else {
		int flags = atoi(v_row[1].c_str());
		if ((flags & UNMAPPED)==0) {
			process_mapped_read(v_row);
		} else if ((flags & M_UNMAPPED)==0) {
			process_unmapped_read(v_row);
		}
	}
	return ;
	
}

int main( int argc, char ** argv) {
	if (argc!=5) {
		fprintf(stderr,"%s mean std cluster_file hg19.fa\n", argv[0]);
		exit(1);
	}

	mean = atof(argv[1]);
	stddev = atof(argv[2]);
	char * cluster_filename = argv[3];
	char * ref_filename = argv[4];
	read_ref(ref_filename);

	//read in the clusters
	ifstream infile(cluster_filename);
        string line;
        string name="";
        while (std::getline(infile, line)) {
		string s_chra;
		int chra = 0;
		unsigned int coorda=0;
		string stranda;
		
		string s_chrb;
		int chrb=0;
		unsigned int coordb=0;
		string strandb;
	
		int support=0;

		istringstream ss(line);
	
		ss >> s_chra >> coorda >> stranda >> s_chrb >> coordb >> strandb  >> support;  



		pos b1 = pos(to_chr(s_chra.c_str()),coorda,stranda=="+");
		pos b2 = pos(to_chr(s_chrb.c_str()),coordb,strandb=="+");
		cluster c = cluster(b1,b2,support);

		//check if already exists
		bool exists=false;
		set<int> b1_foot;
		for (set<int>::iterator sit = cluster_pos[b1].begin(); sit!=cluster_pos[b1].end(); sit++) {
			b1_foot.insert(*sit);
		}
		for (set<int>::iterator sit = cluster_pos[b2].begin(); sit!=cluster_pos[b2].end(); sit++) {
			if (b1_foot.find(*sit)!=b1_foot.end()) {
				//found in both, skip this cluster
				exists=true;
			}
		}

		if (!exists) {
			//want to add it
			//get a new cluster id
			int cid = clusters.size();
			clusters.push_back(c);
			cluster_pos[b1].insert(cid);
			cluster_pos[b2].insert(cid);
		}
	}

	//read in the reads
	unsigned long total_read=0;
	char const field_delim = '\t';
	vector< string > rows;
	rows.reserve(READ_SIZE);
	while (true) {
		//read in a million
		rows.clear();
		char buffer[1024];
		unsigned int read = 0;
		
		char row_buffer[1024*5];
		while(fgets(row_buffer,1024*5,stdin)!=NULL) {
			read++;
			rows.push_back(string(row_buffer));
			if (read==READ_SIZE) {
				break;
			}
		}


		total_read+=read;
		cerr << "\r" << "read: " << total_read << "     "; 

		for (unsigned int i=0; i<read; i++) {
			//process it
			vector<string> v_row;
			istringstream ss(rows[i]);
			for (string field; getline(ss, field, field_delim); ) {
				v_row.push_back(field);
			}
			process_read(v_row);
		}
	
		if (read!=READ_SIZE) {
			break;
		}

	}
	cerr << "CLEAN OUT" << endl;


	cerr << "ASSUMIG 100bp read length " << endl;

	int t27 =0;

	for (map<string, vector<cread> >::iterator mit=reads.begin(); mit!=reads.end(); mit++) {
		const string & qname = mit->first;
		vector<cread> & v = mit->second;
		if (v.size()>2) {
			cerr << "WEIRD ALIGNMENTS" << endl;
			v.clear();
		}
		if (v.size()==2) {
					//cerr << " something " << endl;
			unsigned int d = v[0].inside-v[1].inside;
			if (d<(WEIRD_STDDEV*stddev+mean)) {
					//cerr << " NORMAL " << endl;

				if ( !v[0].inside.sharp && !v[1].inside.sharp) {
					//drop it, it's too normal
					//cerr << " TOO NORMAL " << endl;
					v.clear();
					continue;
				}
				if ( v[0].cid!=-1 && v[1].cid!=-1 && v[0].cid==v[1].cid) {
					//lets see if correct side is clipped
					cluster & c = clusters[v[0].cid];
					unsigned int d01 = v[0].inside-c.b1;
					unsigned int d11 = v[1].inside-c.b1;
					unsigned int d02 = v[0].inside-c.b2;
					unsigned int d12 = v[1].inside-c.b2;
					
					unsigned int m = MIN(MIN(d01,d11),MIN(d02,d12));
					bool clipped_correctly=false;
					if (d01==m && v[0].inside.sharp) {
						clipped_correctly=true;
					} else if (d11==m && v[1].inside.sharp) {
						clipped_correctly=true;
					} else if (d02==m && v[0].inside.sharp) {
						clipped_correctly=true;
					} else if (d12==m && v[1].inside.sharp) {
						clipped_correctly=true;
					}
		
					if (!clipped_correctly) {
						cerr << "SKIPPING BAD CLIP!" << endl;
						v.clear();
						continue;
					}

				}
			}
			if (MAX(v[0].cid,v[1].cid)==27) {
				if (v[0].inside-v[1].inside>(WEIRD_STDDEV*stddev+mean)) {
					t27++;
				}
			}

			if (v[0].cid==-1 && v[1].cid==-1) {
				cerr << "ERRO IN READS" << endl;
				continue;
			}
	
			if (v[0].cid>-1 && v[1].cid>-1 && v[0].cid!=v[1].cid) {
				cerr << "ERROR IN READS x2 " << endl;
				continue;
			}
	
			int cid=MAX(v[0].cid,v[1].cid);
			v[0].cid=cid;
			v[1].cid=cid;



			//lets check if the span the break
			cluster & c = clusters[cid];
			pos & a = v[0].inside;
			pos & b = v[1].inside;
			
			unsigned a_bp = 0;
			if ( (a-c.b1) > (a-c.b2)) {
				a_bp=2;
			} else if ( (a-c.b1) < (a-c.b2)) {
				a_bp=1;
			} else {
				cerr << "EERRRO " << endl;
				continue;
			}
			unsigned b_bp = 0;
			if ( (b-c.b1) > (b-c.b2)) {
				b_bp=2;
			} else if ( (b-c.b1) < (b-c.b2)) {
				b_bp=1;
			} else {
				cerr << "EERRRO " << endl;
				continue;
			}

			if (a_bp!=b_bp) {
				//span the break
				if (a_bp==1) {
					c.b1pairs.insert(a);
					c.b2pairs.insert(b);
				} else {
					c.b2pairs.insert(a);
					c.b1pairs.insert(b);
				}
			} else {
				//dont span
					
			}

			if (a_bp==1) {
				c.b1p.insert(a);
				if (a.sharp) {
					c.b1pc.insert(a);
				}
			} else {
				c.b2p.insert(a);
				if (a.sharp) {
					c.b2pc.insert(a);
				}
			}

			if (b_bp==1) {
				c.b1p.insert(b);
				if (b.sharp) {
					c.b1pc.insert(b);
				}
			} else {
				c.b2p.insert(b);
				if (b.sharp) {
					c.b2pc.insert(b);
				}
			}

			/*for (int i=0; i<v.size(); i++) {
				cread & r = v[i];
				cout << qname << "\t" << r.cid << "\t" << r.inside.chr << ":" << r.inside.coord << endl;
			}*/

		}
	}

	cout << "#BP1\tBP2\tSUPPORT\tEBP1\tEBP2\tSEBP1\tSEBP2\tSUPPORT" << endl;

	for (int cid=0; cid<clusters.size(); cid++) {
		cluster & c = clusters[cid];
		//if (c.b1pairs.size()>0 && c.b2pairs.size()>0) {
			estimate_breakpoint(c);
			cout << c.b1.str() << "\t" << c.b2.str() << "\t" << c.original_support;
			cout << "\t" << c.b1paired.str() << "\t" << c.b2paired.str(); 
			cout << "\t" << c.b1snapped.str() << "\t" << c.b2snapped.str() << "\t" << MIN(c.b1pairs.size(),c.b2pairs.size()) << endl;
		//}		
	}

	cerr << "27: " << t27 << endl;
	
	return 0;
}