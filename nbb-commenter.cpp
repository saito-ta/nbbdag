#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <map>
using namespace std;

char const useful[]=" abcdefghijklmnopqrstuvwxyz.,!?_\nABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-+:;\"'~`@#$%^&*()[]{}<>\\/=|";

int codever;
int codever_lt;
int codever_ge;
//char const* data_required;

int buildlevel =
#include "buildlevel-local"
;

#define STRINGIFY(n) #n
#define TOSTRING(n) STRINGIFY(n)

void die(char const * msg){
//	puts(msg);
//	exit(1);
}

void giveup1(char const * a){
	die((string("commenter: Sorry, the code is beyond my understanding. Giving up. ")+a).c_str());
}

#define giveup() giveup1(TOSTRING(__LINE__))
//#define giveup() 0

enum Piece {
	p_undef,
	p_auto,
	p_int,
	p_chr,
	p_list,
	p_lambda,
};

struct Type;
map<Type const*,Type const*> enlist_memo;
map<Type const*,Type const*> head_memo;

struct Type {
	Piece p;
	Type const * el; // list element or function result
	Type const * fa; // function argument
	Type const * ts; // tuple sibling

	Type(){
		p=p_undef;
		el=nullptr;
		fa=nullptr;
		ts=nullptr;
	}

	Type(Piece in_p){
		p=in_p;
		el=nullptr;
		fa=nullptr;
		ts=nullptr;
	}

	Type * clone() const {
		Type * t=new Type;
		t->p=p;
		t->el=el;
		t->fa=fa;
		t->ts=ts;
		return t;
	}

	Type const * head() const {
		{
			auto it=head_memo.find(this);
			if(it!=head_memo.end()){
				return it->second;
			}
		}
		{
			Type * t=clone();
			t->ts=nullptr;
			head_memo[this]=t;
			return t;
		}
	}

	Type const * auto_as(Type const * t) const {
		return p==p_auto ? t : this;
	}

	int ary() const {
		return 1+(ts?ts->ary():0);
	}

	bool isBaseElemChr() const {
		Type const *t=this;
		while(t->p==p_list){
			t=t->el;
		}
		return t->p==p_chr;
	}
	
	bool is_any() const {
		return true;
	}
	bool is_any_but_auto() const {
		return p!=p_auto;
	}
	bool is_auto() const {
		return p==p_auto;
	}
	bool is_int() const {
		return p==p_int;
	}
	bool is_chr() const {
		return p==p_chr;
	}
	bool is_num() const {
		return p==p_int||p==p_chr;
	}
	bool is_list() const {
		return p==p_list;
	}
	bool is_str() const {
		return p==p_list&&!el->ts&&el->p==p_chr;
	}
	bool is_list_int() const {
		//return p==p_list&&!el->ts&&el->p==p_int;
		return p==p_list&&el->p==p_int;
	}
	bool is_list_list() const {
		return p==p_list&&el->p==p_list;
	}

	bool is_int_or_auto() const {
		return is_int() || is_auto();
	}
	bool is_num_or_auto() const {
		return is_num() || is_auto();
	}
	bool is_str_or_chr_or_auto() const {
		return is_str() || is_chr() || is_auto();
	}
	bool is_list_or_chr() const {
		return is_list() || is_chr();
	}
	bool is_list_or_auto() const {
		return is_list() || is_chr();
	}
};

bool same(Type const *t1,Type const *t2){
	if(t1->p==p_list&&t2->p==p_list){
		return same(t1->el,t2->el);
	}
	else{
		return t1->p==t2->p;
	}
}

Type const * t_auto = new Type(p_auto);
Type const * t_int = new Type(p_int);
Type const * t_chr = new Type(p_chr);

Type const * enlist(Type const * el){
	{
		auto it=enlist_memo.find(el);
		if(it!=enlist_memo.end()){
			return it->second;
		}
	}
	{
		Type * t = new Type;
		t->p=p_list;
		t->el=el;
		enlist_memo[el]=t;
		return t;
	}
}

Type const * enlist2(Type const *t,int level){
	assert(level>=0);
	while(level>0){
		t=enlist(t);
		--level;
	}
	//while(level<0){
	//	t=t->el;
	//	assert(t);
	//	++level;
	//}
	return t;
}

Type const * make_tuple(Type const * t1,Type const * t2){
	if(!t1){
		return t2;
	}
	{
		Type*t=t1->clone();
		t->ts=make_tuple(t1->ts,t2);
		return t;
	}
}

///enum UsedStatus {
///                 // suppose $ is unused:
///	u_unused,    // ct #==> unused
///	u_direct,    // - $ ct #==> used
///	u_indirect,  // - $ - ct #==> unused
///	u_used,      // : - $ $ ct # ==> used
///};

struct Arg {
	Type const * t;
	bool used;
//	UsedStatus us;
	bool optional;
	string desc;
	int scopeid;

	Arg(){
		t=nullptr;
		used=false;
		optional=false;
		scopeid=0;
	}

	Arg*clone() const {
		Arg*a=new Arg;
		a->t=t;
		a->used=used;
		a->optional=optional;
		a->desc=desc;
		a->scopeid=scopeid;
		return a;
	}
};

void mkarg(vector<Arg const*>&args,int scopeid,string desc,Type const * t,int listlevel=0,bool optional=false){
	Arg*a=new Arg;
	a->desc=desc;
	a->t=enlist2(t,listlevel);
	a->optional=optional;
	a->scopeid=scopeid;
	args.push_back(a);
//printf("mkarg %s scopeid=%d\n",a->desc.c_str(),scopeid);
}

void set_used(vector<Arg const*>&args,int nth){
	if(args[nth]->used){
		return;
	}
	{
		Arg*a=args[nth]->clone();
		a->used=true;
		args[nth]=a;
	}
}

struct Node {
	string lit,desc;
	Type const * t;
	int st,ed;    // nibble string position before/after this node
	bool passed;  // this node includes implicit args
	vector<Node*> childs;

	vector<Arg const*> context;
	int ct_arg;  // index to first lambda arg
	int ct_sub;  // index to first let arg made in subtree
	int ct_let;  // index to first let arg made by this node
	int scopeid;

	Node(){
		t=nullptr;
		st=ed=0;
		passed=false;
		ct_arg=ct_let=0;
		scopeid=-1;
	}

	bool args_used(){
		for(int i=ct_arg;i<ct_sub;++i){
			if(context[i]->used){
				return true;
			}
		}
		return false;
	}

};

#define NBBUFSIZE 65536
char nbbuf[NBBUFSIZE];
int nbsize;

void nbread(){
	for(int c;c=getchar(),c!=EOF;){
		if(nbsize+2>NBBUFSIZE){
			giveup();
		}
		nbbuf[nbsize++]=c>>4&15;
		nbbuf[nbsize++]=c>>0&15;
	}
}

int getnb(int nbpos){
	int b;
	if(nbpos>=nbsize){
		b=-1;
	}else{
		b=nbbuf[nbpos];
	}
//printf("getnb(%d)=%d\n",nbpos,b);
	return b;
}

bool get_int_body(int & nbpos,long& v){
	int b=getnb(nbpos++);
	if(b<0) giveup();
	bool neg=true;
	if(b){
		--nbpos;
		neg=false;
	}
	v=0;
	while(1){
		b=getnb(nbpos++);
		if(b<0) giveup();
		if(v&7l<<61) giveup();
		v=v<<3|b&7;
		if(b&8) break;
	}
	if(neg){
		v=~v;
	}
	v=v==7?10:v==10?7:v==-10?-7:v==-7?-10:v;
	return true;
}

bool get_strings_body(int & nbpos,vector<wstring>& v){
	wstringstream ss;
	while(1){
		int b1=getnb(nbpos++);
		if(b1<0) giveup();
		int last=b1&8;
		b1&=7;
		if(b1==0){
			ss<<'\n';
		}
		else if(b1==1){
			ss<<' ';
		}
		else{
			int b2=getnb(nbpos++);
			if(b2<0) giveup();
			int b=b1<<4|b2;
			if(b==32){
				v.push_back(ss.str());
				if(last){
					return get_strings_body(nbpos,v);
				}
				else{
					return true;
				}
			}
			else if(b==127){
				if(codever!=24){
					b=getnb(nbpos++)<<4;
					b|=getnb(nbpos++);
				}
				else{
					// utf8 char, Nibbles 0.24 only
					b=0;
					while(1){
						int b3=getnb(nbpos++);
						b=b<<3|b3&7;
						if(b3&8) break;
					}
					if(b>=32){
						b+=96;
					}
				}
			}
			ss<<wchar_t(b);
		}
		if(last) {
			v.push_back(ss.str());
			return true;
		}
	}
}

string show_string(string v){
	stringstream ss;
	for(char c:v){
		if(c=='\n'){
			ss<<"\\n";
		}
		else if(c=='\\'){
			ss<<"\\\\";
		}
		else if(c=='\"'){
			ss<<"\\\"";
		}
		else if(c=='\''){
			ss<<"\\\'";
		}
		else if(c<0x20||c>126){
			ss<<"\\x"<<hex<<setw(2)<<setfill('0')<<int(c);
		}
		else{
			ss<<c;
		}
	}
	return ss.str();
}

string show_string(wstring v){
	stringstream ss;
	for(wchar_t c:v){
		if(c=='\n'){
			ss<<"\\n";
		}
		else if(c=='\\'){
			ss<<"\\\\";
		}
		else if(c=='\"'){
			ss<<"\\\"";
		}
		else if(c=='\''){
			ss<<"\\\'";
		}
		else if(c<0x20||c>126){
			if(c<256){
				ss<<"\\x"<<hex<<setw(2)<<setfill('0')<<int(c);
			}
			else{
				ss<<"\\"<<int(c);
			}
		}
		else{
			ss<<char(c);
		}
	}
	return ss.str();
}

enum Op0 {
//	ctx0_undef,
	op0_auto,
//	ctx0_onot,
	op0_tuple,
};

struct FnEnv {
	vector<Arg const*> context;
//	int ct_arg;
};

struct Special {
	char const * lit;
	char const * desc;
};

Special foldops_v020[]={
	/* 0 */ { "]", "foldop: max" },
	/* 1 */ { "[", "foldop: min" },
	/* 2 */ { "+", "foldop: add" },
	/* 3 */ { "*", "foldop: mult" },
	/* 4 */ { "-", "foldop: sub" },
	/* 5 */ { "/", "foldop: div" },
	/* 6 */ { "%", "foldop: mod" },
	/* 7 */ { "^", "foldop: pow" },
	/* 8 */ { ">", "foldop: max by fn" },
	/* 9 */ { "<", "foldop: min by fn" },
	/* a */ { ":", "foldop: cons" },
	/* b */ { "",  "foldop: undefined nibble 0xb (try Nibbles version >=0.22)" },
	/* c */ { "",  "foldop: undefined nibble 0xc (try Nibbles version >=0.22)" },
	/* d */ { "",  "foldop: undefined nibble 0xd (try Nibbles version >=0.22)" },
	/* e */ { "",  "foldop: undefined nibble 0xe" },
	/* f */ { "",  "foldop: undefined nibble 0xf" },
};

Special foldops_v022[]={
	/* 0 */ { "]", "foldop: max" },
	/* 1 */ { "[", "foldop: min" },
	/* 2 */ { "|", "foldop: or" },
	/* 3 */ { "&", "foldop: and" },
	/* 4 */ { "+", "foldop: add" },
	/* 5 */ { "*", "foldop: mult" },
	/* 6 */ { "-", "foldop: sub" },
	/* 7 */ { "/", "foldop: div" },
	/* 8 */ { "%", "foldop: mod" },
	/* 9 */ { "^", "foldop: pow" },
	/* a */ { ">", "foldop: max by fn" },
	/* b */ { "<", "foldop: min by fn" },
	/* c */ { ":", "foldop: cons" },
	/* d */ { ";", "foldop: rev cons" },
	/* e */ { "",  "foldop: undefined nibble 0xe" },
	/* f */ { "",  "foldop: undefined nibble 0xf" },
};

