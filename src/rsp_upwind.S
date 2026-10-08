/* Local upwind advection of two velocity or three dye channels. Each source
 * plane streams through a three/four-row cache; velocity coefficients are shared by every
 * channel in an eight-cell batch. No arbitrary per-cell gather or full grid
 * needs to fit DMEM. The shared CFL limiter runs before the advection consumer. */
#include <rsp_queue.inc>
#include "fluid_config.h"
    .set noreorder
    .set at
#define WIDTH PLASMAPONG_GRID_W
#define HEIGHT PLASMAPONG_GRID_H
#define ROW_BYTES (WIDTH*2)
#define PLANE_BYTES (WIDTH*HEIGHT*2)
#ifdef PLASMAPONG_UPWIND_PREFETCH
#define SOURCE_ROWS 4
#else
#define SOURCE_ROWS 3
#endif
#define SOURCE_BYTES (SOURCE_ROWS*ROW_BYTES)
    .data
RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand Advect,16
#ifdef PLASMAPONG_UPWIND_GPU_LIMIT
    RSPQ_DefineCommand Limit,16
#endif
RSPQ_EndOverlayHeader
RSPQ_EmptySavedState
    .quad 0
    .bss
    .align 4
rows: .ds.b SOURCE_BYTES*3
urow: .ds.b ROW_BYTES
vrow: .ds.b ROW_BYTES
output: .ds.b ROW_BYTES*3
params: .ds.b 32
#ifdef PLASMAPONG_UPWIND_INLINE_LIMIT
limit_saved: .ds.b 16
#endif
    .text
Advect:
    move s1,a0
    move s2,a1
    move s3,a2
    move s0,a3
    li s4,%lo(params)
    li t0,31
    li t1,32
    jal DMAExec
    li t2,DMA_IN
    li t3,%lo(params)
    lw t4,0(t3)
    mtc2 t4,$v20.e0
    srl t4,t4,16
    mtc2 t4,$v20.e1
    li t4,128
    mtc2 t4,$v20.e2
    li t4,256
    mtc2 t4,$v22.e0
    vxor $v22,vzero,$v22.e0
    lw t4,12(t3)
    mtc2 t4,$v25.e0
    vxor $v25,vzero,$v25.e0
#ifdef PLASMAPONG_UPWIND_INLINE_LIMIT
    lw t4,4(t3)
    mtc2 t4,$v21.e0
    srl t5,t4,16
    sll t5,t5,1
    mtc2 t5,$v21.e1
    addiu t4,t4,1
    mtc2 t4,$v21.e2
#endif
    lw k0,8(t3)
    ctc2 zero,COP2_CTRL_VCO
    move t8,s1
    li t9,%lo(rows)
    move k1,zero
preload:
    move s0,t8
    move s4,t9
    li t0,2*ROW_BYTES-1
    li t1,2*ROW_BYTES
    jal DMAExec
    li t2,DMA_IN
    addiu t8,t8,PLANE_BYTES
    addiu t9,t9,SOURCE_BYTES
    addiu k1,k1,1
    bne k1,k0,preload
    nop
#ifdef PLASMAPONG_UPWIND_INLINE_LIMIT
    /* Both initial velocity rows must be limited before either is consumed. */
    li t3,2
    bne k0,t3,initial_limit_done
    nop
    li t8,%lo(rows)
    li t9,%lo(rows)+SOURCE_BYTES
    move v1,s1
    jal ClampLoadedRow
    nop
    addiu t8,t8,ROW_BYTES
    addiu t9,t9,ROW_BYTES
    addiu v1,v1,ROW_BYTES
    jal ClampLoadedRow
    nop
initial_limit_done:
#endif
    move s5,zero
    move s6,zero
    move a3,zero
row:
#ifdef PLASMAPONG_UPWIND_PREFETCH
    /* Retire the prior output before reusing it, and the prefetched source
       before consuming it. Source DMA runs alongside the preceding row math. */
    jal DMAWaitIdle
    nop
