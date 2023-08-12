// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0
//#include <linux/of_device.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/printk.h>

#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#include <asm/io.h>

#include "sysarray_rtl.h"

#define DRV_NAME	"sysarray_rtl"

/* <<--regs-->> */
#define SYSARRAY_START 0x4
#define SYSARRAY_CONFIG 0x8
#define SYSARRAY_WEIGHT_BASE 0x0c
#define SYSARRAY_INPUT_BASE 0x10
#define SYSARRAY_OUTPUT_BASE 0x14
#define SYSARRAY_FLIP_MEM_OFFSET 0x18

#define SYSARRAY_START_WEIGHT_READ 0b001
#define SYSARRAY_START_INPUT_READ 0b010
#define SYSARRAY_START_OUTPUT_WRITE 0b011
#define SYSARRAY_START_MATMUL 0b100

#define PFX "sysarray"

#define WIDTH 32
#define HEIGHT WIDTH

#ifdef __riscv
#undef ioread32be
#define ioread32be ioread32
#undef iowrite32be
#define iowrite32be iowrite32
#endif

struct sysarray_driver {
    struct class *class;
    dev_t devno;

    struct platform_driver plat;
};

typedef volatile uint32_t sysarray_register;

//NOTE: The correct packed 32-bit layout of this struct is essential to proper function
struct sysarray_registers {
    const uint32_t _dummy0;
    sysarray_register start_signal;
    sysarray_register config;
    sysarray_register weight_base;
    sysarray_register input_base;
    sysarray_register output_base;
    sysarray_register flip_mem;
    const uint32_t _dummy1;
};

struct sysarray_device {
    struct cdev cdev;
    struct device *pdev; // should be parent device
    struct device *dev;
    struct mutex lock;
    struct completion completion;
    void __iomem *iomem;
    struct sysarray_registers __iomem *registers;
    int irq;
    int err;
    struct sysarray_driver *driver;
    struct module *module;
    int number; //which sysarray this is
};

static struct of_device_id sysarray_device_ids[] = {
	{
		.name = "SLD_SYSARRAY_RTL",
	},
	{
		.name = "eb_102",
	},
	{
		.compatible = "hu,hu_sysarray",
	},
	{ },
};

static struct sysarray_driver sysarray_driver;

static int sysarray_devs; //count of connected sysarray devices

//increment and decrement count of this module
//not completely sure these are necessary
//how does it figure out the correct module
static int sysarray_open(struct inode *inode, struct file *file) {
    struct sysarray_device *sysarray;

    sysarray = container_of(inode->i_cdev, struct sysarray_device, cdev);
    file->private_data = sysarray;

    if(!try_module_get(sysarray->module))
        return -ENODEV;
    else
        return 0;
}

static int sysarray_release(struct inode *inode, struct file *file) {
    struct sysarray_device *sysarray;

    sysarray = file->private_data;
    module_put(sysarray->module);
    return 0;
}

static int sysarray_alloc(struct sysarray_allocation *allocation) {
    void *kernel, *physical;

    printk(KERN_DEBUG "Allocating Sysarray Block\n");

    kernel = kmalloc(allocation->size, GPF_KERNEL);
    if(kernel == NULL) {
        printk(KERN_ERR "Kernel Allocation Failed!\n");
        return -ENOMEM;
    }

    physical = (void*) virt_to_phys(kernel);
    allocation->kernel = kernel;
    allocation->physical = physical;

    printk(KERN_DEBUG "Allocated Block at %px physical, %px kernel\n", allocation->physical, allocation->kernel);
    return 0;
}

static int sysarray_free(struct sysarray_allocation *allocation) {
    printk(KERN_DEBUG "Freeing Sysarray Block (kernel: %p, physical: %p, user: %p)\n", allocation->kernel, allocation->physical, allocation->user);

    kfree(allocation->kernel);

    printk(KERN_DEBUG "Freed!\n");
    return 0;
}
        

//runs a command and waits for completion
static long start_sysarray(struct sysarray_device *sysarray, unsigned instruction) {
    int wait;
    int i;

    printk(KERN_DEBUG "Sysarray starting with opcode: %u\n", instruction);

    reinit_completion(&sysarray->completion);

    printk(KERN_DEBUG "Completion reinitialized\n");

    //iowrite32be(instruction, sysarray->iomem + SYSARRAY_START);
    sysarray->registers->start_signal = instruction;

    printk(KERN_DEBUG "Instruction sent\n");

    wait = wait_for_completion_interruptible(&sysarray->completion);

    printk(KERN_DEBUG "Computation Complete!\n");

    if(wait < 0)
        return -EINTR;
    else
        return 0;
}

