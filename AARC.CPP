//--------------------------------------------------------------------------
//  Project      : instal.prj
//  File Name    : instal.cpp
//  Written by   : Ioan Ghip
//  Created      : 14.03.1994
//  Updated      : 14.03.1994
//--------------------------------------------------------------------------
//  Dest. System : MS-DOS
//--------------------------------------------------------------------------
//  Libraries    :
//--------------------------------------------------------------------------
//  Remarks
//--------------------------------------------------------------------------
//  1993,1994 by FUTURA Software s.r.l.
//--------------------------------------------------------------------------

#include <stdio.h>
#include <malloc.h>
#include <process.h>
#include <ctype.h>
#include <STRING.H>

#define NUMAR_MAGIC 0x1234  //pentru recunoasterea arhivei
#define OCTET 8
typedef unsigned char      byte;
typedef unsigned short int word;
typedef struct ar{
    struct ar *sptr;     //legatura stinga
    struct ar *dptr;     //legatura dreapta
    byte sval;           //caracterul din stinga
    byte dval;           //caracterul din dreapta
} ARBORE;                //structura unui nod de arbore

ARBORE *arbore[256];     //arborele de caractere
long frecvente[256];     //tabloul frecventelor de aparitie
			 //a caracterelor in fisier
byte *codificari[256];   //codificarile caracterelor
word magic=NUMAR_MAGIC;
long lungimeFisier=0L;
void *radacina;          //radacina arborelui de refacere

void scrieBit(byte bit,FILE *iesire)
{
 static byte biti   =0;
 static byte contor =0;
 biti=(biti<<1)|((bit)?1:0);
 if(++contor==OCTET)
 {
    putc(biti,iesire);
    biti=contor=0;
 }
}

void scrieOctet(byte biti,FILE *iesire)
{
   byte masca=0x80;
   while(masca)
   {
       scrieBit(biti & masca,iesire);
       masca>>=1;
   }
}

int citesteBit(FILE *intrare)
{
 static word biti   =0;
 static byte contor =0;
 if(contor==0)
 {
    biti=getc(intrare);
    contor=OCTET;
 }
 contor--;
 biti<<=1;
 return (biti & 0x100) ? 1 : 0;
}

byte citesteOctet(FILE *intrare)
{
    byte rezultat=0;
    int contor=OCTET;
    while(contor--)
    {
       rezultat<<=1;
       rezultat|=citesteBit(intrare);
    }
    return rezultat;
}

void memoreazaDrum(int caracter,byte pozitie,byte *drum)
{
  codificari[caracter]=(byte*)malloc(pozitie+1);
  codificari[caracter][0]=pozitie;
  for(int i=0; i<pozitie; i++)
      codificari[caracter][i+1]=drum[i];
}

void decodificaArbore(ARBORE *adresa)
{
   static byte drum[256];
   static byte pozitie=0;
   drum[pozitie++]=0;
   if(adresa->sptr)
      decodificaArbore(adresa->sptr);
   else
      memoreazaDrum(adresa->sval,pozitie,drum);
   drum[pozitie-1]=1;
   if(adresa->dptr)
      decodificaArbore(adresa->dptr);
   else
      memoreazaDrum(adresa->dval,pozitie,drum);
   pozitie--;
}

int construiesteArbore(FILE* intrare)
{
   ARBORE *temporar;
   unsigned long lmin1,lmin2;
   short min1,min2;
   int i,c;
   //calcul frcventei caracterelor infisier
   while((c=getc(intrare))!=EOF)
   {
      frecvente[c]++;
      lungimeFisier++;
   }
   //constructia arborelui de codificare
   while(1)
   {
     lmin1=lmin2=-1L;
     //calculul celor mai mici doua elemente
     //din tabloul de frcvente
     for(i=0;i<256;i++)
     {
	if(frecvente[i]!=0L)
	{
	   if(frecvente[i]<lmin1)
	   {
	      lmin2=lmin1;
	      min2=min1;
	      lmin1=frecvente[i];
	      min1=i;
	   }
	   else if(frecvente[i]<lmin2)
	   {
	      lmin2=frecvente[i];
	      min2=i;
	   }
	}
     }
     //am gasit radacina ?
     if(lmin2==-1)
	break;
     //constructia unui nod de arbore
     temporar=(ARBORE*)malloc(sizeof(ARBORE));
     temporar->sptr=arbore[min1];
     temporar->dptr=arbore[min2];
     temporar->sval=min1;
     temporar->dval=min2;
     arbore[min1]=temporar;
     //greutatea unui nod este suma greutatilor
     //nodurilor din care a fost compus
     frecvente[min1]=lmin1+lmin2;
     frecvente[min2]=0L;
   }
   return min1;
}
void scrieFisier(FILE *intrare,FILE *iesire)
{
   int c,lungime;
   byte *ptr;
   fseek(intrare,0L,SEEK_SET);
   while((c=getc(intrare))!=EOF)
   {
      ptr=codificari[c];
      lungime=(ptr)?(int)(*ptr++):0;
      while(lungime--)
	 scrieBit(*ptr++,iesire);
   }
   scrieOctet(0,iesire);
}

