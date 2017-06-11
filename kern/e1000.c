#include <kern/e1000.h>

int e1000_attach(struct pci_func *f) {
  pci_func_enable(f);

  boot_map_region(kern_pgdir, E1000_BASE, f->reg_size[0], f->reg_base[0], PTE_PCD | PTE_PWT | PTE_W);
  e1000 = (uint32_t *)E1000_BASE;
  assert(e1000[E1000_STATUS >> 2] == 0x80080783);
  return 0;
}
