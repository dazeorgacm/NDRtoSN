// NDRtoSN.c      
//    
// Converts .ndr file into .lsn or .hsn file v1.2
// Stores tables of names for places and transitions as comments
// Processes inhibitor and priority arcs 
// Processes transition substitution labels
//
// Usage: NDRtoSN file1.ndr file2.lsn
//        NDRtoSN file1.ndr file2.hsn
//
// Compile: gcc -o NDRtoSN NDRtoSN.c al2.c
//
// Uses abstract lists al2.h and al2.c from https://github.com/dazeorgacm/ts
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>

#include "al2.h"

#define __MAIN__

//#define MAXINPSTRLEN 16384
//#define MAXFILENAME 256
#define MAXSTRLEN 1025
#define FILENAMELEN 256

#define nINIT 1024
#define mINIT 1024
#define lINIT 1024
#define atpINIT 2048
#define aptINIT 2048
#define attINIT 1024
#define namesINIT 16384

#define nDELTA 1024
#define mDELTA 1024
#define lDELTA 1024
#define atpDELTA 2048
#define aptDELTA 2048
#define attDELTA 512
#define namesDELTA 8192

#define NDR 1
#define NET 2

static char str[ MAXSTRLEN + 1 ]; /* line buffer */
 
static int n, m, l, maxn, maxm, maxl; /* net size: trs, pls, labels */
static int *tn, fat; /* trs */
static int *pn, fap; /* pls */
static int *mu;      /* marking */

static int *tl, *tltn, fatl; /* trs labels */
 
static char *names; /* all the names */
static int fnames, maxnames;
static int netname=-1;
 
static int *atpp, *atpt, *atpw; /* arcs t->p */
static int *aptp, *aptt, *aptw; /* arcs p->t */
static int *att1, *att2;        /* arcs t->t */
static int fatp, fapt, maxatp, maxapt;
static int fatt, maxatt;

static char HSN_prefix[]="{*HSN(";
static int HSN_prefix_length=6;

void SwallowSpace( char * str, int *i )
{
  
  while( ( str[(*i)]==' ' || str[(*i)]==0xa || str[(*i)]==0xd || str[(*i)]==0x9 ) && str[(*i)]!='\0'  ) (*i)++;

} /* SwallowSpace */

int IsSpace( char * str, int i )
{
   
  if( str[i]==' ' || str[i]==0xa || str[i]==0xd || str[i]==0x9 || str[i]=='\0' )
    return( 1 );
  else
    return( 0 );
  
} /* IsSpace */
	
void GetName( int *i, int *j )
{
 int state;

 if( str[(*i)]=='{' )
 {
   state=1;
   while( str[(*i)]!='\0' && state && str[(*i)]!=0xa && str[(*i)]!=0xd ) 
   {
    names[ (*j)++ ]=str[ (*i)++ ]; 
    if( str[(*i)-1]=='}' && state==1 ) state=0; else
      if( str[(*i)-1]=='\\' && state==2 ) state=1; else
        if( str[(*i)-1]=='\\' && state==1 ) state=2; else
	  if( str[(*i)-1]=='}' && state==2 ) state=1; else 
	    if( state==2 ) state=1;
   }
   names[ (*j)++ ]='\0';
 }
 else
 {
   while( str[(*i)]!=' ' && str[(*i)]!='\0' && str[(*i)]!=0xa && str[(*i)]!=0xd && str[(*i)]!=0x9 && str[(*i)]!='*' && str[(*i)]!='?')
    names[ (*j)++ ]=str[ (*i)++ ];
   names[ (*j)++ ]='\0';
 }

} /* GetName */


void ExpandNames()
{
  char * newnames;

  if( fnames+MAXSTRLEN > maxnames )
  { 
    maxnames+=namesDELTA;
    newnames = (char*) realloc( names, maxnames );
    if( newnames==NULL ) { printf( "*** not enough memory (ExpandNames)\n" ); exit(3); }
    else names=newnames;
  }

} /* ExpandNames */

void ExpandP()
{
  int *newpn, *newmu;

  if( m >= maxm - 2 ) 
  {
    maxm+=mDELTA;
    newpn = (int*) realloc( pn, maxm * sizeof(int) );
    newmu = (int*) realloc( mu, maxm * sizeof(int) );
    if( newpn==NULL || newmu==NULL ) 
      { printf( "*** not enough memory (ExpandP)\n" ); exit(3); }
    else 
      { pn=newpn; mu=newmu; }
  }

} /* ExpandP */