#endif
    addiu a0,s6,-ROW_BYTES
    bgez a0,above_ready
    nop
    addiu a0,a0,SOURCE_BYTES
above_ready:
    addiu a2,s6,ROW_BYTES
    slti t3,a2,SOURCE_BYTES
    bnez t3,below_ready
    nop
    addiu a2,a2,-SOURCE_BYTES
below_ready:
    bnez s5,not_top
    nop
    move a0,s6
    b source_ready
    nop
not_top:
    li t3,HEIGHT-1
    bne s5,t3,load_below
    nop
    move a2,s6
    b source_ready
    nop
load_below:
#ifndef PLASMAPONG_UPWIND_PREFETCH
    move t8,s1
    li t9,%lo(rows)
    move k1,zero
source_plane:
    addu s0,t8,a3
    addiu s0,s0,ROW_BYTES
    addu s4,t9,a2
    li t0,ROW_BYTES-1
    li t1,ROW_BYTES
    jal DMAExec
    li t2,DMA_IN
    addiu t8,t8,PLANE_BYTES
    addiu t9,t9,SOURCE_BYTES
    addiu k1,k1,1
    bne k1,k0,source_plane
    nop
#endif
source_ready:
#ifdef PLASMAPONG_UPWIND_INLINE_LIMIT
    li t3,2
    bne k0,t3,source_limit_done
    nop
    beqz s5,source_limit_done
    li t3,HEIGHT-1
    beq s5,t3,source_limit_done
    nop
    /* The previous row already limited current/above. Only the newly
       loaded below row remains, including when it arrived by prefetch. */
    addiu t8,a2,%lo(rows)
    addiu t9,t8,SOURCE_BYTES
    addu v1,s1,a3
    addiu v1,v1,ROW_BYTES
    jal ClampLoadedRow
    nop
source_limit_done:
#endif
    li t3,2
    bne k0,t3,load_velocity
    nop
    /* Velocity advection already has both coefficient rows in its source
       cache. Reuse them instead of issuing two additional DMAs per row. */
    addiu a1,s6,%lo(rows)
    addiu fp,a1,SOURCE_BYTES
    b velocity_ready
    nop
load_velocity:
    addu s0,s2,a3
    li s4,%lo(urow)
    li t0,ROW_BYTES-1
    li t1,ROW_BYTES
    jal DMAExec
    li t2,DMA_IN
    addu s0,s2,a3
    addiu s0,s0,PLANE_BYTES
    li s4,%lo(vrow)
    li t0,ROW_BYTES-1
    li t1,ROW_BYTES
    jal DMAExec
    li t2,DMA_IN
#ifdef PLASMAPONG_UPWIND_INLINE_LIMIT
    li t8,%lo(urow)
    li t9,%lo(vrow)
    addu v1,s2,a3
    jal ClampLoadedRow
    nop
#endif
    li a1,%lo(urow)
    li fp,%lo(vrow)
velocity_ready:
#ifdef PLASMAPONG_UPWIND_PREFETCH
    slti t3,s5,HEIGHT-2
    beqz t3,prefetch_done
    nop
    /* A fourth row holds y+2 while this batch still reads y-1/y/y+1. */
    addiu v0,s6,2*ROW_BYTES
    slti t3,v0,SOURCE_BYTES
    bnez t3,prefetch_slot_ready
    nop
    addiu v0,v0,-SOURCE_BYTES
prefetch_slot_ready:
    addu t8,s1,a3
    addiu t8,t8,2*ROW_BYTES
    addiu t9,v0,%lo(rows)
    move k1,zero
prefetch_plane:
    move s0,t8
    move s4,t9
    li t0,ROW_BYTES-1
    li t1,ROW_BYTES
    jal DMAExec
    li t2,DMA_IN_ASYNC
    addiu t8,t8,PLANE_BYTES
    addiu t9,t9,SOURCE_BYTES
    addiu k1,k1,1
    bne k1,k0,prefetch_plane
    nop