Special zipops[]={
	/* 0 */ { "~","zipop: by" },
	/* 1 */ { ":","zipop: cons" },
	/* 2 */ { ",","zipop: make tuple" },
	/* 3 */ { "+","zipop: add" },
	/* 4 */ { "*","zipop: mult" },
	/* 5 */ { "-","zipop: sub" },
	/* 6 */ { "/","zipop: div" },
	/* 7 */ { "%","zipop: mod" },
	/* 8 */ { "^","zipop: pow" },
	/* 9 */ { "]","zipop: max" },
	/* a */ { "[","zipop: min" },
	/* b */ { "!","zipop: abs diff" },
	/* c */ { "=","zipop: subscript" },
	/* d */ { "?","zipop: index" },
	/* e */ { "zipop: undefined nibble 0xe" },
	/* f */ { "zipop: undefined nibble 0xf" },
};

Special chclass[]={
	/* 0 */ { "a","isAlpha" },
	/* 1 */ { "A","not.isAlpha" },
	/* 2 */ { "n","isAlphaNum" },
	/* 3 */ { "N","not.isAlphaNum" },
	/* 4 */ { "s","isSpace" },
	/* 5 */ { "S","not.isSpace" },
	/* 6 */ { "l","isLower" },
	/* 7 */ { "L","not.isLower" },
	/* 8 */ { "u","isUpper" },
	/* 9 */ { "U","not.isUpper" },
	/* a */ { "p","isPrint" },
	/* b */ { "P","not.isPrint" },
	/* c */ { "d","isDigit" },
	/* d */ { "D","not.isDigit" },
	/* e */ { "$","isSym" },
	/* f */ { "!","not.isSym" },
};

struct PostData {
	string lit;
	string desc;
	string data;
	string str;
	bool exists;
	int base;
	bool is_dec;  // false:hex true:decimal
	int offs;
};

PostData post;

Type const * coerce_xorchr(Type const *t1,Type const *t2){
	assert(t1->p==p_int||t1->p==p_chr||t1->p==p_auto);
	assert(t2->p==p_int||t2->p==p_chr||t2->p==p_auto);
	return
		t1->p == p_chr && t2->p != p_chr ? t_chr :
		t1->p != p_chr && t2->p == p_chr ? t_chr :
		t_int;
}

Type const * coerce_orchr(Type const *t1,Type const *t2){
	assert(t1->p==p_int||t1->p==p_chr||t1->p==p_auto);
	assert(t2->p==p_int||t2->p==p_chr||t2->p==p_auto);
	return t1->p == p_chr || t2->p == p_chr ? t_chr : t_int;
}

Type const * coerce2(Type const *t1,Type const *t2){
	if(!t1||!t2){
		return nullptr;
	}
	if(t1->p==p_auto){
		return t2;
	}
	if(t2->p==p_auto){
		return t1;
	}
	{
		Type const * t;
		if(t1->p==p_chr&&t2->p==p_chr){
			t=t_chr;
		}
		else 
		if(t1->p==p_int&&t2->p==p_int){
			t=t_int;
		}
		else
		if((t1->p==p_chr||t1->p==p_int)&&(t2->p==p_chr||t2->p==p_int)){
			t=enlist(t_chr);
		}
		else
		if(t2->p==p_list&&t2->el->p==p_chr){
			if(t1->p==p_chr||!t1->isBaseElemChr()){
				t=enlist(t_chr);
			}
			else{
				t=t1->head();
			}
		}
		else
		if(t1->p==p_list&&t1->el->p==p_chr){
			return coerce2(t2,t1);
		}
		else
		if(t1->p==p_list && (t2->p==p_chr||t2->p==p_int)){
			t=enlist(coerce2(t1->el->head(),t2->head()));
		}
		else
		if((t1->p==p_chr||t1->p==p_int) && t2->p==p_list){
			return coerce2(t2,t1);
		}
		else
		if(t1->p==p_list&&t2->p==p_list){
			t=enlist(coerce2(t1->el->head(),t2->el->head()));
		}
		else{
			t=new Type;
		}
		return make_tuple(t,coerce2(t1->ts,t2->ts));
	}
}

Type const * coerce_unvecorchr(Type const *t1,Type const *t2){
	return t1->p==p_list ? t1 : coerce_orchr(t1,t2);
}

// takes element types (maybe tuple)
// returns element type (maybe tuple)
Type const * coerce_zipop(Special*s,Type const *t1,Type const *t2){
	switch(s->lit[0]){

	// always int
	case '*':
	case '/':
	case '%':
	case '!':
	case '?':
		return t_int;

	// xorChr
	// 0.23 pt says Integer but compiler crushes: `/ :"hoge"~ +
	case '+':  
	case '-':
	case '^':  // 0.23 pow -- defined as xorChr ??
		if(codever<25){
			return coerce_xorchr(t1,t2);
		}
		else{
			return t_int;
		}

	// element of t1
	case '=':
		return t1; // t1->head()?
		
	// ! ,10 ,10 : #==> [[int]]
	// ! "hoge" "hoge" : #==> [[chr]]
	// ! % "hoge" ~ % "hoge" ~ : #==> [[chr]]
	// ! % "hoge" ~ ,10 : #==> [[chr]]
	case ':':
	case ';':
		{
			Type const * t = coerce2(t1,t2);
			if(t->p!=p_list){
				t=enlist(t);
			}
			return t;
		}

	// unvecOrChr
	// 0.23 assertion fails: ! ,2 "hoge" ]
	// 0.23 assertion fails: ! "hoge" 1 ]
	case '[':
	case ']':
		return coerce_unvecorchr(t1,t2);
		
	// tuple
	case ',':
		return make_tuple(t1,t2);

	// element type
	case '&':  // 0.23
	case '|':  // 0.23
	case 'x':  // 0.23 unimplemented
	default:
		return coerce2(t1,t2);
	}
}

Type const * coerce_foldop(Special*s,Type const *t1){
	switch(s->lit[0]){
	// `/ ,10 : #==> [int] (actually compile-time Haskell error)
	// `\ ,10 : #==> [[int]]
	case ':':
	case ';':
		return enlist(t1);

	default:
		return coerce_zipop(s,t1,t1);
	}
}

int depth_for_returntype_join(Type const * t){
//printf("t->p=%d\n",t->p);
	int i_am_tuple=0;
	if(t->ts){
		i_am_tuple=1;
	}
	int depth=0;
	while(t){
		if(t->p==p_list&&t->el->p!=p_chr){
			int d1=1+depth_for_returntype_join(t->el);
			if(depth<d1) depth=d1;
		}
		t=t->ts;
	}
//printf("returning %d+%d\n",i_am_tuple,depth);
	return i_am_tuple+depth;
}

Type const * returntype_join_aux(Type const * t,int target);

Type const * returntype_join_aux_tuple(Type const * t,int target){
	if(t){
		return make_tuple(returntype_join_aux(t->head(),target-1),returntype_join_aux_tuple(t->ts,target));
	}
	else{
		return nullptr;
	}
}

Type const * returntype_join_aux(Type const * t,int target){
	if(target==1){
		return enlist(t_chr);
	}
	else if(t->ts){
		return returntype_join_aux_tuple(t,target);
	}
	else if(t->p==p_list&&t->p!=p_chr){
		return enlist(returntype_join_aux(t->el,target-1));
	}
	else{
		return t;
	}
}

Type const * returntype_join(Type const * t){
//printf("target=%d\n",depth_for_returntype_join(t));
	return returntype_join_aux(t,depth_for_returntype_join(t));
}

// return type of mul
Type const * vectorise_int(Type const * shape){
	Type const * t = nullptr;
	if(shape){
		if(shape->p==p_list){
			t=enlist(vectorise_int(shape->el));
		}
		else{
			t=t_int;
		}
		t = make_tuple(t,vectorise_int(shape->ts));
	}
	return t;
}

// return type of add
Type const * vectorise_xorchr(Type const * shape, Type const * elt){
	Type const * t = nullptr;
	if(shape){
		if(shape->p==p_list){
			t=enlist(vectorise_xorchr(shape->el,elt));
		}
		else{
			t=coerce_xorchr(elt,shape);
		}
		t = make_tuple(t,vectorise_xorchr(shape->ts,elt));
	}
	return t;
}

string mkvar(int varid){
	string v;
	int i=varid;
	while(i>=26){
		v=char(97+i%26)+v;
		i/=26;
	}
	v=char(65+i)+v;
//printf("mkvar %s\n",v.c_str());
	return v;
}

bool select_commext(Node*c1,Node*c2){
	if(c2->passed){
		return false;
	}
	{
		int l1=c1->ed-c1->st;
		int l2=c2->ed-c2->st;
		return l1>l2?false:l1<l2?true:memcmp(nbbuf+c1->st,nbbuf+c2->st,l1)<0;
	}
}

Node* parse1_or_implicit(int nbpos,Op0 op0,char*autodesc,vector<Arg const*>const&args,int&varid,int depth);
Node* parse1(int nbpos,Op0 op0,char*autodesc,vector<Arg const*>const&args,int& varid,int depth);

#define OP_REDEF(_lit,_desc)  n->lit=_lit; n->desc=_desc; 
#define OP_DEF(_lit,_desc)    do{ Node * n = new Node; OP_REDEF(_lit,_desc) n->ed=n->st=nbpos; n->context=context; n->ct_let=context.size(); int myvarid=varid;
#define OP_END           OP_END_DEBUG if(n){ varid=myvarid; } return n; }while(0);
//#define OP_END_DEBUG           if(n)printf("OP_END %s\n",n->desc.c_str());
#define OP_END_DEBUG 0;

#define OP_NB(_x)        if(getnb(n->ed++)!=_x) break;

#define OP_NEW_CHILD(_c) Node*_c=new Node; _c->ed=_c->st=n->ed; _c->context=n->context; _c->ct_let=n->context.size();
#define OP_ADD_CHILD(_c) _c->ct_sub=_c->ct_arg=n->context.size(); n->childs.push_back(_c); n->ed=_c->ed; n->context=_c->context; n->ct_let=n->context.size(); n->passed|=_c->passed; if(n->scopeid<_c->scopeid && _c->scopeid<=depth){ n->scopeid=_c->scopeid; }
#define OP_CHILD_AUTODESC(_c,_is,_autodesc)  Node * _c=parse1_or_implicit(n->ed,op0_auto,_autodesc,n->context,myvarid,depth+1); if(!_c->t->_is()) break; OP_ADD_CHILD(_c)
#define OP_CHILD(_c,_is) OP_CHILD_AUTODESC(_c,_is,nullptr)
#define OP_CHILD_TUPLE(_c,_ary)  Node*_c; if(_ary<2){ OP_CHILD(c1,is_any) _c=c1; }else{ OP_NEW_CHILD(c1) c1->desc="implicit tuple"; add_child_tuple(c1,myvarid,depth,_ary); OP_ADD_CHILD(c1) _c=c1; }
#define OP_CHILD_OP0(_c,_is,_op0)  Node * _c=parse1_or_implicit(n->ed,_op0,nullptr,n->context,myvarid,depth+1); if(!_c->t->_is()) break; OP_ADD_CHILD(_c)