void ExpandT()
{
  int *newtn;

  if( n >= maxn - 2 ) 
  {
    maxn+=nDELTA;
    newtn = (int*) realloc( tn, maxn * sizeof(int) );
    if( newtn==NULL ) 
      { printf( "*** not enough memory (ExpandT)\n" ); exit(3); }
    else 
      { tn=newtn; }
  }
  
} /* ExpandT */

void ExpandTL()
{
  int *newtl, *newtltn;

  if( l >= maxl - 2 ) 
  {
    maxl+=lDELTA;
    newtl = (int*) realloc( tl, maxl * sizeof(int) );
    newtltn = (int*) realloc( tltn, maxl * sizeof(int) );
    if( newtl==NULL || newtltn==NULL ) 
      { printf( "*** not enough memory (ExpandTL)\n" ); exit(3); }
    else 
      { tl=newtl; tltn=newtltn; }
  }
  
} /* ExpandTL */

void ExpandAtp()
{
  int *newatpp, *newatpt, *newatpw;

  if( fatp>=maxatp ) 
  {
    maxatp+=atpDELTA;
    newatpp = (int*) realloc( atpp, maxatp * sizeof(int) );
    newatpt = (int*) realloc( atpt, maxatp * sizeof(int) );
    newatpw = (int*) realloc( atpw, maxatp * sizeof(int) );
    if( newatpp==NULL || newatpt==NULL || newatpw==NULL )
      { printf( "*** not enough memory (ExpandAtp)\n" ); exit(3); }
    else 
      { atpp=newatpp; atpt=newatpt; atpw=newatpw; }
  }

} /* ExpandAtp */

void ExpandApt()
{
  int *newaptp, *newaptt, *newaptw;

  if( fapt>=maxapt ) 
  {
    maxapt+=aptDELTA;
    newaptp = (int*) realloc( aptp, maxapt * sizeof(int) );
    newaptt = (int*) realloc( aptt, maxapt * sizeof(int) );
    newaptw = (int*) realloc( aptw, maxapt * sizeof(int) );
    if( newaptp==NULL || newaptt==NULL || newaptw==NULL )
      { printf( "*** not enough memory (ExpandApt)\n" ); exit(3); }
    else 
      { aptp=newaptp; aptt=newaptt; aptw=newaptw; }
  }

} /* ExpandApt */

void ExpandAtt()
{
  int *newatt1, *newatt2;

  if( fatt>=maxatt ) 
  {
    maxatt+=attDELTA;
    newatt1 = (int*) realloc( att1, maxatt * sizeof(int) );
    newatt2 = (int*) realloc( att2, maxatt * sizeof(int) );
    if( newatt1==NULL || newatt2==NULL )
      { printf( "*** not enough memory (ExpandAtt)\n" ); exit(3); }
    else 
      { att1=newatt1; att2=newatt2; }
  }

} /* ExpandAtt */