prefetch_done:
#endif
    move s7,zero
.macro coefficient source,dest
    vabs $v03,\source,\source
    vmudm $v04,$v03,$v20.e0
    vmadh $v04,$v03,$v20.e1
    vmadn $v05,$v22,K1
    vsar $v04,COP2_ACC_MD
    vmudn $v04,$v04,$v20.e2
    vmudl $v05,$v05,$v20.e2
    vor \dest,$v04,$v05
.endm
batch:
    addu t3,s7,a1
    lqv $v01,0,t3
    addu t3,s7,fp
    lqv $v02,0,t3
    coefficient $v01,$v08
    coefficient $v02,$v09
    addiu t5,s7,%lo(rows)
    addiu t6,s7,%lo(output)
    li t7,%lo(params)+16
    move k1,zero
channel:
    addu t4,t5,s6
    lqv $v10,0,t4
    addiu t3,t4,-2
    lqv $v11,0,t3
    lrv $v11,16,t3
    addiu t3,t4,2
    lqv $v12,0,t3
    lrv $v12,16,t3
    bnez s7,left_ready
    nop
    mfc2 t3,$v10.e0
    mtc2 t3,$v11.e0
left_ready:
    li t3,ROW_BYTES-16
    bne s7,t3,right_ready
    nop
    mfc2 t3,$v10.e7
    mtc2 t3,$v12.e7
right_ready:
    addu t4,t5,a0
    lqv $v14,0,t4
    addu t4,t5,a2
    lqv $v15,0,t4
    vge $v18,$v01,vzero
    vmrg $v11,$v11,$v12
    vge $v18,$v02,vzero
    vmrg $v12,$v14,$v15
    vsub $v11,$v11,$v10
    vsub $v12,$v12,$v10
    vmulf $v13,$v11,$v08
    vmacf $v13,$v12,$v09
    vadd $v13,$v13,$v10
    lw t8,0(t7)
    li t9,32768
    beq t8,t9,no_decay
    sll t8,t8,1
    mtc2 t8,$v24.e0
    vmudm $v13,$v13,$v24.e0
    vmadn $v26,$v25,K1
    vsar $v13,COP2_ACC_MD
no_decay:
    sqv $v13,0,t6
    addiu t5,t5,SOURCE_BYTES
    addiu t6,t6,ROW_BYTES
    addiu t7,t7,4
    addiu k1,k1,1
    bne k1,k0,channel
    nop
    addiu s7,s7,16
    li t3,ROW_BYTES
    bne s7,t3,batch
    nop
    move t8,s3
    li t9,%lo(output)
    move k1,zero
store_plane:
    addu s0,t8,a3
    move s4,t9
    li t0,ROW_BYTES-1
    li t1,ROW_BYTES
    jal DMAExec
#ifdef PLASMAPONG_UPWIND_PREFETCH
    li t2,DMA_OUT_ASYNC
#else
    li t2,DMA_OUT
#endif
    addiu t8,t8,PLANE_BYTES
    addiu t9,t9,ROW_BYTES
    addiu k1,k1,1
    bne k1,k0,store_plane
    nop
    addiu s5,s5,1
    li t3,HEIGHT
    beq s5,t3,done
    nop
    addiu a3,a3,ROW_BYTES
    addiu s6,s6,ROW_BYTES
    li t3,SOURCE_BYTES
    bne s6,t3,row
    nop
    b row
    move s6,zero
done:
#if defined(PLASMAPONG_UPWIND_PREFETCH) || defined(PLASMAPONG_UPWIND_INLINE_LIMIT)
    jal DMAWaitIdle
    nop
#endif
    j RSPQ_Loop
    nop
#ifdef PLASMAPONG_UPWIND_GPU_LIMIT
#include "rsp_upwind_limit.inc"
#endif

#ifdef PLASMAPONG_UPWIND_INLINE_LIMIT
#include "rsp_upwind_inline_limit.inc"
#endif
