#include <ctype.h>
#include <vector>
#include <set>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <zlib.h>
#include <omp.h>
#include <queue>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <limits>

#define	SZ	100	//max ocpy count
#define MAX_WALK	150

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

using namespace std;


double abs(double a) {
	if (a<0) {
		return -a;
	}
	return a;
}

class pos {
	public:
		int chr;
		unsigned int coord;
		pos(int chr, unsigned int coord);
		pos();
		bool operator<(const pos &other) const;
		bool operator>(const pos &other) const;
		bool operator==(const pos &other) const;
		bool operator!=(const pos &other) const;
		string str() const;
};

class edge {
	public:
		pos posa;
		pos posb;
		bool genomic;
		edge(pos posa, pos posb,bool genomic);
		edge reverse();
		edge();
		unsigned int length();
		bool operator<(const edge &other) const;
		bool operator>(const edge &other) const;
		bool operator==(const edge &other) const;
		bool operator!=(const edge &other) const;
		bool is_forward();
		unsigned int bound_cp();
		edge canonical();
};


class edge_info { 
	public:
		edge_info();
		unsigned int length;
		unsigned int normal;
		unsigned int tumor;
		int type;
		int copy_number;
		double scores[SZ];
		void poisson();
};

class walk  {
	public:
		map<edge,int> used_edges;
		vector<edge> w;
		double scores[SZ];
		int restarts;
		walk(int restarts);
		walk(const walk & w);
		vector<walk> successors();
		walk append_edge(edge e);
		string str();
		void edge_score_correction(edge_info & ei , int old_cp, int new_cp);
		bool operator<(const walk &other) const;
		bool operator>(const walk &other) const;
		double score() const;
};

//some global variables
set<pos> bps;
map<pos,int> bp_support;

map<edge, edge_info > genomic_edges;
map<edge, edge_info > somatic_edges;
map<pos, set<pos> > jump_edges;

edge fake_edge=edge(pos(0,0),pos(0,0),true);

pos h1 = pos(0,0);
pos h2 = pos(0,1);
edge he = edge(h1,h2,true);

//
// The pos class
//


pos::pos(int chr, unsigned int coord) {
	this->chr=chr;
	this->coord=coord;
}

