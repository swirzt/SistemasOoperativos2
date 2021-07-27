/// Routines to manage address spaces (memory for executing user programs).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#include "address_space.hh"
#include "executable.hh"
#include "threads/system.hh"
#include <stdio.h>

#include <string.h>

#ifdef SWAP
#include <stdlib.h>
#include "vmem/swappedlist.hh"
#include <inttypes.h>
#include <iostream>
#endif

unsigned int AddressSpace::Translate(unsigned int virtualAddr)
{
  uint32_t page = virtualAddr / PAGE_SIZE;
  uint32_t offset = virtualAddr % PAGE_SIZE;
  uint32_t physicalPage = pageTable[page].physicalPage;
  return physicalPage * PAGE_SIZE + offset;
}

#ifdef SWAP
unsigned charlen(char *word)
{
  unsigned i = 0;
  while (*(word + i++))
    ;
  return i;
}

void swapName(int pid, char *name)
{
  name[0] = 'S';
  name[1] = 'W';
  name[2] = 'A';
  name[3] = 'P';
  name[4] = '.';
  char pidc[20];
  std::sprintf(pidc, "%d", pid);
  int i, len = charlen(pidc);
  for (i = 0; i < len + 1; i++)
  {
    name[i + 5] = pidc[i];
  }
  // name[i + 5] = '\0';
}
#endif

/// First, set up the translation from program memory to physical memory.
/// For now, this is really simple (1:1), since we are only uniprogramming,
/// and we have a single unsegmented page table.
AddressSpace::AddressSpace(OpenFile *executable_file, unsigned pid)
{
  ASSERT(executable_file != nullptr);

  exe = new Executable(executable_file);
  ASSERT(exe->CheckMagic());

  // How big is address space?
  size = exe->GetSize() + USER_STACK_SIZE;

  // We need to increase the size to leave room for the stack.
  numPages = DivRoundUp(size, PAGE_SIZE);
  size = numPages * PAGE_SIZE;

#ifdef SWAP
  swapName(pid, nombreswap);
  fileSystem->Create(nombreswap, 0);
  swap = fileSystem->Open(nombreswap);

  swapped = new SwappedList(numPages);
#else
  ASSERT(numPages <= pages->CountClear()); // Porque me gusta
#endif
  // Check we are not trying to run anything too big -- at least until we
  // have virtual memory.

  DEBUG('a', "Initializing address space, num pages %u, size %u\n",
        numPages, size);

  // First, set up the translation.

  pageTable = new TranslationEntry[numPages];
  for (unsigned i = 0; i < numPages; i++)
  {
    pageTable[i].virtualPage = i;
#ifndef DEMAND_LOADING
    int phy = pages->Find();
    // // Queremos que la máquina cierre por ahora (hasta tener virtual)
    // ASSERT(phy != -1);
    pageTable[i].physicalPage = phy;
    pageTable[i].valid = true;
#else
    pageTable[i].physicalPage = -1;
    pageTable[i].valid = false;
#endif
    pageTable[i].use = false;
    pageTable[i].dirty = false;
    pageTable[i].readOnly = false;
    // If the code segment was entirely on a separate page, we could
    // set its pages to be read-only.
  }
#ifndef DEMAND_LOADING

  char *mainMemory = machine->GetMMU()->mainMemory;

  // Zero out the entire address space, to zero the unitialized data
  // segment and the stack segment.
  // memset(mainMemory, 0, size);
  for (unsigned i = 0; i < numPages; i++)
  {
    memset(&mainMemory[pageTable[i].physicalPage * PAGE_SIZE], 0, PAGE_SIZE);
  }

  // Then, copy in the code and data segments into memory.
  uint32_t codeSize = exe->GetCodeSize();
  uint32_t initDataSize = exe->GetInitDataSize();
  if (codeSize > 0)
  {
    int i = 0;
    uint32_t read = 0;
    uint32_t virtualAddr = exe->GetCodeAddr();
    // ASSERT(codeSize + virtualAddr <= numPages * PAGE_SIZE); //Quiero ver si puedo leer todo el codigo y me entra en mi espacio asignado
    DEBUG('a', "Initializing code segment, at 0x%X, size %u\n",
          virtualAddr, codeSize);
    while (read < codeSize)
    {
      uint32_t addr = Translate(virtualAddr + i);
      exe->ReadCodeBlock(&mainMemory[addr], 1, read);
      read++;
      i++;
    }
  }

  if (initDataSize > 0)
  {
    int i = 0;
    uint32_t read = 0;
    uint32_t virtualAddr = exe->GetInitDataAddr();
    DEBUG('a', "Initializing data segment, at 0x%X, size %u\n",
          virtualAddr, initDataSize);
    while (read < initDataSize)
    {
      uint32_t addr = Translate(virtualAddr + i);
      exe->ReadDataBlock(&mainMemory[addr], 1, read);
      read++;
      i++;
    }
  }
#endif
}
/// Deallocate an address space.
///
/// Nothing for now!
AddressSpace::~AddressSpace()
{
  for (unsigned i = 0; i < numPages; i++)
  {
    int phy = pageTable[i].physicalPage;
    if (phy != -1)
      pages->Clear(phy);
  }
  delete[] pageTable;
#ifdef SWAP
  delete swap;
#endif
}