#define OP_FN_ADD_CHILD(_c,_e) OP_ADD_CHILD(_c) _c->ct_sub=_e->context.size(); purge_args(n->context,depth+1,_c->ct_arg); n->ct_let=n->context.size();
#define OP_FN_ENV(_e) FnEnv*_e=new FnEnv; _e->context=n->context;
#define OP_FN_ARG(_e,_t)      unzip_tuple(_e,myvarid,depth+1,_t);
#define OP_FN_ARG_OPT(_e,_t)      unzip_tuple(_e,myvarid,depth+1,_t,0,true);
#define OP_FN_CHILD(_c,_e)    Node * _c=parse1_or_implicit(n->ed,op0_tuple,nullptr,_e->context,myvarid,depth+1); OP_FN_ADD_CHILD(_c,_e)
#define OP_FN_CHILD_TUPLE(_c,_e,_ary)  Node*_c; if(_ary<2){ OP_FN_CHILD(c1,_e) _c=c1; }else{ OP_NEW_CHILD(c1) c1->desc="implicit tuple"; c1->context=_e->context; add_child_tuple(c1,myvarid,depth,_ary); OP_FN_ADD_CHILD(c1,_e) _c=c1; }

void purge_args(vector<Arg const *> & context,int scopeid,int start){
	int i=start;
	int j=start;
//printf("purge_args scopeid=%d\n",scopeid);
	while(i<context.size()){
		if(context[i]->scopeid!=scopeid){
//printf("alive: %d %d %s\n",i,context[i]->scopeid,context[i]->desc.c_str());
			if(i!=j){
				context[j]=context[i];
			}
			++j;
		}
		else{
//printf("dead : %d %d %s\n",i,context[i]->scopeid,context[i]->desc.c_str());
		}
		++i;
	}
	context.resize(j);
}

#define OP_TUPLE_ELEMENTS(_ary)	for(int i=0;i<_ary;++i){ OP_CHILD_OP0(c1,is_any,i==0?op0_tuple:op0_auto) } { Type*t=nullptr; for(int i=ary;i--;){ Type*t1=n->childs[i]->t->clone(); t1->ts=t; t=t1; } n->t=t; } /* n->op=op_tuple; */

void add_child_tuple(Node*n,int&myvarid,int depth,int ary){
	OP_TUPLE_ELEMENTS(ary);
}

#define OP_OPTION(_c,_desc) Node* _c=nullptr;if(getnb(n->ed)==0){ ++n->ed; OP_NEW_CHILD(c1) c1->lit="~"; c1->desc=_desc; OP_ADD_CHILD(c1) _c=c1; }

#define OP_CHECK(_cond)  if(!(_cond)) break;
#define OP_FAIL          break;

#define OP_TYPE(_t)      { Type const * _t1 = _t; if(_t1->ts){ OP_LET(_t1->ts) n->t=_t1->head(); }else{ n->t=_t1; } }

#define OP_LET(_t) unzip_tuple(n,myvarid,n->scopeid,_t);
#define OP_LET2(_t,_ll) unzip_tuple(n,myvarid,n->scopeid,_t,_ll);

// unzip fn args
void unzip_tuple(FnEnv*e,int&myvarid,int scopeid,Type const *t,int ll=0,bool opt=false){
	if(t){
		unzip_tuple(e,myvarid,scopeid,t->ts,ll,opt);
		mkarg(e->context,scopeid,mkvar(myvarid++),t->head(),ll,opt);
	}
}

// unzip let args
void unzip_tuple(Node*n,int&myvarid,int scopeid,Type const *t,int ll=0){
	if(t){
		unzip_tuple(n,myvarid,scopeid,t->ts,ll);
		mkarg(n->context,scopeid,mkvar(myvarid++),t->head(),ll);
	}
}

#define OP_QUIET_VERSION_LT(_x) OP_CHECK(codever<_x)
#define OP_QUIET_VERSION_GE(_x) OP_CHECK(codever>=_x)

#define OP_VERSION_LT(_x) if(!(codever< _x)){ if(codever_lt>_x) codever_lt=_x; break; }
#define OP_VERSION_GE(_x) if(!(codever>=_x)){ if(codever_ge<_x) codever_ge=_x; break; }
#define OP_VERSION_GE_LT(_ge,_lt) if(!(codever>=_ge&&codever<_lt)){ if(codever_ge<_ge) codever_ge=_ge; if(codever_lt>_lt) codever_lt=_lt; break; }

struct MemoKey {
	int nbpos;
	Op0 op0;
	char* autodesc;
	vector<Arg const*> context;
	int varid;
	int depth;
};
bool operator<(MemoKey const & a,MemoKey const & b){
	return
		a.nbpos < b.nbpos ? true:
		b.nbpos < a.nbpos ? false:
		a.op0 < b.op0 ? true:
		b.op0 < a.op0 ? false:
		a.autodesc < b.autodesc ? true:
		b.autodesc < a.autodesc ? false:
		a.varid < b.varid ? true:
		b.varid < a.varid ? false:
		a.depth < b.depth ? true:
		b.depth < a.depth ? false:
		a.context < b.context;
}

map<MemoKey,pair<Node*,int>> memo;

Node* parse1_or_implicit(int nbpos,Op0 op0,char*autodesc,vector<Arg const*>const&context,int&varid,int depth){
	MemoKey k;
	k.nbpos=nbpos;
	k.op0=op0;
	k.autodesc=autodesc;
	k.context=context;
	k.varid=varid;
	k.depth=depth;
	auto it=memo.find(k);
	if(it!=memo.end()){
		varid=it->second.second;
		return it->second.first;
	}
	Node*n=parse1(nbpos,op0,autodesc,context,varid,depth);
	if(!n){
		int nth;
		for(nth=context.size();nth--;){
			Arg const*a=context[nth];
			if(!a->optional&&!a->used){
				goto found;
			}
		}
		nth=context.size()-1;

		found:;
		n=new Node;
		n->passed=true;
		n->ed=n->st=nbpos;
		n->context=context;
		set_used(n->context,nth);
		n->ct_let=n->context.size();
		int myvarid=varid;
		Arg const*a=n->context[nth];
		n->desc="implicit arg = "+a->desc;
		if(n->scopeid<a->scopeid && a->scopeid<=depth){
			n->scopeid=a->scopeid;
		}
		if(a->t->p==p_lambda){
			int ary=a->t->fa->ary();
			OP_TUPLE_ELEMENTS(ary)
			OP_TYPE(a->t->el)
		}
		else{
			OP_TYPE(a->t)
		}
		varid=myvarid;
	}
	memo[k]={n,varid};
	return n;
}

