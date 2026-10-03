/* Exact eight-pass Q12 Gauss-Seidel solve. Pass p processes row y-p:
 * its below row is ready from pass p-1 this tick, its current row from
 * pass p-1 last tick, and its above row from pass p last tick. Nine pressure
 * rows and eight divergence rows keep that dependency wavefront in DMEM.
 * Only divergence input and final pressure output cross RDRAM.
 */
#include <rsp_queue.inc>
    .set noreorder
    .set at
#define ROW_BYTES 192
    .data
RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand PressureSolve,12
RSPQ_EndOverlayHeader
RSPQ_EmptySavedState
    .quad 0
    .bss
    .align 4
rows: .ds.b ROW_BYTES*9
rows_end:
divrows: .ds.b ROW_BYTES*8
divrows_end:
    .text
/* a0=pressure, a1=divergence, a2=unused. DMAExec clobbers s0/s4/t0/t1/t2.
 * s1=output cursor, s2=input cursor, s3=tick, s5=pass, s6/s7=current buffers,
 * fp/k0=outer ring cursors, v0=row, v1=half-row loop count. */
PressureSolve:
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
    li t3,7
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
    li t3,8
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
    li t3,39
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
