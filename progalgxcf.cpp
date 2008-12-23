/* XCF Flash PROM JTAG programming algorithms

Copyright (C) 2004 Andrew Rogers

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

Changes:
Dmitry Teytelman [dimtey@gmail.com] 14 Jun 2006 [applied 13 Aug 2006]:
    Code cleanup for clean -Wall compile.
*/


 

#include "progalgxcf.h"

const byte ProgAlgXCF::SERASE=0x0a;
const byte ProgAlgXCF::ISCTESTSTATUS=0xe3;
const byte ProgAlgXCF::ISC_ENABLE=0xe8;
const byte ProgAlgXCF::ISC_PROGRAM=0xea;
const byte ProgAlgXCF::ISC_ADDRESS_SHIFT=0xeb;
const byte ProgAlgXCF::ISC_ERASE=0xec;
const byte ProgAlgXCF::ISC_DATA_SHIFT=0xed;
const byte ProgAlgXCF::CONFIG=0xee;
const byte ProgAlgXCF::ISC_READ=0xef;
const byte ProgAlgXCF::ISC_DISABLE=0xf0;
const byte ProgAlgXCF::IDCODE=0xfe;
const byte ProgAlgXCF::BYPASS=0xff;

const byte ProgAlgXCF::BIT3=0x08;
const byte ProgAlgXCF::BIT4=0x10;

ProgAlgXCF::ProgAlgXCF(Jtag &j, IOBase &i, int bs)
{
  block_size=bs;
  jtag=&j;
  io=&i;
}
/* Tries to implement "xflow_erase_optimized" for the serial devices 
 * from the XCF..1532.bsd" files */ 
int ProgAlgXCF::erase()
{
  byte data[4];
  int i;
  jtag->shiftIR(&ISC_DISABLE);
  io->cycleTCK(40000);
  byte ircap[1];
  jtag->shiftIR(&BYPASS,ircap);
  if((ircap[0]&BIT3)==BIT3){
    fprintf(stderr,"Device is write protected.\n");
    return 1;
  }
  jtag->shiftIR(&ISC_ENABLE);
  data[0]=0x37;
  jtag->shiftDR(data,0,6);
  jtag->shiftIR(&ISC_ADDRESS_SHIFT);
  jtag->longToByteArray(1,data);
  jtag->shiftDR(data,0,16);
  io->cycleTCK(2);
  
  printf("Erasing"); fflush(stdout);
  jtag->shiftIR(&ISC_ERASE);
  for(i=0; i<32;i++)
  {
      byte xcstatus[1];
      usleep(500000);
      jtag->shiftIR(&ISCTESTSTATUS);
      jtag->shiftDR(0,xcstatus,8);
      fprintf(stderr,"."); fflush(stdout);
      if (xcstatus[0] & 0x04)
          break;
  }
  if (i < 32)
  {
      printf("done.\n");
      return 0;
  }
  else
  {
      printf("failed.\n");
      return 1;
  }
      
}

int ProgAlgXCF::program(BitFile &file)
{
  jtag->shiftIR(&ISC_DISABLE);
  usleep(1000);
  io->setTapState(IOBase::TEST_LOGIC_RESET);
  byte data[4];
  jtag->shiftIR(&ISC_ENABLE);
  data[0]=0x37;
  jtag->shiftDR(data,0,6);

  for(unsigned int i=0; i<file.getLength(); i+=block_size){
    int frame=i/(block_size/32);
    int j;

    if(io->getVerbose())
      printf("                            \rProgramming frames 0x%04x to 0x%04x",frame,frame+31); ;
    jtag->shiftIR(&ISC_DATA_SHIFT);
    if((i+block_size)<=file.getLength()){
      jtag->shiftDR(&(file.getData())[i/8],0,block_size);
    }
    else{
      int rem=(file.getLength()-i)/8; // Bytes remaining
      int pad=(block_size/8)-rem;
      byte paddata[pad]; for(int k=0; k<pad; k++)paddata[k]=0xff;
      jtag->shiftDR(&(file.getData())[i/8],0,rem*8,0,false); // Do not goto EXIT1-DR
      jtag->shiftDR(paddata,0,pad*8);
    }
    jtag->longToByteArray(frame,data);
    jtag->shiftIR(&ISC_ADDRESS_SHIFT);
    io->cycleTCK(1);
    jtag->shiftDR(data,0,16);
    io->cycleTCK(1);
    jtag->shiftIR(&ISC_PROGRAM);
    for (j=0; j<28; j++)
      {
      byte xcstatus[1];
      usleep(500);
      jtag->shiftIR(&ISCTESTSTATUS);
      jtag->shiftDR(0,xcstatus,8);
      if (xcstatus[0] & 0x04)
          break;
      else if(io->getVerbose())
	{
	  printf(".");
	  fflush(stdout);
	}
      }
    if(j == 28)
      {
	printf("\nProgranmming frame %4d failed\n", frame);
	return 1;
      }
  } 
  printf("\n");
  jtag->shiftIR(&BYPASS);
  io->tapTestLogicReset();
  return 0;
}

int ProgAlgXCF::verify(BitFile &file)
{
  return 0;
}

void ProgAlgXCF::reconfig(void)
{
  jtag->shiftIR(&CONFIG);
  io->cycleTCK(1);
  jtag->shiftIR(&BYPASS);
  io->cycleTCK(1);
  io->tapTestLogicReset();
}