Node* parse1(int nbpos,Op0 op0,char*autodesc,vector<Arg const*>const&context,int& varid,int depth){

	if(getnb(nbpos)<0){
		return nullptr;
	}
	if(getnb(nbpos)==6&&getnb(nbpos+1)<0){
		return nullptr;
	}

OP_DEF("~","auto")
	OP_CHECK(op0==op0_auto)
	OP_NB(0)
	if(autodesc){
		n->desc+=string(" = ")+autodesc;
	}
	OP_TYPE(t_auto)
OP_END

// n-ary tuple
OP_DEF("~","tuple")
	OP_CHECK(op0==op0_tuple);
	OP_NB(0)
	int ary=2;
	int b1;
	while(b1=getnb(n->ed++),b1==0){
		n->lit+="~";
		++ary;
	}
	--n->ed;
	OP_TUPLE_ELEMENTS(ary);
	if(ary>2){
		stringstream ss;
		ss<<ary<<"-ary tuple";
		n->desc=ss.str();
	}
OP_END

OP_DEF("","integer")
	OP_NB(1)
	long v;
	get_int_body(n->ed,v);
	stringstream ss;
	ss<<v;
	n->lit=ss.str();
	OP_TYPE(t_int)
OP_END

// string or string list
OP_DEF("","string")
	OP_NB(2)
	vector<wstring>v;
	get_strings_body(n->ed,v);
	for(auto&& s:v){
		n->lit+="\""+show_string(s)+"\"";
	}
	if(v.size()>1){
		n->desc="string list";
		n->t=enlist(enlist(t_chr));
	}
	else{
		n->t=enlist(t_chr);
	}
OP_END

// argument
OP_DEF("","")
	int rnth=0;
	int b;
	while(b=getnb(n->ed++),b==6){
		rnth+=3;
		n->lit+=";";
	}
	OP_CHECK(b>=3&&b<=5)
	rnth+=b-2;
	n->lit+="$@_"[(rnth-1)%3];
	int nth=n->context.size()-rnth;
	set_used(n->context,nth);
	Arg const*a=n->context[nth];
	n->desc+="= "+a->desc;
	if(n->scopeid<a->scopeid && a->scopeid<=depth){
		n->scopeid=a->scopeid;
	}
	if(a->t->p==p_lambda){
		int ary=a->t->fa->ary();
		OP_TUPLE_ELEMENTS(ary)
		OP_TYPE(a->t->el)
	}
	else{
		OP_TYPE(a->t)
	}
OP_END

OP_DEF(";~","save fn")
	OP_NB(6)
	OP_NB(0)
	OP_CHILD_OP0(c1,is_any,op0_tuple)
	OP_FN_ENV(c2e)
	OP_FN_ARG(c2e,c1->t)
	OP_FN_CHILD(c2,c2e)
	OP_CHECK(c2->args_used())
	Type*ta=new Type;
	ta->p=p_lambda;
	ta->el=c2->t;
	ta->fa=c1->t;
	OP_LET(ta)
	OP_TYPE(c2->t)
OP_END

OP_DEF("==","equal?")
	OP_NB(6)
	OP_NB(0)
	OP_CHILD_OP0(c1,is_any,op0_tuple)
	OP_FN_ENV(c2e)
	OP_FN_ARG(c2e,c1->t)
	OP_FN_CHILD(c2,c2e)
	OP_CHECK(!c2->args_used())
	OP_TYPE(t_int)
OP_END

// recursion: 66,a00
OP_DEF("`;","recursion")
	OP_NB(6)
	OP_NB(6)
	OP_CHILD_OP0(c1,is_any,op0_tuple) // initial value
	int fnpos=n->context.size();
	OP_FN_ENV(cimpe)
	OP_FN_ARG(cimpe,c1->t)
	OP_NEW_CHILD(cimp)
	cimp->desc="cond,base,rec";
	{
		Node*n=cimp;
		n->context=cimpe->context;
		OP_OPTION(c19,"not")
		OP_CHILD(c2,is_any) // condition
		OP_CHILD_OP0(c3,is_any,op0_tuple) // base case

		Type * ta=new Type;
		ta->p=p_lambda;
		ta->el=c3->t;
		ta->fa=c1->t;
		OP_FN_ARG(cimpe,ta)
		n->context.insert(n->context.begin()+fnpos,cimpe->context.back());
		OP_CHILD_TUPLE(c4,c3->t->ary()) // recursive case
		n->t=c3->t;
	}
	OP_FN_ADD_CHILD(cimp,cimpe)
	OP_TYPE(cimp->t)
	cimp->t=nullptr;
OP_END

OP_DEF(";","save")
	if(codever<23){
		n->desc="let";
	}
	OP_NB(6)
	OP_CHILD(c1,is_any);
	OP_LET(c1->t)
	OP_TYPE(c1->t)
OP_END

OP_DEF(":","append")
	OP_NB(7)
	OP_CHILD_OP0(c1,is_any,op0_tuple) // it seems extra values go nowhere
	OP_CHILD_AUTODESC(c2,is_any,"[]")
	Type const * t;
	if(c2->t->is_auto()){
		t=enlist(c1->t);
	}else{
		t=coerce2(c1->t,c2->t);
		if(t->p!=p_list){
			t=enlist(t);
		}
	}
	OP_TYPE(t);
OP_END

OP_DEF("`D","to base from data")
	OP_NB(8)
	OP_NB(0)
	OP_NB(0)
	OP_NEW_CHILD(c1)
	long v;
	get_int_body(c1->ed,v);
	stringstream ss;
	ss<<v;
	c1->lit=ss.str();
	c1->desc="to base from data arg";
	OP_ADD_CHILD(c1)
	OP_TYPE(enlist(v<0?t_chr:t_int))
	post.exists=true;
	post.desc="to base from data";
	post.base=v;
OP_END

OP_DEF("`@","to base")
	OP_NB(8)
	OP_NB(7)
	OP_CHILD_AUTODESC(c1,is_int_or_auto,"10")
	OP_CHILD_AUTODESC(c2,is_num_or_auto,"tbd")
	OP_TYPE(enlist(t_int))
OP_END

OP_DEF("\x5d","max")
	OP_NB(8)
	OP_CHILD_AUTODESC(c1,is_num_or_auto,"0")
	OP_CHILD(c2,is_num)
	OP_CHECK(select_commext(c1,c2))
	OP_TYPE(coerce_orchr(c1->t,c2->t))
OP_END

OP_DEF("+","add")
	OP_NB(8)
	OP_CHILD_AUTODESC(c1,is_num_or_auto,"1")
	OP_CHILD_AUTODESC(c2,is_any,"1")
	//OP_TYPE(c2->t,coerce_xorchr(c1->t,c2->t))
	OP_TYPE(vectorise_xorchr(c2->t,c1->t->auto_as(t_int)))
OP_END

OP_DEF("+","sum")
	OP_NB(8)
	OP_CHILD(c1,is_list_int)
	OP_LET2(c1->t->el->ts,1)
	OP_TYPE(t_int)
OP_END

OP_DEF("+","concat")
	OP_NB(8)
	OP_CHILD(c1,is_list_list)
	OP_TYPE(c1->t->el)
OP_END

OP_DEF("%","split (remove empties)")
	OP_NB(8)
	OP_CHILD(c1,is_str)
	OP_CHILD_AUTODESC(c2,is_str_or_chr_or_auto,"words")
	OP_TYPE(enlist(enlist(t_chr)))
OP_END

OP_DEF("*","join")
	OP_NB(8)
	OP_CHILD(c1,is_str)
	OP_CHILD(c2,is_list)
	//OP_TYPE(enlist(t_chr))
	OP_TYPE(returntype_join(c2->t))
OP_END

OP_DEF("&","justify")
	OP_NB(8)
	OP_CHILD(c1,is_str)
	OP_CHILD(c2,is_int)
	OP_OPTION(c3,"center")
	OP_CHILD(c4,is_any)
	bool high=c4->t->is_list()&&!c4->t->el->is_chr();
	OP_TYPE(high?enlist(enlist(t_chr)):enlist(t_chr))
OP_END

OP_DEF(".~~","append until null")
	OP_NB(9)
	OP_NB(0)
	OP_NB(0)
	OP_CHILD_OP0(c1,is_any,op0_tuple)
	OP_FN_ENV(c2e)
	OP_FN_ARG(c2e,c1->t)
	OP_FN_CHILD(c2,c2e)
	OP_TYPE(enlist(c1->t))
OP_END

OP_DEF("`*","product")
	OP_NB(9)
	OP_NB(0)
	OP_CHILD(c1,is_list_int)
	OP_LET2(c1->t->el->ts,1)
	OP_TYPE(t_int)
OP_END

OP_DEF("`*","nary cartesian product")
	OP_NB(9)
	OP_NB(0)
	OP_CHILD(c1,is_list_list)
	OP_TYPE(c1->t)
OP_END

OP_DEF("-~","strip")
	OP_NB(9)
	OP_NB(0)
	OP_CHILD(c1,is_str)
	OP_TYPE(enlist(t_chr))
OP_END

OP_DEF("`/","divmod")
	OP_NB(9)
	OP_CHILD_AUTODESC(c1,is_num_or_auto,"2")
	OP_NB(0xd)
	OP_CHILD_AUTODESC(c2,is_num_or_auto,"2")
	OP_LET(c1->t)
	OP_TYPE(t_int)
OP_END

OP_DEF("-","subtract")
	OP_NB(9)
	OP_CHILD_AUTODESC(c1,is_num_or_auto,"1")
	OP_CHILD_AUTODESC(c2,is_num_or_auto,"1")
	OP_TYPE(coerce_xorchr(c1->t,c2->t))
OP_END

OP_DEF("=","subscript (wrapped)")
	OP_NB(9)
	OP_CHILD(c1,is_num)
	OP_CHILD(c2,is_list)
	OP_TYPE(c2->t->el)
OP_END

OP_DEF("|","partition")
	OP_NB(9)
	OP_CHILD(c1,is_list)
	//OP_NB(0)
	//OP_NB(0)
	OP_OPTION(c11,"par......")
	OP_CHECK(c11)
	OP_OPTION(c12,"...tition")
	OP_CHECK(c12)
	OP_OPTION(c2,"not")
	OP_FN_ENV(c3e)
	OP_FN_ARG(c3e,c1->t->el)
	OP_FN_CHILD(c3,c3e)
	OP_LET(c1->t)
	OP_TYPE(c1->t)
OP_END

OP_DEF("|","filter")
	OP_NB(9);
	OP_CHILD(c1,is_list)
	OP_OPTION(c2,"not")
	OP_FN_ENV(c3e)
	OP_FN_ARG(c3e,c1->t->el)
	OP_FN_CHILD(c3,c3e)
	OP_CHECK(c2||c3->args_used())
	OP_TYPE(c1->t)
OP_END

OP_DEF("!","zip with")
	OP_NB(9);
	OP_CHILD(c1,is_list)
	OP_OPTION(c2,"")
	OP_CHECK(!c2)
	OP_FN_ENV(c3e)
	OP_FN_ARG(c3e,c1->t->el)
	OP_FN_CHILD(c3,c3e)
	OP_CHECK(!c3->args_used())
	OP_NEW_CHILD(c4)
	int b5=getnb(c4->ed++);
	OP_CHECK(!(b5<0))
	Special*sp=&zipops[b5];
	c4->lit =sp->lit;
	c4->desc=sp->desc;
	OP_ADD_CHILD(c4)
	if(sp->lit=="~"){
		OP_FN_ENV(c5e)
		OP_FN_ARG(c5e,c3->t->is_list()?c3->t->el:c3->t)
		OP_FN_ARG(c5e,c1->t->el)
		OP_FN_CHILD(c5,c5e)
		OP_TYPE(enlist(c5->t))
	}
	else{
		Type const *tz=coerce_zipop(sp,c1->t->el,c3->t->is_list()?c3->t->el:c3->t);
		OP_TYPE(enlist(tz));
	}
OP_END

OP_DEF("``;","recursion")
	OP_NB(0xa)
	OP_NB(0x0)
	OP_NB(0x0)
	OP_CHILD_OP0(c1,is_any,op0_tuple) // initial value
	int fnpos=n->context.size();
	OP_FN_ENV(cimpe)
	OP_FN_ARG(cimpe,c1->t)
	OP_NEW_CHILD(cimp)
	cimp->desc="cond,base,rec";
	{
		Node*n=cimp;
		n->context=cimpe->context;
		OP_OPTION(c19,"not")
		OP_CHILD(c2,is_any) // condition
		OP_CHILD_OP0(c3,is_any,op0_tuple) // base case

		Type * ta=new Type;
		ta->p=p_lambda;
		ta->el=c3->t;
		ta->fa=c1->t;
		OP_FN_ARG(cimpe,ta)
		n->context.insert(n->context.begin()+fnpos,cimpe->context.back());
		OP_CHILD_TUPLE(c4,c3->t->ary()) // recursive case
		n->t=c3->t;
	}
	OP_FN_ADD_CHILD(cimp,cimpe)
	OP_TYPE(cimp->t)
	cimp->t=nullptr;
OP_END

OP_DEF("\\","char class")
	OP_NB(0xa)
	OP_CHILD(c1,is_chr)
	OP_NEW_CHILD(c2)
	int b2=getnb(c2->ed++);
	OP_CHECK(!(b2<0))
	c2->lit =chclass[b2].lit;
	c2->desc=chclass[b2].desc;
	OP_ADD_CHILD(c2)
	OP_TYPE(enlist(t_chr))
OP_END

OP_DEF("\x5b","min")
	OP_NB(0xa)
	OP_CHILD(c1,is_int)
	OP_CHILD(c2,is_num)
	OP_CHECK(select_commext(c1,c2))
	n->t=coerce_orchr(c1->t,c2->t);
OP_END

OP_DEF("*","multiply")
	OP_NB(0xa)
	OP_CHILD_AUTODESC(c1,is_int_or_auto,"-1")
	OP_CHILD_AUTODESC(c2,is_any,"2")
	OP_TYPE(vectorise_int(c2->t))
OP_END

// # Nibbles 0.24
// /
// 	,2
// 	~
// 		3
// 		4
// 	#
// 		~ # this means tuple
// 			8
// 			9
// 		~ # this means auto (<0.24 error)

OP_DEF("/","foldr")
	OP_NB(0xa)
	OP_CHILD(c1,is_list)

	if(codever<24){

		// version 0.2 .. 0.23
		// 5 cases
		if(c1->t->el->ts){
			OP_OPTION(c2,"initial value for foldr follows")
			if(c2){
				n->desc="hidden foldr list tuple";
				OP_CHILD_OP0(c3,is_any,op0_tuple)
				OP_FN_ENV(c4e)
				OP_FN_ARG(c4e,c3->t);
				OP_FN_ARG(c4e,c1->t->el);
				int ary=c3->t->ary();
				OP_FN_CHILD_TUPLE(c4,c4e,ary)
				OP_LET(c3->t->ts)
				OP_TYPE(c3->t->head())
			}
			else{
				n->desc="hidden foldr1 list tuple";
				OP_FN_ENV(c4e)
				OP_FN_ARG(c4e,c1->t->el);
				OP_FN_ARG(c4e,c1->t->el);
				int ary=c1->t->el->ary();
				OP_FN_CHILD_TUPLE(c4,c4e,ary)
				OP_LET(c1->t->el->ts)
				OP_TYPE(c1->t->el->head())
			}
		}
		else{
			if(getnb(n->ed)==0){
				OP_CHILD_OP0(c3,is_any,op0_tuple)
				OP_FN_ENV(c4e)
				OP_FN_ARG(c4e,c3->t);
				OP_FN_ARG(c4e,c1->t->el);
				int ary=c3->t->ary();
				OP_FN_CHILD_TUPLE(c4,c4e,ary)
				OP_LET(c3->t->ts);
				OP_TYPE(c3->t->head())
			}
			else{
				OP_FN_ENV(c3e)
				OP_FN_ARG(c3e,c1->t->el);
				OP_FN_ARG(c3e,c1->t->el);
				OP_FN_CHILD(c3,c3e)
				if(c3->args_used()){
					n->desc="foldr1";
					OP_TYPE(c1->t->el)
				}
				else{
					OP_FN_ENV(c4e)
					OP_FN_ARG(c4e,c3->t);
					OP_FN_ARG(c4e,c1->t->el);
					OP_FN_CHILD(c4,c4e)
					OP_TYPE(c3->t)
				}
			}
		}

	}
	else{

		// version 0.24 ..
		// 3 cases
		if(getnb(n->ed)==0){
			OP_CHILD_OP0(c3,is_any,op0_tuple)
			OP_FN_ENV(c4e)
			OP_FN_ARG(c4e,c3->t);
			OP_FN_ARG(c4e,c1->t->el);
			int ary=c3->t->ary();
			OP_FN_CHILD_TUPLE(c4,c4e,ary)
			OP_LET(c3->t->ts);
			OP_TYPE(c3->t->head())
		}
		else{
			OP_FN_ENV(c3e)
			OP_FN_ARG(c3e,c1->t->el);
			OP_FN_ARG(c3e,c1->t->el);
			Node n_saved=*n;
			OP_FN_CHILD(c3,c3e)
			if(c3->args_used()){
				*n=n_saved;
				n->desc="foldr1";
				int ary=c1->t->el->ary();
				OP_FN_CHILD_TUPLE(c3,c3e,ary)
				OP_LET(c1->t->el->ts)
				OP_TYPE(c1->t->el->head())
			}
			else{
				OP_FN_ENV(c4e)
				OP_FN_ARG(c4e,c3->t);
				OP_FN_ARG(c4e,c1->t->el);
				OP_FN_CHILD(c4,c4e)
				OP_TYPE(c3->t)
			}
		}
	}
	
OP_END


// (0.2,0.21) hex int is not b02 but edxd
// (0.2,0.21) hex any is not found
// (1.00) compiler crashes: hex ~ 
OP_DEF("hex","to/from hex")
	OP_NB(0xb)
	OP_NB(0x0)
	OP_NB(0x2)
	OP_CHILD(c1,is_any)
	OP_VERSION_GE(22)
	if(c1->t->is_list_or_chr()){
		OP_TYPE(t_int)
	}
	else{
		OP_TYPE(enlist(t_chr))
	}
OP_END

OP_DEF("<<","init")
	OP_NB(0xb)
	OP_NB(0x0)
	OP_CHILD(c1,is_list)
	OP_TYPE(c1->t)
OP_END

OP_DEF("`|","bit union")
	OP_NB(0xb)
	OP_NB(0x0)
	OP_CHILD_AUTODESC(c1,is_num_or_auto,"1")
	OP_CHILD(c2,is_num)
	OP_CHECK(select_commext(c1,c2))
	OP_TYPE(coerce_orchr(c1->t,c2->t))
OP_END

OP_DEF("`^","bit xor")
	OP_NB(0xb)
	OP_NB(0x0)
	OP_CHILD(c1,is_num)
	OP_CHILD_AUTODESC(c2,is_num_or_auto,"1")
	OP_TYPE(coerce_xorchr(c1->t,c2->t))
OP_END

OP_DEF("`@","from base")
	OP_NB(0xb)
	OP_NB(0x0)
	OP_CHILD_AUTODESC(c1,is_num_or_auto,"10")
	OP_CHILD(c2,is_list)
	OP_TYPE(c1->t->is_auto()?t_int:c1->t)
OP_END

// Nibbles 0.24
// same as / (foldr1) in that first ~ means tuple and rest auto, in c3.

OP_DEF("`.","iterate while uniq")
	OP_NB(0xb)
	OP_NB(0x2)
	OP_CHILD_OP0(c1,is_any,op0_tuple)
	OP_OPTION(c2,"inf")
	OP_FN_ENV(c3e)
	OP_FN_ARG(c3e,c1->t)
	int ary=c1->t->ary();
	OP_FN_CHILD_TUPLE(c3,c3e,ary)
	OP_TYPE(enlist(c1->t));
OP_END

OP_DEF("`,","range from 0")
	OP_NB(0xb)
	OP_NB(0x7)
	OP_CHILD_AUTODESC(c1,is_num_or_auto,"infinity")
	OP_TYPE(enlist(c1->t->is_auto()?t_int:c1->t))
OP_END

OP_DEF("``p","permutations")
	OP_NB(0xb)
	OP_NB(0x8)
	OP_NB(0x0)
	OP_CHILD(c1,is_list)
	OP_VERSION_GE(100)
	OP_TYPE(enlist(c1->t))
OP_END

OP_DEF("`<","take also drop")
	OP_NB(0xb)
	OP_NB(0x8)
	OP_CHILD(c1,is_int)
	OP_CHILD(c2,is_list)
	OP_VERSION_GE(100)
	OP_LET(c2->t)
	OP_TYPE(c2->t)
OP_END

OP_DEF("`=","chunk by")
	if(codever<23){
		n->desc="group by";
	}
	OP_NB(0xb)
	OP_NB(0x9)
	OP_CHILD(c1,is_list)
	OP_FN_ENV(c2e)
	OP_FN_ARG(c2e,c1->t->el)
	OP_FN_CHILD(c2,c2e)
	OP_CHECK(c2->args_used())
	OP_TYPE(enlist(c1->t))
OP_END

OP_DEF("`%","step")
	OP_NB(0xb)
	OP_NB(0xc)
	OP_CHILD_AUTODESC(c1,is_num_or_auto,"2")
	OP_CHILD(c2,is_list)
	OP_TYPE(c2->t)
OP_END

OP_DEF("=~","group by (also sorts)")
	if(codever<23){
		n->desc="group all by";
	}
	OP_NB(0xb)
	OP_NB(0xc)
	OP_CHILD(c1,is_list)
	OP_OPTION(c2,"nosort")
	OP_FN_ENV(c3e)
	OP_FN_ARG(c3e,c1->t->el)
	OP_FN_CHILD(c3,c3e)
	OP_CHECK(c3->args_used())
	OP_TYPE(enlist(c1->t))
OP_END

OP_DEF("or","or")
	OP_NB(0xb)
	OP_NB(0xc)
	OP_CHILD(c1,is_list)
	OP_OPTION(c2,"tbd")
	OP_FN_ENV(c3e)
	OP_FN_ARG(c3e,c1->t->el)
	OP_FN_CHILD(c3,c3e)
	OP_CHECK(!c3->args_used())
	OP_TYPE(c1->t)
OP_END

OP_DEF("`/","chunks of")
	OP_NB(0xb)
	OP_NB(0xe)
	OP_CHILD_AUTODESC(c1,is_num_or_auto,"2")
	OP_CHILD(c2,is_list)
	OP_TYPE(enlist(c2->t))
OP_END

OP_DEF("`%","moddiv")
	OP_NB(0xb)
	OP_CHILD(c1,is_num)
	OP_NB(0xd)
	OP_CHILD_AUTODESC(c2,is_num_or_auto,"2")
	OP_LET(t_int)
	OP_TYPE(t_int)
OP_END

OP_DEF("/","divide")
	OP_NB(0xb)
	OP_CHILD(c1,is_num)
	OP_CHILD_AUTODESC(c2,is_num_or_auto,"2")
	OP_TYPE(t_int)
OP_END

OP_DEF("<","take")
	OP_NB(0xb)
	OP_CHILD(c1,is_num)
	OP_CHILD(c2,is_list)
	OP_TYPE(c2->t)
OP_END

OP_DEF("\\","reverse")
	OP_NB(0xb)
	OP_CHILD(c1,is_list)
	OP_TYPE(c1->t)
OP_END

OP_DEF(">>","tail")
	OP_NB(0xc)
	OP_NB(0x0)
	OP_CHILD(c1,is_list)
	OP_TYPE(c1->t)
OP_END

OP_DEF("`>~","drop take while")
	OP_NB(0xc)
	OP_NB(0x0)
	OP_NB(0x0)
	OP_CHILD(c1,is_list)
	OP_OPTION(c2,"not")
	OP_FN_ENV(c3e)
	OP_FN_ARG(c3e,c1->t->el)
	OP_FN_CHILD(c3,c3e)
	OP_LET(c1->t)
	OP_TYPE(c1->t)
OP_END

OP_DEF("!=","abs diff")
	OP_NB(0xc)
	OP_NB(0x0)
	OP_CHILD_AUTODESC(c1,is_num_or_auto,"0")
	OP_CHILD(c2,is_num)
	OP_CHECK(select_commext(c1,c2))
	OP_TYPE(t_int)
OP_END

OP_DEF("`&","bit intersection")
	OP_NB(0xc)
	OP_NB(0x0)
	OP_CHILD(c1,is_num)
	OP_CHILD_AUTODESC(c2,is_num_or_auto,"-2")
	OP_TYPE(coerce_orchr(c1->t,c2->t))
OP_END

OP_DEF("`>","drop also take")
	OP_NB(0xc)
	OP_NB(0x0)
	OP_CHILD(c1,is_num)
	OP_CHILD(c2,is_list)
	OP_LET(c2->t)
	OP_TYPE(c2->t)
OP_END

OP_DEF("``p","permutations")
	OP_NB(0xc)
	OP_NB(0x1)
	OP_NB(0x8)
	OP_CHILD(c1,is_list)
	OP_VERSION_LT(21)
	OP_TYPE(c1->t)
OP_END

OP_DEF("`_","subsequences")
	OP_NB(0xc)
	OP_NB(0xc)
	OP_CHILD_AUTODESC(c1,is_int_or_auto,"2")
	OP_CHILD(c2,is_list)
	OP_TYPE(enlist(c2->t))
OP_END

OP_DEF("`\x29","to uppercase")
	OP_NB(0xc)
	OP_CHILD(c1,is_chr)
	OP_NB(0x2)
	OP_TYPE(c1->t)
OP_END

OP_DEF("`\x28","to lowercase")
	OP_NB(0xc)
	OP_CHILD(c1,is_chr)
	OP_NB(0x7)
	OP_TYPE(c1->t)
OP_END

OP_DEF("%","modulus")
	OP_NB(0xc)
	OP_CHILD(c1,is_num)
	OP_CHILD_AUTODESC(c2,is_num_or_auto,"2")
	OP_TYPE(t_int)
OP_END

OP_DEF(">","drop")
	OP_NB(0xc)
	OP_CHILD(c1,is_int)
	OP_CHILD(c2,is_list)
	OP_TYPE(c2->t)
OP_END

OP_DEF(".","map")
	OP_NB(0xc)
	OP_CHILD(c1,is_list)
	OP_FN_ENV(c2e)
	OP_FN_ARG(c2e,c1->t->el)
	OP_FN_CHILD(c2,c2e)
	OP_TYPE(enlist(c2->t))
OP_END

// char
OP_DEF("","char")
	OP_NB(0xd)
	OP_NB(0x2)
	string ch;
	int b2=getnb(n->ed++);
	if(b2<0) giveup();
	if(b2==0){
		ch="\n";
	}
	else if(b2==1){
		ch=" ";
	}
	else if(b2>=8){
		ch="/.,`a@A0"[b2-8];
	}
	else{
		int b3=getnb(n->ed++);
		if(b3<0) giveup();
		int bb=b2<<4|b3;
		if(bb==127){
			int b4=getnb(n->ed++);
			if(b4<0) giveup();
			int b5=getnb(n->ed++);
			if(b5<0) giveup();
			bb=b4<<4|b5;
		}
		ch=(char)bb;
	}
	n->lit="\'"+show_string(ch)+"\'";
	OP_TYPE(t_chr)
OP_END

OP_DEF("``@","to bits")
	OP_NB(0xd)
	OP_NB(0x7)
	OP_NB(0x0)
	OP_CHILD(c1,is_num)
	OP_VERSION_GE(25)
	OP_TYPE(enlist(t_int))
OP_END

OP_DEF("`$","signum")
	OP_NB(0xd)
	OP_NB(0x7)
	if(codever>=25){
		OP_CHILD(c1,is_num)
	}
	else{
		OP_CHILD(c1,is_num_or_auto)
	}
	OP_TYPE(t_int)
OP_END

OP_DEF("=~","subscript nowrap")
	OP_NB(0xd)
	OP_NB(0x8)
	OP_CHILD(c1,is_num)
	OP_CHILD(c2,is_list)
	OP_TYPE(c2->t->el)
OP_END

OP_DEF("=\\","scanl")
	OP_NB(0xd)
	OP_NB(0xc)
	OP_CHILD(c1,is_list)

	if(codever<24){

		// version 0.2 .. 0.23
		// 5 cases
		if(c1->t->el->ts){
			OP_OPTION(c2,"initial value for scanl follows")
			if(c2){
				n->desc="hidden scanl list tuple";
				OP_CHILD_OP0(c3,is_any,op0_tuple)
				OP_FN_ENV(c4e)
				OP_FN_ARG(c4e,c3->t);
				OP_FN_ARG(c4e,c1->t->el);
				int ary=c3->t->ary();
				OP_FN_CHILD_TUPLE(c4,c4e,ary)
				OP_TYPE(enlist(c3->t))
			}
			else{
				n->desc="hidden scanl1 list tuple";
				OP_FN_ENV(c4e)
				OP_FN_ARG(c4e,c1->t->el);
				OP_FN_ARG(c4e,c1->t->el);
				int ary=c1->t->el->ary();
				OP_FN_CHILD_TUPLE(c4,c4e,ary)
				OP_TYPE(c1->t)
			}
		}
		else{
			if(getnb(n->ed)==0){
				OP_CHILD_OP0(c3,is_any,op0_tuple)
				OP_FN_ENV(c4e)
				OP_FN_ARG(c4e,c3->t);
				OP_FN_ARG(c4e,c1->t->el);
				int ary=c3->t->ary();
				OP_FN_CHILD_TUPLE(c4,c4e,ary)
				OP_TYPE(enlist(c3->t))
			}
			else{
				OP_FN_ENV(c3e)
				OP_FN_ARG(c3e,c1->t->el);
				OP_FN_ARG(c3e,c1->t->el);
				OP_FN_CHILD(c3,c3e)
				if(c3->args_used()){
					n->desc="scanl1";
					OP_TYPE(c1->t)
				}
				else{
					OP_FN_ENV(c4e)
					OP_FN_ARG(c4e,c3->t);
					OP_FN_ARG(c4e,c1->t->el);
					OP_FN_CHILD(c4,c4e)
					OP_TYPE(enlist(c3->t))
				}
			}
		}
	}
	else{

		// version 0.24 ..
		// 3 cases
		if(getnb(n->ed)==0){
			OP_CHILD_OP0(c3,is_any,op0_tuple)
			OP_FN_ENV(c4e)
			OP_FN_ARG(c4e,c3->t);
			OP_FN_ARG(c4e,c1->t->el);
			int ary=c3->t->ary();
			OP_FN_CHILD_TUPLE(c4,c4e,ary)
			OP_TYPE(enlist(c3->t))
		}
		else{
			OP_FN_ENV(c3e)
			OP_FN_ARG(c3e,c1->t->el);
			OP_FN_ARG(c3e,c1->t->el);
			Node n_saved=*n;
			OP_FN_CHILD(c3,c3e)
			if(c3->args_used()){
				*n=n_saved;
				n->desc="scanl1";
				int ary=c1->t->el->ary();
				OP_FN_CHILD_TUPLE(c3,c3e,ary)
				OP_TYPE(c1->t)
			}
			else{
				OP_FN_ENV(c4e)
				OP_FN_ARG(c4e,c3->t);
				OP_FN_ARG(c4e,c1->t->el);
				OP_FN_CHILD(c4,c4e)
				OP_TYPE(enlist(c3->t))
			}
		}
	}
OP_END

OP_DEF("ch","chr")
	OP_NB(0xd)
	OP_NB(0xd)
	OP_CHILD_AUTODESC(c1,is_int_or_auto,"256")
	OP_TYPE(t_chr)
OP_END

OP_DEF("`\\","n chunks")
	OP_NB(0xd)
	OP_NB(0xe)
	OP_CHILD_AUTODESC(c1,is_int_or_auto,"2")
	OP_CHILD(c2,is_list)
	OP_TYPE(enlist(c2->t))
OP_END

OP_DEF(",","range from 1")
	OP_NB(0xd)
	OP_CHILD_AUTODESC(c1,is_num_or_auto,"infinity")
	OP_TYPE(enlist(c1->t->is_auto()?t_int:c1->t))
OP_END

OP_DEF(",","length")
	OP_NB(0xd)
	OP_CHILD(c1,is_list)
	OP_TYPE(t_int)
OP_END

OP_DEF("``p","permutations")
	OP_NB(0xe)
	OP_NB(0x1)
	OP_NB(0x9)
	OP_CHILD(c1,is_list)
	OP_VERSION_GE_LT(21,100)
	OP_TYPE(enlist(c1->t))
OP_END

OP_DEF("hex","to hex")
	OP_NB(0xe)
	OP_NB(0xd)
	OP_CHILD(c1,is_num)
	OP_NB(0xd)
	OP_VERSION_LT(22)
	OP_TYPE(enlist(t_chr))
OP_END

OP_DEF("%~","split by")
	OP_NB(0xe)
	OP_CHILD(c1,is_list)
	OP_NB(0x0)
	OP_FN_ENV(c2e)
	OP_FN_ARG(c2e,c1->t->el)
	OP_FN_CHILD(c2,c2e);
	OP_TYPE(enlist(make_tuple(c1->t,c1->t)))
OP_END

OP_DEF("`\x28","uncons")
	OP_NB(0xe)
	OP_CHILD(c1,is_list)
	OP_NB(0x1)
	OP_LET(c1->t)
	OP_LET(c1->t->el->ts)
	OP_TYPE(c1->t->el->head())
OP_END

OP_DEF("`\x29","swapped uncons")
	OP_NB(0xe)
	OP_CHILD(c1,is_list)
	OP_NB(0x2)
	OP_LET(c1->t->el)
	OP_TYPE(c1->t)
OP_END

OP_DEF("`'","transpose")
	OP_NB(0xe)
	OP_CHILD(c1,is_list)
	OP_NB(0x3)
	if(c1->t->el->ts){
		OP_LET2(c1->t->el->ts,1);
		OP_TYPE(enlist(c1->t->el->head()));
	}else{
		OP_TYPE(c1->t->is_list_list()?c1->t:enlist(c1->t))
	}
OP_END

OP_DEF("`$","uniq")
	OP_NB(0xe)
	OP_CHILD(c1,is_list)
	OP_NB(0x4)
	OP_TYPE(c1->t)
OP_END

OP_DEF("`&","list intersection")
	OP_NB(0xe)
	OP_CHILD(c1,is_list)
	OP_NB(0x5)
	OP_OPTION(c2,"uniq")
	OP_FN_ENV(c3e)
	OP_FN_ARG(c3e,c1->t->el)
	OP_FN_CHILD(c3,c3e)
//	if(c3->args_used()){
//		OP_CHILD(c4,is_any)
//	}
//	OP_TYPE(c1->t)
	if(c3->args_used()){
		OP_CHILD(c4,is_any)
		OP_TYPE(coerce2(c1->t,c4->t))
	}else{
		OP_TYPE(coerce2(c1->t,c3->t))
	}
OP_END

OP_DEF("`|","list union")
	OP_NB(0xe)
	OP_CHILD(c1,is_list)
	OP_NB(0x6)
	OP_OPTION(c2,"uniq")
	OP_FN_ENV(c3e)
	OP_FN_ARG(c3e,c1->t->el)
	OP_FN_CHILD(c3,c3e)
//	if(c3->args_used()){
//		OP_CHILD(c4,is_any)
//	}
//	OP_TYPE(c1->t)
	if(c3->args_used()){
		OP_CHILD(c4,is_any)
		OP_TYPE(coerce2(c1->t,c4->t))
	}else{
		OP_TYPE(coerce2(c1->t,c3->t))
	}
OP_END

OP_DEF("`^","list xor")
	OP_NB(0xe)
	OP_CHILD(c1,is_list)
	OP_NB(0x7)
	OP_OPTION(c2,"uniq")
	OP_FN_ENV(c3e)
	OP_FN_ARG(c3e,c1->t->el)
	OP_FN_CHILD(c3,c3e)
//	if(c3->args_used()){
//		OP_CHILD(c4,is_any)
//	}
//	OP_TYPE(c1->t)
	if(c3->args_used()){
		OP_CHILD(c4,is_any)
		OP_TYPE(coerce2(c1->t,c4->t))
	}else{
		OP_TYPE(coerce2(c1->t,c3->t))
	}
OP_END

OP_DEF("`:","list of 2 lists")
	OP_NB(0xe)
	OP_CHILD(c1,is_list)
	OP_NB(0x8)
	OP_CHILD(c2,is_list)
	OP_CHECK(same(c1->t,c2->t))
	OP_TYPE(enlist(c1->t))
OP_END

OP_DEF("`-","list difference")
	OP_NB(0xe)
	OP_CHILD(c1,is_list)
	OP_NB(0x8)
	OP_OPTION(c2,"uniq")
	OP_FN_ENV(c3e)
	OP_FN_ARG(c3e,c1->t->el)
	OP_FN_CHILD(c3,c3e)
//	if(c3->args_used()){
//		OP_CHILD(c4,is_any)
//	}
//	OP_TYPE(c1->t)
	if(c3->args_used()){
		OP_CHILD(c4,is_any)
		OP_TYPE(coerce2(c1->t,c4->t))
	}else{
		OP_TYPE(coerce2(c1->t,c3->t))
	}
OP_END

OP_DEF("`<~","take drop while")
	OP_NB(0xe)
	OP_CHILD(c1,is_list)
	OP_NB(0x9)
	OP_OPTION(c2,"not")
	OP_FN_ENV(c3e)
	OP_FN_ARG(c3e,c1->t->el)
	OP_FN_CHILD(c3,c3e)
	OP_LET(c1->t)
	OP_TYPE(c1->t)
OP_END

OP_DEF("`?","find indices [by]")
	OP_NB(0xe)
	OP_CHILD(c1,is_list)
	OP_NB(0xa)
	OP_FN_ENV(c2e)
	OP_FN_ARG(c2e,c1->t->el)
	OP_FN_CHILD(c2,c2e)
	OP_TYPE(enlist(t_int))
OP_END

OP_DEF("`/","special folds")
	OP_NB(0xe)
	OP_CHILD(c1,is_list)
	OP_NB(0xb)
	OP_NEW_CHILD(c2)
	int b3=getnb(c2->ed++);
	OP_CHECK(!(b3<0))
	Special*sp=&(codever<22?foldops_v020:foldops_v022)[b3];
	c2->lit =sp->lit;
	c2->desc=sp->desc;
	OP_ADD_CHILD(c2)
	if(sp->lit=="<"||sp->lit==">"){
		OP_FN_ENV(c3e)
		OP_FN_ARG(c3e,c1->t->el)
		OP_FN_CHILD(c3,c3e)
		OP_LET(c1->t->el->ts)
		OP_TYPE(c1->t->el->head())
	}
	else{
		OP_TYPE(coerce_foldop(sp,c1->t->el))
	}
OP_END

OP_DEF("`\\","special scans")
	OP_NB(0xe)
	OP_CHILD(c1,is_list)
	OP_NB(0xc)
	OP_NEW_CHILD(c2)
	int b3=getnb(c2->ed++);
	OP_CHECK(!(b3<0))
	Special*sp=&(codever<22?foldops_v020:foldops_v022)[b3];
	c2->lit =sp->lit;
	c2->desc=sp->desc;
	OP_ADD_CHILD(c2)
	if(sp->lit=="<"||sp->lit==">"){
		OP_FN_ENV(c3e)
		OP_FN_ARG(c3e,c1->t->el)
		OP_FN_CHILD(c3,c3e)
		OP_TYPE(c1->t)
	}
	else{
		OP_TYPE(enlist(coerce_foldop(sp,c1->t->el)))
	}
OP_END

OP_DEF("`<","sort")
	OP_NB(0xe)
	OP_CHILD(c1,is_list)
	OP_NB(0xd)
	OP_TYPE(c1->t)
OP_END

OP_DEF(">~","drop while")
	OP_NB(0xe)
	OP_CHILD(c1,is_list)
	OP_NB(0xe)
	OP_OPTION(c2,"not")
	OP_FN_ENV(c3e)
	OP_FN_ARG(c3e,c1->t->el)
	OP_FN_CHILD(c3,c3e)
	OP_TYPE(c1->t)
OP_END

OP_DEF("<~","take while")
	OP_NB(0xe)
	OP_CHILD(c1,is_list)
	OP_NB(0xf)
	OP_OPTION(c2,"not")
	OP_FN_ENV(c3e)
	OP_FN_ARG(c3e,c1->t->el)
	OP_FN_CHILD(c3,c3e)
	OP_TYPE(c1->t)
OP_END

OP_DEF("o","ord")
	OP_NB(0xe)
	OP_CHILD(c1,is_chr)
	OP_TYPE(t_int)
OP_END

OP_DEF("`<","take also drop")
	OP_NB(0xe)
	OP_CHILD(c1,is_int)
	OP_NB(0xb)
	OP_CHILD(c2,is_list)
	OP_VERSION_LT(100)
	OP_LET(c2->t)
	OP_TYPE(c2->t)
OP_END

OP_DEF("^","pow")
	OP_NB(0xe)
	OP_CHILD_AUTODESC(c1,is_int_or_auto,"10")
	OP_CHILD_AUTODESC(c2,is_int_or_auto,"2")
	OP_TYPE(t_int)
OP_END

OP_DEF("^","replicate")
	OP_NB(0xe)
	OP_CHILD_AUTODESC(c1,is_int_or_auto,"infinity")
	OP_CHILD(c2,is_list_or_chr)
	OP_TYPE(c2->t->is_chr()?enlist(c2->t):c2->t)
OP_END

OP_DEF("`#","hashmod")
	OP_NB(0xf)
	OP_NB(0x0)
	OP_OPTION(c1,"nosalt")
	OP_CHILD(c2,is_any)
	OP_NEW_CHILD(c3)
	c3->desc="hashmod arg";
	long v;
	get_int_body(c3->ed,v);
	stringstream ss;
	ss<<v;
	c3->lit=ss.str();
	OP_ADD_CHILD(c3)
	OP_TYPE(t_int)
	post.exists=true;
	post.desc="hashmod";
OP_END

OP_DEF("`%","split list (keep empties)")
	OP_NB(0xf)
	OP_NB(0x1)
	OP_CHILD(c1,is_list)
	if(codever>=24){
		OP_CHILD_AUTODESC(c2,is_any,"default")
	}
	else{
		OP_CHILD(c2,is_any_but_auto)
	}
	OP_TYPE(enlist(c1->t))
OP_END

OP_DEF("`p","int to str")
	OP_NB(0xf)
	OP_NB(0x1)
	OP_CHILD(c1,is_num)
	OP_TYPE(enlist(t_chr))
OP_END

OP_DEF("error","error")
	OP_NB(0xf)
	OP_NB(0xd)
	OP_NB(0x0)
	OP_NB(0x2)
	OP_VERSION_GE(23)
	OP_CHILD(c1,is_any)
	if(codever<24){
		OP_TYPE(enlist(t_chr))
	}
	else{
		OP_TYPE(c1->t)
	}
OP_END

OP_DEF("r","read str at base")
	OP_QUIET_VERSION_LT(22)
	OP_NB(0xf)
	OP_CHILD(c1,is_str)
	OP_CHILD(c2,is_int)
	OP_TYPE(t_int)
OP_END

OP_DEF("`r","read int")
	OP_QUIET_VERSION_GE(22)
	OP_NB(0xf)
	OP_CHILD(c1,is_str)
	OP_NB(0x1)
	OP_VERSION_GE(22)
	OP_TYPE(t_int)
OP_END

OP_DEF("?","index")
	OP_NB(0xf)
	OP_CHILD(c1,is_list)
	OP_OPTION(c2,"by")
	if(c2){
		OP_OPTION(c3,"not")
		OP_FN_ENV(c4e)
		OP_FN_ARG(c4e,c1->t->el)
		OP_FN_CHILD(c4,c4e)
	}else{
		OP_CHILD(c3,is_any)
		OP_CHECK(same(c1->t->el,c3->t))
	}
	OP_TYPE(t_int)
OP_END

OP_DEF("-","diff")
	OP_NB(0xf)
	OP_CHILD(c1,is_list)
	OP_CHILD(c2,is_list)
	OP_CHECK(same(c1->t,c2->t))
	OP_TYPE(c1->t)
OP_END

//OP_DEF("?,","if nonnull")
OP_DEF("?,","if/else (lazy list)")
	OP_NB(0xf)
	OP_NB(0xd)
	OP_CHILD(c1,is_list)
	OP_FN_ENV(c2e)
	OP_FN_ARG_OPT(c2e,c1->t)
	OP_FN_CHILD(c2,c2e)
	if(getnb(n->ed)==0){
		OP_OPTION(c3,"default");
		OP_LET(c2->t->ts)
		OP_TYPE(c2->t->head())
	}
	else{
		int ary=c2->t->ary();
		OP_CHILD_TUPLE(c3,ary)
		Type const*tz=coerce2(c2->t,c3->t);
		OP_LET(tz->ts)
		OP_TYPE(tz->head())
	}
OP_END

OP_DEF("?","if/else")
	OP_NB(0xf)
	OP_CHILD(c1,is_num)
	OP_FN_ENV(c2e)
	OP_FN_ARG_OPT(c2e,c1->t)
	OP_FN_CHILD(c2,c2e)
	if(getnb(n->ed)==0){
		OP_OPTION(c3,"default");
		OP_LET(c2->t->ts)
		OP_TYPE(c2->t->head())
	}
	else{
		int ary=c2->t->ary();
		OP_CHILD_TUPLE(c3,ary)
		Type const*tz=coerce2(c2->t,c3->t);
		OP_LET(tz->ts)
		OP_TYPE(tz->head())
	}
OP_END

OP_DEF("","sorry, i don't understand this code. please report.")
	++n->ed;
	OP_TYPE(new Type)
OP_END

	// unreachable

}