void ReadNDR( FILE * f )
{
 int i, found, p, t, inames, len, w, mup, tt, ii;
 char *name1, *name2;

 m=0; n=0; l=0;
 while( ! feof( f ) )
 {
   ExpandNames();
      
   fgets( str, MAXSTRLEN, f ); 
   if( feof(f) ) break;
   if( str[0]=='#' ) continue; /* comment line */
   
   len=strlen(str); i=0;
   SwallowSpace( str, &i );
   if( i==len ) continue; /*empty line */
   
   switch( str[i++] )
   {
     case 'p':
     	SwallowSpace( str, &i );
	while( ! IsSpace( str,i) && i<len )i++; /* x */
	SwallowSpace( str, &i );
	while( ! IsSpace( str,i) && i<len )i++; /* y */
	SwallowSpace( str, &i );
	ExpandP();
	pn[ ++m ] = fnames;
	mu[ m ] = 0;
	GetName( &i, &fnames );
	found=0;
	for( p=1; p<m; p++ )
	  if( strcmp( names+pn[p], names+pn[m] )==0 ) { found=1; break; }
	if( ! found )
	{  
	  p=m;
	  for( t=1; t<=n; t++ )
	      if( strcmp( names+tn[t], names+pn[m] )==0 ) { found=1; break; }
	}
	if( found ) { printf( "*** duplicate name: %s\n", names+pn[m] ); exit(2); }
	
	/* marking */
	SwallowSpace( str, &i );
        mup=atoi( str+i );
        mu[ p ]=mup;	  
	break;
	
     case 't':
       	SwallowSpace( str, &i );
	while( ! IsSpace( str,i) && i<len )i++; /* xpos */
	SwallowSpace( str, &i );
	while( ! IsSpace( str,i) && i<len )i++; /* ypos */
	SwallowSpace( str, &i );
	ExpandT();
	tn[ ++n ] = fnames;
	GetName( &i, &fnames );
	found=0;
	for( p=1; p<=m; p++ )
	  if( strcmp( names+pn[p], names+tn[n] )==0 ) { found=1; break; }
	if( ! found )
	{  
	  for( t=1; t<n; t++ )
	      if( strcmp( names+tn[t], names+tn[n] )==0 ) { found=1; break; }
	}
	if( found ) { printf( "*** duplicate name: %s\n", names+tn[n] ); exit(2); }
	// tuta1
	SwallowSpace( str, &i );
	while( ! IsSpace( str,i) && i<len )i++; /* anchor */
	SwallowSpace( str, &i );
	while( ! IsSpace( str,i) && i<len )i++; /* eft */
	SwallowSpace( str, &i );
	while( ! IsSpace( str,i) && i<len )i++; /* lft */
	SwallowSpace( str, &i );
	while( ! IsSpace( str,i) && i<len )i++; /* anchor */
	SwallowSpace( str, &i );
	ii=fnames;
	GetName( &i, &fnames );
	if(memcmp(HSN_prefix,names+ii,HSN_prefix_length)==0)
	{
//printf("%d %s\n",n,names+ii);
	   ExpandTL();
           tl[ ++l ]=ii+HSN_prefix_length; tltn[ l ]=n;
           names[fnames-3]='\0';
           fnames-=2;         
	}
	break;
      
     case 'e':
	SwallowSpace( str, &i );
	inames=fnames;
	name1=names+inames;
	GetName( &i, &inames );
	SwallowSpace( str, &i );
	if( isdigit(str[i])) while( ! IsSpace( str,i) && i<len )i++; /* rad */
	SwallowSpace( str, &i );
	if( isdigit(str[i])) while( ! IsSpace( str,i) && i<len )i++; /* ang */
	SwallowSpace( str, &i );
	name2=names+inames;
	GetName( &i, &inames );
	/* start from end */
	i=strlen( str )-1;
	while( IsSpace( str,i) && i>0 )i--; 
	while( ! IsSpace( str,i) && i>0 )i--; /* anchor */
	while( IsSpace( str,i) && i>0 )i--;
	while( ! IsSpace( str,i) && i>0 )i--; /* weight */
	w=atoi( str+i+1 ); /* multiplicity */
		
	/* recognize arc */
	
	if( fapt>=maxapt || fatp>=maxatp ) { ExpandAtp(); ExpandApt(); }
	
	found=0;
	for( p=1; p<=m; p++ )
	  if( strcmp( names+pn[p], name1 )==0 ) { ExpandApt(); aptp[fapt]=p; found=1; break; }
	if( found )
	{
	  found=0;
	  for( t=1; t<=n; t++ )
	    if( strcmp( names+tn[t], name2 )==0 ) { aptw[fapt]=w; aptt[fapt++]=t; found=1; break; }
	  if( ! found ) { printf( "*** unknown arc: %s -> %s\n", name1, name2 ); exit(2); }
	}
	else
	{	
	  found=0;
	  for( t=1; t<=n; t++ )
	    if( strcmp( names+tn[t], name1 )==0 ) { found=1; break; }
	  if( ! found ) { printf( "*** unknown arc: %s -> %s\n", name1, name2 ); exit(2); }
	  found=0;
	  for( p=1; p<=m; p++ )
	    if( strcmp( names+pn[p], name2 )==0 ) { ExpandAtp(); atpt[fatp]=t;
	                                            atpw[fatp]=w; atpp[fatp++]=p; found=1; break; }
	  if( ! found )
	  {
	    found=0;
	    for( tt=1; tt<=n; tt++ ) // tuta
	      if( strcmp( names+tn[tt], name2 )==0 ) { // printf("tt: %d->%d\n",t,tt);
	                                               ExpandAtt(); att1[fatt]=t;
	                                               att2[fatt++]=tt; found=1; break; }
	    if( ! found ) { printf( "*** unknown arc: %s -> %s\n", name1, name2 ); exit(2); }
	  }
	}
	break;
     
     case 'h':
       SwallowSpace( str, &i );
       netname = fnames;
       GetName( &i, &fnames );
       break;
	
   } /* switch */    
 } /* while */
}/* ReadNDR */

