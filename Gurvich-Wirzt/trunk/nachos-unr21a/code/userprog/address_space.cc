/// Routines to manage address spaces (memory for executing user programs).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#include "address_space.hh"
#include "executable.hh"
#include "threads/system.hh"

#include <string.h>

unsigned int AddressSpace::Translate(unsigned int virtualAddr)
{
  uint32_t page = virtualAddr / PAGE_SIZE;
  uint32_t offset = virtualAddr % PAGE_SIZE;
  uint32_t physicalPage = pageTable[page].physicalPage;
  return physicalPage * PAGE_SIZE + offset;
}

/// First, set up the translation from program memory to physical memory.
/// For now, this is really simple (1:1), since we are only uniprogramming,
/// and we have a single unsegmented page table.
AddressSpace::AddressSpace(OpenFile *executable_file)
{
  ASSERT(executable_file != nullptr);

  Executable exe(executable_file);
  ASSERT(exe.CheckMagic());

  // How big is address space?

  unsigned size = exe.GetSize() + USER_STACK_SIZE;
  // We need to increase the size to leave room for the stack.
  numPages = DivRoundUp(size, PAGE_SIZE);
  size = numPages * PAGE_SIZE;

  ASSERT(numPages <= pages->CountClear());
  // Check we are not trying to run anything too big -- at least until we
  // have virtual memory.

  DEBUG('a', "Initializing address space, num pages %u, size %u\n",
        numPages, size);

  // First, set up the translation.

  pageTable = new TranslationEntry[numPages];
  for (unsigned i = 0; i < numPages; i++)
  {
    pageTable[i].virtualPage = i;
    int phy = pages->Find();
    // // Queremos que la máquina cierre por ahora (hasta tener virtual)
    // ASSERT(phy != -1);
    pageTable[i].physicalPage = phy;
    pageTable[i].valid = true;
    pageTable[i].use = false;
    pageTable[i].dirty = false;
    pageTable[i].readOnly = false;
    // If the code segment was entirely on a separate page, we could
    // set its pages to be read-only.
  }

  char *mainMemory = machine->GetMMU()->mainMemory;

  // Zero out the entire address space, to zero the unitialized data
  // segment and the stack segment.
  // memset(mainMemory, 0, size);
  for (unsigned i = 0; i < numPages; i++)
  {
    memset(&mainMemory[pageTable[i].physicalPage * PAGE_SIZE], 0, PAGE_SIZE);
  }

  // Then, copy in the code and data segments into memory.
  uint32_t codeSize = exe.GetCodeSize();
  uint32_t initDataSize = exe.GetInitDataSize();
  if (codeSize > 0)
  {
    int i = 0;
    uint32_t read = 0;
    uint32_t virtualAddr = exe.GetCodeAddr();
    // ASSERT(codeSize + virtualAddr <= numPages * PAGE_SIZE); //Quiero ver si puedo leer todo el codigo y me entra en mi espacio asignado
    DEBUG('a', "Initializing code segment, at 0x%X, size %u\n",
          virtualAddr, codeSize);
    while (read < codeSize)
    {
      uint32_t addr = Translate(virtualAddr + i);
      exe.ReadCodeBlock(&mainMemory[addr], 1, read);
      read++;
      i++;
    }
  }

  if (initDataSize > 0)
  {
    int i = 0;
    uint32_t read = 0;
    uint32_t virtualAddr = exe.GetInitDataAddr();
    DEBUG('a', "Initializing data segment, at 0x%X, size %u\n",
          virtualAddr, initDataSize);
    while (read < initDataSize)
    {
      uint32_t addr = Translate(virtualAddr + i);
      exe.ReadDataBlock(&mainMemory[addr], 1, read);
      read++;
      i++;
    }
    // exe.ReadDataBlock(&mainMemory[virtualAddr], initDataSize, 0);
  }
}

/// Deallocate an address space.
///
/// Nothing for now!
AddressSpace::~AddressSpace()
{
  for (unsigned i = 0; i < numPages; i++)
    pages->Clear(pageTable[i].physicalPage);
  delete[] pageTable;
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
}

/// On a context switch, restore the machine state so that this address space
/// can run.
///
/// For now, tell the machine where to find the page table.
void AddressSpace::RestoreState()
{
  machine->GetMMU()->pageTable = pageTable;
  machine->GetMMU()->pageTableSize = numPages;
}