void print_type(Type const * t){
	if(!t){
		printf("null");
	}
	else {
		if(t->p==p_lambda){
			printf("fn(");
			print_type(t->fa);
			printf("->");
			print_type(t->el);
			printf(")");
		}
		else if(t->p==p_list){
			printf("[");
			print_type(t->el);
			printf("]");
		}
		else if(t->p==p_auto){
			printf("auto");
		}
		else if(t->p==p_int){
			printf("int");
		}
		else if(t->p==p_chr){
			printf("chr");
		}
		else{
			printf("???");
		}
		if(t->ts){
			printf(",");
			print_type(t->ts);
		}
	}
}

bool show_type;
bool show_pson;
bool show_json;

void show1(Node*n,int level=0){
//printf("%4d%4d%4d%4ld|",n->ct_arg,n->ct_sub,n->ct_let,n->context.size());
	printf("%*s",level*4,"");
	if(!n->lit.empty()){
		printf("%s ",n->lit.c_str());
	}
//	printf("#(%s) ",n->desc.c_str());
	printf("(%s) ",n->desc.c_str());
	if(show_type){
//		printf("::");
		printf(":");
		print_type(n->t);
		printf(" ");
	}
	if(n->ct_arg<n->ct_sub){
		printf("arg(");
		for(int i=n->ct_sub;--i>=n->ct_arg;){
			Arg const*a=n->context[i];
			printf(" %s",a->desc.c_str());
			if(show_type){
				printf("::");
				print_type(a->t);
			}
		}
		printf(" ) ");
	}
	if(n->ct_let<n->context.size()){
		printf("let(");
		for(int i=n->context.size();--i>=n->ct_let;){
			Arg const*a=n->context[i];
			printf(" %s",a->desc.c_str());
			if(show_type){
				printf("::");
				print_type(a->t);
			}
		}
		printf(" ) ");
	}
	puts("");
	for(Node* c:n->childs){
		show1(c,level+1);
	}
}