/// Set the initial values for the user-level register set.
///
/// We write these directly into the “machine” registers, so that we can
/// immediately jump to user code.  Note that these will be saved/restored
/// into the `currentThread->userRegisters` when this thread is context
/// switched out.
void AddressSpace::InitRegisters()
{
  for (unsigned i = 0; i < NUM_TOTAL_REGS; i++)
  {
    machine->WriteRegister(i, 0);
  }

  // Initial program counter -- must be location of `Start`.
  machine->WriteRegister(PC_REG, 0);

  // Need to also tell MIPS where next instruction is, because of branch
  // delay possibility.
  machine->WriteRegister(NEXT_PC_REG, 4);

  // Set the stack register to the end of the address space, where we
  // allocated the stack; but subtract off a bit, to make sure we do not
  // accidentally reference off the end!
  machine->WriteRegister(STACK_REG, numPages * PAGE_SIZE - 16);
  DEBUG('a', "Initializing stack register to %u\n",
        numPages * PAGE_SIZE - 16);
}

/// On a context switch, save any machine state, specific to this address
/// space, that needs saving.
///
/// For now, nothing!
void AddressSpace::SaveState()
{
#ifdef SWAP
  TranslationEntry *tlb = machine->GetMMU()->tlb;

  for (unsigned i = 0; i < TLB_SIZE; i++)
  {
    if (tlb[i].valid)
    {
      unsigned vpn = tlb[i].virtualPage;
      pageTable[vpn] = tlb[i];
    }
  }
#endif
}

//Random swap policy
#ifdef SWAP
int PickVictim()
{
  return rand() % NUM_PHYS_PAGES;
}
#endif

/// On a context switch, restore the machine state so that this address space
/// can run.
///
/// For now, tell the machine where to find the page table.
void AddressSpace::RestoreState()
{
#ifndef USE_TLB
  machine->GetMMU()->pageTable = pageTable;
  machine->GetMMU()->pageTableSize = numPages;
#else
  for (unsigned i = 0; i < TLB_SIZE; i++) //Invalidamos el estdo de cada entrada de la TLB
  {
    machine->GetMMU()->tlb[i].valid = false;
  }
#endif
}