static void load_block(quantized *target, quantized *source, int M, int N, int m, int n, int col, int row) {
    int i, j;
    //the ordering of these loops should matter for cache locality
    //perhaps we could optimize by using larger reads than chars, but we would then need the dimensions to be multiples of the larger types' width in bytes.
    for(i = 0; i < m; i++) {
        for(j = 0; i < n; j++) {
            target[i * n + j] = source[(row + i) * N + (col + j)];
        }
    }
}

static void store_block(quantized *target, quantized *source, int M, int N, int m, int n, int row, int col) {
    int i, j;
    for(i = 0; i < m; i++) {
        for(j = 0; j < n; j++) {
            source[(row + i) * N + (col + j)] = target[i * n + j];
        }
    }
}


static void accumulate(quantized *acc, quantized *src, int len) {
    int i;
    for(i = 0; i < len; i++)
        acc[i] += src[i];
}

//user is currently responsible for correct pointer mapaping
//computes W x A
static long sysarray_gemm(struct sysarray_device *sysarray, struct sysarray_gemm_config *config) {
    int N0, M, N1;
    int astrides, shared_strides, wstrides;
    int n0, m, n1; //block sizes
    int rc;

    quantized *weights, activations;
    quantized *out; 

    quantized *wblock, ablock; //we need the blocks since blocks in a larger matrix are not contiguous in memory
    quantized *acc, *scratch; //width * height. In theory, we could get rid of acc with a function that accumulates a block straight into the output

    printk(KERN_DEBUG "Running GEMM\n");

    printk(KERN_DEBUG "Dimensions are (%d x %d), (%d, %d)\n", N0, M, M, N1);

    shared_strides = wstrides = (M + (HEIGHT - 1)) / HEIGHT; //TODO: Check height constraints
    astrides = (M + (WIDTH - 1)) / WIDTH;

    n0 = m = WIDTH;
    n1 = WIDTH;

    int wblock_size = n0 * m;
    int ablock_size = m * n1;

    printk(KERN_DEBUG "wstrides %d, shared_strides %d, astrides %d\n", wstrides, shared_strides, astrides);

    printk(KERN_DEBUG "Block Dimensions are (%d x %d) X (%d x %d)\n", n0, m, m, n1); 

    int q, r, i;

    sysarray->registers->weight_base = wblock;
    sysarray->registers->input_base = ablock;
    sysarray->registers->output_base = scratch;

    for(q = 0; q < wstrides; q++) {
        for(r = 0; r < astrides; r++) {
            printk(KERN_DEBUG "Computing output block (%d, %d)\n", q, r);
            //zero the accumulator
            memset(acc, 0, astrides * wstrides);
            for(i = 0; i < shared_strides; i++) {
                load_block(wblock, weights, N0, M, n0, m, q * n0, i * m);
                load_block(ablock, weights, M, N1, m, n1, i * m, r * n1);
                
                start_sysarray(sysarray, SYSARRAY_START_WEIGHT_READ);
                start_sysarray(sysarray, SYSARRAY_START_INPUT_READ);
                start_sysarray(sysarray, SYSARRAY_START_MATMUL);
                start_sysarray(sysarray, SYSARRAY_START_OUTPUT_WRITE);

                accumulate(acc, scratch, n0 * n1);
            }

            store_block(acc, out, N0, N1, n0, n1, q * n0, r * n1);
        }
    }
}

static void print_start(int num, signed char *ptr) {
    int i;
    for(i = 0; i < num; i++) {
        printk(KERN_DEBUG "arg[%d]: %d\n", i, *(ptr++));
    }
}

