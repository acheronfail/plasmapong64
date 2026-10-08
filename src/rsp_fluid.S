/* Exact eight-pass Q12 Gauss-Seidel solve. Pass p processes row y-p:
 * its below row is ready from pass p-1 this tick, its current row from
 * pass p-1 last tick, and its above row from pass p last tick. Nine pressure
 * rows and eight divergence rows keep that dependency wavefront in DMEM.
 * Only divergence input and final pressure output cross RDRAM.
 */
#include <rsp_queue.inc>
#include "fluid_config.h"
    .set noreorder
    .set at
#if PLASMAPONG_PRESSURE_Q3
#include "rsp_pressure_q3.inc"
#else
#define ROW_BYTES 192
    .data
RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand PressureSolve,12
RSPQ_EndOverlayHeader
RSPQ_EmptySavedState
    .quad 0
    .bss
    .align 4
rows: .ds.b ROW_BYTES*(PLASMAPONG_PRESSURE_PASSES+1)
rows_end:
divrows: .ds.b ROW_BYTES*PLASMAPONG_PRESSURE_PASSES
divrows_end:
#ifdef PLASMAPONG_PRESSURE_VECTOR_SUM
sum_words: .ds.b 16
#endif
    .text
/* a0=pressure, a1=divergence, a2=unused. DMAExec clobbers s0/s4/t0/t1/t2.
 * s1=output cursor, s2=input cursor, s3=tick, s5=pass, s6/s7=current buffers,
 * fp/k0=outer ring cursors, v0=row, v1=half-row loop count. */
PressureSolve:
#ifdef PLASMAPONG_PRESSURE_VECTOR_SUM
    li t3,1
    mtc2 t3,$v20.e0
    vxor $v20,vzero,$v20.e0
    li t3,0x55
    ctc2 t3,COP2_CTRL_VCC
#endif
    addiu s1,a0,ROW_BYTES
    addiu s2,a1,ROW_BYTES
    li s3,1
    li fp,%lo(rows)+ROW_BYTES
    li k0,%lo(divrows)+ROW_BYTES
    /* First-pass pressure starts at zero regardless of previous commands. */
    li t3,%lo(rows)
    li t4,%lo(rows_end)
zero_rows:
    sw zero,0(t3)
    addiu t3,t3,4
    bne t3,t4,zero_rows
    nop
wave_tick:
    slti t3,s3,32
    beqz t3,wave_loaded
    nop
    move s0,s2
    move s4,k0
    li t0,ROW_BYTES-1
    li t1,ROW_BYTES
    jal DMAExec
    li t2,DMA_IN
    addiu s2,s2,ROW_BYTES
wave_loaded:
    move s5,zero
    move s6,fp
    move s7,k0
wave_pass:
    subu v0,s3,s5
    blez v0,wave_next_tick
    nop
    slti t3,v0,32
    beqz t3,wave_next_pass
    nop
    addiu k1,s6,-ROW_BYTES
    li t3,%lo(rows)
    sltu t4,k1,t3
    beqz t4,above_ready
    nop
    li k1,%lo(rows_end)-ROW_BYTES
above_ready:
    addiu a2,s6,ROW_BYTES
    li t3,%lo(rows_end)
    bne a2,t3,below_ready
    nop
    li a2,%lo(rows)
below_ready:
    /* Neumann top/bottom copy equals this row's prior-pass value. Each
       current[x] is read before being overwritten, exactly like that copy. */
    li t3,1
    bne v0,t3,not_top
    nop
    move k1,s6
not_top:
    li t3,31
    bne v0,t3,not_bottom
    nop
    move a2,s6
not_bottom:
    addiu a0,s6,4
    addiu a1,s7,4
    addiu k1,k1,4
    addiu a2,a2,4
    li v1,2
    beqz s5,first_half
    move t4,zero
    lw t4,0(s6)
#ifdef PLASMAPONG_PRESSURE_VECTOR_SUM
    /* The right/above/below values are still from the prior sweep. Sum four
       independent 32-bit inputs in the vector accumulator, then preserve the
       exact serial left-neighbour recurrence and arithmetic rounding. */
.macro scalar_cell offset
    lw t5,\offset(a1)
    lw t6,\offset(k1)
    lw t7,\offset+4(a0)
    lw t8,\offset(a2)
    addu t5,t5,t6
    addu t7,t7,t8
    addu t5,t5,t7
    addu t4,t4,t5
    sra t4,t4,2
    sw t4,\offset(a0)
.endm
    scalar_cell 0
    scalar_cell 4
    scalar_cell 8
    addiu t6,k1,-4
    addiu t7,a2,-4
    li t8,%lo(sum_words)
    .set cell,12
.macro sum_load
    addiu t3,a0,cell+4
    lqv $v01,0,t3
    lrv $v01,16,t3
    lqv $v02,cell+4,s7
    lqv $v03,cell+4,t6
    lqv $v04,cell+4,t7