#ifdef DEMAND_LOADING
void AddressSpace::LoadPage(int vpn)
{
  char *mainMemory = machine->GetMMU()->mainMemory;

#ifndef SWAP
  int phy = pages->Find();
  if (phy == -1)
    ASSERT(false);
#else
  int phy = pages->Find(vpn, currentThread->pid);
  if (phy == -1)
  {
    phy = PickVictim();
    unsigned *tokill = pages->GetOwner(phy);
    unsigned tokillPID = tokill[1];                                    // Este es el PID del Proceso
    unsigned tokillVPN = tokill[0];                                    // Esta es la VPN para de la pagina fisica para ese proceso
    activeThreads->Get(tokillPID)->space->WriteToSwap(tokillVPN, phy); // Le decimos al proceso que guarde su página
  }

#endif

  pageTable[vpn].physicalPage = phy;
  pageTable[vpn].valid = true;
  memset(&mainMemory[pageTable[vpn].physicalPage * PAGE_SIZE], 0, PAGE_SIZE); //Solo cuando es la pila
  unsigned int vaddrinit = vpn * PAGE_SIZE;

#ifdef SWAP
  int whereswap = swapped->Find(vpn);

  if (whereswap == -1)
  {
#endif
    if (vaddrinit <= size - USER_STACK_SIZE) // size es getSize + USER_STACK_SIZE
    {
      uint32_t dataSizeI = exe->GetInitDataSize();
      uint32_t dataSizeU = exe->GetUninitDataSize();
      uint32_t dataAddr = exe->GetInitDataAddr();

      if (dataSizeI + dataSizeU == 0) // No hay sección Data
      {
        exe->ReadCodeBlock(&mainMemory[pageTable[vpn].physicalPage * PAGE_SIZE], PAGE_SIZE, vaddrinit);
      }
      else if (vaddrinit < dataAddr && dataSizeI + dataSizeU > 0) // Hay data y estamos en codeBlock
      {
        if (vaddrinit + PAGE_SIZE >= dataAddr) // Arranca en code termina en Data
        {
          unsigned leercode = dataAddr - vaddrinit;
          unsigned leerdata = PAGE_SIZE - leercode;
          exe->ReadCodeBlock(&mainMemory[pageTable[vpn].physicalPage * PAGE_SIZE], leercode, vaddrinit);
          exe->ReadDataBlock(&mainMemory[pageTable[vpn].physicalPage * PAGE_SIZE] + leercode, leerdata, 0);
        }
        else // Esta todo en Code
        {
          exe->ReadCodeBlock(&mainMemory[pageTable[vpn].physicalPage * PAGE_SIZE], PAGE_SIZE, vaddrinit);
        }
      }
      else if (vaddrinit < dataAddr + dataSizeI && vaddrinit >= dataAddr && dataSizeI + dataSizeU > 0) // Estamos en data Init
      {
        if (vaddrinit + PAGE_SIZE >= dataAddr + dataSizeI)
        { // Arranca en init, termina afuera
          unsigned leerInit = dataAddr + dataSizeI - vaddrinit;
          exe->ReadDataBlock(&mainMemory[pageTable[vpn].physicalPage * PAGE_SIZE], leerInit, vaddrinit - dataAddr);
        }
        else // Esta todo en data Init
        {
          exe->ReadDataBlock(&mainMemory[pageTable[vpn].physicalPage * PAGE_SIZE], PAGE_SIZE, vaddrinit - dataAddr);
        }
      }
    }
#ifdef SWAP
  }
  else
  {
    printf("Che estoy leyendo del swap %d %d\n", phy, whereswap);
    swap->ReadAt(&mainMemory[phy * PAGE_SIZE], PAGE_SIZE, whereswap * PAGE_SIZE);
  }
#endif
}

#ifdef SWAP
void AddressSpace::WriteToSwap(unsigned vpn, unsigned phy)
{
  if (currentThread->space == this)
  {
    TranslationEntry *tlb = machine->GetMMU()->tlb;
    for (unsigned i = 0; i < TLB_SIZE; i++)
    {
      if (tlb[i].physicalPage == phy && tlb[i].valid)
      {
        pageTable[vpn] = tlb[i];
        // tlb[i].valid = false;
      }
    }
  }
  if (pageTable[vpn].dirty)
  {
    int whereswap = swapped->Find(vpn);
    if (whereswap == -1)
      whereswap = swapped->Add(vpn);

    char *mainMemory = machine->GetMMU()->mainMemory;
    swap->WriteAt(&mainMemory[pageTable[vpn].physicalPage * PAGE_SIZE], PAGE_SIZE, whereswap * PAGE_SIZE);
    pageTable[vpn].dirty = false;
  }
  pageTable[vpn].valid = false;
}
#endif

#endif