static long sysarray_ioctl(struct file *file, unsigned int cm, unsigned long arg) {
    struct sysarray_device *sysarray;
    struct sysarray_allocation allocation;
    int rc;

    sysarray = file->private_data;

    printk(KERN_DEBUG "Sysarray Issued Command %x With Arg %x\n", cm, arg);

    switch(cm) {
        case SYSARRAY_RUN_MATMUL:
            rc = start_sysarray(sysarray, SYSARRAY_START_MATMUL);
            break;
        case SYSARRAY_LOAD_WEIGHTS:
            //iowrite32be(arg, sysarray->iomem + SYSARRAY_WEIGHT_BASE);
            sysarray->registers->weight_base = arg;
            rc = start_sysarray(sysarray, SYSARRAY_START_WEIGHT_READ);
            break;
        case SYSARRAY_LOAD_INPUT:
            //iowrite32be(arg, sysarray->iomem + SYSARRAY_INPUT_BASE);
            sysarray->registers->input_base = arg;
            rc = start_sysarray(sysarray, SYSARRAY_START_INPUT_READ);
            break;
        case SYSARRAY_STORE_OUTPUT:
            //iowrite32be(arg, sysarray->iomem + SYSARRAY_OUTPUT_BASE);
            sysarray->registers->output_base = arg;
            rc = start_sysarray(sysarray, SYSARRAY_START_OUTPUT_WRITE);
            break;
        case SYSARRAY_SET_CONFIG:
            //iowrite32be(arg, sysarray->iomem + SYSARRAY_CONFIG);
            sysarray->registers->config = arg;
            rc = 0;
            break;
        case SYSARRAY_FLIP_MEM:
            iowrite32be(arg, sysarray->iomem + SYSARRAY_FLIP_MEM_OFFSET);
            rc = 0;
            break;
        case SYSARRAY_GEMM:
            rc = -1;//sysarray_gemm((struct sysarray_gemm_config*) arg);
            break;
        case SYSARRAY_ALLOC_BUFFER:
            if ((rc = copy_from_user(&allocation, (struct sysarray_allocation*) arg, sizeof(struct sysarray_allocation)))) break;
            if ((rc = sysarray_alloc(&allocation))) break;
            rc = copy_to_user((struct sysarray_allocation*) arg, &allocation, sizeof(struct sysarray_allocation));
            break; 
        case SYSARRAY_FREE_BUFFER:
            if ((rc = copy_from_user(&allocation, (struct sysarray_allocation*) arg, sizeof(struct sysarray_allocation)))) break;
            rc = sysarray_free(&allocation);
            break;
        default:
            rc = -EINVAL;
            break;
    }

    return rc;
}

static const struct file_operations sysarray_fops = {
    .owner  = THIS_MODULE,
    .open = sysarray_open,
    .release = sysarray_release,
    .unlocked_ioctl = sysarray_ioctl,
};

//this should be the bare minimum
static irqreturn_t sysarray_irq(int irq, void *dev) {
    struct sysarray_device *sysarray = dev_get_drvdata(dev);

    printk(KERN_DEBUG "Sysarray handling interrupt %d\n", irq);

    complete_all(&sysarray->completion);
    return IRQ_HANDLED;
}
    

static int sysarray_create_cdev(struct sysarray_device *sysarray, int ndev) {
    dev_t devno = MKDEV(MAJOR(sysarray->driver->devno), ndev);
    const char *name = sysarray->driver->plat.driver.name;
    int rc;

    cdev_init(&sysarray->cdev, &sysarray_fops);

    sysarray->cdev.owner = sysarray->module;

    if((rc = cdev_add(&sysarray->cdev, devno, 1))) {
        dev_err(sysarray->pdev, "Error %d adding cdev %d\n", rc, ndev);
        return rc;
    }

    sysarray->dev = device_create(sysarray->driver->class, sysarray->pdev, devno, NULL, "%s.%i", name, ndev);
    if (IS_ERR(sysarray->dev)) {
        rc = PTR_ERR(sysarray->pdev);
        dev_err(sysarray->pdev, "Error %d creating device %d\n", rc, ndev);
        sysarray->dev = NULL;
        cdev_del(&sysarray->cdev);
        return rc;
    }

    dev_set_drvdata(sysarray->dev, sysarray);
    return 0;
}

static void sysarray_destroy_cdev(struct sysarray_device *sysarray, int ndev) {
    dev_t devno = MKDEV(MAJOR(sysarray->driver->devno), ndev);

    device_destroy(sysarray->driver->class, devno);
    cdev_del(&sysarray->cdev);
}


