#include <linux/kernel.h>
#include <linux/fs.h>
#include "util.h"



// trim functionality to clear the device's memory
// kernal mode disallows page faults, hence kfree
int scull_trim(scull_dev *dev)
{
    scull_qset *next, *dptr;
    int qset = dev->qset;
    for (dptr = dev->data; dptr; dptr = next) { /* all the list items */
        if (dptr->data) {
            for (int i = 0; i < qset; i++)
                kfree(dptr->data[i]);
            kfree(dptr->data);
            dptr->data = NULL;
        } // if
        next = dptr->next;
        kfree(dptr);
    } // for
    dev->size = 0;
    dev->quantum = SCULL_QUANTUM_SIZE
    dev->qset = SCULL_QSET_SIZE;
    dev->data = NULL;
    return 0;
 } // scull_trim()