pos::pos() {
	chr=-1;
	coord=-1;
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

bool pos::operator==(const pos &other) const {
	if (chr!=other.chr || coord!=other.coord) {
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

string pos::str() const {
	char buffer[5000];
	char cchr[10];
	if (chr==23) {
		cchr[0]='X'; cchr[1]='\0';
	} else if (chr==24) {
		cchr[0]='Y'; cchr[1]='\0';
	} else {
		sprintf(cchr,"%d",chr);
	}
	sprintf(buffer,"chr%s:%u" , cchr,coord);
	return string(buffer);
}


//
// The edge class
//
//if supporting > 0 then its a free edge!


bool edge::operator<(const edge &other) const {
	if (posa<other.posa) {
		return true;
	} else if (posa>other.posa) {
		return false;
	} else {
		if (posb==other.posb) {
			return genomic<other.genomic;
		} else {
			return posb<other.posb;
		}
	}
}

bool edge::operator>(const edge &other) const {
	if (posa>other.posa) {
		return true;
	} else if (posa<other.posa) {
		return false;
	} else {
		if (posb==other.posb) {
			return genomic>other.genomic;
		} else {
			return posb>other.posb;
		}
	}
}

edge edge::reverse() {
	return edge(posb,posa,genomic);
}

edge edge::canonical() {
	if (posa<posb) {
		return edge(posa,posb,genomic);
	} else {
		return edge(posb,posa,genomic);
	}
}

unsigned int edge::length() {
	if (posa.chr==posb.chr) {
		if (posa.coord>posb.coord) {
			return posa.coord-posb.coord;
		} else {
			return posb.coord-posa.coord;
		}
	}
	return 0;
}

bool edge::operator==(const edge &other) const {
	return (posa==other.posa && posb==other.posb && genomic==other.genomic);
}

bool edge::operator!=(const edge &other) const {
	return !(*this==other);
}

edge::edge(pos posa, pos posb,bool genomic) {
	this->posa=posa;
	this->posb=posb;
	this->genomic=genomic;
}

edge::edge() {
	*this=fake_edge;
}


bool edge::is_forward() {
	return posb>posa;
}



edge_info::edge_info() {
	length=0;
	normal=0;
	tumor=0;
	type=5;
	copy_number=-1;
	for (int i=0; i<SZ; i++) {
		scores[i]=0;
	}
}

void edge_info::poisson() {
		
	if (normal==0 || tumor==0) {
		cerr << normal << " " << tumor << "NORMAL OR TUMOR IS ZERO, is this ok?" << endl;
	}

	// a bit of smoothing like thing?
	normal+=2;
	tumor+=2;

	scores[0]=normal*0.1-tumor*log(normal*0.1);
	for (int i=1; i<SZ; i++) {
		scores[i]=normal*i-tumor*log(normal*i);
	}

	//make sure the min value is zero
	double min=10e100;
	for (int i=0; i<SZ; i++) {
		if (min>scores[i]) {
			min=scores[i];
		}
	}
	for (int i=0; i<SZ; i++) {
		//cerr << scores[i] << "\t" << scores[i]-min << endl;
		scores[i]-=min;
	}
}



walk::walk(int restarts) {
	this->restarts=restarts;
	for (int i=0; i<SZ; i++) {
		scores[i]=0;
	}
}

walk::walk(const walk & parent) {
	this->used_edges=parent.used_edges;
	this->w=parent.w;
	for (int i=0; i<SZ; i++) {
		scores[i]=parent.scores[i];
	}
	this->restarts=parent.restarts;
}

double walk::score() const {
	double my_min=10e100;
	for (int i=0; i<SZ; i++) {
		my_min=MIN(my_min,scores[i]);
	}
	return my_min;
}

bool walk::operator>(const walk &other) const {
	return score()>other.score();
}
bool walk::operator<(const walk &other) const {
	return score()<other.score();
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



map<edge, edge_info> edges;

void read_edges(char * filename) {
	FILE * fptr = fopen(filename,"r");
	if (fptr==NULL) {
		cerr << "failed to open file" << endl;
		exit(1);
	}
	char buffer[5000];
	while (fgets(buffer,5000,fptr)) {
		//2       chrX:137812363  chrX:138585163  772800  264181  129674
		if (buffer[0]=='#') {
			continue;
		}
		int cp,length,tumor,normal;
		long unsigned int coorda, coordb;
		char posa[100];
		char chra[10];
		int ichra=-1;
		char posb[100];
		char chrb[10];
		int ichrb=-1;
		int ret = sscanf(buffer,"%d\t%s\t%s\t%d\t%d\t%d",&cp,posa,posb,&length,&tumor,&normal);
		int i=0;
		for (; posa[i]!=':'; i++) {
			chra[i]=posa[i];
		}
		chra[i]='\0';
		ichra=to_chr(chra);
		coorda=atol(posa+i+1);

		for (i=0; posb[i]!=':'; i++) {
			chrb[i]=posb[i];
		}
		chrb[i]='\0';
		coordb=atol(posb+i+1);
		ichrb=to_chr(chrb);

		

		if (cp>2) {
			//insert the nodes
			bps.insert(pos(ichra,coorda));
			bps.insert(pos(ichrb,coordb));

			//add the edges
			edge e = edge(pos(ichra,coorda),pos(ichrb,coordb),true);
	
			edge_info ei;
			ei.length=length;
			ei.type=0;
			ei.copy_number=cp;

			ei.normal=normal;
			ei.tumor=tumor;
			ei.poisson();

			genomic_edges[e]=ei;
			genomic_edges[e.reverse()]=ei;
			
		}
		
	}
}


void read_links(char * filename) {
	FILE * fptr = fopen(filename,"r");
	if (fptr==NULL) {
		cerr << "failed to open file" << endl;
		exit(1);
	}
	char buffer[5000];
	while (fgets(buffer,5000,fptr)) {
		if (buffer[0]=='#') {
			continue;
		}
		//chr1    59446789        +       chr1    59446926        +       13
		char chra[10];
		int ichra=-1;
		char stranda;
		unsigned long coorda;

		char chrb[10];
		int ichrb=-1;
		char strandb;
		unsigned long coordb;

		int support;


		int ret = sscanf(buffer,"%s\t%lu\t%c\t%s\t%lu\t%c\t%d",chra,&coorda,&stranda,chrb,&coordb,&strandb,&support);

		edge_info ei;

		ichra=to_chr(chra);
		ichrb=to_chr(chrb);

		pos posa=pos(ichra,coorda);
		if (bps.find(posa)==bps.end()) {
			continue;
		}	
		pos posb=pos(ichrb,coordb);
		if (bps.find(posb)==bps.end()) {
			continue;
		}	

		//get the edge type
		int type=0;
		if (stranda=='+') {
			if (strandb=='+') {
				type=0;	
			} else {
				//negative
				type=2;
			}
		} else {
			if (strandb=='+') {
				type=3;
			} else {
				type=1;
			}
		}	

		//get the normal support
		if (bp_support.find(posa)==bp_support.end() ) {
			cerr << "failed to find posa in bp_support " << endl;
		}
		if (bp_support.find(posb)==bp_support.end() ) {
			cerr << "failed to find posb in bp_support " << endl;
		}
		int normal=(bp_support[posa]+bp_support[posb])/4; //the lambda , average of two sites (2 copies each)
		int tumor=support;	


		//set up the ei
		ei.length=0;
		ei.type=type;
		//cerr << posa.str() << " " << posb.str() << " " << normal << " " << tumor << endl;	
		ei.normal=normal;
		ei.tumor=tumor;
		ei.poisson();
		

		//put in the forward
		edge e = edge(posa,posb,false);
		somatic_edges[e]=ei;

		//put in the reverse
		if (type<2) {
			type=1-type;
		} else {
			type=5-type;
		}
		somatic_edges[e.reverse()]=ei;


		jump_edges[posa].insert(posb);
		jump_edges[posb].insert(posa);
	}
}




void read_bp_coverages(char * filename) {
	FILE * fptr = fopen(filename,"r");
	if (fptr==NULL) {
		cerr << "failed to open file" << endl;
		exit(1);
	}
	char buffer[5000];
	while (fgets(buffer,5000,fptr)) {
		if (buffer[0]=='#') {
			continue;
		}
		//chr1    59446926        12
		char chra[10];
		int ichra=-1;
		unsigned long coorda=0;
		int support=0;
		int ref = sscanf(buffer,"%s\t%lu\t%d",chra,&coorda,&support);
		ichra=to_chr(chra);
		bp_support[pos(ichra,coorda)]=support;
	}

}


edge get_previous_genomic_edge(pos p) {
	set<pos>::iterator sit = bps.find(p);
	if (sit==bps.end()) {
		cerr << "SDFDSFS" << endl;
		exit(1);
	}
	sit--;
	if (sit!=bps.end()) {
		edge e = edge(p,*sit,true);
		if (genomic_edges.find(e)!=genomic_edges.end()) {
			return e;
		}
	}
	return fake_edge;
}

edge get_next_genomic_edge(pos p) {
	set<pos>::iterator sit = bps.find(p);
	if (sit==bps.end()) {
		cerr << "SDFDSFS" << endl;
		exit(1);
	}
	sit++;
	if (sit!=bps.end()) {
		edge e = edge(p,*sit,true);
		if (genomic_edges.find(e)!=genomic_edges.end()) {
			return e;
		}
	}
	return fake_edge;
}

set<pos> get_genomic_edges(pos p) {
	set<pos> r;
	set<pos>::iterator sit=bps.find(p);
	if (sit==bps.end()) {
		cerr << " A BAD ERROR IN GET GENOMIC EDGES " << endl;
		exit(1);
	}	

	//try the previous position
	sit--;
	if (sit!=bps.end() ) {
		pos previous = *sit;
		edge e = edge(previous, p,true);
		if (genomic_edges.find(e)!=genomic_edges.end()) {
			r.insert(previous);
		}
	}
	
	//try the next position
	sit=bps.find(p);
	sit++;
	if (sit!=bps.end() ) {
		pos next = *sit;
		edge e = edge(p, next,true);
		if (genomic_edges.find(e)!=genomic_edges.end()) {
			r.insert(next);
		}
	}

	return r;

	
}


void find_connected_component(pos p, set<pos> & so_far) {
	//add this node to the set
	if (so_far.count(p)>0) {
		cerr << " An error has happened" << endl;
		exit(1);
	}
	so_far.insert(p);

	//try going out on all genomic edges
	set<pos> ges = get_genomic_edges(p);
	for (set<pos>::iterator xit = ges.begin(); xit != ges.end(); xit++) {
		pos x = *xit;
		if (so_far.count(x)==0) {
			find_connected_component(x,so_far);
		}
	}

	//try going out on all free edges	
	set<pos> jumps = jump_edges[p];
	for (set<pos>::iterator xit = jumps.begin(); xit != jumps.end(); xit++) {
		pos x = *xit;
		if (so_far.count(x)==0) {
			find_connected_component(x,so_far);
		}
	}		
}


map<pos, int> find_connected_components() {
	int cid=0;
	map<pos, int> connected;
	for (set<pos>::iterator sit = bps.begin(); sit!=bps.end(); sit++) {
		pos p = *sit;
		//already found this component
		if (connected.find(p)!=connected.end()) {
			continue;
		}

		//find the the component
		set<pos> component;
		find_connected_component(p,component);
		for (set<pos>::iterator xit = component.begin(); xit != component.end(); xit++) {
			pos x = *xit;
			if (connected.find(x)!=connected.end()) {
				cerr << "double found a component!" << endl;
				exit(1);
			}
			connected[x]=cid;
		}
		/*if (component.size()>2) {	
			cout << "found component of size " << component.size() << endl;
		}*/
		cid++;
	}
	
	return connected;

}

void walk::edge_score_correction(edge_info & ei , int old_m, int new_m) {
	if (old_m>=new_m) {
		cerr << "Problem in lowering copy count, really should recompute, +/- inf" << endl;
		exit(1);
	}
	//subtract out the old and add the new
	for (int i=1; i<SZ; i++) {
		scores[i]+=(ei.scores[i*old_m]-ei.scores[0]);
	}
	//add in the new
	for (int i=1; i<SZ; i++) {
		scores[i]+=(ei.scores[i*new_m]-ei.scores[0]);
	}
}

walk walk::append_edge(edge e) {
	walk x = walk(*this);
	//add it to the walk
	x.w.push_back(e);
	
	//check if this is a restart request
	if (e==he) {
		if (restarts<=used_edges[he]) {
			cerr << "ERRROR FLKjF" << endl;
			exit(1);
		}
		x.used_edges[he]++;
		return x;
	}

	//otherwise we need to actually update the score
	edge ce = e.canonical();
	if (e.genomic) {
		edge_info & ei = genomic_edges[ce];
		x.edge_score_correction(ei,x.used_edges[ce],x.used_edges[ce]+1);
		x.used_edges[ce]++;
	} else {
		edge_info & ei = somatic_edges[ce];
		x.edge_score_correction(ei,x.used_edges[ce],x.used_edges[ce]+1);
		x.used_edges[ce]++;
	}
	
	return x;		

}

vector<walk> walk::successors() {
	vector<walk> s;

	//add the restart if we are allowed
	if (restarts>used_edges[he]) {
		s.push_back(append_edge(he));
	}

	//if this is a walk of size zero do the thing
	if (w.size()==0 || w.size()>=MAX_WALK) {
		return s;
	} 

	//ok not a brand new walk
	//find out orientation of last edge
	edge & last_e = w.back();
	if (last_e == he) {
		//try ever start edge
		for (map<edge,edge_info>::iterator mit = genomic_edges.begin(); mit!=genomic_edges.end(); mit++ ) {
			edge e = mit->first;
			s.push_back(append_edge(e));
			//s.push_back(append_edge(e.reverse()));
		}	
	} else {
		//try the next genomic edge
		if (last_e.is_forward()) {
			edge e = get_next_genomic_edge(last_e.posb);	
			if (e!=fake_edge) {
				s.push_back(append_edge(e));
			}
		} else {
			edge e = get_previous_genomic_edge(last_e.posb);	
			if (e!=fake_edge) {
				s.push_back(append_edge(e));
			}
		}		
		//try all linking out free edges
	}
	return s;
}

string walk::str() {
	string s;
	stringstream ss;
	ss << scores[0] << "," << scores[2] << "," << scores[6] << "," << scores[20] << "," << scores[40] <<  "\t:\t";
	s+=ss.str();
	for (int i=0; i<w.size(); i++) {
		s=s+"-> ("+w[i].posa.str() + "," + w[i].posb.str() + ")";
	}
	return s;
}


int main ( int argc, char ** argv) {
	if (argc!=4) {
		cerr << argv[0] << " links bp_coverages edges " << endl;
		exit(1);
	}
	
	char * links_filename = argv[1];
	char * bp_coverages_filename = argv[2];
	char * edges_filename = argv[3];


	read_edges(edges_filename);
	read_bp_coverages(bp_coverages_filename);
	read_links(links_filename);

	map<pos,int> connected_components = find_connected_components();
	map<int, int> connected_components_sizes;
	for (map<pos,int>::iterator mit = connected_components.begin(); mit!=connected_components.end(); mit++) {
		connected_components_sizes[mit->second]++;	
	}

	//lets find the max comopnent and look at that
	int max_id=-1;
	int max=-1;
	for (map<int, int>::iterator mit = connected_components_sizes.begin(); mit!=connected_components_sizes.end(); mit++) {
		if (mit->second>max) {
			max_id=mit->first;
			max=mit->second;
		}
	}	
		
	if (max_id==-1) {
		cerr << "No max components found....";
		exit(1);
	}

	cerr << "Considering max component of size " << max << " id " << max_id << endl;

	//drop everything but this component
	//genomic first
	set<edge> to_remove;
	for (map<edge, edge_info>::iterator mit = genomic_edges.begin(); mit!=genomic_edges.end(); mit++ ) {
		edge e = mit->first;
		if (connected_components[e.posa]!=max_id || connected_components[e.posb]!=max_id) {
			to_remove.insert(e);
		}
	}
	for (set<edge>::iterator sit = to_remove.begin(); sit!=to_remove.end(); sit++ ){
		genomic_edges.erase(*sit);
	}
	to_remove.clear();
	//now drop the somatic
	for (map<edge, edge_info>::iterator mit = somatic_edges.begin(); mit!=somatic_edges.end(); mit++ ) {
		edge e = mit->first;
		if (connected_components[e.posa]!=max_id || connected_components[e.posb]!=max_id) {
			to_remove.insert(e);
		}
	}
	for (set<edge>::iterator sit = to_remove.begin(); sit!=to_remove.end(); sit++ ){
		somatic_edges.erase(*sit);
	}
	//reconstruct bps
	bps.clear();
	for (map<pos,int>::iterator mit=connected_components.begin(); mit!=connected_components.end(); mit++ ){
		if (mit->second==max_id) {
			bps.insert(mit->first);
		}
	}

	//now we just have the largest component left!


	//lets find a lower bound on the scores
	double lower_bound=0;
	//first take the genomic
	//for (map<edge,edge_info>::iterator mit=genomic_edges.begin(); mit!=genomic_edges.end()
	//then take the somatic			

	walk w=walk(1);
	cout << w.str() << endl;

	priority_queue<walk> pq; 
	pq.push(w);
	
	while (pq.size()>0) {
		walk w = pq.top();
		pq.pop();

		vector<walk> children = w.successors();
		
	}	

	/*vector<walk> ws = w.successors();
	ws=ws[0].successors();
	for (int i=0; i<ws.size(); i++) {
		cout << ws[i].str() << endl;
	}	
	ws=ws[0].successors();
	for (int i=0; i<ws.size(); i++) {
		cout << ws[i].str() << endl;
	}	
	ws=ws[0].successors();
	for (int i=0; i<ws.size(); i++) {
		cout << ws[i].str() << endl;
	}*/	
	
	return 0;
}
