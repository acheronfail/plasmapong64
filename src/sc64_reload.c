/* Development-only AUX protocol: SummerCart64 docs/01_memory_map.md.
 * Reboot targets this project's pinned libdragon IPL3, not arbitrary ROMs. */
#include <libdragon.h>
#include <usb.h>
#include "sc64_reload.h"
#define REG(address) (*(volatile uint32_t *)(address))
#define SC_READ(offset) io_read(0x1fff0000u+(offset))
#define SC_WRITE(offset,value) io_write(0x1fff0000u+(offset),(value))
#define PING 0xff000000u
#define HALT 0xff000001u
#define REBOOT 0xff000002u

static void unlock(void) {
    SC_WRITE(0x10,0); SC_WRITE(0x10,0x5f554e4c); SC_WRITE(0x10,0x4f434b5f);
}

__attribute__((noreturn)) static void reboot(unsigned tv) {
    /* Drain before entering here. No DMA or interrupt may use cartridge/RAM
       while IPL3 replaces the program. The pinned IPL3 handles warm RDRAM. */
    data_cache_writeback_invalidate_all();
    inst_cache_invalidate_all();
    for(unsigned i=0x40;i<0x1000;i+=4)
        REG(0xa4000000u+i)=REG(0xb0000000u+i);
    asm volatile(
        ".set push\n.set noreorder\n"
        "move $s4,%0\n"
        "li $s3,0\nli $s5,1\nli $s6,0x3f\nli $s7,0\n"
        "li $sp,0xa4001ff0\nli $ra,0xa4001550\n"
        "li $t1,0\nli $t2,0x40\nli $t3,0xa4000040\n"
        "jr $t3\nnop\n.set pop\n" :: "r"(tv) : "memory");
    __builtin_unreachable();
}

void sc64_reload_poll(void) {
    if(usb_getcart()!=CART_SC64) return;
    if(!(SC_READ(0)&(1u<<23))) return;
    uint32_t message=SC_READ(0x18);
    SC_WRITE(0x14,1u<<28);
    if(message==PING) { SC_WRITE(0x18,PING); return; }
    if(message!=HALT && message!=REBOOT) return;
    unsigned tv=get_tv_type();
    rspq_wait(); /* Pinned SDK also fences completed RDP work. */
    disable_interrupts();
    REG(0xa4500004)=0; /* Clear queued AI length. */
    REG(0xa4500008)=0; /* AI control: stop playback. */
    REG(0xa4400000)=0; /* VI blank. */
    while(REG(0xa4600010)&3) {} /* PI DMA / IO idle. */
    while(REG(0xa4800018)&3) {} /* SI DMA / IO idle. */
    REG(0xa4040010)=2; /* SP set halt. */
    while(REG(0xa4040018)) {} /* SP DMA idle. */
    REG(0xa404001c)=0;
    REG(0xa4080000)=0;
    SC_WRITE(0x18,message); /* HALT acknowledgement means ROM is no longer read. */
    if(message==REBOOT) reboot(tv);
    for(;;) {
        /* io_read/io_write serialize PI accesses even with interrupts off. */
        if(SC_READ(0x0c)!=0x53437632) unlock();
        if(!(SC_READ(0)&(1u<<23))) continue;
        message=SC_READ(0x18);
        SC_WRITE(0x14,1u<<28);
        if(message==PING || message==HALT || message==REBOOT) SC_WRITE(0x18,message);
        if(message==REBOOT) reboot(tv);
    }
}