//warning: very thread-unsafe
int sysarray_device_register(struct sysarray_device *sysarray, struct platform_device *pdev) {
    struct resource *res;
    int rc;

    sysarray->pdev = &pdev->dev;
    init_completion(&sysarray->completion);

    if((rc = sysarray_create_cdev(sysarray, sysarray->number)))
        return rc; //is there a potential memory leek in not freeing this?

#ifndef __sparc
    sysarray->irq = of_irq_get(pdev->dev.of_node, 0);
#else
    sysarray->irq = pdev->archdata.irqs[0];
#endif
    //we need to register with the interrupt handler, since the sysarray fires one when the computation finishes
    if((rc = request_irq(sysarray->irq, sysarray_irq, IRQF_SHARED, "sysarray_rtl", sysarray->pdev))) {
        dev_info(sysarray->pdev, "cannot request IRQ number %d\n", sysarray->irq);
        //maybe destroy cdev, but not sure would that be a double free?
        sysarray_destroy_cdev(sysarray, sysarray->number);
        return rc;
    }

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0); //last argument is index, so 0 is first MMIO region. Should this be changed depending on the number?
    sysarray->iomem = devm_ioremap_resource(sysarray->pdev, res);
    if(!sysarray->iomem) {
        dev_info(sysarray->pdev, "cannot map sysarray mmio registers!\n");
        free_irq(sysarray->irq, sysarray->pdev);
        sysarray_destroy_cdev(sysarray, sysarray->number);
        return -EIO; //is this the correct error?
    } 
    sysarray->registers = (struct sysarray_registers*) sysarray->iomem;

    dev_info(sysarray->pdev, "sysarray registered");
    platform_set_drvdata(pdev, sysarray);

    return 0;
}


static int sysarray_probe(struct platform_device *pdev)
{
	struct sysarray_device *sysarray;
	int rc;

	sysarray = kzalloc(sizeof(*sysarray), GFP_KERNEL);
	if (sysarray == NULL)
		return -ENOMEM;
	sysarray->module = THIS_MODULE;
	sysarray->number = sysarray_devs;
	sysarray->driver = &sysarray_driver;
	if((rc = sysarray_device_register(sysarray, pdev))){
        kfree(sysarray);
        return rc;
    } else {
        sysarray_devs++;
        printk(KERN_DEBUG "Sysarray Device %d Registered", sysarray_devs);
        return 0;
    }
}

static int __exit sysarray_remove(struct platform_device *pdev)
{
	struct sysarray_device *sysarray = platform_get_drvdata(pdev);
    
    //TODO: THIS NEEDS TO BE IMPLEMENTED
	//esp_device_unregister(esp);
	kfree(sysarray);
	return 0;
}

static struct sysarray_driver sysarray_driver = {
	.plat = {
		.probe		= sysarray_probe,
		.remove		= sysarray_remove,
		.driver		= {
			.name = DRV_NAME,
			.owner = THIS_MODULE,
			.of_match_table = sysarray_device_ids,
		},
	},
};

void sysarray_driver_remove(void) {
    dev_t devno = MKDEV(MAJOR(sysarray_driver.devno), 0);
    class_destroy(sysarray_driver.class);
    unregister_chrdev_region(devno, SYSARRAY_DEVICES);
}


static int __init sysarray_init(void) {
    printk(KERN_DEBUG "Initializing sysarray driver!\n");

    struct sysarray_driver *driver = &sysarray_driver;

    const char* name = driver->plat.driver.name;
    int rc; 

    sysarray_driver.class = class_create(driver->plat.driver.owner, name);
    
    if (IS_ERR(sysarray_driver.class)) {
        pr_err(PFX "Failed to create sysarray class\n");
        return PTR_ERR(sysarray_driver.class);
    }

    if((rc = alloc_chrdev_region(&sysarray_driver.devno, 0, SYSARRAY_DEVICES, name))) {
        pr_err(PFX "Failed to allocate chrdev region\n");
        class_destroy(sysarray_driver.class);
        return rc;
    }

    if((rc = platform_driver_register(&driver->plat))) {
        sysarray_driver_remove();
        return rc;
    }

    return 0;
}

static void __exit sysarray_exit(void)
{
	sysarray_driver_remove();
}

module_init(sysarray_init)
module_exit(sysarray_exit)

MODULE_DEVICE_TABLE(of, sysarray_device_ids);

MODULE_AUTHOR("Moe Frumkin <mfrumki1@jhu.edu>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("sysarray_rtl driver");
