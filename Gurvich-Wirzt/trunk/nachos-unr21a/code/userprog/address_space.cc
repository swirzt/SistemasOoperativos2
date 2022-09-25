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
AddressSpace::AddressSpace(Executable *executable, unsigned pid)
{
  // ASSERT(executable_file != nullptr);

  // exe = new Executable(executable_file);
  exe = executable;
  ASSERT(exe->CheckMagic());

  // How big is address space?
  size = exe->GetSize() + USER_STACK_SIZE;

  // We need to increase the size to leave room for the stack.
  numPages = DivRoundUp(size, PAGE_SIZE);
  size = numPages * PAGE_SIZE;

#ifdef SWAP
  swapName(pid, nombreswap);
  fileSystem->Create(nombreswap);
  swap = fileSystem->Open(nombreswap);

  swapped = new SwappedList(numPages);
#else
  ASSERT(numPages <= pages->CountClear()); // Si no hay swap necesitamos que el programa entre en memeoria
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
    if (pageTable[i].valid && phy != -1)
      pages->Clear(phy);
  }
  delete[] pageTable;
#ifdef SWAP
  fileSystem->Remove(nombreswap);
#ifndef FILESYS_STUB
  fileSystem->Close(swap);
#else
  delete swap;
#endif
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

unsigned nextVictim = 0;

// Random swap policy
#ifdef SWAP
int PickVictim()
{
#ifdef POLICY_RANDOM_PICK
  // DEBUG('e', "Policy RANDOM PICK selected\n");
  return rand() % NUM_PHYS_PAGES;
#endif

#ifdef POLICY_FIFO
  // DEBUG('e', "Policy FIFO selected\n");
  int position = nextVictim;
  nextVictim = (nextVictim + 1) % NUM_PHYS_PAGES;
  return position;
#endif

#ifdef POLICY_CLOCK
  // DEBUG('e', "Policy CLOCK selected\n");
  currentThread->space->SaveState();
  for (int ronda = 1; ronda < 5; ronda++)
  {
    for (unsigned pos = nextVictim; pos < nextVictim + NUM_PHYS_PAGES; pos++)
    {
      int *checkReplace = pages->GetOwner(pos % NUM_PHYS_PAGES);
      int checkReplacePID = checkReplace[1];
      int checkReplaceVPN = checkReplace[0];
      TranslationEntry *entry = &(activeThreads->Get(checkReplacePID)->space->pageTable[checkReplaceVPN]);
      switch (ronda)
      {
      case 1:
        if (!entry->use && !entry->dirty)
        {
          nextVictim = (pos + 1) % NUM_PHYS_PAGES;
          return pos % NUM_PHYS_PAGES;
        }
        break;
      case 2:
        if (!entry->use && entry->dirty)
        {
          nextVictim = (pos + 1) % NUM_PHYS_PAGES;
          return pos % NUM_PHYS_PAGES;
        }
        else
          entry->use = false;
        break;
      case 3:
        if (!entry->use && !entry->dirty)
        {
          nextVictim = (pos + 1) % NUM_PHYS_PAGES;
          return pos % NUM_PHYS_PAGES;
        }
        break;
      case 4:
        if (!entry->use && entry->dirty)
        {
          nextVictim = (pos + 1) % NUM_PHYS_PAGES;
          return pos % NUM_PHYS_PAGES;
        }
        break;
      }
    }
  }
  nextVictim = (nextVictim + 1) % NUM_PHYS_PAGES; // Pa calmar al GCC
  return nextVictim;
#endif
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
  for (unsigned i = 0; i < TLB_SIZE; i++) // Invalidamos el estdo de cada entrada de la TLB
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
    int *tokill = pages->GetOwner(phy);
    int tokillPID = tokill[1];                                         // Este es el PID del Proceso
    int tokillVPN = tokill[0];                                         // Esta es la VPN para de la pagina fisica para ese proceso
    activeThreads->Get(tokillPID)->space->WriteToSwap(tokillVPN, phy); // Le decimos al proceso que guarde su página
    // pages->Clear(phy); //No es necesaria porque Mark sobreescribe, no le importa si no está limpia
    pages->Mark(phy, vpn, currentThread->pid);
  }

#endif

  pageTable[vpn].physicalPage = phy;
  pageTable[vpn].valid = true;
  unsigned int vaddrinit = vpn * PAGE_SIZE;

#ifdef SWAP
  int whereswap = swapped->Find(vpn);

  if (whereswap == -1)
  {
#endif
    uint32_t dataSize = exe->GetInitDataSize();
    uint32_t dataAddr = exe->GetInitDataAddr();
    uint32_t codeSize = exe->GetCodeSize();
    uint32_t codeAddr = exe->GetCodeAddr();

    unsigned int codeCopy = 0;
    if (codeSize > 0 && codeSize + codeAddr > vaddrinit) // Tengo que leer algo en Code
    {
      codeCopy = codeSize - vaddrinit;
      codeCopy = codeCopy > PAGE_SIZE ? PAGE_SIZE : codeCopy; // Reviso que no se pase de una página
      exe->ReadCodeBlock(&mainMemory[phy * PAGE_SIZE], codeCopy, vaddrinit - codeAddr);
    }
    unsigned int dataCopy = 0;
    if (codeCopy < PAGE_SIZE && dataSize > 0 && vaddrinit < dataAddr + dataSize) // Tengo que leer data inicializada
    {
      int offset = codeCopy > 0 ? 0 : vaddrinit - dataAddr;
      dataCopy = PAGE_SIZE - codeCopy;
      dataCopy = dataCopy + vaddrinit + codeCopy > dataSize + dataAddr ? dataSize + dataAddr - (vaddrinit + codeCopy) : dataCopy; // Si lo que tengo que leer se pasa de data inicializada, achico la lectura
      exe->ReadDataBlock(&mainMemory[phy * PAGE_SIZE + codeCopy], dataCopy, offset);
    }
    unsigned int copied = codeCopy + dataCopy;
    if (copied < PAGE_SIZE) // Pongo en 0 la memoria para data no inicializada o STACK
    {
      memset(&mainMemory[phy * PAGE_SIZE + copied], 0, PAGE_SIZE - copied);
    }
#ifdef SWAP
  }
  else
  {
    swap->ReadAt(&mainMemory[phy * PAGE_SIZE], PAGE_SIZE, whereswap * PAGE_SIZE);
    stats->fromSwap++;
  }
#endif
}

#ifdef SWAP
void AddressSpace::WriteToSwap(unsigned vpn, int phy)
{
  if (currentThread->space == this)
  {
    TranslationEntry *tlb = machine->GetMMU()->tlb;
    for (unsigned i = 0; i < TLB_SIZE; i++)
    {
      if (tlb[i].valid && tlb[i].physicalPage == phy)
      {
        pageTable[vpn] = tlb[i];
        tlb[i].valid = false;
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
    stats->toSwap++;
  }
  pageTable[vpn].valid = false;
}
#endif
#endif
