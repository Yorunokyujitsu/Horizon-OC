/*
 * Copyright (c) Souldbminer
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

/*
 * Exception Vector Table (ARM7TDMI):
 * 0x00: Reset
 * 0x04: Undefined Instruction
 * 0x08: Software Interrupt (SWI)
 * 0x0C: Prefetch Abort
 * 0x10: Data Abort
 * 0x14: Reserved
 * 0x18: IRQ
 * 0x1C: FIQ
 */

#include <stdint.h>
#include <string.h>

extern uint32_t _sdata;     /* Start of .data section */
extern uint32_t _edata;     /* End of .data section */
extern uint32_t _sidata;    /* Start of .data in SRAM */
extern uint32_t _sbss;      /* Start of .bss section */
extern uint32_t _ebss;      /* End of .bss section */
extern uint32_t _estack;    /* End of stack */

extern int main(void);

/* Forward declarations */
void Reset_Handler(void);
void Undefined_Handler(void);
void SWI_Handler(void);
void PrefetchAbort_Handler(void);
void DataAbort_Handler(void);
void IRQ_Handler(void);
void FIQ_Handler(void);


void Reset_Handler(void) {
    uint32_t data_size = (uint32_t)(&_edata) - (uint32_t)(&_sdata);
    if (data_size > 0) {
        memcpy(&_sdata, &_sidata, data_size);
    }
    
    uint32_t bss_size = (uint32_t)(&_ebss) - (uint32_t)(&_sbss);
    if (bss_size > 0) {
        memset(&_sbss, 0, bss_size);
    }
    
    main();
    
    while (1) {
        __asm__ volatile("wfi"); 
    }
}


void DataAbort_Handler(void) {
    uint32_t fault_addr;
    uint32_t fault_status;
    
    __asm__ volatile("mrc p15, 0, %0, c6, c0, 0" : "=r"(fault_addr));   /* DFAR */
    __asm__ volatile("mrc p15, 0, %0, c5, c0, 0" : "=r"(fault_status)); /* DFSR */
    
    (void)fault_addr;
    (void)fault_status;
    
    while (1) {
        __asm__ volatile("nop");
    }
}

void PrefetchAbort_Handler(void) {
    uint32_t fault_addr;
    uint32_t fault_status;
    
    __asm__ volatile("mrc p15, 0, %0, c6, c0, 2" : "=r"(fault_addr));   /* IFAR */
    __asm__ volatile("mrc p15, 0, %0, c5, c0, 1" : "=r"(fault_status)); /* IFSR */
    
    (void)fault_addr;
    (void)fault_status;
    
    while (1) {
        __asm__ volatile("nop");
    }
}

void Undefined_Handler(void) {
    while (1) {
        __asm__ volatile("nop");
    }
}

void SWI_Handler(void) {
    uint32_t *lr;
    uint32_t swi_instr;
    uint32_t swi_number;
    
    __asm__ volatile("mov %0, lr" : "=r"(lr));
    swi_instr = *(lr - 1);
    swi_number = swi_instr & 0xFFFFFF;  /* Lower 24 bits = SWI number */
    
    (void)swi_number;  /* Use SWI number for dispatch */
}

void IRQ_Handler(void) {
    while (1) {
        __asm__ volatile("nop");
    }
}

void FIQ_Handler(void) {
    while (1) {
        __asm__ volatile("nop");
    }
}

void Default_Handler(void) {
    while (1) {
        __asm__ volatile("swi 0");
    }
}

__attribute__((section(".vectors")))
void (*const exception_vectors[])(void) = {
    Reset_Handler,              /* 0x00 - Reset */
    Undefined_Handler,          /* 0x04 - Undefined Instruction */
    SWI_Handler,                /* 0x08 - Software Interrupt */
    PrefetchAbort_Handler,      /* 0x0C - Prefetch Abort */
    DataAbort_Handler,          /* 0x10 - Data Abort */
    (void (*)(void))0,          /* 0x14 - Reserved */
    IRQ_Handler,                /* 0x18 - IRQ */
    FIQ_Handler                 /* 0x1C - FIQ */
};

static inline uint32_t get_cpsr(void) {
    uint32_t cpsr;
    __asm__ volatile("mrs %0, cpsr" : "=r"(cpsr));
    return cpsr;
}

static inline void set_cpsr(uint32_t cpsr) {
    __asm__ volatile("msr cpsr, %0" : : "r"(cpsr));
}

static inline void disable_irq(void) {
    uint32_t cpsr = get_cpsr();
    set_cpsr(cpsr | 0x80);  /* Set I bit (disable IRQ) */
}

static inline void enable_irq(void) {
    uint32_t cpsr = get_cpsr();
    set_cpsr(cpsr & ~0x80);  /* Clear I bit (enable IRQ) */
}

static inline void disable_fiq(void) {
    uint32_t cpsr = get_cpsr();
    set_cpsr(cpsr | 0x40);  /* Set F bit (disable FIQ) */
}

static inline void enable_fiq(void) {
    uint32_t cpsr = get_cpsr();
    set_cpsr(cpsr & ~0x40);  /* Clear F bit (enable FIQ) */
}

int main(void) {
    while (1) {
        __asm__("nop");
    }
    return 0;
}