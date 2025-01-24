#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>

struct scull_qset {
    void **data;
    struct scull_qset *next;
}; // struct scull_qset


struct scull_dev {
    struct scull_qset *data; /* pointer to first quantum set */
    int quantum;             /* current quantum set */
    int qset;                /* number of quantum sets */
    unsigned long size;      /* amount of data stored here */
    unsigned int access_key; /* later used by sculluid and scullpriv */
    struct semaphore sem;    /* mutual exclusion semaphore */
    struct cdev cdev;        /* char device structure */
}; // struct scull_dev


// trim functionality to clear the device's memory
// kernal mode disallows page faults, hence kfree
int scull_trim(struct scull_dev *dev)
{
    struct scull_qset *next, *dptr;
    int qset 
    for (dptr = dev->data; dptr; dptr = next) { /* all the list items */
        if (dptr->data) {
        for (i = 0; i < qset; i++)
            kfree(dptr->data[i]);
        kfree(dptr->data);
        dptr->data = NULL;
        } // if
        next = dptr->next;
        kfree(dptr);
    } // for
    dev->size = 0;
    dev->quantum = scull_quantum; // TODO
    dev->qset = scull_qset;
    dev->data = NULL;
    return 0;
 } // scull_trim()