void WriteNMP( FILE * f )
{
  int p; 
  
  for( p=1; p<=m; p++ )
  {
    fprintf( f, "; %d %s\n", p, names + pn[p]  );
  }

}/* WriteNMP */

void WriteNMT( FILE * f )
{
  int t; 
  
  for( t=1; t<=n; t++ )
  {
    fprintf( f, "; %d %s\n", t, names + tn[t] );
  }

}/* WriteNMT */

struct pl_sub {
  char * cptype;
  char * cphnam;
  char * cplnum;
};

void ProcessHSNlabels( FILE * f )
{
  int i,j,t,isubn,cptype,cphname,cplnum,pst,hp,lp,v1,v2,p,found;
  struct l2 * qq=NULL;
  struct l2 * el2;
  struct pl_sub * e;
  
  for(j=1;j<=l;j++)
  {
    // tuta3
    t=tltn[ j ];
    strcpy(str,names+tl[ j ]);
//fprintf( f, "%s\n", str );
    i=0;
    SwallowSpace(str,&i);
    isubn=fnames;
    GetName(&i,&fnames);   
//printf("substitute transition %d (%s) by subnet %s\n", t, names+tl[ j ], names+isubn );
    pst=0;
    // reset queue
    while(str[i]!='\0')
    {
      SwallowSpace(str,&i);
      cptype=fnames;
      GetName(&i,&fnames);
      SwallowSpace(str,&i);
      cphname=fnames;
      GetName(&i,&fnames);
      SwallowSpace(str,&i);
      cplnum=fnames;
      GetName(&i,&fnames);
//printf("place type %s name %s merged with %s\n",names+cptype,names+cphname,names+cplnum);
      e=malloc(sizeof(struct pl_sub));
      if(e==NULL) {printf("karaul e!\n"); exit(13);}
      e->cptype=names+cptype;
      e->cphnam=names+cphname;
      e->cplnum=names+cplnum;
      el2=malloc(sizeof(struct l2));
      if(el2==NULL) {printf("karaul el2!\n"); exit(13);}
      el2->content=(void *)e;
      in_l2_tail(&qq,el2 );
      pst++;
      // add to queue and increment counter
    }
    fprintf( f, "; HSN substitution transition: t nmp subnet\n"); 
    fprintf(f,"%d %d %s\n",t,pst,names+isubn); 
    fprintf( f, "; HSN place mapping: hp lp\n"); 
    while((el2=from_l2_head( &qq ))!=NULL) 
    {
      e=(struct pl_sub *)el2->content;
      found=0;
//printf("mapping: %s\n",e->cphnam);
	for( p=1; p<=m; p++ )
	  if( strcmp( names+pn[p], e->cphnam )==0 ) { found=1; hp=p; break; }
	  //else printf("other name: %s\n",names+pn[p]);
      if(found==0)
      {
        printf("*** error: invalid HSN label place name %s\n",e->cphnam);
        exit(3);
      }
      lp=atoi(e->cplnum);
      if(strcmp(e->cptype,"i")==0)
      {
        v1=hp; v2=lp;
      }
      else if(strcmp(e->cptype,"o")==0)
      {
        v1=hp; v2=-lp;
      }
      else if(strcmp(e->cptype,"s")==0)
      {
        v1=-hp; v2=lp;
      }
      else if(strcmp(e->cptype,"f")==0)
      {
        v1=-hp; v2=-lp;
      } 
      else
      {
        printf("*** error: invalid HSN label place type %s\n",e->cptype);
        exit(3);
      }
      fprintf(f,"%d %d\n",v1,v2);
      free(e); 
      free(el2);
    }
    
    // print ht np <subnet>
    // print queue hp  lp
    // free queue
    
  }
}/* ProcessHSNlabels */

void WriteLSN( FILE * f )
{
  int i, p, nnmu=0; 
  
  for( p=1; p<=m; p++ )
    if(mu[p]>0) nnmu++;
  
  fprintf( f, "; LSN obtained from NDR\n");
  fprintf( f, "; m n narcs nnmu, nst\n");
  fprintf( f, "%d %d %d %d %d\n", m, n, fapt+fatp+fatt, nnmu, l );
  
  fprintf( f, "; p->t: p t w\n");
  for( i=0; i<fapt; i++ )
    fprintf( f, "%d %d %d\n", aptp[i], aptt[i], (aptw[i]>0)?aptw[i]:-1 );
  
  fprintf( f, "; t->p: -p t w\n");  
  for( i=0; i<fatp; i++ )
    fprintf( f, "%d %d %d\n", -atpp[i], atpt[i], atpw[i] );
  
  fprintf( f, "; t->t: -t1 -t2 0\n");    
  for( i=0; i<fatt; i++ )
    fprintf( f, "%d %d %d\n", -att1[i], -att2[i], 0 );
    
  fprintf( f, "; mu(p):\n");
  for( p=1; p<=m; p++ )
    if(mu[p]>0) fprintf( f, "%d %d\n", p, mu[p] );
      
  if(l>0) 
  {
     ProcessHSNlabels( f );
  }
  
  fprintf( f, "; Table of places\n; no name\n");
  WriteNMP( f );
  
  fprintf( f, "; Table of transitions\n; no name\n");
  WriteNMT( f );
  
  fprintf( f, "; end of LSN\n");

}/* WriteLSN */

