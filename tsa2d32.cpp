/*
	Copyright (C) 1995-2008	Edward Der-Hua Liu, Hsin-Chu, Taiwan
*/

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include "gcin.h"
#include "pho.h"


int *phidx, *sidx, phcount;
int bfsize, phidxsize;
char *bf;
u_char *sf;

static int qcmp(const void *a, const void *b)
{
  int idxa=*((int *)a);
  int idxb=*((int *)b);
  u_char lena,lenb, len;
  int cha, chb;
  int i;
  u_short ka,kb;

  lena=bf[idxa]; idxa+=1 + sizeof(usecount_t);
  lenb=bf[idxb]; idxb+=1 + sizeof(usecount_t);
  cha=idxa + lena * sizeof(phokey_t);
  chb=idxb + lenb * sizeof(phokey_t);
  len=Min(lena,lenb);

  for(i=0;i<len;i++) {
    memcpy(&ka, &bf[idxa], sizeof(phokey_t));
    memcpy(&kb, &bf[idxb], sizeof(phokey_t));
    if (ka > kb) return 1;
    if (kb > ka) return -1;
    idxa+=2;
    idxb+=2;
  }

  if (lena > lenb)
    return 1;
  if (lena < lenb)
    return -1;

  int tlena = utf8_tlen(&bf[cha], lena);
  int tlenb = utf8_tlen(&bf[chb], lenb);

  if (tlena > tlenb)
    return 1;
  if (tlena < tlenb)
    return -1;

  return memcmp(&bf[cha], &bf[chb], tlena);
}


static int qcmp_usecount(const void *a, const void *b)
{
  int idxa=*((int *)a);  char *pa = (char *)&sf[idxa];
  int idxb=*((int *)b);  char *pb = (char *)&sf[idxb];
  u_char lena,lenb, len;
  int i;
  u_short ka,kb;
  usecount_t usecounta, usecountb;

  lena=*(pa++); memcpy(&usecounta, pa, sizeof(usecount_t)); pa+= sizeof(usecount_t);
  lenb=*(pb++); memcpy(&usecountb, pb, sizeof(usecount_t)); pb+= sizeof(usecount_t);
  len=Min(lena,lenb);

  for(i=0;i<len;i++) {
    memcpy(&ka, pa, sizeof(phokey_t));
    memcpy(&kb, pb, sizeof(phokey_t));
    if (ka > kb) return 1;
    if (kb > ka) return -1;
    pa+=sizeof(phokey_t);
    pb+=sizeof(phokey_t);
  }

  if (lena > lenb)
    return 1;
  if (lena < lenb)
    return -1;

  // now lena == lenb
  int tlena = utf8_tlen(pa, lena);
  int tlenb = utf8_tlen(pb, lenb);

  if (tlena > tlenb)
    return 1;
  if (tlena < tlenb)
    return -1;

  return usecountb - usecounta;
}

void send_gcin_message(Display *dpy, char *s);
#if WIN32
void init_gcin_program_files();
 #pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
#endif

