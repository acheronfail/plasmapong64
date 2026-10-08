/* Q3 pressure -> Q4 velocity gradient. Weighted differences use the 48-bit
 * accumulator rather than saturating a 17-bit neighbour difference. The
 * rounded reciprocal differs from exact division by at most one Q4 unit. */
#include <rsp_queue.inc>
#include "fluid_config.h"
    .set noreorder
    .set at
#define ROW (PLASMAPONG_GRID_W*2)
#define HEIGHT PLASMAPONG_GRID_H
    .data
RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand GradientShort,12
RSPQ_EndOverlayHeader
RSPQ_EmptySavedState
    .quad 0
    .bss
    .align 4
above: .ds.b ROW
middle: .ds.b ROW
below: .ds.b ROW
urow: .ds.b ROW
vrow: .ds.b ROW
    .text
.macro short_dma base,buffer,direction
    move s0,\base
    li s4,%lo(\buffer)
    li t0,ROW-1
    li t1,ROW
    jal DMAExec
    li t2,\direction
.endm
GradientShort:
    move s1,a0
    move s2,a1
    move s3,a2
    move s5,zero
    li t3,(524288+PLASMAPONG_CELL_Q4/2)/PLASMAPONG_CELL_Q4
    mtc2 t3,$v20.e0
    subu t3,zero,t3
    mtc2 t3,$v20.e1
    li t3,-16383
    mtc2 t3,$v20.e2
    li t3,16383
    mtc2 t3,$v20.e3
    ctc2 zero,COP2_CTRL_VCO
    li fp,%lo(above)
    li k0,%lo(middle)
    li k1,%lo(below)
    short_dma s3,middle,DMA_IN
short_row:
    li t3,HEIGHT-1
    beq s5,t3,short_loaded
    nop
    addiu s0,s3,ROW
    move s4,k1
    li t0,ROW-1
    li t1,ROW
    jal DMAExec
    li t2,DMA_IN
short_loaded:
    short_dma s1,urow,DMA_IN
    short_dma s2,vrow,DMA_IN
    move s6,zero
short_batch:
    addu t3,k0,s6
    addiu t4,t3,-2
    lqv $v01,0,t4
    lrv $v01,16,t4
    addiu t4,t3,2
    lqv $v02,0,t4
    lrv $v02,16,t4
    addu t4,fp,s6
    lqv $v03,0,t4
    addu t4,k1,s6
    lqv $v04,0,t4
    addiu t5,s6,%lo(urow)
    addiu t6,s6,%lo(vrow)
    lqv $v05,0,t5
    lqv $v06,0,t6
    vmulf $v07,$v02,$v20.e0
    vmacf $v07,$v01,$v20.e1
    vmulf $v08,$v04,$v20.e0
    vmacf $v08,$v03,$v20.e1
    vsub $v05,$v05,$v07
    vsub $v06,$v06,$v08
    vge $v05,$v05,$v20.e2
    vlt $v05,$v05,$v20.e3
    vge $v06,$v06,$v20.e2
    vlt $v06,$v06,$v20.e3
    sqv $v05,0,t5
    sqv $v06,0,t6
    addiu s6,s6,16
    slti t3,s6,ROW
    bnez t3,short_batch
    nop
    sh zero,%lo(urow)(zero)
    sh zero,%lo(urow)+ROW-2(zero)
    beqz s5,short_zero_v
    li t3,HEIGHT-1
    bne s5,t3,short_store
    nop
short_zero_v:
    li t3,%lo(vrow)
    .set part,0
    .rept PLASMAPONG_GRID_W/8
    sqv vzero,part,t3
    .set part,part+16
    .endr
short_store:
    short_dma s1,urow,DMA_OUT
    short_dma s2,vrow,DMA_OUT
    move t3,fp
    move fp,k0
    move k0,k1
    move k1,t3
    addiu s1,s1,ROW
    addiu s2,s2,ROW
    addiu s3,s3,ROW
    addiu s5,s5,1
    slti t3,s5,HEIGHT
    bnez t3,short_row
    nop
    j RSPQ_Loop
    nop
