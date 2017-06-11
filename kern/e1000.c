#include <kern/e1000.h>

int e1000_attach(struct pci_func *f) {
  pci_func_enable(f);
  return 0;
}
