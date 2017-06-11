#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#define Desktop_82540EM_A_Vendor_ID 0x8086
#define Desktop_82540EM_A_Device_ID 0x100E

#define E1000_BASE KSTACKTOP

#define E1000_NTX       64
#define TX_PKT_SIZE     1518

#define E1000_STATUS    0x00008  /* Device Status - RO */
#define E1000_TDBAL     0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH     0x03804  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN     0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH       0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT       0x03818  /* TX Descripotr Tail - RW */
#define E1000_TCTL      0x00400  /* TX Control - RW */
#define E1000_TIPG      0x00410  /* TX Inter-packet gap -RW */

#define E1000_TCTL_EN   0x00000002    /* enable tx */
#define E1000_TCTL_PSP  0x00000008    /* pad short packets */
#define E1000_TCTL_CT   0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD 0x003ff000    /* collision distance */

#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done */

#include <inc/string.h>
#include <kern/pci.h>
#include <kern/pmap.h>

uint32_t *volatile e1000;

int e1000_attach(struct pci_func *f);
/* Transmit Descriptor */
struct e1000_tx_desc {
  uint64_t buffer_addr;       /* Address of the descriptor's data buffer */
  union {
    uint32_t data;
    struct {
      uint16_t length;    /* Data buffer length */
      uint8_t cso;        /* Checksum offset */
      uint8_t cmd;        /* Descriptor control */
    } flags;
  } lower;
  union {
    uint32_t data;
    struct {
      uint8_t status;     /* Descriptor status */
      uint8_t css;        /* Checksum start */
      uint16_t special;
    } fields;
  } upper;
};

#endif	// JOS_KERN_E1000_H