.endm
    sum_load
    vmudh $v05,$v20,$v01.q0
    vmadm $v05,$v20,$v01.q1
    vmadh $v05,$v20,$v02.q0
    vmadm $v05,$v20,$v02.q1
    vmadh $v05,$v20,$v03.q0
    vmadm $v05,$v20,$v03.q1
    vmadh $v05,$v20,$v04.q0
    vmadm $v05,$v20,$v04.q1
    vsar $v06,COP2_ACC_MD
    vsar $v07,COP2_ACC_LO
    vmrg $v08,$v06,$v07
    sqv $v08,0,t8
    .set cell,28
    .rept 9
    sum_load
    /* Vector arithmetic for the next four cells pairs with the preceding
       scalar recurrence. Consume every scratch word before replacing it. */
    lw t5,0(t8)
    vmudh $v05,$v20,$v01.q0
    lw t9,4(t8)
    vmadm $v05,$v20,$v01.q1
    lw a3,8(t8)
    vmadh $v05,$v20,$v02.q0
    lw v1,12(t8)
    vmadm $v05,$v20,$v02.q1
    addu t4,t4,t5
    vmadh $v05,$v20,$v03.q0
    sra t4,t4,2
    vmadm $v05,$v20,$v03.q1
    sw t4,cell-16(a0)
    vmadh $v05,$v20,$v04.q0
    addu t4,t4,t9
    vmadm $v05,$v20,$v04.q1
    sra t4,t4,2
    vsar $v06,COP2_ACC_MD
    sw t4,cell-12(a0)
    addu t4,t4,a3
    vsar $v07,COP2_ACC_LO
    sra t4,t4,2
    sw t4,cell-8(a0)
    addu t4,t4,v1
    sra t4,t4,2
    sw t4,cell-4(a0)
    vmrg $v08,$v06,$v07
    sqv $v08,0,t8
    .set cell,cell+16
    .endr
    .set part,0
    .rept 4
    lw t5,part(t8)
    addu t4,t4,t5
    sra t4,t4,2
    sw t4,156+part(a0)
    .set part,part+4
    .endr
    scalar_cell 172
    scalar_cell 176
    scalar_cell 180
    b row_done
    nop
#else
normal_half:
    .set cell,0
    .rept 23
    lw t5,cell(a1)
    lw t6,cell(k1)
    lw t7,cell+4(a0)
    lw t8,cell(a2)
    addu t5,t5,t6
    addu t7,t7,t8
    addu t5,t5,t7
    addu t4,t4,t5
    sra t4,t4,2
    sw t4,cell(a0)
    .set cell,cell+4
    .endr
    addiu a0,a0,92
    addiu a1,a1,92
    addiu k1,k1,92
    addiu v1,v1,-1
    bnez v1,normal_half
    addiu a2,a2,92
    b row_done
    nop
#endif
first_half:
    /* Initial current/below pressure is zero, including the left edge. */
    .set cell,0
    .rept 23
    lw t5,cell(a1)
    lw t6,cell(k1)
    addu t5,t5,t6
    addu t4,t4,t5
    sra t4,t4,2
    sw t4,cell(a0)
    .set cell,cell+4
    .endr
    addiu a0,a0,92
    addiu a1,a1,92
    addiu v1,v1,-1
    bnez v1,first_half
    addiu k1,k1,92
row_done:
    lw t5,4(s6)
    sw t4,ROW_BYTES-4(s6)
    sw t5,0(s6)
    li t3,PLASMAPONG_PRESSURE_PASSES-1
    bne s5,t3,wave_next_pass
    nop
    move s0,s1
    move s4,s6
    li t0,ROW_BYTES-1
    li t1,ROW_BYTES
    jal DMAExec
    li t2,DMA_OUT
    li t3,1
    bne v0,t3,store_bottom
    nop
    addiu s0,s1,-ROW_BYTES
    jal DMAExec
    li t2,DMA_OUT
store_bottom:
    li t3,31
    bne v0,t3,store_done
    nop
    addiu s0,s1,ROW_BYTES
    jal DMAExec
    li t2,DMA_OUT
store_done:
    addiu s1,s1,ROW_BYTES
wave_next_pass:
    addiu s5,s5,1
    li t3,PLASMAPONG_PRESSURE_PASSES
    beq s5,t3,wave_next_tick
    nop
    addiu s6,s6,-ROW_BYTES
    li t3,%lo(rows)
    sltu t4,s6,t3
    beqz t4,current_ready
    nop
    li s6,%lo(rows_end)-ROW_BYTES
current_ready:
    addiu s7,s7,-ROW_BYTES
    li t3,%lo(divrows)
    sltu t4,s7,t3
    beqz t4,wave_pass
    nop
    b wave_pass
    li s7,%lo(divrows_end)-ROW_BYTES
wave_next_tick:
    addiu s3,s3,1
    li t3,31+PLASMAPONG_PRESSURE_PASSES
    beq s3,t3,done
    nop
    addiu fp,fp,ROW_BYTES
    li t3,%lo(rows_end)
    bne fp,t3,pressure_cursor_ready
    nop
    li fp,%lo(rows)
pressure_cursor_ready:
    addiu k0,k0,ROW_BYTES
    li t3,%lo(divrows_end)
    bne k0,t3,wave_tick
    nop
    b wave_tick
    li k0,%lo(divrows)
done:
    j RSPQ_Loop
    nop

#endif
