#include <kern/e1000.h>

struct e1000_tx_desc tx_queue[E1000_NTX] __attribute__((aligned(16)));
char tx_pkt_bufs[E1000_NTX][TX_PKT_SIZE];

int e1000_attach(struct pci_func *f) {
  pci_func_enable(f);

  boot_map_region(kern_pgdir, E1000_BASE, f->reg_size[0], f->reg_base[0], PTE_PCD | PTE_PWT | PTE_W);
  e1000 = (uint32_t *)E1000_BASE;
  assert(e1000[E1000_STATUS >> 2] == 0x80080783);

  memset(tx_queue   , 0, sizeof(tx_queue));
  memset(tx_pkt_bufs, 0, sizeof(tx_pkt_bufs));
  int i;
  for (i = 0; i < E1000_NTX; i++) {
    tx_queue[i].buffer_addr = PADDR(tx_pkt_bufs[i]);
    tx_queue[i].upper.data |= E1000_TXD_STAT_DD;
  }

  e1000[E1000_TDBAL >> 2] = PADDR(tx_queue);
  e1000[E1000_TDBAH >> 2] = 0;
  e1000[E1000_TDLEN >> 2] = sizeof(tx_queue);
  e1000[E1000_TDH   >> 2] = 0;
  e1000[E1000_TDT   >> 2] = 0;

  e1000[E1000_TCTL  >> 2] |= E1000_TCTL_EN;
  e1000[E1000_TCTL  >> 2] |= E1000_TCTL_PSP;
  e1000[E1000_TCTL  >> 2] &= ~E1000_TCTL_CT;  //0x000ff0
  e1000[E1000_TCTL  >> 2] |= (0x10) << 4;     //0x000100
  e1000[E1000_TCTL  >> 2] &= ~E1000_TCTL_COLD;//0x3ff000
  e1000[E1000_TCTL  >> 2] |= (0x40) << 12;    //0x012000
  e1000[E1000_TIPG  >> 2]  = 10 | (4 << 10) | (6 << 20); // IPGT | IPGR1 | IPGR2

  return 0;
}
