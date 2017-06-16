#include <kern/e1000.h>

struct e1000_tx_desc tx_queue[E1000_NTX] __attribute__((aligned(16)));
char tx_bufs[E1000_NTX][TX_PKT_SIZE];

struct e1000_rx_desc rx_queue[E1000_NRX] __attribute__((aligned(16)));
char rx_bufs[E1000_NRX][RX_PKT_SIZE];

int e1000_attach(struct pci_func *f) {
  pci_func_enable(f);

  boot_map_region(kern_pgdir, E1000_BASE, f->reg_size[0], f->reg_base[0], PTE_PCD | PTE_PWT | PTE_W);
  e1000 = (uint32_t *)E1000_BASE;
  assert(e1000[E1000_STATUS >> 2] == 0x80080783);

  memset(tx_queue   , 0, sizeof(tx_queue));
  memset(tx_bufs, 0, sizeof(tx_bufs));
  int i;
  for (i = 0; i < E1000_NTX; i++) {
    tx_queue[i].buffer_addr = PADDR(tx_bufs[i]);
    tx_queue[i].upper.data |= E1000_TXD_STAT_DD;
  }

  e1000[E1000_TDBAL >> 2] = PADDR(tx_queue);
  e1000[E1000_TDBAH >> 2] = 0;
  e1000[E1000_TDLEN >> 2] = sizeof(tx_queue);
  e1000[E1000_TDH   >> 2] = 0;
  e1000[E1000_TDT   >> 2] = 0;

  e1000[E1000_TCTL  >> 2] |=  E1000_TCTL_EN;
  e1000[E1000_TCTL  >> 2] |=  E1000_TCTL_PSP;
  e1000[E1000_TCTL  >> 2] &= ~E1000_TCTL_CT;   //0x000ff0
  e1000[E1000_TCTL  >> 2] |=  (0x10) << 4;     //0x000100
  e1000[E1000_TCTL  >> 2] &= ~E1000_TCTL_COLD; //0x3ff000
  e1000[E1000_TCTL  >> 2] |=  (0x40) << 12;    //0x012000
  e1000[E1000_TIPG  >> 2]  =  10 | (4 << 10) | (6 << 20); // IPGT | IPGR1 | IPGR2

  memset(rx_queue, 0, sizeof(rx_queue));
  memset(rx_bufs, 0, sizeof(rx_bufs));
  for(i = 0; i < E1000_NRX; i++)
    rx_queue[i].buffer_addr = PADDR(rx_bufs[i]);

  // hard-code QEMU's default MAC address of 52:54:00:12: 34:56
  e1000[E1000_RAL0  >> 2] = 0x12005452;
  e1000[E1000_RAH0  >> 2] = (0x00005634 | E1000_RAH_AV);

  e1000[E1000_RDBAL >> 2] = PADDR(rx_queue);
  e1000[E1000_RDBAH >> 2] = 0;
  e1000[E1000_RDLEN >> 2] = sizeof(rx_queue);
  e1000[E1000_RDH   >> 2] = 1;
  e1000[E1000_RDT   >> 2] = 0;

  e1000[E1000_RCTL  >> 2]  =  E1000_RCTL_EN;
  e1000[E1000_RCTL  >> 2] &= ~E1000_RCTL_LPE;
  e1000[E1000_RCTL  >> 2] &= ~E1000_RCTL_LBM_MASK;
  e1000[E1000_RCTL  >> 2] &= ~E1000_RCTL_RDMTS_MASK;
  e1000[E1000_RCTL  >> 2] &= ~E1000_RCTL_MO_MASK;
  e1000[E1000_RCTL  >> 2] |=  E1000_RCTL_BAM;
  e1000[E1000_RCTL  >> 2] &= ~E1000_RCTL_BSEX;
  e1000[E1000_RCTL  >> 2] &= ~E1000_RCTL_SZ_MASK;
  e1000[E1000_RCTL  >> 2] |=  E1000_RCTL_SZ_2048;
  e1000[E1000_RCTL  >> 2] |=  E1000_RCTL_SECRC;

  return 0;
}

int
e1000_transmit(char *data, int len){
  if(data == NULL || len < 0 || len > TX_PKT_SIZE)
    return -E_INVAL;

  uint32_t tdt = e1000[E1000_TDT >> 2];
  if(!(tx_queue[tdt].upper.data & E1000_TXD_STAT_DD))
    return -E_TX_FULL;

  memset(tx_bufs[tdt], 0 , sizeof(tx_bufs[tdt]));
  memmove(tx_bufs[tdt], data, len);
  tx_queue[tdt].lower.flags.length  = len;
  tx_queue[tdt].lower.data         |= E1000_TXD_CMD_RS;
  tx_queue[tdt].lower.data         |= E1000_TXD_CMD_EOP;
  tx_queue[tdt].upper.data         &= ~E1000_TXD_STAT_DD;

  e1000[E1000_TDT >> 2] = (tdt + 1) % E1000_NTX;
  return 0;
}

int
e1000_receive(char *data){
  if(data == NULL)
    return -E_INVAL;
  uint32_t rdt = (e1000[E1000_RDT >> 2] + 1) % E1000_NRX;
  if(!(rx_queue[rdt].status & E1000_RXD_STAT_DD))
    return -E_RX_EMPTY;
  if(!(rx_queue[rdt].status & E1000_RXD_STAT_EOP))
    return -E_RX_LONG;

  while(rdt == e1000[E1000_RDH >> 2]);
  uint32_t len = rx_queue[rdt].length;
  memmove(data, rx_bufs[rdt], len);
  rx_queue[rdt].status &= ~E1000_RXD_STAT_DD;
  rx_queue[rdt].status &= ~E1000_RXD_STAT_EOP;

  e1000[E1000_RDT >> 2] = rdt;
  return len;
}
