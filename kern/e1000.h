#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#define Desktop_82540EM_A_Vendor_ID 0x8086
#define Desktop_82540EM_A_Device_ID 0x100E

#include <kern/pci.h>

int e1000_attach(struct pci_func *f);

#endif	// JOS_KERN_E1000_H
