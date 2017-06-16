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
#define E1000_TXD_CMD_EOP    0x01000000 /* End of Packet */
#define E1000_TXD_CMD_RS     0x08000000 /* Report Status */


#define E1000_NRX       128
#define RX_PKT_SIZE     2048

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
#define E1000_TXD_CMD_EOP    0x01000000 /* End of Packet */
#define E1000_TXD_CMD_RS     0x08000000 /* Report Status */


#define E1000_RA       0x05400      /* Receive Address - RW Array */
#define E1000_RAL0     (E1000_RA+0) /* Receive Address - 0 Low */
#define E1000_RAH0     (E1000_RA+4) /* Receive Address - 0 High */
/* Receive Address */
#define E1000_RAH_AV  0x80000000        /* Receive descriptor valid */

#define E1000_RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    0x02808  /* RX Descriptor Length - RW */
#define E1000_RDH      0x02810  /* RX Descriptor Head - RW */
#define E1000_RDT      0x02818  /* RX Descriptor Tail - RW */

#define E1000_RCTL     0x00100  /* RX Control - RW */

#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_LPE            0x00000020    /* long packet enable */
#define E1000_RCTL_LBM_MASK       0x000000C0    /* tcvr loopback mode */
#define E1000_RCTL_RDMTS_MASK     0x00000300    /* rx desc min threshold size */
#define E1000_RCTL_MO_MASK        0x00007000    /* multicast offset shift */
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
#define E1000_RCTL_BSEX           0x02000000    /* Buffer size extension */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 0 */
#define E1000_RCTL_SZ_MASK        0x00030000    /* rx buffer size 2048 */
#define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */

#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */
#define E1000_RXD_STAT_EOP      0x02    /* End of Packet */


#include <inc/string.h>
#include <inc/error.h>
#include <kern/pci.h>
#include <kern/pmap.h>

uint32_t *volatile e1000;

int e1000_attach(struct pci_func *f);
int e1000_transmit(char *data, int len);
int e1000_receive(char *data);

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

/* Receive Descriptor */
struct e1000_rx_desc {
    uint64_t buffer_addr; /* Address of the descriptor's data buffer */
    uint16_t length;     /* Length of data DMAed into data buffer */
    uint16_t csum;       /* Packet checksum */
    uint8_t status;      /* Descriptor status */
    uint8_t errors;      /* Descriptor Errors */
    uint16_t special;
};

#endif	// JOS_KERN_E1000_H