void scrieArbore(FILE* iesire)
{
   byte *ptr;
   int i,lungime;
   fwrite(&magic,sizeof(magic),1,iesire);
   fwrite(&lungimeFisier,sizeof(lungimeFisier),1,iesire);
   for(i=0;i<256;i++)
   {
      ptr=codificari[i];
      lungime=(ptr)?(int)(*ptr++):0;
      if(lungime)
      {
	 scrieBit(1,iesire);
	 scrieOctet(lungime,iesire);
	 while(lungime--)
	       scrieBit(*ptr++,iesire);
      }
      else
	 scrieBit(0,iesire);
   }
}

void citesteDrum(int caracter,FILE *intrare)
{
   ARBORE *ptr;
   int i,lungime;
   int pozitie;
   lungime=citesteOctet(intrare);
   ptr =(ar*) radacina;
   for(i=0;i<lungime;i++)
   {
       pozitie=citesteBit(intrare);
       //daca nu exista nodul,il creaza
       if(pozitie && !ptr->dptr)
	  ptr->dptr=(ARBORE*)calloc(1,sizeof(ARBORE));
       if(!pozitie && !ptr->sptr)
	  ptr->sptr=(ARBORE*)calloc(1,sizeof(ARBORE));
       ptr=(pozitie)?ptr->dptr:ptr->sptr;
   }
   ptr->sval=caracter;
   ptr->dval=0xff;
}

int decodificaOctet(FILE* intrare)
{
   ARBORE *ptr;
   ptr=(ar*)radacina;
   do
   {
      ptr=citesteBit(intrare)?(ar*)ptr->dptr:(ar*)ptr->sptr;
   }
   while(!ptr->dval);
   return ptr->sval;
}

void decodificaFisier(FILE *intrare,FILE*iesire)
{
   while(lungimeFisier--)
      putc(decodificaOctet(intrare),iesire);
}

void citesteCodificare(FILE* intrare)
{
  int i;
  fread(&magic,sizeof(magic),1,intrare);
  if(magic!=NUMAR_MAGIC)
  {
     printf("Fisierul de intrare nu este compactat !\n");
     exit(1);
  }
  fread(&lungimeFisier,sizeof(lungimeFisier),1,intrare);
  radacina=calloc(1,2+2*sizeof(char*));
  for(i=0;i<256;i++)
     if(citesteBit(intrare))
	citesteDrum(i,intrare);
}


int main(int argc,char** argv)
{
  FILE *intrare,*iesire;
  if(argc<4 || argv[1][0] == '?')
  {
   printf("Program de arhivare AARC, realizat la FUTURA SOFTWARE S.R.L.\n");
   printf("Loc. BUCURESTI Str.Cetatea Histriei Nr.1 TEL/FAX (401) 726.72.04\n");
   printf("Se va folosi numai cu acordul firmei prezentate mai sus.\n");
   printf("                   (c) 1994\n");
   printf("Utilizare aarc c / e fis_de_arhivat nume_arhiva\n");
   printf("   c - pentru arhivare\n");
   printf("   e - pentru dezarhivare\n");
   return 1;
  }
  if((toupper(argv[1][0])!='C'&& toupper(argv[1][0])!='E'))
  {
    printf("Comanda incorecta: %s\n",argv[1]);
    return 1;
  }
  if((intrare=fopen(argv[2],"rb"))==NULL)
  {
    printf("Nu pot deschide fisierul de intrare !\n");
    return 1;
  }
  if((iesire=fopen(argv[3],"wb"))==NULL)
  {
    printf("Nu pot deschide fisierul de iesire !\n");
    return 1;
  }
  if(toupper(argv[1][0])=='C')
  {
     decodificaArbore(arbore[construiesteArbore(intrare)]);
     scrieArbore(iesire);
     scrieFisier(intrare,iesire);
  }
  else
  {
    citesteCodificare(intrare);
    decodificaFisier(intrare,iesire);
  }
  fclose(intrare);
  fclose(iesire);
  return 0;
}
//--------------------------------------------------------------------------
//  End of File  : instal.cpp
//--------------------------------------------------------------------------