int NDRtoLSN( char * NetFileName, char * LSNFileName, int write_name_tables )
{
 char nFileName[ FILENAMELEN+1 ];
 FILE * NetFile, * LSNFile, * nFile, * OutFile;
 int format;
 int z;
   
 /* open files */
 if( strcmp( NetFileName, "-" )==0 ) NetFile = stdin;
   else NetFile = fopen( NetFileName, "r" );
 if( NetFile == NULL ) {printf( "*** error open file %s\n", NetFileName );exit(2);}
 LSNFile = fopen( LSNFileName, "w" );
 if( LSNFile == NULL ) {printf( "*** error open file %s\n", LSNFileName );exit(2);}
   
 /* init net size  */
 maxn=nINIT;
 maxm=mINIT; 
 maxl=lINIT;
 maxnames=namesINIT;
 maxatp=atpINIT;
 maxapt=aptINIT;
 maxatp=attINIT;

 /* allocate arrays */
 tn = (int*) calloc( maxn, sizeof(int) ); n=0;
 tl = (int*) calloc( maxl, sizeof(int) ); l=0;
 tltn = (int*) calloc( maxl, sizeof(int) ); 
 
 pn = (int*) calloc( maxm, sizeof(int) ); m=0;
 mu = (int*) calloc( maxm, sizeof(int) );
 
 names = (char*) calloc( maxnames, sizeof(char) ); fnames=0;
 aptp = (int*) calloc( maxapt, sizeof(int) );
 aptw = (int*) calloc( maxapt, sizeof(int) );
 aptt = (int*) calloc( maxapt, sizeof(int) ); fapt=0;
 atpp = (int*) calloc( maxatp, sizeof(int) );
 atpw = (int*) calloc( maxatp, sizeof(int) );
 atpt = (int*) calloc( maxatp, sizeof(int) ); fatp=0;
 att1 = (int*) calloc( maxatt, sizeof(int) );
 att2 = (int*) calloc( maxatt, sizeof(int) ); fatt=0;

 if( tn==NULL || tl==NULL || tltn==NULL ||
     pn==NULL || 
     mu==NULL ||
     names==NULL ||
     aptp==NULL || aptt==NULL || aptw==NULL ||
     atpp==NULL || atpt==NULL || atpw==NULL ||
     att1==NULL || att2==NULL )
   { printf( "*** not enough memory for net\n" ); return(3); }  
   
 
 ReadNDR( NetFile ); 
 
 if( NetFile != stdin ) fclose( NetFile );

 WriteLSN( LSNFile );
 fclose( LSNFile );
 
 if(write_name_tables)
 {
   sprintf( nFileName, "%s.nmp", LSNFileName );
   nFile = fopen( nFileName, "w" );
   if( nFile == NULL ) {printf( "*** error open file %s\n", nFileName );exit(2);}
   WriteNMP( nFile );
   fclose( nFile );
 
   sprintf( nFileName, "%s.nmt", LSNFileName );
   nFile = fopen( nFileName, "w" );
   if( nFile == NULL ) {printf( "*** error open file %s\n", nFileName );exit(2);}
   WriteNMT( nFile );
   fclose( nFile );
 }

 free( tn ); free(atpp); free(atpw); free(atpt);
 free( tl ); free( tltn );
 
 free( pn ); free(aptp); free(aptw); free(aptt);
 
 free(att1); free(att2);
  
 free( names );
 
 return(0);
 
}/* ReadNDRNET */

#ifdef __MAIN__
int main( int argc, char *argv[] )
{
  if( argc < 3 ) return 2;
  
  NDRtoLSN( argv[1], argv[2], 0 );
}
#endif


// @ Dmitry Zaitsev 2022 daze@acm.org