int main(int argc, char **argv)
{
  FILE *fp,*fw;
  u_char s[1024];
  u_char chbuf[MAX_PHRASE_LEN * CH_SZ];
  u_short phbuf[80];
  int i,j,idx,len, ofs;
  u_short kk;
  int hashidx[TSIN_HASH_N];
  u_char clen;
  gboolean reload = getenv("GCIN_NO_RELOAD")==NULL;

  if (argc > 1) {
    if ((fp=fopen(argv[1], "r"))==NULL) {
       printf("Cannot open %s\n", argv[1]);
       exit(-1);
    }
  } else
    fp=stdin;

  skip_utf8_sigature(fp);

  bfsize=300000;
  if (!(bf=(char *)malloc(bfsize))) {
    puts("malloc err");
    exit(1);
  }

  phidxsize=30000;
  if (!(phidx=(int *)malloc(phidxsize*4))) {
    puts("malloc err");
    exit(1);
  }

  int lineCnt=0;
  phcount=ofs=0;
  while (!feof(fp)) {
    usecount_t usecount=0;

    lineCnt++;

    fgets((char *)s,sizeof(s),fp);
    len=strlen((char *)s);
    if (s[0]=='#')
      continue;

    if (s[len-1]=='\n')
      s[--len]=0;

    if (len==0)
      continue;

    i=0;
    int chbufN=0;
    int charN = 0;
    while (s[i]!=' ' && i<len) {
      int len = utf8_sz((char *)&s[i]);

      memcpy(&chbuf[chbufN], &s[i], len);

      i+=len;
      chbufN+=len;
      charN++;
    }

    while (i < len && s[i]==' ' || s[i]=='\t')
      i++;

    int phbufN=0;
    while (i<len && phbufN < charN && s[i]!=' ') {
      kk=0;

      while (s[i]!=' ' && i<len) {
        if (kk==(BACK_QUOTE_NO << 9))
          kk|=s[i];
        else
          kk |= lookup(&s[i]);

        i+=utf8_sz((char *)&s[i]);
      }

      i++;
      phbuf[phbufN++]=kk;
    }

    if (phbufN!=charN) {
      fprintf(stderr, "Line %d problem in phbufN!=chbufN %d != %d\n",
        lineCnt, phbufN, chbufN);
      exit(-1);
    }

    clen=phbufN;

    while (i<len && s[i]==' ')
      i++;

    if (i==len)
      usecount = 0;
    else
      usecount = atoi((char *)&s[i]);

    /*      printf("len:%d\n", clen); */
    phidx[phcount++]=ofs;
    memcpy(&bf[ofs++],&clen,1);
    memcpy(&bf[ofs],&usecount, sizeof(usecount_t)); ofs+=sizeof(usecount_t);
    memcpy(&bf[ofs],phbuf, clen * sizeof(phokey_t));
    ofs+=clen * sizeof(phokey_t);
    memcpy(&bf[ofs], chbuf, chbufN);
    ofs+=chbufN;
    if (ofs+100 >= bfsize) {
      bfsize+=65536;
      if (!(bf=(char *)realloc(bf,bfsize))) {
        puts("realloc err");
        exit(1);
      }
    }
    if (phcount+100 >= phidxsize) {
      phidxsize+=1000;
      if (!(phidx=(int *)realloc(phidx,phidxsize*4))) {
        puts("realloc err");
        exit(1);
      }
    }
  }
  fclose(fp);

  /* dumpbf(bf,phidx); */

  puts("Sorting ....");
  qsort(phidx,phcount,4,qcmp);

  if (!(sf=(u_char *)malloc(bfsize))) {
    puts("malloc err");
    exit(1);
  }

  if (!(sidx=(int *)malloc(phidxsize*4))) {
    puts("malloc err");
    exit(1);
  }


  // delete duplicate
  ofs=0;
  j=0;
  for(i=0;i<phcount;i++) {
    idx = phidx[i];
    sidx[j]=ofs;
    len=bf[idx];
    int tlen = utf8_tlen(&bf[idx + 1 + sizeof(usecount_t) + sizeof(phokey_t)*len], len);
    clen=sizeof(phokey_t)*len + tlen + 1 + sizeof(usecount_t);

    if (i && !qcmp(&phidx[i-1], &phidx[i]))
      continue;

    memcpy(&sf[ofs], &bf[idx], clen);
    j++;
    ofs+=clen;
  }

  phcount=j;
#if 1
  puts("Sorting by usecount ....");
  qsort(sidx, phcount, 4, qcmp_usecount);
#endif

  for(i=0;i<256;i++)
    hashidx[i]=-1;

  for(i=0;i<phcount;i++) {
    phokey_t kk,jj;

    idx=sidx[i];
    idx+= 1 + sizeof(usecount_t);
    memcpy(&kk, &sf[idx], sizeof(phokey_t));
    jj=kk;
    kk>>=TSIN_HASH_SHIFT;
    if (hashidx[kk] < 0) {
      hashidx[kk]=i;
    }
  }

  if (hashidx[0]==-1)
    hashidx[0]=0;

  hashidx[TSIN_HASH_N-1]=phcount;
  for(i=TSIN_HASH_N-2;i>=0;i--) {
    if (hashidx[i]==-1)
      hashidx[i]=hashidx[i+1];
  }

  for(i=1; i< TSIN_HASH_N; i++) {
    if (hashidx[i]==-1)
      hashidx[i]=hashidx[i-1];
  }

  printf("Writing data tsin32 %d\n", ofs);
  if ((fw=fopen("tsin32","wb"))==NULL) {
    puts("create err");
    exit(-1);
  }

  fwrite(sf,1,ofs,fw);
  fclose(fw);

  puts("Writing data tsin32.idx");
  if ((fw=fopen("tsin32.idx","wb"))==NULL) {
    puts("create err");
    exit(-1);
  }

  fwrite(&phcount,4,1,fw);
  fwrite(hashidx,1,sizeof(hashidx),fw);
  fwrite(sidx,4,phcount,fw);
  printf("%d phrases\n",phcount);

  fclose(fw);

  if (reload) {
    dbg("reload\n");
    gtk_init(&argc, &argv);
    send_gcin_message(GDK_DISPLAY(), RELOAD_TSIN_DB);
  }

  exit(0);
}