// pson

string escape_pson_string(string v){
	stringstream ss;
	for(char c:v){
		if(c=='\\'||c=='\"'||c=='\''||c=='$'||c=='@'||c=='`'){
			ss<<"\\"<<c;
		}
		else if(c=='\n'){
			ss<<"\\n";
		}
		else if(c<0x20||c>126){
			ss<<"\\x"<<hex<<setw(2)<<setfill('0')<<int(c);
		}
		else{
			ss<<c;
		}
	}
	return ss.str();
}

void print_pson_subtree(Node*n){
	printf("{");
	if(!n->lit.empty()){
		printf("lit=>\"%s\",",escape_pson_string(n->lit).c_str());
	}
	printf("desc=>\"%s\",",escape_pson_string(n->desc).c_str());
	printf("type=>\"");
	print_type(n->t);
	printf("\",");
	if(n->ct_arg<n->ct_sub){
		printf("args=>[");
		for(int i=n->ct_sub;--i>=n->ct_arg;){
			Arg const*a=n->context[i];
			printf("{desc=>\"%s\",type=>\"",escape_pson_string(a->desc).c_str());
			print_type(a->t);
			printf("\"},");
		}
		printf("],");
	}
	if(n->ct_let<n->context.size()){
		printf("lets=>[");
		for(int i=n->context.size();--i>=n->ct_let;){
			Arg const*a=n->context[i];
			printf("{desc=>\"%s\",type=>\"",escape_pson_string(a->desc).c_str());
			print_type(a->t);
			printf("\"},");
		}
		printf("],");
	}
	if(!n->childs.empty()){
		printf("childs=>[");
		for(Node* c:n->childs){
			print_pson_subtree(c);
		}
		printf("],");
	}
	if(n->passed){
		printf("passed=>1,");
	}
	printf("},");
}

void print_pson(Node*n,char const*note){
	printf("{nibbles_version=>\"0.%d\",commenter_version=>\"0.1.3.%d\",code=>",codever,buildlevel);
	print_pson_subtree(n);
	if(post.exists){
		printf("postdata=>{");
		if(!post.lit.empty()){
			printf("lit=>\"%s\",",post.lit.c_str());
		}
		printf("desc=>\"%s\",",post.desc.c_str());
		printf("data=>\"%s\",",post.data.c_str());
		printf("format=>\"%s\",",post.is_dec?"dec":"hex");
		if(-post.base>=2){
			printf("str=>\"%s\",",escape_pson_string(post.str).c_str());
		}
		printf("},");
	}
	if(*note){
		printf("note=>\"%s\",",escape_pson_string(note).c_str());
	}
	printf("}");
}

// JSON

string escape_json_string(string v){
	stringstream ss;
	for(char c:v){
		if(c=='\"'||c=='\\'){
			ss<<"\\"<<c;
		}
		else if(c=='\n'){
			ss<<"\\n";
		}
		else if(c<0x20||c>126){
			ss<<"\\u"<<hex<<setw(4)<<setfill('0')<<int(c);
		}
		else{
			ss<<c;
		}
	}
	return ss.str();
}

#define JSON_SEP(_sepvar) ({ printf("%s",_sepvar); _sepvar=","; })

void print_json_subtree(Node*n){
	printf("{");
	char const * sep1="";
	if(!n->lit.empty()){
		JSON_SEP(sep1);
		printf("\"lit\":\"%s\"",escape_json_string(n->lit).c_str());
	}
	JSON_SEP(sep1);
	printf("\"desc\":\"%s\"",escape_json_string(n->desc).c_str());
	JSON_SEP(sep1);
	printf("\"type\":\"");
	print_type(n->t);
	printf("\"");
	if(n->ct_arg<n->ct_sub){
		JSON_SEP(sep1);
		printf("\"args\":[");
		const char * sep2="";
		for(int i=n->ct_sub;--i>=n->ct_arg;){
			Arg const*a=n->context[i];
			JSON_SEP(sep2);
			printf("{\"desc\":\"%s\",\"type\":\"",escape_json_string(a->desc).c_str());
			print_type(a->t);
			printf("\"}");
		}
		printf("]");
	}
	if(n->ct_let<n->context.size()){
		JSON_SEP(sep1);
		printf("\"lets\":[");
		const char * sep2="";
		for(int i=n->context.size();--i>=n->ct_let;){
			Arg const*a=n->context[i];
			JSON_SEP(sep2);
			printf("{\"desc\":\"%s\",\"type\":\"",escape_json_string(a->desc).c_str());
			print_type(a->t);
			printf("\"}");
		}
		printf("]");
	}
	if(!n->childs.empty()){
		JSON_SEP(sep1);
		printf("\"childs\":[");
		const char * sep2="";
		for(Node* c:n->childs){
			JSON_SEP(sep2);
			print_json_subtree(c);
		}
		printf("]");
	}
	if(n->passed){
		JSON_SEP(sep1);
		printf("\"passed\":1");
	}
	printf("}");
}

void print_json(Node*n,char const*note){
	printf("{\"nibbles_version\":\"%d.%02d\",\"commenter_version\":\"0.1.3.%d\",\"code\":",codever/100,codever%100,buildlevel);
	print_json_subtree(n);
	if(post.exists){
		printf(",\"postdata\":{");
		if(!post.lit.empty()){
			printf("\"lit\":\"%s\",",post.lit.c_str());
		}
		printf("\"desc\":\"%s\"",post.desc.c_str());
		printf(",\"data\":\"%s\"",post.data.c_str());
		printf(",\"format\":\"%s\"",post.is_dec?"dec":"hex");
		if(-post.base>=2){
			printf(",\"str\":\"%s\"",escape_json_string(post.str).c_str());
		}
		printf("}");
	}
	if(*note){
		printf(",\"note\":\"%s\"",escape_json_string(note).c_str());
	}
	printf("}");
}

// stdin:
// NBB code

// argv[1]
// "0" : simple
// "1" : show type
// "2" : pson
// "JSON" : JSON

// argv[2] : Nibbles version

int main(int argc,char**argv){
	if(argc>1&&argv[1][0]=='1'){
		show_type=true;
	}
	if(argc>1&&argv[1][0]=='2'){
		show_pson=true;
	}
	if(argc>1&&argv[1][0]=='J'){
		show_json=true;
	}
	codever=25;
	if(argc>2){
		sscanf(argv[2],"%d",&codever);
	}
	assert(codever==100||codever==25||codever==24||codever==23||codever==22||codever==21||codever==20);
	codever_lt=1000;
	codever_ge=0;
	nbread();
	
	vector<Arg const*> args;
	if(codever>=25){
		/* 0 ;;@ */ mkarg(args,-1,"sndLine"   ,t_chr,1,true);
		/* 1 ;;$ */ mkarg(args,-1,"intMatrix" ,t_int,2,true);
		/* 2  ;_ */ mkarg(args,-1,"allInput"  ,t_chr,1,true);
		/* 3  ;@ */ mkarg(args,-1,"allLines"  ,t_chr,2,true);
		/* 4  ;$ */ mkarg(args,-1,"sndInt"    ,t_int,0,true);  // or 1000
		/* 5   _ */ mkarg(args,-1,"ints"      ,t_int,1,true);
		/* 6   @ */ mkarg(args,-1,"fstLine"   ,t_chr,1,true);  // or printables
		/* 7   $ */ mkarg(args,-1,"fstInt"    ,t_int,0,true);  // or 100
	}
	else{
		/* 0 ;;@ */ mkarg(args,-1,"allLines"  ,t_chr,2,true);
		/* 1 ;;$ */ mkarg(args,-1,"intMatrix" ,t_int,2,true);
		/* 2  ;_ */ mkarg(args,-1,"allInput"  ,t_chr,1,true);
		/* 3  ;@ */ mkarg(args,-1,"sndLine"   ,t_chr,1,true);
		/* 4  ;$ */ mkarg(args,-1,"sndInt"    ,t_int,0,true);  // or 1000
		/* 5   _ */ mkarg(args,-1,"ints"      ,t_int,1,true);
		/* 6   @ */ mkarg(args,-1,"fstLine"   ,t_chr,1,true);  // or printables
		/* 7   $ */ mkarg(args,-1,"fstInt"    ,t_int,0,true);  // or 100
	}

	int varid=0;
	
	Node*n=parse1(0,op0_auto,nullptr,args,varid,0);
	n->ct_arg=n->ct_sub=args.size();
	while(!post.exists){
		Node*p=new Node;
		p->st=n->st;
		p->ed=n->ed;
		p->context=n->context;
		p->ct_arg=n->ct_arg;
		p->ct_sub=n->ct_sub;

		if(n->t->is_int()){
			FnEnv *e=new FnEnv;
			e->context=p->context;
			int ct0=e->context.size();
			unzip_tuple(e,varid,0,t_int,0,true);
			int ct1=e->context.size();
			unzip_tuple(e,varid,0,t_int,0,true);
			int ct2=e->context.size();
			Node*c=parse1(p->ed,p->context[p->ct_arg-1]->used?op0_auto:op0_tuple,nullptr,e->context,varid,0);
			if(!c) break;
			if(c->t->is_auto()){
				post.offs=1;
				post.exists=true;
				post.desc="fstInt";
				post.lit="~";
				break;
			}
			p->childs.push_back(n);
			p->childs.push_back(c);
			p->ed=c->ed;
			p->context=c->context;
			purge_args(p->context,0,ct0);
			p->ct_let=c->context.size();
			c->ct_arg=ct0;
			c->ct_sub=ct2;
			bool arg2used=false;
			for(int i=ct0;i<ct1;++i){
				arg2used|=c->context[i]->used;
			}
			bool arg1used=false;
			for(int i=ct1;i<ct2;++i){
				arg1used|=c->context[i]->used;
			}
			if(arg2used){
				p->desc="implicit foldl1 with implicit range";
				//p->args.push_back(c->args_st[a2[0]]);
				//p->args.push_back(c->args_st[a1[0]]);
				p->t=t_int; //p->t=c->t;
			}
			else if(arg1used){
				p->desc="implicit map with implicit range";
				//p->args.push_back(c->args_st[a1[0]]);
				p->t=enlist(c->t);
			}
			else{
				p->desc="implicit string concatenation";
				p->t=enlist(t_chr);
			}
		}
		else if(n->t->is_list()&&!n->t->is_str()){
			FnEnv *e=new FnEnv;
			e->context=p->context;
			int ct0=e->context.size();
			unzip_tuple(e,varid,0,n->t->el,0,true);
			int ct1=e->context.size();
			unzip_tuple(e,varid,0,n->t->el,0,true);
			int ct2=e->context.size();
			Node*c=parse1(p->ed,p->context[p->ct_arg-1]->used?op0_auto:op0_tuple,nullptr,e->context,varid,0);
			if(!c) break;
			if(c->t->is_auto()){
				post.offs=1;
				post.exists=true;
				post.desc="fstInt";
				post.lit="~";
				break;
			}
			p->childs.push_back(n);
			p->childs.push_back(c);
			p->ed=c->ed;
			p->context=c->context;
			purge_args(p->context,0,ct0);
			p->ct_let=c->context.size();
			c->ct_arg=ct0;
			c->ct_sub=ct2;
			bool arg2used=false;
			for(int i=ct0;i<ct1;++i){
				arg2used|=c->context[i]->used;
			}
			bool arg1used=false;
			for(int i=ct1;i<ct2;++i){
				arg1used|=c->context[i]->used;
			}
			if(arg2used){
				p->desc="implicit foldl1";
				//p->args.push_back(c->args_st[a2[0]]);
				//p->args.push_back(c->args_st[a1[0]]);
				//ny unzip
				p->t=n->t->el; //p->t=c->t;
			}
			else if(arg1used){
				p->desc="implicit map";
				//p->args.push_back(c->args_st[a1[0]]);
				p->t=enlist(c->t);
			}
			else{
				p->desc="implicit string concatenation";
				p->t=enlist(t_chr);
			}
		}
		else{
			Node*c=parse1(p->ed,p->context[p->ct_arg-1]->used?op0_auto:op0_tuple,nullptr,p->context,varid,0);
			if(!c) break;
			if(c->t->is_auto()){
				post.offs=1;
				post.exists=true;
				post.desc="fstInt";
				post.lit="~";
				break;
			}
			p->childs.push_back(n);
			p->childs.push_back(c);
			p->ed=c->ed;
			p->ct_let=c->context.size();
			c->ct_arg=c->context.size();
			c->ct_sub=c->context.size();
			p->context=c->context;
			p->desc="implicit string concatenation";
			p->t=enlist(t_chr);
		}
		n=p;
	}
	if(post.exists){
		int st=n->ed+post.offs;
		int i=nbsize;
		if(i>st&&nbbuf[i-1]==6){
			--i;
		}
		stringstream ss;
		while(i>st){
			int b=nbbuf[--i];
			if(b==6) b=0;
			else if(b==0) b=6;
			ss<<char(b<10?'0'+b:'a'-10+b);
		}
		string hexstr=ss.str();
		if(codever>=22){
			post.data=hexstr;
		}
		else{
			int fds[2];
			pipe(fds);
			if(!fork()){
				dup2(fds[1],1);
				close(fds[1]);
				system(("echo "+hexstr+"|tr a-z A-Z|DC_LINE_LENGTH= dc -e'16i?n'").c_str());
				exit(0);
			}
			close(fds[1]);
			wait(nullptr);
			stringstream ss2;
			char buf[257];
			int n;
			while(n=read(fds[0],buf,256),n>0){
				buf[n]=0;
				ss2<<buf;
			}
			post.data=ss2.str();
			post.is_dec=true;
		}
		if(post.data.empty()){
			post.data="0";
		}
		if(-post.base>=2&&-post.base<=strlen(useful)){
			int base=-post.base;
			vector<int> vec;
			for(char c:hexstr){
				vec.push_back(c-(c<'a'?'0':'a'-10));
			}
			string s;
			while(!vec.empty()){
				int mod=0;
				for(int& i:vec){
					int j=mod*16+i;
					i=j/base;
					mod=j%base;
				}
				s=useful[mod]+s;
				while(!vec.empty()&&vec[0]==0){
					vec.erase(vec.begin());
				}
				post.str="\""+show_string(s)+"\"";
			}
		}
	}
	{	
		char note[1024]="";
		if(codever_lt!=1000 && codever_ge!=0){
			sprintf(note,"This code seems to be written in Nibbles version >= %d.%02d and < %d.%02d\n",codever_ge/100,codever_ge%100,codever_lt/100,codever_lt%100);
		}
		else{
			if(codever_ge!=0){
				sprintf(note,"This code seems to be written in Nibbles version >= %d.%02d\n",codever_ge/100,codever_ge%100);
			}
			if(codever_lt!=1000){
				sprintf(note,"This code seems to be written in Nibbles version < %d.%02d\n",codever_lt/100,codever_lt%100);
			}
		}
		if(show_pson){
			print_pson(n,note);
		}
		else
		if(show_json){
			print_json(n,note);
		}
		else{
			printf("%s",note);
			show1(n);
			if(post.exists){
				puts((string("(data for: ")+post.desc+")").c_str());
			}
		}
	}

	return 0;
}
